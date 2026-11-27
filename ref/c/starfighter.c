/* SPDX-License-Identifier: Apache-2.0 */
/* ref/c/starfighter.c: Bit-exact C11 reference port of om/starfighter.om. */
#include "gfx_common.h"

#define SF_HERO 195u

static int32_t sf_st[8];
static int32_t st_x[64], st_y[64], st_sp[64]; static uint32_t st_c[64];
static int32_t en_x[24], en_y[24], en_vx[24], en_vy[24], en_hp[24], en_kind[24], en_ph[24];
static int32_t pb_x[40], pb_y[40], pb_vx[40], pb_vy[40], pb_on[40]; static uint32_t pb_c[40];
static int32_t eb_x[32], eb_y[32], eb_vx[32], eb_vy[32], eb_on[32];
static int32_t rg_x[12], rg_y[12], rg_r[12], rg_max[12]; static uint32_t rg_c[12];
static int32_t sp_x[96], sp_y[96], sp_vx[96], sp_vy[96], sp_life[96]; static uint32_t sp_c[96];

static uint32_t sf_rand(void) {
    uint32_t s = (uint32_t)sf_st[0];
    s = s * 1664525u + 1013904223u;
    sf_st[0] = (int32_t)(s & 2147483647u);
    return s;
}

static uint32_t sf_strand_col(uint64_t k) {
    uint64_t m = k & 3u;
    if (m == 0) return rgb(0, 228, 255);
    if (m == 1) return rgb(255, 209, 102);
    if (m == 2) return rgb(34, 229, 138);
    return rgb(184, 107, 255);
}

static void sf_spawn_ring(int32_t x, int32_t y, int32_t rmax, uint32_t c) {
    for (int i = 0; i < 12; i++) {
        if (rg_r[i] == 0) { rg_x[i] = x; rg_y[i] = y; rg_r[i] = 4; rg_max[i] = rmax; rg_c[i] = c; return; }
    }
}

static void sf_spawn_sparks(int32_t x, int32_t y, uint32_t c) {
    int32_t n = 0;
    for (int i = 0; i < 96; i++) {
        if (sp_life[i] <= 0) {
            int32_t ang = n * 85;
            int32_t spd = 20 + (int32_t)(sf_rand() & 31u);
            sp_x[i] = x << 4; sp_y[i] = y << 4;
            sp_vx[i] = (icos1024(ang) * spd) >> 12;
            sp_vy[i] = (isin1024(ang) * spd) >> 12;
            sp_life[i] = 18; sp_c[i] = c;
            if (++n >= 12) return;
        }
    }
}

static void sf_spawn_wave(int32_t wv) {
    sf_st[4] = wv;
    for (uint64_t i = 0; i < 12; i++) {
        for (int s = 0; s < 24; s++) {
            if (en_hp[s] <= 0) {
                int32_t col = (int32_t)(i % 6u), row = (int32_t)(i / 6u);
                en_x[s] = 110 + col * 84; en_y[s] = 54 + row * 46 - 60;
                en_vx[s] = 1; en_vy[s] = 1;
                int32_t kind = (int32_t)((i + (uint64_t)wv) % 3u);
                en_kind[s] = kind;
                en_hp[s] = kind == 2 ? 4 : 2;
                en_ph[s] = (int32_t)i * 42;
                break;
            }
        }
    }
}

static void sf_init(void) {
    sf_st[0] = 987654321; sf_st[1] = 320; sf_st[2] = 294;
    sf_st[3] = 0; sf_st[4] = 1; sf_st[5] = 100; sf_st[6] = 0; sf_st[7] = 0;
    for (uint64_t i = 0; i < 64; i++) {
        st_x[i] = (int32_t)(sf_rand() % 640u);
        st_y[i] = (int32_t)(sf_rand() % 360u);
        uint64_t layer = i % 3u;
        st_sp[i] = 1 + (int32_t)layer * 2;
        if (layer == 0) st_c[i] = rgb(58, 76, 122);
        else if (layer == 1) st_c[i] = rgb(132, 178, 228);
        else st_c[i] = rgb(215, 244, 255);
    }
    memset(en_hp, 0, sizeof(en_hp)); memset(pb_on, 0, sizeof(pb_on));
    memset(eb_on, 0, sizeof(eb_on)); memset(rg_r, 0, sizeof(rg_r));
    memset(sp_life, 0, sizeof(sp_life));
    sf_spawn_wave(1);
}

static void sf_fire_bolt(int32_t x, int32_t y, int32_t vx, int32_t vy, uint32_t c) {
    for (int i = 0; i < 40; i++) {
        if (!pb_on[i]) { pb_x[i] = x; pb_y[i] = y; pb_vx[i] = vx; pb_vy[i] = vy; pb_c[i] = c; pb_on[i] = 1; return; }
    }
}

static void sf_fire_ebolt(int32_t x, int32_t y, int32_t vx, int32_t vy) {
    for (int i = 0; i < 32; i++) {
        if (!eb_on[i]) { eb_x[i] = x; eb_y[i] = y; eb_vx[i] = vx; eb_vy[i] = vy; eb_on[i] = 1; return; }
    }
}

static void sf_kill_enemy(int idx) {
    int32_t ex = en_x[idx], ey = en_y[idx], kd = en_kind[idx];
    uint32_t c = kd == 1 ? rgb(184, 107, 255) : (kd == 2 ? rgb(255, 209, 102) : rgb(0, 228, 255));
    en_hp[idx] = 0;
    sf_st[3] += (kd + 1) * 150;
    sf_spawn_ring(ex, ey, 46, c);
    sf_spawn_ring(ex, ey, 28, rgb(255, 245, 210));
    sf_spawn_sparks(ex, ey, c);
}

static void sf_step(uint64_t fr) {
    sf_st[7] = (int32_t)fr;
    for (int i = 0; i < 64; i++) {
        int32_t ny = st_y[i] + st_sp[i];
        if (ny >= 360) { ny -= 360; st_x[i] = (int32_t)(sf_rand() % 640u); }
        st_y[i] = ny;
    }
    int32_t px = 320 + ((isin1024((int32_t)fr * 11) * 185) >> 12);
    int32_t py = 292 + ((icos1024((int32_t)fr * 7) * 18) >> 12);
    sf_st[1] = px; sf_st[2] = py;
    if (sf_st[6] > 0) sf_st[6]--;
    if ((fr % 45u) == 10u) sf_st[6] = 12;
    if ((fr % 5u) == 0u) {
        sf_fire_bolt(px - 14, py - 10, 0, -11, sf_strand_col(fr / 5u));
        sf_fire_bolt(px + 14, py - 10, 0, -11, sf_strand_col(fr / 5u + 2u));
        if ((fr % 15u) == 0u) {
            sf_fire_bolt(px - 22, py - 4, -3, -10, sf_strand_col(1));
            sf_fire_bolt(px + 22, py - 4, 3, -10, sf_strand_col(3));
        }
    }
    for (int b = 0; b < 40; b++) {
        if (pb_on[b]) {
            int32_t bx = pb_x[b] + pb_vx[b], by = pb_y[b] + pb_vy[b];
            pb_x[b] = bx; pb_y[b] = by;
            if (by < 24 || bx < 0 || bx >= 640) pb_on[b] = 0;
            else {
                for (int e = 0; e < 24; e++) {
                    if (en_hp[e] > 0 && iabs(bx - en_x[e]) < 18 && iabs(by - en_y[e]) < 14) {
                        pb_on[b] = 0;
                        if (--en_hp[e] <= 0) sf_kill_enemy(e);
                        else sf_spawn_ring(bx, by, 16, rgb(255, 255, 255));
                        break;
                    }
                }
            }
        }
    }
    if (sf_st[6] > 0) {
        for (int e = 0; e < 24; e++) {
            if (en_hp[e] > 0 && en_y[e] > 36 && en_y[e] < py) {
                int32_t dx = iabs(en_x[e] - px), dy = py - en_y[e];
                if (dx < 32 + (dy >> 1) && (fr % 3u) == 0u) {
                    if (--en_hp[e] <= 0) sf_kill_enemy(e);
                }
            }
        }
    }
    int alive = 0;
    for (uint64_t e = 0; e < 24; e++) {
        if (en_hp[e] > 0) {
            alive++;
            int32_t ph = en_ph[e] + 12; en_ph[e] = ph;
            int32_t ex = en_x[e] + ((isin1024(ph) * 3) >> 12);
            int32_t ey = en_y[e] < 86 ? (en_y[e] + 2) : (en_y[e] + ((icos1024(ph) * 2) >> 12));
            ex = iclamp(ex, 44, 596); ey = iclamp(ey, 38, 220);
            en_x[e] = ex; en_y[e] = ey;
            if (((fr + e * 7u) % 37u) == 0u) sf_fire_ebolt(ex, ey + 12, (isin1024(ph) * 3) >> 12, 4);
        }
    }
    if (alive < 4) sf_spawn_wave(sf_st[4] + 1);
    for (int b = 0; b < 32; b++) {
        if (eb_on[b]) {
            int32_t bx = eb_x[b] + eb_vx[b], by = eb_y[b] + eb_vy[b];
            eb_x[b] = bx; eb_y[b] = by;
            if (by >= 356 || bx < 0 || bx >= 640) eb_on[b] = 0;
            else if (iabs(bx - px) < 14 && iabs(by - py) < 12) {
                eb_on[b] = 0;
                if (sf_st[5] > 20) sf_st[5] -= 5;
                sf_spawn_ring(px, py, 26, rgb(255, 96, 120));
            }
        }
    }
    for (int i = 0; i < 12; i++) {
        if (rg_r[i] > 0) { int32_t nr = rg_r[i] + 3; rg_r[i] = nr >= rg_max[i] ? 0 : nr; }
    }
    for (int i = 0; i < 96; i++) {
        if (sp_life[i] > 0) { sp_x[i] += sp_vx[i]; sp_y[i] += sp_vy[i]; sp_life[i]--; }
    }
    (void)en_vx; (void)en_vy;
}

static void sf_draw_enemy(int32_t x, int32_t y, int32_t kd) {
    if (kd == 0) {
        draw_glow_line(x, y + 12, x - 15, y - 8, rgb(255, 76, 92));
        draw_glow_line(x, y + 12, x + 15, y - 8, rgb(255, 76, 92));
        draw_line(x - 15, y - 8, x, y - 12, rgb(255, 170, 80));
        draw_line(x + 15, y - 8, x, y - 12, rgb(255, 170, 80));
        fill_rect(x - 4, y - 4, 8, 8, rgb(255, 190, 92));
        fill_rect(x - 2, y - 2, 4, 4, rgb(255, 255, 225));
    } else if (kd == 1) {
        draw_glow_line(x - 18, y - 4, x - 6, y + 10, rgb(190, 92, 255));
        draw_glow_line(x + 18, y - 4, x + 6, y + 10, rgb(190, 92, 255));
        draw_glow_line(x - 18, y - 4, x + 18, y - 4, rgb(255, 96, 220));
        fill_rect(x - 5, y - 6, 10, 10, rgb(128, 52, 216));
        fill_rect(x - 2, y - 3, 4, 4, rgb(0, 238, 255));
    } else {
        draw_glow_line(x - 20, y, x, y - 12, rgb(38, 232, 142));
        draw_glow_line(x + 20, y, x, y - 12, rgb(38, 232, 142));
        draw_glow_line(x - 20, y, x, y + 12, rgb(255, 206, 86));
        draw_glow_line(x + 20, y, x, y + 12, rgb(255, 206, 86));
        fill_rect(x - 8, y - 5, 16, 10, rgb(28, 112, 88));
        fill_rect(x - 4, y - 2, 8, 4, rgb(255, 228, 110));
    }
}

static void sf_draw_player(void) {
    int32_t px = sf_st[1], py = sf_st[2], fr = sf_st[7];
    for (uint64_t s = 0; s < 4; s++) {
        uint32_t c = sf_strand_col(s);
        int32_t ox = (int32_t)s * 6 - 9;
        int32_t base_ph = fr * 28 + (int32_t)s * 128;
        for (int k = 0; k < 14; k++) {
            int32_t wave = (isin1024(base_ph + k * 52) * 5) >> 12;
            int32_t sx = px + ox + wave, sy = py + 14 + k * 3;
            uint32_t sc = (uint32_t)(14 - k);
            uint32_t rr = uclamp255((((c >> 16) & 255u) * sc) >> 3);
            uint32_t gg = uclamp255((((c >> 8) & 255u) * sc) >> 3);
            uint32_t bb = uclamp255(((c & 255u) * sc) >> 3);
            px_add(sx, sy, rr, gg, bb); px_add(sx + 1, sy, rr, gg, bb);
        }
    }
    int32_t sw = sf_st[6];
    if (sw > 0) {
        for (uint64_t s = 0; s < 4; s++) {
            uint32_t c = sf_strand_col(s);
            int32_t off = (int32_t)s * 2 - 3;
            int32_t tx = px + off * (58 + sw * 6);
            draw_glow_line(px + off * 6, py - 10, tx, 28, c);
            draw_line(px + off * 6, py - 10, tx, 28, rgb(240, 255, 255));
        }
    }
    draw_glow_line(px, py - 18, px - 26, py + 12, rgb(0, 228, 255));
    draw_glow_line(px, py - 18, px + 26, py + 12, rgb(0, 228, 255));
    draw_line(px - 26, py + 12, px - 8, py + 6, rgb(140, 220, 255));
    draw_line(px + 26, py + 12, px + 8, py + 6, rgb(140, 220, 255));
    fill_rect(px - 5, py - 12, 10, 22, rgb(190, 218, 245));
    fill_rect(px - 14, py + 2, 28, 6, rgb(64, 114, 176));
    fill_rect(px - 3, py - 8, 6, 10, rgb(0, 232, 255));
    fill_rect(px - 1, py - 6, 2, 5, rgb(255, 255, 255));
    fill_rect(px - 24, py + 7, 4, 5, rgb(255, 209, 102));
    fill_rect(px + 21, py + 7, 4, 5, rgb(184, 107, 255));
}

static void sf_draw_hud(void) {
    blend_rect(0, 0, 640, 26, rgb(6, 10, 24), 220);
    fill_rect(0, 26, 160, 2, rgb(0, 228, 255));
    fill_rect(160, 26, 160, 2, rgb(255, 209, 102));
    fill_rect(320, 26, 160, 2, rgb(34, 229, 138));
    fill_rect(480, 26, 160, 2, rgb(184, 107, 255));
    draw_label("OMEGA STARFIGHTER", 12, 9, 1, rgb(0, 230, 255));
    draw_label("SCORE", 168, 9, 1, rgb(164, 194, 234));
    draw_uint((uint64_t)sf_st[3], 210, 9, 1, rgb(255, 255, 255));
    draw_label("WAVE", 300, 9, 1, rgb(255, 209, 102));
    draw_uint((uint64_t)sf_st[4], 334, 9, 1, rgb(255, 244, 200));
    draw_label("SHIELD", 424, 9, 1, rgb(34, 229, 138));
    fill_rect(472, 8, 110, 10, rgb(18, 28, 48));
    fill_rect(474, 10, iclamp(sf_st[5], 0, 106), 6, rgb(34, 229, 138));
    rect_outline(472, 8, 110, 10, rgb(100, 220, 180));
}

static uint32_t sf_fade_col(uint32_t c, uint32_t fade, uint32_t urm) {
    return rgb(uclamp255((((c >> 16) & 255u) * fade) / urm),
               uclamp255((((c >> 8) & 255u) * fade) / urm),
               uclamp255(((c & 255u) * fade) / urm));
}

static void sf_render(void) {
    for (uint32_t y = 0; y < 360; y++) {
        uint32_t col = rgb(3u + ((y * 8u) >> 8), 6u + ((y * 12u) >> 8), 16u + ((y * 26u) >> 8));
        size_t row = (size_t)y * 640u;
        for (size_t x = 0; x < 640; x++) fb[row + x] = col;
    }
    for (int i = 0; i < 64; i++) {
        px_set(st_x[i], st_y[i], st_c[i]);
        if (st_sp[i] > 2) {
            px_set(st_x[i], st_y[i] - 1, st_c[i]);
            px_set(st_x[i], st_y[i] - 2, rgb(80, 140, 200));
        }
    }
    for (int i = 0; i < 12; i++) {
        if (rg_r[i] > 0) {
            uint32_t fade = (uint32_t)(rg_max[i] - rg_r[i]), urm = (uint32_t)rg_max[i];
            draw_ring(rg_x[i], rg_y[i], rg_r[i], sf_fade_col(rg_c[i], fade, urm));
            draw_ring(rg_x[i], rg_y[i], rg_r[i] + 1, sf_fade_col(rg_c[i], fade >> 1, urm));
        }
    }
    for (int b = 0; b < 40; b++) {
        if (pb_on[b]) {
            draw_glow_line(pb_x[b], pb_y[b], pb_x[b], pb_y[b] + 10, pb_c[b]);
            draw_line(pb_x[b], pb_y[b], pb_x[b], pb_y[b] + 8, rgb(255, 255, 255));
        }
    }
    for (int b = 0; b < 32; b++) {
        if (eb_on[b]) {
            int32_t bx = eb_x[b], by = eb_y[b];
            px_add(bx, by, 255, 120, 90);
            px_add(bx - 1, by, 220, 60, 80); px_add(bx + 1, by, 220, 60, 80);
            px_add(bx, by - 1, 220, 60, 80); px_add(bx, by + 1, 220, 60, 80);
            px_set(bx, by, rgb(255, 240, 210));
        }
    }
    for (int e = 0; e < 24; e++) if (en_hp[e] > 0) sf_draw_enemy(en_x[e], en_y[e], en_kind[e]);
    for (int i = 0; i < 96; i++) {
        if (sp_life[i] > 0) {
            int32_t sx = sp_x[i] >> 4, sy = sp_y[i] >> 4;
            uint32_t c = sp_c[i], ulf = (uint32_t)sp_life[i];
            uint32_t rr = uclamp255((((c >> 16) & 255u) * ulf) >> 4);
            uint32_t gg = uclamp255((((c >> 8) & 255u) * ulf) >> 4);
            uint32_t bb = uclamp255(((c & 255u) * ulf) >> 4);
            px_add(sx, sy, rr, gg, bb); px_add(sx + 1, sy, rr, gg, bb); px_add(sx, sy + 1, rr, gg, bb);
        }
    }
    sf_draw_player();
    sf_draw_hud();
}

int main(int argc, char **argv) {
    gfx_init();
    int mode = 0; uint64_t param = 300;
    if (!parse_cli(argc, argv, SF_HERO, &mode, &param)) {
        fprintf(stderr, "ERR bad args\n");
        return 2;
    }
    sf_init();
    if (mode == 0) {
        uint64_t t0 = monons(); uint32_t dce = 0;
        for (uint64_t f = 0; f < param; f++) { sf_step(f); sf_render(); dce ^= sample_fb_u32(f); }
        uint64_t ns = monons() - t0;
        uint8_t dig[32]; sha_fb_ppm(dig);
        uint64_t us_f = ns ? ns / (param * 1000ull) : 0;
        uint64_t fps100 = ns ? (param * 100000000000ull) / ns : 0;
        uint64_t vmhwm = read_vmhwm_kb();
        printf("GAME=starfighter FRAMES=%llu NS=%llu US_PER_FRAME=%llu FPS_X100=%llu ANTI_DCE=%08x VMHWM_KB=%llu FINAL_PPM_SHA256=",
               (unsigned long long)param, (unsigned long long)ns, (unsigned long long)us_f, (unsigned long long)fps100, dce, (unsigned long long)vmhwm);
        print_hex32(dig); printf("\n");
        return 0;
    }
    if (mode == 1) {
        uint64_t hf = SF_HERO >= param ? param - 1 : SF_HERO;
        uint8_t hdig[32], fdig[32], sdig[32];
        for (uint64_t f = 0; f < param; f++) {
            sf_step(f); sf_render(); frame_hashes[f] = hash_fb_u32();
            if (f == hf) sha_fb_ppm(hdig);
        }
        sha_fb_ppm(fdig); sha_stream_u32((size_t)param, sdig);
        printf("GAME=starfighter FRAMES=%llu HERO_FRAME=%llu HERO_PPM_SHA256=",
               (unsigned long long)param, (unsigned long long)hf);
        print_hex32(hdig); printf(" FINAL_PPM_SHA256=");
        print_hex32(fdig); printf(" STREAM_SHA256=");
        print_hex32(sdig); printf("\n");
        return 0;
    }
    for (uint64_t f = 0; f <= param; f++) { sf_step(f); sf_render(); }
    return write_ppm();
}
