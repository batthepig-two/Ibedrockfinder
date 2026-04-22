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
 * The program prompts for the pattern (rows of '#' bedrock, '.' stone,
 * '?' unknown), the edition / dimension / Y level, the search center and
 * radius, and whether to match all 8 rotations + reflections, then scans
 * the bedrock layer as fast as a single CPU thread can — early-exiting
 * each anchor on the rarest cell first to maximize cull rate, using
 * pure 32-bit integer math identical to the reference web app's hash.
 *
 * Hash function is bit-for-bit compatible with the JS version, so hits
 * agree exactly between this CLI and the browser.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATTERN  32
#define MAX_KNOWN    (MAX_PATTERN * MAX_PATTERN)
#define MAX_VARIANTS 8

/* ---- hash mixers (match JS reference exactly) ---- */
static inline uint32_t mix_java(int32_t x, int32_t y, int32_t z) {
    uint32_t h = (uint32_t)x * 0x85ebca6bu;
    h ^= (uint32_t)z * 0xc2b2ae35u;
    h ^= (uint32_t)y * 0x27d4eb2fu;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}
static inline uint32_t mix_be(int32_t x, int32_t y, int32_t z,
                              uint32_t lo, uint32_t hi) {
    uint32_t h = (uint32_t)x * 0x85ebca6bu;
    h ^= (uint32_t)z * 0xc2b2ae35u;
    h ^= (uint32_t)y * 0x27d4eb2fu;
    h ^= lo;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= hi;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

/* ---- types ---- */
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

/* ---- pattern input ---- */
static void read_pattern(Pattern *p) {
    p->w = prompt_int("Pattern width",  2);
    p->h = prompt_int("Pattern height", 16);
    if (p->w < 1) p->w = 1;
    if (p->h < 1) p->h = 1;
    if (p->w > MAX_PATTERN) p->w = MAX_PATTERN;
    if (p->h > MAX_PATTERN) p->h = MAX_PATTERN;
    printf("\nEnter %d row(s) of width %d.  '#'=bedrock, '.'=stone, '?'=unknown.\n",
           p->h, p->w);
    printf("Spaces ignored. Short rows padded with '?'. Press Enter for all-unknown row.\n\n");
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
    /* sort by reject probability descending: cells that reject more often
     * go first so the inner loop bails out faster on misses.
     * P(reject | want bedrock) = 1 - pB.  P(reject | want stone) = pB.
     * Bubble sort is fine (n is tiny). */
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
    printf("    by Batthepig                                              \n\n");

    /* 1) pattern */
    Pattern base;
    read_pattern(&base);

    /* 2) edition */
    char edS[16];
    prompt_str("\nEdition (java/bedrock)", "java", edS, sizeof edS);
    int edition = (!strcmp(edS, "bedrock") || !strcmp(edS, "b")) ? 1 : 0;
    int64_t seed = 0;
    if (edition == 1) seed = prompt_i64("Seed", 0);

    /* 3) dimension + Y */
    char dimS[24];
    prompt_str("Dim (overworld_floor/nether_floor/nether_ceiling)",
               "overworld_floor", dimS, sizeof dimS);
    int dim = 0;
    if      (!strcmp(dimS, "nether_floor"))   dim = 1;
    else if (!strcmp(dimS, "nether_ceiling")) dim = 2;
    int y = prompt_int("Y level", -60);

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

    /* 6) variants */
    Pattern variants[MAX_VARIANTS];
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

    uint32_t seed_lo = (uint32_t)((uint64_t)seed & 0xffffffffu);
    uint32_t seed_hi = (uint32_t)(((uint64_t)seed >> 32) & 0xffffffffu);

    int x_min = cx - radius, x_max = cx + radius;
    int z_min = cz - radius, z_max = cz + radius;

    /* 7) anchor count for progress */
    int64_t total_anchors = 0;
    for (int p = 0; p < nv; p++) {
        if (always_b && variants[p].needs_s) continue;
        if (never_b  && variants[p].needs_b) continue;
        int64_t xr = x_max - x_min - variants[p].w + 2;
        int64_t zr = z_max - z_min - variants[p].h + 2;
        if (xr < 1 || zr < 1) continue;
        total_anchors += xr * zr;
    }

    printf("\nSearching %lld anchors across %d orientation(s)... ",
           (long long)total_anchors, nv);
    fflush(stdout);

    /* 8) the inner loop */
    Heap top; heap_init(&top, max_results);
    double t0 = now_sec();
    int64_t scanned = 0;
    int64_t dot_step = total_anchors / 40; if (dot_step < 1) dot_step = 1;
    int64_t next_dot = dot_step;

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
                    uint32_t hh = (edition == 0)
                        ? mix_java(ax, y, az)
                        : mix_be(ax, y, az, seed_lo, seed_hi);
                    bool isB = ((hh & 0xffffffu) < threshold);
                    if (isB != (kc[i].v == 1)) { ok = false; break; }
                }
                if (ok) {
                    double dxf = (double)(x - cx), dzf = (double)(z - cz);
                    Hit h = { x, z, (uint8_t)p, sqrt(dxf*dxf + dzf*dzf) };
                    heap_push(&top, h);
                }
            }
            scanned += (z_end - z_min + 1);
            if (scanned >= next_dot) {
                fputc('.', stdout); fflush(stdout);
                next_dot += dot_step;
            }
        }
    }
    double dt = now_sec() - t0;
    printf("\n\nFinished in %.2fs   (%.2f M anchors/sec)\n",
           dt, dt > 0 ? scanned / dt / 1e6 : 0.0);

    /* 9) sort & print */
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
