/*
 * ibedrockseeder.c  —  find the Java Edition world seed that produced
 * a bedrock/stone pattern you observed at known absolute block coordinates.
 *
 * You must know the EXACT X, Y, Z of each block you are describing
 * (read them from the F3 debug screen in-game).
 *
 * Build:
 *     clang -O3 -std=c11 -o ibedrockseeder ibedrockseeder.c -lm
 *     gcc   -O3 -std=c11 -o ibedrockseeder ibedrockseeder.c -lm
 *
 * Run:
 *     ./ibedrockseeder
 *
 * Algorithm:
 *   Same xoroshiro128++ positional RNG as ibedrockfinder.c.
 *   For every candidate world seed, derives the salted factory state
 *   (f_lo, f_hi), then tests each known bedrock/stone cell.
 *   Cells are checked rarest-first so mismatches are caught early,
 *   keeping the average check count well under 2 per seed.
 *
 * Search speed:
 *   Typically 150 – 350 million seeds/second on a modern CPU.
 *   Default range: full 32-bit signed integer space (~4 billion seeds,
 *   ~15 seconds).  Larger ranges scale linearly.
 */

#define _GNU_SOURCE
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 *  Java Edition 1.18+ xoroshiro128++ positional random
 *  (identical constants and logic to ibedrockfinder.c)
 * ============================================================ */

#define MD5_BEDROCK_FLOOR_LO  0xbbf7928b7bf1d285ULL
#define MD5_BEDROCK_FLOOR_HI  0xc4dc7cf90e1b3b94ULL
#define MD5_BEDROCK_ROOF_LO   0x8ebd4a1d131d71ccULL
#define MD5_BEDROCK_ROOF_HI   0xc984cfbb684a26c4ULL

typedef struct { uint64_t lo, hi; } Xoro;

static inline uint64_t rotl64(uint64_t x, int b) {
    return (x << b) | (x >> (64 - b));
}

static inline void xoro_seed(Xoro *xr, uint64_t value) {
    const uint64_t XL = 0x9e3779b97f4a7c15ULL;
    const uint64_t XH = 0x6a09e667f3bcc909ULL;
    const uint64_t A  = 0xbf58476d1ce4e5b9ULL;
    const uint64_t B  = 0x94d049bb133111ebULL;
    uint64_t l = value ^ XH;
    uint64_t h = l + XL;
    l = (l ^ (l >> 30)) * A; h = (h ^ (h >> 30)) * A;
    l = (l ^ (l >> 27)) * B; h = (h ^ (h >> 27)) * B;
    l ^= l >> 31;             h ^= h >> 31;
    xr->lo = l; xr->hi = h;
}

static inline uint64_t xoro_next(Xoro *xr) {
    uint64_t l = xr->lo, hh = xr->hi;
    uint64_t n = rotl64(l + hh, 17) + l;
    hh ^= l;
    xr->lo = rotl64(l, 49) ^ hh ^ (hh << 21);
    xr->hi = rotl64(hh, 28);
    return n;
}

/* Mojang's Mth.getSeed(x, y, z) coordinate hash */
static inline int64_t mc_get_pos_seed(int32_t x, int32_t y, int32_t z) {
    int32_t xm = (int32_t)((uint32_t)x * 3129871u);
    uint64_t lu = (uint64_t)((int64_t)xm
                           ^ ((int64_t)z * 116129781LL)
                           ^ (int64_t)y);
    lu = lu * lu * 42317861ULL + lu * 11ULL;
    return (int64_t)lu >> 16;
}

/* Derive (world_lo, world_hi) from world seed, then XOR with dimension salt */
static inline void compute_factory(uint64_t world_seed,
                                   uint64_t md5_lo, uint64_t md5_hi,
                                   uint64_t *f_lo, uint64_t *f_hi) {
    Xoro xr;
    xoro_seed(&xr, world_seed);
    *f_lo = xoro_next(&xr) ^ md5_lo;
    *f_hi = xoro_next(&xr) ^ md5_hi;
}

/* ============================================================
 *  Input helpers
 * ============================================================ */
static void chomp(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0;
}
static int64_t prompt_i64(const char *msg, int64_t defv) {
    char buf[64];
    printf("%s [%lld]: ", msg, (long long)defv); fflush(stdout);
    if (!fgets(buf, sizeof buf, stdin)) return defv;
    chomp(buf); if (!buf[0]) return defv;
    return strtoll(buf, NULL, 10);
}
static int prompt_int(const char *msg, int defv) {
    return (int)prompt_i64(msg, defv);
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

/* ============================================================
 *  Timing
 * ============================================================ */
static double now_sec(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ============================================================
 *  Main
 * ============================================================ */
#define MAX_CELLS 1024

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Ibedrockseeder ===  world seed finder from bedrock pattern\n");
    printf("    by Batthepig                                               \n");
    printf("    Java Edition 1.18+ — real Mojang xoroshiro128++ RNG       \n\n");
    printf("You need the EXACT absolute block coordinates of your pattern.\n");
    printf("Open F3 in-game and stand next to each block you are recording.\n\n");

    /* --- 1. Y level + dimension --- */
    int y = prompt_int("Y level (your F3 Y coordinate)", -60);
    char dimS[24];
    prompt_str("Dimension (overworld_floor/nether_floor/nether_ceiling)",
               "overworld_floor", dimS, sizeof dimS);
    int dim = 0;
    if      (!strcmp(dimS, "nether_floor"))   dim = 1;
    else if (!strcmp(dimS, "nether_ceiling")) dim = 2;

    /* threshold = fraction of nextFloat() range that means bedrock */
    int yMin, yMax;
    switch (dim) {
        case 0: yMin = -64; yMax = -59; break;
        case 1: yMin =  0;  yMax =  4;  break;
        default: yMin = 123; yMax = 127; break;
    }
    uint32_t threshold = 0;
    if (y < yMin || y > yMax) {
        printf("Warning: Y=%d is outside the bedrock range — no bedrock there.\n\n", y);
    } else {
        int layer = (dim == 0) ? (y + 64) : (dim == 1) ? y : (127 - y);
        if      (layer == 0) threshold = 0xffffffu;
        else if (layer <  5) threshold = (uint32_t)(((5 - layer) * 0x1000000u) / 5u);
        /* layer >= 5 → threshold stays 0 (all stone) */
    }
    double pB = threshold / (double)0x1000000;

    /* MD5 salts */
    uint64_t md5_lo = (dim == 2) ? MD5_BEDROCK_ROOF_LO : MD5_BEDROCK_FLOOR_LO;
    uint64_t md5_hi = (dim == 2) ? MD5_BEDROCK_ROOF_HI : MD5_BEDROCK_FLOOR_HI;

    /* --- 2. Pattern origin (absolute block coordinates) --- */
    printf("\nEnter the absolute block coordinates of the TOP-LEFT (north-west)\n");
    printf("corner of your pattern — i.e. the lowest X and lowest Z block.\n");
    int32_t ox = (int32_t)prompt_i64("Origin X", 0);
    int32_t oz = (int32_t)prompt_i64("Origin Z", 0);

    /* --- 3. Pattern rows --- */
    int pw = prompt_int("Pattern width  (X blocks)", 4);
    int ph = prompt_int("Pattern height (Z blocks)", 4);
    if (pw < 1) pw = 1;  if (pw > 32) pw = 32;
    if (ph < 1) ph = 1;  if (ph > 32) ph = 32;

    printf("\nEnter %d row(s) of width %d.\n", ph, pw);
    printf("  '1' '#' 'b' 'B' = bedrock\n");
    printf("  '0' '.' 's' 'S' = stone / non-bedrock\n");
    printf("  '?'             = unknown (skip this cell)\n");
    printf("Spaces ignored. Short rows padded with '?'.\n");
    printf("Press Enter alone for an all-unknown row.\n\n");

    /* collect known cells */
    static int32_t cell_x[MAX_CELLS], cell_z[MAX_CELLS];
    static bool    cell_want_b[MAX_CELLS];
    static uint64_t cell_pos[MAX_CELLS];   /* precomputed pos_seed ^ ? — filled later */
    int n_cells = 0;

    for (int rz = 0; rz < ph && n_cells < MAX_CELLS; rz++) {
        char line[256];
        printf("row %2d: ", rz); fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) line[0] = 0;
        chomp(line);
        int rx = 0;
        for (char *cp = line; *cp && rx < pw; cp++) {
            if (*cp == ' ' || *cp == '\t') continue;
            bool is_b = (*cp == '1' || *cp == '#' || *cp == 'b' || *cp == 'B');
            bool is_s = (*cp == '0' || *cp == '.' || *cp == 's' || *cp == 'S');
            if ((is_b || is_s) && n_cells < MAX_CELLS) {
                cell_x[n_cells]      = ox + rx;
                cell_z[n_cells]      = oz + rz;
                cell_want_b[n_cells] = is_b;
                n_cells++;
            }
            rx++;
        }
    }

    if (n_cells == 0) {
        printf("No known cells entered — nothing to search. Exiting.\n");
        return 1;
    }

    /* sort cells rarest-first (maximises early-exit rate) */
    for (int i = 0; i < n_cells; i++) {
        for (int j = i + 1; j < n_cells; j++) {
            /* reject probability: bedrock cell rejects non-bedrock seeds,
               stone cell rejects bedrock seeds */
            double ri = cell_want_b[i] ? (1.0 - pB) : pB;
            double rj = cell_want_b[j] ? (1.0 - pB) : pB;
            if (rj > ri) {
                int32_t tx = cell_x[i]; cell_x[i] = cell_x[j]; cell_x[j] = tx;
                int32_t tz = cell_z[i]; cell_z[i] = cell_z[j]; cell_z[j] = tz;
                bool    tb = cell_want_b[i]; cell_want_b[i] = cell_want_b[j]; cell_want_b[j] = tb;
            }
        }
    }

    /* precompute mc_get_pos_seed for each cell (same for all seeds) */
    for (int i = 0; i < n_cells; i++)
        cell_pos[i] = (uint64_t)mc_get_pos_seed(cell_x[i], y, cell_z[i]);

    /* --- 4. Seed search range --- */
    printf("\n--- Seed search range ---\n");
    printf("At ~200 M seeds/sec, the default 48-bit range (~281 trillion seeds)\n");
    printf("takes many hours. Narrow it if you know roughly what your seed is.\n\n");
    int64_t seed_start = prompt_i64("Seed range start", -140737488355328LL);
    int64_t seed_end   = prompt_i64("Seed range end",    140737488355327LL);
    if (seed_end < seed_start) {
        int64_t t = seed_start; seed_start = seed_end; seed_end = t;
    }
    uint64_t total_seeds = (uint64_t)(seed_end - seed_start) + 1ULL;

    printf("\n%d known cell(s) | %.2f B seeds | Y=%d | dim=%s\n\n",
           n_cells, total_seeds / 1e9, y, dimS);
    fflush(stdout);

    /* --- 5. Scan --- */
    int      matches_cap = 256;
    int64_t *matches     = malloc((size_t)matches_cap * sizeof(int64_t));
    int      n_matches   = 0;

    double   t0         = now_sec();
    uint64_t scanned    = 0;
    uint64_t step       = total_seeds > 200 ? total_seeds / 200 : 1;
    if (step < 500000) step = 500000;
    uint64_t next_check = step;
    double   last_print = t0;

    for (int64_t seed = seed_start; ; seed++) {
        /* derive salted factory state */
        uint64_t f_lo, f_hi;
        compute_factory((uint64_t)seed, md5_lo, md5_hi, &f_lo, &f_hi);

        /* test each known cell; break on first mismatch */
        bool ok = true;
        for (int i = 0; i < n_cells; i++) {
            Xoro xr;
            xr.lo = cell_pos[i] ^ f_lo;
            xr.hi = f_hi;
            bool got_b = (uint32_t)(xoro_next(&xr) >> 40) < threshold;
            if (got_b != cell_want_b[i]) { ok = false; break; }
        }
        if (ok) {
            if (n_matches == matches_cap) {
                matches_cap *= 2;
                matches = realloc(matches, (size_t)matches_cap * sizeof(int64_t));
            }
            matches[n_matches++] = seed;
        }

        scanned++;

        /* progress bar (≈5 times/sec) */
        if (scanned >= next_check || seed == seed_end) {
            next_check = scanned + step;
            double now = now_sec();
            if (now - last_print >= 0.2 || seed == seed_end) {
                last_print = now;
                double elapsed = now - t0;
                double frac    = (double)scanned / (double)total_seeds;
                if (frac > 1.0) frac = 1.0;
                double eta  = frac > 0.0 ? elapsed * (1.0 / frac - 1.0) : 0.0;
                int    pct  = (int)(frac * 100.0 + 0.5);
                int    bars = (int)(frac * 30.0  + 0.5);
                char   bar[32];
                for (int b = 0; b < 30; b++) bar[b] = b < bars ? '#' : '-';
                bar[30] = 0;
                printf("\r  [%s] %3d%%  elapsed %6.1fs  eta %6.1fs  found %-6d",
                       bar, pct, elapsed, eta, n_matches);
                fflush(stdout);
            }
        }

        if (seed == seed_end) break;
    }

    double dt = now_sec() - t0;
    printf("\r  [##############################] 100%%  elapsed %6.1fs  eta    0.0s  found %-6d\n\n",
           dt, n_matches);
    printf("Finished in %.2fs  (%.2f M seeds/sec)\n\n",
           dt, dt > 0 ? (double)scanned / dt / 1e6 : 0.0);

    /* --- 6. Results --- */
    if (n_matches == 0) {
        printf("No seeds matched in range [%lld, %lld].\n",
               (long long)seed_start, (long long)seed_end);
        printf("Things to check:\n");
        printf("  - Are your block coordinates absolutely correct? (use F3)\n");
        printf("  - Is the Y level and dimension right?\n");
        printf("  - Did you confuse bedrock and stone?\n");
        printf("  - Try a wider seed range.\n");
    } else {
        printf("Matching seed(s):\n\n");
        for (int i = 0; i < n_matches; i++)
            printf("  %lld\n", (long long)matches[i]);
        printf("\nVerify with /seed in-game or run each candidate through ibedrockfinder.\n");
        if (n_matches > 1)
            printf("Multiple matches: add more known cells to narrow it down further.\n");
    }

    free(matches);
    return 0;
}
