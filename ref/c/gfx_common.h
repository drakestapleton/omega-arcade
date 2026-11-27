/* SPDX-License-Identifier: Apache-2.0 */
/* ref/c/gfx_common.h: Bit-exact C11 reference graphics, fixed-point trig,
 * 5x7 font, streaming FIPS 180-4 SHA-256, and CLI harness matching om/gfx_common.om. */
#ifndef OMEGA_ARCADE_GFX_COMMON_H
#define OMEGA_ARCADE_GFX_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FB_W 640
#define FB_H 360
#define FB_N 230400

static uint32_t fb[FB_N];
static uint8_t rowbuf[1920];
static int32_t sin_lut[256];
static uint32_t frame_hashes[2048];

static const int32_t SIN_Q1[65] = {
    0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189, 1285,
    1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191, 2276, 2359,
    2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229,
    3290, 3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822,
    3857, 3889, 3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085,
    4091, 4095, 4096
};

static const char *GFX_FONT5X7 =
    ">ACEIA>4<4444>>A168@ON11>11N26:BO22O@N11A>68@NAA>O124888>AA>AA>>AA?12<"
    ">AAOAAANAANAAN>A@@@A>LBAAABLO@@N@@OO@@N@@@>A@GAA>AAAOAAA>44444>7222BB<"
    "ABDHDBA@@@@@@OAKEEAAAAIECAAA>AAAAA>NAAN@@@>AAAEB=NAANDBA?@@>11NO444444"
    "AAAAAA>AAAAA:4AAAEEKAAA:4:AAAA:4444O1248@O0440440000O0000000000";

static const uint32_t SHA_K[64] = {
    1116352408u, 1899447441u, 3049323471u, 3921009573u, 961987163u, 1508970993u,
    2453635748u, 2870763221u, 3624381080u, 310598401u, 607225278u, 1426881987u,
    1925078388u, 2162078206u, 2614888103u, 3248222580u, 3835390401u, 4022224774u,
    264347078u, 604807628u, 770255983u, 1249150122u, 1555081692u, 1996064986u,
    2554220882u, 2821834349u, 2952996808u, 3210313671u, 3336571891u, 3584528711u,
    113926993u, 338241895u, 666307205u, 773529912u, 1294757372u, 1396182291u,
    1695183700u, 1986661051u, 2177026350u, 2456956037u, 2730485921u, 2820302411u,
    3259730800u, 3345764771u, 3516065817u, 3600352804u, 4094571909u, 275423344u,
    430227734u, 506948616u, 659060556u, 883997877u, 958139571u, 1322822218u,
    1537002063u, 1747873779u, 1955562222u, 2024104815u, 2227730452u, 2361852424u,
    2428436474u, 2756734187u, 3204031479u, 3329325298u
};

static const uint32_t SHA_IV[8] = {
    1779033703u, 3144134277u, 1013904242u, 2773480762u,
    1359893119u, 2600822924u, 528734635u, 1541459225u
};

typedef struct {
    uint32_t h[8];
    uint8_t blk[64];
    uint64_t pos;
    uint64_t total;
} Sha256Ctx;

static inline uint32_t rotr32(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32u - n));
}

static void sha_reset(Sha256Ctx *c) {
    for (int i = 0; i < 8; i++) c->h[i] = SHA_IV[i];
    c->pos = 0;
    c->total = 0;
}

static void sha_compress(Sha256Ctx *c) {
    uint32_t w[64], v[8];
    for (int t = 0; t < 16; t++) {
        int o = t * 4;
        w[t] = ((uint32_t)c->blk[o] << 24) | ((uint32_t)c->blk[o + 1] << 16) |
               ((uint32_t)c->blk[o + 2] << 8) | (uint32_t)c->blk[o + 3];
    }
    for (int t = 16; t < 64; t++) {
        uint32_t g0 = rotr32(w[t - 15], 7) ^ rotr32(w[t - 15], 18) ^ (w[t - 15] >> 3);
        uint32_t g1 = rotr32(w[t - 2], 17) ^ rotr32(w[t - 2], 19) ^ (w[t - 2] >> 10);
        w[t] = g1 + w[t - 7] + g0 + w[t - 16];
    }
    for (int i = 0; i < 8; i++) v[i] = c->h[i];
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(v[4], 6) ^ rotr32(v[4], 11) ^ rotr32(v[4], 25);
        uint32_t ch = (v[4] & v[5]) ^ ((~v[4]) & v[6]);
        uint32_t t1 = v[7] + s1 + ch + SHA_K[i] + w[i];
        uint32_t s0 = rotr32(v[0], 2) ^ rotr32(v[0], 13) ^ rotr32(v[0], 22);
        uint32_t mj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        uint32_t t2 = s0 + mj;
        v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
        v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) c->h[i] += v[i];
}

static inline void sha_push(Sha256Ctx *c, uint8_t b) {
    c->blk[c->pos++] = b;
    c->total++;
    if (c->pos == 64) {
        sha_compress(c);
        c->pos = 0;
    }
}

static void sha_finish(Sha256Ctx *c, uint8_t dig[32]) {
    uint64_t bits = c->total * 8u;
    sha_push(c, 0x80u);
    if (c->pos > 56) {
        while (c->pos < 64) c->blk[c->pos++] = 0;
        sha_compress(c);
        c->pos = 0;
    }
    while (c->pos < 56) c->blk[c->pos++] = 0;
    for (int j = 0; j < 8; j++) {
        c->blk[56 + j] = (uint8_t)((bits >> ((7 - j) * 8)) & 255u);
    }
    sha_compress(c);
    for (int j = 0; j < 8; j++) {
        uint32_t v = c->h[j];
        for (int q = 0; q < 4; q++) {
            dig[j * 4 + q] = (uint8_t)((v >> ((3 - q) * 8)) & 255u);
        }
    }
}

static void sha_fb_ppm(uint8_t dig[32]) {
    Sha256Ctx c;
    sha_reset(&c);
    const char *hdr = "P6\n640 360\n255\n";
    for (size_t i = 0; i < 15; i++) sha_push(&c, (uint8_t)hdr[i]);
    for (size_t i = 0; i < FB_N; i++) {
        uint32_t px = fb[i];
        sha_push(&c, (uint8_t)((px >> 16) & 255u));
        sha_push(&c, (uint8_t)((px >> 8) & 255u));
        sha_push(&c, (uint8_t)(px & 255u));
    }
    sha_finish(&c, dig);
}

static void sha_stream_u32(size_t n, uint8_t dig[32]) {
    Sha256Ctx c;
    sha_reset(&c);
    for (size_t i = 0; i < n; i++) {
        uint32_t v = frame_hashes[i];
        sha_push(&c, (uint8_t)((v >> 24) & 255u));
        sha_push(&c, (uint8_t)((v >> 16) & 255u));
        sha_push(&c, (uint8_t)((v >> 8) & 255u));
        sha_push(&c, (uint8_t)(v & 255u));
    }
    sha_finish(&c, dig);
}

static uint32_t hash_fb_u32(void) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < FB_N; i++) {
        h = (h ^ fb[i]) * 16777619u;
    }
    return h;
}

static uint32_t sample_fb_u32(uint64_t fr) {
    uint32_t h = 2166136261u;
    for (uint64_t y = 0; y < 360; y++) {
        uint64_t x = (y * 73u + fr * 17u) % 640u;
        h = (h ^ fb[y * 640u + x]) * 16777619u;
    }
    return h;
}

#define UNUSED_FN __attribute__((unused))

static inline int32_t iabs(int32_t x) { return x < 0 ? -x : x; }
static inline int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }
static inline int32_t iclamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline uint32_t uclamp255(uint32_t v) { return v > 255u ? 255u : v; }
static inline uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (uclamp255(r) << 16) | (uclamp255(g) << 8) | uclamp255(b);
}

static void gfx_init(void) {
    for (int i = 0; i < 64; i++) {
        sin_lut[i] = SIN_Q1[i];
        sin_lut[i + 64] = SIN_Q1[64 - i];
        sin_lut[i + 128] = -SIN_Q1[i];
        sin_lut[i + 192] = -SIN_Q1[64 - i];
    }
}

static inline int32_t isin1024(int32_t a) {
    int32_t m = a & 1023;
    int32_t idx = m >> 2;
    int32_t idx2 = (idx + 1) & 255;
    int32_t frac = m & 3;
    return (sin_lut[idx] * (4 - frac) + sin_lut[idx2] * frac) >> 2;
}

static inline int32_t icos1024(int32_t a) { return isin1024(a + 256); }

static inline void px_set(int32_t x, int32_t y, uint32_t c) {
    if (x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
    fb[(size_t)y * 640u + (size_t)x] = c;
}

static inline void px_add(int32_t x, int32_t y, uint32_t dr, uint32_t dg, uint32_t db) {
    if (x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
    size_t idx = (size_t)y * 640u + (size_t)x;
    uint32_t cur = fb[idx];
    uint32_t r = uclamp255(((cur >> 16) & 255u) + dr);
    uint32_t g = uclamp255(((cur >> 8) & 255u) + dg);
    uint32_t b = uclamp255((cur & 255u) + db);
    fb[idx] = (r << 16) | (g << 8) | b;
}

static inline uint32_t blend_ch(uint32_t bg, uint32_t fg, uint32_t al) {
    return (bg * (256u - al) + fg * al) >> 8;
}

static inline void px_blend(int32_t x, int32_t y, uint32_t c, uint32_t al) {
    if (x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
    size_t idx = (size_t)y * 640u + (size_t)x;
    uint32_t bg = fb[idx];
    uint32_t r = blend_ch((bg >> 16) & 255u, (c >> 16) & 255u, al);
    uint32_t g = blend_ch((bg >> 8) & 255u, (c >> 8) & 255u, al);
    uint32_t b = blend_ch(bg & 255u, c & 255u, al);
    fb[idx] = (r << 16) | (g << 8) | b;
}

static UNUSED_FN void fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
    w = imin(x + w, FB_W);
    h = imin(y + h, FB_H);
    x = imax(x, 0);
    y = imax(y, 0);
    if (x >= w || y >= h) return;
    for (int32_t yy = y; yy < h; yy++) {
        size_t row = (size_t)yy * 640u;
        for (int32_t xx = x; xx < w; xx++) fb[row + (size_t)xx] = c;
    }
}

static UNUSED_FN void blend_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c, uint32_t al) {
    w = imin(x + w, FB_W);
    h = imin(y + h, FB_H);
    x = imax(x, 0);
    y = imax(y, 0);
    if (x >= w || y >= h) return;
    for (int32_t yy = y; yy < h; yy++) {
        for (int32_t xx = x; xx < w; xx++) px_blend(xx, yy, c, al);
    }
}

static UNUSED_FN void rect_outline(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    fill_rect(x, y, w, 1, c);
    fill_rect(x, y + h - 1, w, 1, c);
    fill_rect(x, y, 1, h, c);
    fill_rect(x + w - 1, y, 1, h, c);
}

static UNUSED_FN void draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) {
    int32_t dx = iabs(x1 - x0);
    int32_t dy = -iabs(y1 - y0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t er = dx + dy;
    for (;;) {
        px_set(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = er * 2;
        if (e2 >= dy) { er += dy; x0 += sx; }
        if (e2 <= dx) { er += dx; y0 += sy; }
    }
}

static UNUSED_FN void px_glow_pt(int32_t cx, int32_t cy, uint32_t c) {
    uint32_t r = (c >> 16) & 255u, g = (c >> 8) & 255u, b = c & 255u;
    uint32_t hr = r >> 1, hg = g >> 1, hb = b >> 1;
    px_add(cx, cy, r, g, b);
    px_add(cx - 1, cy, hr, hg, hb);
    px_add(cx + 1, cy, hr, hg, hb);
    px_add(cx, cy - 1, hr, hg, hb);
    px_add(cx, cy + 1, hr, hg, hb);
}

static UNUSED_FN void draw_glow_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) {
    int32_t dx = iabs(x1 - x0);
    int32_t dy = -iabs(y1 - y0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t er = dx + dy;
    for (;;) {
        px_glow_pt(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = er * 2;
        if (e2 >= dy) { er += dy; x0 += sx; }
        if (e2 <= dx) { er += dx; y0 += sy; }
    }
}

static UNUSED_FN void ring_oct(int32_t cx, int32_t cy, int32_t dx, int32_t dy, uint32_t c) {
    uint32_t r = (c >> 16) & 255u, g = (c >> 8) & 255u, b = c & 255u;
    px_add(cx + dx, cy + dy, r, g, b);
    px_add(cx - dx, cy + dy, r, g, b);
    px_add(cx + dx, cy - dy, r, g, b);
    px_add(cx - dx, cy - dy, r, g, b);
    px_add(cx + dy, cy + dx, r, g, b);
    px_add(cx - dy, cy + dx, r, g, b);
    px_add(cx + dy, cy - dx, r, g, b);
    px_add(cx - dy, cy - dx, r, g, b);
}

static UNUSED_FN void draw_ring(int32_t cx, int32_t cy, int32_t rad, uint32_t c) {
    if (rad <= 0) return;
    int32_t x = rad, y = 0, err = 1 - rad;
    while (x >= y) {
        ring_oct(cx, cy, x, y, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

static size_t font_idx(uint8_t ch) {
    if (ch >= 48 && ch <= 57) return (size_t)(ch - 48);
    if (ch >= 65 && ch <= 90) return (size_t)(ch - 65 + 10);
    if (ch == 58) return 36;
    if (ch == 45) return 37;
    return 38;
}

static void draw_glyph(uint8_t ch, int32_t x, int32_t y, int32_t sc, uint32_t c) {
    size_t gi = font_idx(ch) * 7u;
    for (int r = 0; r < 7; r++) {
        uint32_t bits = (uint32_t)(GFX_FONT5X7[gi + (size_t)r] - 48);
        for (int col = 0; col < 5; col++) {
            if ((bits >> (4 - col)) & 1u) {
                int32_t px = x + col * sc, py = y + r * sc;
                if (sc == 1) px_set(px, py, c);
                else fill_rect(px, py, sc, sc, c);
            }
        }
    }
}

static void draw_label(const char *s, int32_t x, int32_t y, int32_t sc, uint32_t c) {
    int32_t cx = x, step = 6 * sc;
    for (size_t i = 0; s[i]; i++) {
        draw_glyph((uint8_t)s[i], cx, y, sc, c);
        cx += step;
    }
}

static void draw_uint(uint64_t val, int32_t x, int32_t y, int32_t sc, uint32_t c) {
    uint8_t buf[20];
    size_t k = 0;
    if (val == 0) buf[k++] = '0';
    else while (val > 0) { buf[k++] = (uint8_t)('0' + (val % 10u)); val /= 10u; }
    int32_t cx = x, step = 6 * sc;
    for (size_t i = 0; i < k; i++) {
        draw_glyph(buf[k - 1 - i], cx, y, sc, c);
        cx += step;
    }
}

static uint64_t read_vmhwm_kb(void) {
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) return 0;
    char line[256];
    uint64_t val = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmHWM:", 6) == 0) {
            val = strtoull(line + 6, NULL, 10);
            break;
        }
    }
    fclose(fp);
    return val;
}

static uint64_t monons(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void print_hex32(const uint8_t dig[32]) {
    for (int i = 0; i < 32; i++) printf("%02x", dig[i]);
}

static int write_ppm(void) {
    if (fwrite("P6\n640 360\n255\n", 1, 15, stdout) != 15) return 1;
    for (size_t y = 0; y < 360; y++) {
        size_t base = y * 640u;
        for (size_t x = 0; x < 640; x++) {
            uint32_t c = fb[base + x];
            rowbuf[x * 3u] = (uint8_t)((c >> 16) & 255u);
            rowbuf[x * 3u + 1u] = (uint8_t)((c >> 8) & 255u);
            rowbuf[x * 3u + 2u] = (uint8_t)(c & 255u);
        }
        if (fwrite(rowbuf, 1, 1920, stdout) != 1920) return 1;
    }
    return 0;
}

static int parse_cli(int argc, char **argv, uint64_t def_hero, int *mode_out, uint64_t *param_out) {
    *mode_out = 0;
    *param_out = 300;
    if (argc <= 1) return 1;
    if (argc > 3) return 0;
    if (strcmp(argv[1], "bench") == 0) { *mode_out = 0; *param_out = 300; }
    else if (strcmp(argv[1], "verify") == 0) { *mode_out = 1; *param_out = 300; }
    else if (strcmp(argv[1], "ppm") == 0) { *mode_out = 2; *param_out = def_hero; }
    else return 0;
    if (argc == 3) {
        const char *s = argv[2];
        if (!s[0] || strlen(s) > 10) return 0;
        uint64_t v = 0;
        for (size_t i = 0; s[i]; i++) {
            if (s[i] < '0' || s[i] > '9') return 0;
            v = v * 10u + (uint64_t)(s[i] - '0');
        }
        if (*mode_out == 2) { if (v >= 2000) return 0; }
        else { if (v == 0 || v > 2000) return 0; }
        *param_out = v;
    }
    return 1;
}

#endif
