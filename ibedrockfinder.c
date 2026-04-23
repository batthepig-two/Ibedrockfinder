/*
 * ibedrockfinder.c — single-file interactive Minecraft bedrock pattern
 * finder, built for a-Shell on iPhone (works on any POSIX terminal too).
 *
 * Build (in a-Shell, or anywhere with a C11 compiler):
 *     clang -O3 -std=c11 -o ibedrockfinder ibedrockfinder.c
 *
 * Run:
 *     ./ibedrockfinder
 *
 * Algorithms:
 *
 *   Java Edition 1.18+   — REAL world-gen algorithm.
 *     Uses Mojang's xoroshiro128++ XoroshiroPositionalRandomFactory
 *     derived from the world seed XORed with the MD5 of the salt
 *     "minecraft:bedrock_floor" / "minecraft:bedrock_roof", combined
 *     with the standard Mth.getSeed(x, y, z) position mixer.  Constants
 *     and seed pipeline match the cubiomes reference library, so hits
 *     should match what you actually see in your Java world.
 *
 *   Bedrock Edition       — APPROXIMATE.
 *     Mojang has not published the C++ Bedrock Edition world-gen code,
 *     and there is no community-reverse-engineered reference of the same
 *     quality as cubiomes.  This program uses a fast 64-bit-seed-mixed
 *     positional hash with the same per-layer probabilities (5/5, 4/5,
 *     3/5, 2/5, 1/5).  The pattern statistics (frequency of any given
 *     shape) match the real game, but exact coordinates of any specific
 *     hit will NOT match a Bedrock Edition world.  Use Java mode for
 *     coordinate-accurate searches.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cubiomes/rng.h"

#define MAX_PATTERN  32
#define MAX_KNOWN    (MAX_PATTERN * MAX_PATTERN)
#define MAX_VARIANTS 8

/* ============================================================ */
/*  Java Edition 1.18+ — real xoroshiro128++ positional random  */
/* ============================================================ */

/* MD5("minecraft:bedrock_floor") = bbf7928b7bf1d285c4dc7cf90e1b3b94
 * MD5("minecraft:bedrock_roof")  = 8ebd4a1d131d71ccc984cfbb684a26c4
 *
 * Mojang treats the first 8 bytes of the MD5 as a big-endian uint64
 * which XORs the factory's seed-low, and the next 8 bytes the same way
 * for seed-high.  (Same convention as cubiomes' md5_octave table.) */
#define MD5_BEDROCK_FLOOR_LO  0xbbf7928b7bf1d285ULL
#define MD5_BEDROCK_FLOOR_HI  0xc4dc7cf90e1b3b94ULL
#define MD5_BEDROCK_ROOF_LO   0x8ebd4a1d131d71ccULL
#define MD5_BEDROCK_ROOF_HI   0xc984cfbb684a26c4ULL

/* xoroshiro128++ types and functions are provided by cubiomes/rng.h:
 *   Xoroshiro  — struct with .lo, .hi (u64)
 *   xSetSeed(&xr, seed)   — Stafford13-mixed constructor (matches Mojang)
 *   xNextLong(&xr)        — xoroshiro128++ next u64
 *   rotl64(x, b)          — 64-bit left-rotate
 * These are identical to Mojang's XoroshiroRandomSource. */


/* Mojang's Mth.getSeed(x, y, z): coordinate hash used by every
 * XoroshiroPositionalRandom .at(x, y, z) call. */
static inline int64_t mc_get_pos_seed(int32_t x, int32_t y, int32_t z) {
    /* x * 3129871 is performed in 32-bit signed (allowed to wrap) */
    int32_t xm = (int32_t)((uint32_t)x * 3129871u);
    uint64_t lu = (uint64_t)((int64_t)xm
                           ^ ((int64_t)z * 116129781LL)
                           ^ (int64_t)y);
    lu = lu * lu * 42317861ULL + lu * 11ULL;
    return (int64_t)lu >> 16;   /* arithmetic right shift */
}

/* World positional factory (lo, hi) derived from the world seed.
 * Mojang: new XoroshiroRandomSource(seed).forkPositional()
 *      → (nextLong(), nextLong()) of that source. */
static void java_world_factory(uint64_t world_seed,
                               uint64_t *out_lo, uint64_t *out_hi) {
    Xoroshiro xr;
    xSetSeed(&xr, world_seed);
    *out_lo = xNextLong(&xr);
    *out_hi = xNextLong(&xr);
}

/* fromHashOf("minecraft:bedrock_floor") on a positional factory:
 * XOR the factory's (lo, hi) with the MD5 halves of the salt. */
static inline void java_apply_salt(uint64_t world_lo, uint64_t world_hi,
                                   uint64_t md5_lo, uint64_t md5_hi,
                                   uint64_t *out_lo, uint64_t *out_hi) {
    *out_lo = world_lo ^ md5_lo;
    *out_hi = world_hi ^ md5_hi;
}

/* The bedrock test for one block at (x, y, z) under a salted positional
 * factory.  Returns true iff the block would be bedrock when its
 * nextFloat() (top 24 bits of next long) is below the threshold. */
static inline bool is_bedrock_java(uint64_t f_lo, uint64_t f_hi,
                                   int32_t x, int32_t y, int32_t z,
                                   uint32_t threshold24) {
    Xoroshiro xr;
    xr.lo = (uint64_t)mc_get_pos_seed(x, y, z) ^ f_lo;
    xr.hi = f_hi;
    uint32_t top24 = (uint32_t)(xNextLong(&xr) >> 40);
    return top24 < threshold24;
}

/* ============================================================ */
/*  Bedrock Edition — approximate (see file header)             */
/* ============================================================ */
static inline bool is_bedrock_be(int32_t x, int32_t y, int32_t z,
                                 uint32_t seed_lo, uint32_t seed_hi,
                                 uint32_t threshold24) {
    uint32_t h = (uint32_t)x * 0x85ebca6bu;
    h ^= (uint32_t)z * 0xc2b2ae35u;
    h ^= (uint32_t)y * 0x27d4eb2fu;
    h ^= seed_lo;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= seed_hi;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return (h & 0xffffffu) < threshold24;
}

/* ============================================================ */
/*  Pattern types                                               */
/* ============================================================ */
typedef struct { int dx, dz; uint8_t v; } Known;
typedef struct {
    int w, h;
    uint8_t cells[MAX_PATTERN * MAX_PATTERN]; /* 0=stone 1=bedrock 2=wild */
    Known   known[MAX_KNOWN];
    int     n_known;
    bool    needs_b, needs_s;
} Pattern;

/* ---- prompt helpers ---- */
static void chomp(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0;
}
static int prompt_int(const char *msg, int defv) {
    char buf[64];
    printf("%s [%d]: ", msg, defv); fflush(stdout);
    if (!fgets(buf, sizeof buf, stdin)) return defv;
    chomp(buf); if (!buf[0]) return defv;
    return atoi(buf);
}
static int64_t prompt_i64(const char *msg, int64_t defv) {
    char buf[64];
    printf("%s [%lld]: ", msg, (long long)defv); fflush(stdout);
    if (!fgets(buf, sizeof buf, stdin)) return defv;
    chomp(buf); if (!buf[0]) return defv;
    return strtoll(buf, NULL, 10);
}
static void prompt_str(const char *msg, const char *defv,
                       char *out, size_t cap) {
    printf("%s [%s]: ", msg, defv); fflush(stdout);
    if (!fgets(out, (int)cap, stdin)) {
        strncpy(out, defv, cap); out[cap-1] = 0; return;
    }
    chomp(out);
    if (!out[0]) { strncpy(out, defv, cap); out[cap-1] = 0; }
}

/* ---- pattern row input (size already chosen) ---- */
static void read_pattern_rows(Pattern *p) {
    if (p->w < 1) p->w = 1;
    if (p->h < 1) p->h = 1;
    if (p->w > MAX_PATTERN) p->w = MAX_PATTERN;
    if (p->h > MAX_PATTERN) p->h = MAX_PATTERN;
    printf("\nEnter %d row(s) of width %d.  '1'=bedrock, '0'=stone, '?'=unknown.\n",
           p->h, p->w);
    printf("(You can also use '#' for bedrock and '.' for stone.)\n");
    printf("Spaces ignored. Short rows padded with '?'. Press Enter for an all-unknown row.\n\n");
    for (int z = 0; z < p->h; z++) {
        char line[256];
        printf("row %2d: ", z); fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) line[0] = 0;
        chomp(line);
        int x = 0;
        for (char *c = line; *c && x < p->w; c++) {
            if (*c == ' ' || *c == '\t') continue;
            uint8_t v;
            if (*c == '#' || *c == 'B' || *c == 'b' || *c == '1')      v = 1;
            else if (*c == '.' || *c == 'S' || *c == 's' || *c == '0') v = 0;
            else                                                       v = 2;
            p->cells[z * p->w + x++] = v;
        }
        while (x < p->w) p->cells[z * p->w + x++] = 2;
    }
}

/* ---- orient + dedupe ---- */
static void orient_pattern(const Pattern *src, int o, Pattern *dst) {
    bool flip = o >= 4;
    int rot = o & 3;
    int sw = src->w, sh = src->h;
    int dw = (rot & 1) ? sh : sw;
    int dh = (rot & 1) ? sw : sh;
    dst->w = dw; dst->h = dh;
    for (int dz = 0; dz < dh; dz++) for (int dx = 0; dx < dw; dx++) {
        int sx, sz;
        switch (rot) {
            default:
            case 0: sx = dx;          sz = dz;          break;
            case 1: sx = dz;          sz = sh - 1 - dx; break;
            case 2: sx = sw - 1 - dx; sz = sh - 1 - dz; break;
            case 3: sx = sw - 1 - dz; sz = dx;          break;
        }
        if (flip) sx = sw - 1 - sx;
        dst->cells[dz * dw + dx] = src->cells[sz * sw + sx];
    }
}
static bool pat_eq(const Pattern *a, const Pattern *b) {
    if (a->w != b->w || a->h != b->h) return false;
    return memcmp(a->cells, b->cells, (size_t)a->w * a->h) == 0;
}

/* ---- known-cell list, sorted rarest-first to maximize early-out ---- */
static void build_known(Pattern *p, double pB) {
    p->n_known = 0;
    p->needs_b = false; p->needs_s = false;
    for (int z = 0; z < p->h; z++) for (int x = 0; x < p->w; x++) {
        uint8_t v = p->cells[z * p->w + x];
        if (v == 2) continue;
        p->known[p->n_known++] = (Known){ x, z, v };
        if (v == 1) p->needs_b = true; else p->needs_s = true;
    }
    /* sort by reject probability descending */
    for (int i = 0; i < p->n_known; i++)
        for (int j = i + 1; j < p->n_known; j++) {
            double ri = p->known[i].v ? (1.0 - pB) : pB;
            double rj = p->known[j].v ? (1.0 - pB) : pB;
            if (rj > ri) { Known t = p->known[i]; p->known[i] = p->known[j]; p->known[j] = t; }
        }
}

/* ---- top-N max-heap by distance (root = current worst kept) ---- */
typedef struct { int32_t x, z; uint8_t o; double d; } Hit;
typedef struct { Hit *a; int n, cap; } Heap;
static void heap_init(Heap *h, int cap) {
    h->a = (Hit *)malloc(sizeof(Hit) * (size_t)cap);
    h->n = 0; h->cap = cap;
}
static inline void heap_swap(Hit *a, Hit *b) { Hit t = *a; *a = *b; *b = t; }
static void heap_up(Heap *h, int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->a[p].d < h->a[i].d) { heap_swap(&h->a[p], &h->a[i]); i = p; }
        else break;
    }
}
static void heap_down(Heap *h, int i) {
    for (;;) {
        int l = 2*i + 1, r = 2*i + 2, b = i;
        if (l < h->n && h->a[l].d > h->a[b].d) b = l;
        if (r < h->n && h->a[r].d > h->a[b].d) b = r;
        if (b == i) break;
        heap_swap(&h->a[b], &h->a[i]);
        i = b;
    }
}
static void heap_push(Heap *h, Hit hit) {
    if (h->n < h->cap) { h->a[h->n++] = hit; heap_up(h, h->n - 1); }
    else if (hit.d < h->a[0].d) { h->a[0] = hit; heap_down(h, 0); }
}
static int hit_cmp(const void *a, const void *b) {
    double da = ((const Hit *)a)->d, db = ((const Hit *)b)->d;
    return da < db ? -1 : da > db ? 1 : 0;
}

/* ---- timing ---- */
static double now_sec(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ---- main ---- */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Ibedrockfinder ===  single-file C bedrock pattern finder\n");
    printf("    by Batthepig                                              \n");
    printf("    Java mode = real Mojang xoroshiro128++ + bedrock-salt MD5\n");
    printf("    Bedrock Edition mode = approximate (see source header)   \n\n");

    /* 1) Y level + dimension first (Y is what you see on F3 in-game) */
    int y = prompt_int("Y level", -60);
    char dimS[24];
    prompt_str("Dimension (overworld_floor/nether_floor/nether_ceiling)",
               "overworld_floor", dimS, sizeof dimS);
    int dim = 0;
    if      (!strcmp(dimS, "nether_floor"))   dim = 1;
    else if (!strcmp(dimS, "nether_ceiling")) dim = 2;

    /* 2) pattern size + rows */
    static Pattern base;
    base.w = prompt_int("Pattern width  (X size)", 2);
    base.h = prompt_int("Pattern length (Z size)", 16);
    read_pattern_rows(&base);

    /* 3) edition + seed (both editions are seed-dependent now) */
    char edS[16];
    prompt_str("\nEdition (java/bedrock)", "java", edS, sizeof edS);
    int edition = (!strcmp(edS, "bedrock") || !strcmp(edS, "b")) ? 1 : 0;
    int64_t seed = prompt_i64("World seed", 0);

    /* 4) area */
    int cx = prompt_int("Center X", 0);
    int cz = prompt_int("Center Z", 0);
    int radius = prompt_int("Radius (blocks)", 5000);
    int rotations = prompt_int("Match all 8 rotations? (1=yes, 0=no)", 1);
    int max_results = prompt_int("Max results to keep (closest first)", 200);

    /* 5) threshold from layer */
    int yMin, yMax;
    switch (dim) {
        case 0: yMin = -64; yMax = -59; break;
        case 1: yMin = 0;   yMax = 4;   break;
        default: yMin = 123; yMax = 127; break;
    }
    bool always_b = false, never_b = false;
    uint32_t threshold = 0;
    if (y < yMin || y > yMax) {
        never_b = true;
    } else {
        int layer = (dim == 0) ? (y + 64) : (dim == 1) ? y : (127 - y);
        if      (layer == 0)  { always_b = true; threshold = 0xffffffu; }
        else if (layer >= 5)  { never_b  = true; }
        else                  { threshold = (uint32_t)(((5 - layer) * 0x1000000u) / 5u); }
    }
    double pB = threshold / (double)0x1000000;

    /* 6) variants  (static: ~100 KB total — keep off the WASM stack) */
    static Pattern variants[MAX_VARIANTS];
    int nv = 0;
    if (rotations) {
        for (int o = 0; o < 8; o++) {
            Pattern v; orient_pattern(&base, o, &v);
            bool dup = false;
            for (int j = 0; j < nv; j++) if (pat_eq(&v, &variants[j])) { dup = true; break; }
            if (!dup) variants[nv++] = v;
        }
    } else {
        variants[nv++] = base;
    }
    for (int i = 0; i < nv; i++) build_known(&variants[i], pB);

    /* 7) precompute edition-specific PRNG state from seed */
    uint64_t java_f_lo = 0, java_f_hi = 0;
    if (edition == 0) {
        uint64_t world_lo, world_hi;
        java_world_factory((uint64_t)seed, &world_lo, &world_hi);
        /* dim 2 (nether ceiling) uses bedrock_roof; floors use bedrock_floor */
        if (dim == 2) {
            java_apply_salt(world_lo, world_hi,
                            MD5_BEDROCK_ROOF_LO, MD5_BEDROCK_ROOF_HI,
                            &java_f_lo, &java_f_hi);
        } else {
            java_apply_salt(world_lo, world_hi,
                            MD5_BEDROCK_FLOOR_LO, MD5_BEDROCK_FLOOR_HI,
                            &java_f_lo, &java_f_hi);
        }
    }
    uint32_t be_seed_lo = (uint32_t)((uint64_t)seed & 0xffffffffu);
    uint32_t be_seed_hi = (uint32_t)(((uint64_t)seed >> 32) & 0xffffffffu);

    int x_min = cx - radius, x_max = cx + radius;
    int z_min = cz - radius, z_max = cz + radius;

    /* 8) anchor count for progress */
    int64_t total_anchors = 0;
    for (int p = 0; p < nv; p++) {
        if (always_b && variants[p].needs_s) continue;
        if (never_b  && variants[p].needs_b) continue;
        int64_t xr = x_max - x_min - variants[p].w + 2;
        int64_t zr = z_max - z_min - variants[p].h + 2;
        if (xr < 1 || zr < 1) continue;
        total_anchors += xr * zr;
    }

    printf("\n%s mode | seed %lld | dim %s | Y %d (layer %s) | %lld anchors x %d orient(s)\n",
           edition == 0 ? "Java (real PRNG)" : "Bedrock Edition (approximate)",
           (long long)seed, dimS, y,
           always_b ? "0 always-bedrock" : never_b ? "out-of-range" : "valid",
           (long long)total_anchors, nv);
    if (edition == 1) {
        printf("Note: Bedrock Edition results are pattern-statistical, not coordinate-exact.\n");
    }
    printf("\n");
    fflush(stdout);

    /* 9) the inner loop */
    Heap top; heap_init(&top, max_results);
    double t0 = now_sec();
    int64_t scanned = 0;
    int64_t step = total_anchors / 200; if (step < 100000) step = 100000;
    int64_t next_check = step;
    double  last_print = t0;

    for (int p = 0; p < nv; p++) {
        Pattern *pat = &variants[p];
        if (always_b && pat->needs_s) continue;
        if (never_b  && pat->needs_b) continue;
        const Known *kc = pat->known;
        int n = pat->n_known;
        int x_end = x_max - pat->w + 1;
        int z_end = z_max - pat->h + 1;
        for (int x = x_min; x <= x_end; x++) {
            for (int z = z_min; z <= z_end; z++) {
                bool ok = true;
                for (int i = 0; i < n; i++) {
                    int ax = x + kc[i].dx, az = z + kc[i].dz;
                    bool isB = (edition == 0)
                        ? is_bedrock_java(java_f_lo, java_f_hi, ax, y, az, threshold)
                        : is_bedrock_be(ax, y, az, be_seed_lo, be_seed_hi, threshold);
                    if (isB != (kc[i].v == 1)) { ok = false; break; }
                }
                if (ok) {
                    double dxf = (double)(x - cx), dzf = (double)(z - cz);
                    Hit h = { x, z, (uint8_t)p, sqrt(dxf*dxf + dzf*dzf) };
                    heap_push(&top, h);
                }
            }
            scanned += (z_end - z_min + 1);
            if (scanned >= next_check) {
                next_check = scanned + step;
                double now = now_sec();
                if (now - last_print >= 0.2) {
                    last_print = now;
                    double elapsed = now - t0;
                    double frac = (double)scanned / (double)total_anchors;
                    if (frac > 1.0) frac = 1.0;
                    double eta = frac > 0 ? elapsed * (1.0 / frac - 1.0) : 0.0;
                    int pct = (int)(frac * 100.0 + 0.5);
                    int bars = (int)(frac * 30.0 + 0.5);
                    char bar[32];
                    for (int b = 0; b < 30; b++) bar[b] = b < bars ? '#' : '-';
                    bar[30] = 0;
                    printf("\r  [%s] %3d%%  elapsed %6.1fs  eta %6.1fs  hits %d   ",
                           bar, pct, elapsed, eta, top.n);
                    fflush(stdout);
                }
            }
        }
    }
    double dt = now_sec() - t0;
    printf("\r  [##############################] 100%%  elapsed %6.1fs  eta    0.0s  hits %d   \n\n",
           dt, top.n);
    printf("Finished in %.2fs   (%.2f M anchors/sec)\n",
           dt, dt > 0 ? scanned / dt / 1e6 : 0.0);

    /* 10) sort & print */
    qsort(top.a, top.n, sizeof(Hit), hit_cmp);
    printf("Kept %d closest hits:\n\n", top.n);
    printf("    %-8s %-8s %-3s %-10s\n", "X", "Z", "O", "DIST");
    printf("    -------- -------- --- ----------\n");
    for (int i = 0; i < top.n; i++) {
        printf("    %-8d %-8d %-3u %-10.1f\n",
               top.a[i].x, top.a[i].z, top.a[i].o, top.a[i].d);
    }
    free(top.a);
    return 0;
}
