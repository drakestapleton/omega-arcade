/* SPDX-License-Identifier: Apache-2.0 */
/* ref/c/raycast_doom.c: Bit-exact C11 reference port of om/raycast_doom.om. */
#include "gfx_common.h"

#define RC_HERO 185u

static const char *RC_MAP_STR =
    "1111111111111111100000200000000110300020040400011000000000000301"
    "1000200000000001120020030004000110000000000000011040000000020021"
    "1000003000000001100000000000030110200000040000011020000000002001"
    "1000030000002001100000000000000110040002003000011111111111111111";

static uint8_t rmap[256];
static uint32_t bg_row[360];
static int32_t rc_st[8];
static int64_t rc_hit[8];

static void rc_init(void) {
    for (int i = 0; i < 256; i++) rmap[i] = (uint8_t)(RC_MAP_STR[i] - '0');
    rc_st[0] = 10240; rc_st[1] = 10240; rc_st[2] = 110;
    rc_st[3] = 0; rc_st[4] = 99; rc_st[5] = 0;
    for (uint32_t y = 0; y < 180; y++) {
        uint32_t d = 179u - y;
        bg_row[y] = rgb(8u + ((d * 14u) >> 8), 14u + ((d * 24u) >> 8), 30u + ((d * 48u) >> 8));
    }
    for (uint32_t y = 0; y < 180; y++) {
        uint32_t band = ((y >> 3) & 1u) == 0u ? 6u : 0u;
        bg_row[y + 180] = rgb(14u + ((y * 40u) >> 8) + band,
                              18u + ((y * 48u) >> 8) + band,
                              28u + ((y * 64u) >> 8) + band);
    }
}

static int rc_blocked(int32_t x, int32_t y) {
    if (x < 4096 || y < 4096) return 1;
    int32_t mx = x >> 12, my = y >> 12;
    if (mx >= 16 || my >= 16) return 1;
    return rmap[my * 16 + mx] != 0;
}

static void rc_try_move(int32_t dx, int32_t dy) {
    int32_t px = rc_st[0], py = rc_st[1];
    int32_t nx = px + dx, ny = py + dy, m = 900;
    if (!rc_blocked(nx + m, py) && !rc_blocked(nx - m, py)) {
        rc_st[0] = nx;
        px = nx;
    }
    if (!rc_blocked(px, ny + m) && !rc_blocked(px, ny - m)) {
        rc_st[1] = ny;
    }
}

static void rc_step(uint64_t fr) {
    int32_t turn = 2, spd = 165;
    if (fr < 60) { turn = 1; spd = 175; }
    else if (fr < 130) { turn = 4; spd = 155; }
    else if (fr < 210) { turn = -3; spd = 165; }
    else { turn = 3; spd = 170; }
    int32_t ang = (rc_st[2] + turn) & 1023;
    rc_st[2] = ang;
    int32_t dx = (icos1024(ang) * spd) >> 12;
    int32_t dy = (isin1024(ang) * spd) >> 12;
    rc_try_move(dx, dy);
    rc_st[5] += 28;
    if (rc_st[3] > 0) rc_st[3]--;
    if ((fr % 25u) == 10u) {
        rc_st[3] = 6;
        if (rc_st[4] > 0) rc_st[4]--;
    }
}

static uint32_t rc_wall_color(uint64_t wtype, int32_t side, int32_t tx, int32_t ty, uint32_t fog) {
    uint32_t r = 96, g = 118, b = 146;
    if (wtype == 1) {
        r = 92; g = 116; b = 152;
        int32_t stagger = ((ty >> 3) & 1) * 8;
        if ((ty & 7) == 0) { r = 36; g = 48; b = 68; }
        else if (((tx + stagger) & 15) == 0) { r = 40; g = 52; b = 72; }
        else if ((ty & 7) == 1) { r = 122; g = 148; b = 188; }
    } else if (wtype == 2) {
        r = 28; g = 72; b = 112;
        if (ty >= 13 && ty <= 18) { r = 18; g = 232; b = 255; }
        else if ((tx & 7) == 0) { r = 20; g = 118; b = 160; }
    } else if (wtype == 3) {
        r = 196; g = 136; b = 44;
        if (ty < 3 || ty > 28) { r = 94; g = 64; b = 26; }
        else if (tx >= 12 && tx <= 19) { r = 255; g = 206; b = 88; }
    } else {
        r = 104; g = 62; b = 164;
        if (ty >= 14 && ty <= 17) { r = 38; g = 236; b = 148; }
        else if ((ty & 7) == 0) { r = 56; g = 34; b = 92; }
    }
    if (side == 1) { r = (r * 180u) >> 8; g = (g * 180u) >> 8; b = (b * 180u) >> 8; }
    uint32_t inv = 256u - fog;
    r = (r * inv + 10u * fog) >> 8;
    g = (g * inv + 16u * fog) >> 8;
    b = (b * inv + 30u * fog) >> 8;
    return (r << 16) | (g << 8) | b;
}

static uint64_t rc_map_cell(int32_t mx, int32_t my) {
    if (mx < 0 || mx >= 16 || my < 0 || my >= 16) return 1;
    return rmap[my * 16 + mx];
}

static void rc_cast_col(int32_t col, int64_t dirx, int64_t diry, int64_t plx, int64_t ply) {
    int64_t camx = (((int64_t)col * 2 - 640) * 4096) / 640;
    int64_t rdx = dirx + ((plx * camx) >> 12);
    int64_t rdy = diry + ((ply * camx) >> 12);
    int64_t px = rc_st[0], py = rc_st[1];
    int32_t mx = (int32_t)(px >> 12), my = (int32_t)(py >> 12);
    int64_t ddx = rdx != 0 ? 16777216LL / (rdx < 0 ? -rdx : rdx) : 1000000000LL;
    int64_t ddy = rdy != 0 ? 16777216LL / (rdy < 0 ? -rdy : rdy) : 1000000000LL;
    int32_t stx = rdx < 0 ? -1 : 1;
    int32_t sty = rdy < 0 ? -1 : 1;
    int64_t sdx = rdx < 0 ? ((px - ((int64_t)mx << 12)) * ddx) >> 12
                          : ((((int64_t)(mx + 1) << 12) - px) * ddx) >> 12;
    int64_t sdy = rdy < 0 ? ((py - ((int64_t)my << 12)) * ddy) >> 12
                          : ((((int64_t)(my + 1) << 12) - py) * ddy) >> 12;
    int32_t side = 0;
    uint64_t wtype = 1;
    for (int step = 0; step < 32; step++) {
        if (sdx < sdy) { sdx += ddx; mx += stx; side = 0; }
        else { sdy += ddy; my += sty; side = 1; }
        uint64_t cell = rc_map_cell(mx, my);
        if (cell != 0) { wtype = cell; break; }
    }
    int64_t pdist = side == 0 ? (sdx - ddx) : (sdy - ddy);
    if (pdist < 64) pdist = 64;
    int64_t wpos = side == 0 ? (py + ((pdist * rdy) >> 12)) : (px + ((pdist * rdx) >> 12));
    int32_t tx = (int32_t)((wpos & 4095) >> 7);
    int32_t lineh = (int32_t)(1474560LL / pdist);
    if (lineh < 2) lineh = 2;
    int32_t dstart = 180 - (lineh >> 1);
    int32_t dend = 180 + (lineh >> 1);
    int32_t cs = imax(dstart, 0), ce = imin(dend, 359);
    int64_t fog_l = (pdist - 3072) >> 7;
    if (fog_l < 0) fog_l = 0;
    if (fog_l > 220) fog_l = 220;
    uint32_t fog = (uint32_t)fog_l;
    size_t ucol = (size_t)col;
    for (int32_t y = 0; y < cs; y++) fb[(size_t)y * 640u + ucol] = bg_row[y];
    int64_t tstep = 2097152LL / (int64_t)lineh;
    int64_t tpos = (int64_t)(cs - dstart) * tstep;
    for (int32_t y = cs; y <= ce; y++) {
        int32_t ty = (int32_t)((tpos >> 16) & 31);
        tpos += tstep;
        fb[(size_t)y * 640u + ucol] = rc_wall_color(wtype, side, tx, ty, fog);
    }
    for (int32_t y = ce + 1; y < 360; y++) fb[(size_t)y * 640u + ucol] = bg_row[y];
    (void)rc_hit;
}

static void rc_draw_weapon(void) {
    int32_t bp = rc_st[5];
    int32_t bx = (icos1024(bp) * 8) >> 12;
    int32_t by = (iabs(isin1024(bp)) * 6) >> 12;
    int32_t ft = rc_st[3];
    int32_t cx = 320 + bx;
    int32_t base_y = 246 + by + ft * 4;
    if (ft > 2) {
        int32_t my = base_y - 14;
        draw_ring(cx, my, 24, rgb(0, 210, 255));
        draw_ring(cx, my, 16, rgb(255, 215, 90));
        draw_ring(cx, my, 9, rgb(255, 255, 240));
        draw_glow_line(cx - 30, my, cx + 30, my, rgb(0, 220, 255));
        draw_glow_line(cx, my - 26, cx, my + 26, rgb(255, 220, 110));
        draw_glow_line(cx - 20, my - 20, cx + 20, my + 20, rgb(180, 110, 255));
        draw_glow_line(cx - 20, my + 20, cx + 20, my - 20, rgb(40, 240, 150));
    }
    fill_rect(cx - 38, base_y + 22, 76, 96, rgb(22, 28, 42));
    fill_rect(cx - 30, base_y + 8, 60, 110, rgb(32, 42, 60));
    fill_rect(cx - 18, base_y - 6, 36, 124, rgb(44, 56, 78));
    rect_outline(cx - 38, base_y + 22, 76, 96, rgb(76, 96, 130));
    rect_outline(cx - 18, base_y - 6, 36, 124, rgb(96, 122, 162));
    fill_rect(cx - 10, base_y - 12, 20, 12, rgb(18, 22, 34));
    fill_rect(cx - 6, base_y - 10, 12, 8, rgb(0, 230, 255));
    fill_rect(cx - 26, base_y + 14, 5, 88, rgb(0, 228, 255));
    fill_rect(cx - 14, base_y + 4, 4, 98, rgb(255, 209, 102));
    fill_rect(cx + 10, base_y + 4, 4, 98, rgb(34, 229, 138));
    fill_rect(cx + 21, base_y + 14, 5, 88, rgb(184, 107, 255));
    draw_glow_line(cx - 24, base_y + 16, cx - 24, base_y + 96, rgb(0, 180, 220));
    draw_glow_line(cx - 12, base_y + 6, cx - 12, base_y + 98, rgb(220, 170, 60));
    draw_glow_line(cx + 12, base_y + 6, cx + 12, base_y + 98, rgb(25, 190, 110));
    draw_glow_line(cx + 23, base_y + 16, cx + 23, base_y + 96, rgb(150, 80, 220));
}

static void rc_draw_minimap(void) {
    int32_t mx0 = 522, my0 = 10;
    blend_rect(mx0 - 4, my0 - 4, 106, 106, rgb(6, 10, 22), 205);
    rect_outline(mx0 - 4, my0 - 4, 106, 106, rgb(0, 212, 255));
    for (int gy = 0; gy < 16; gy++) {
        for (int gx = 0; gx < 16; gx++) {
            uint8_t cell = rmap[gy * 16 + gx];
            uint32_t col = rgb(16, 24, 40);
            if (cell == 1) col = rgb(104, 134, 176);
            else if (cell == 2) col = rgb(0, 216, 255);
            else if (cell == 3) col = rgb(245, 186, 72);
            else if (cell == 4) col = rgb(156, 96, 236);
            fill_rect(mx0 + gx * 6, my0 + gy * 6, 5, 5, col);
        }
    }
    int32_t ppx = mx0 + ((rc_st[0] * 6) >> 12);
    int32_t ppy = my0 + ((rc_st[1] * 6) >> 12);
    int32_t ang = rc_st[2];
    for (int r = 0; r < 5; r++) {
        int32_t ra = ang + r * 32 - 64;
        int32_t rx = ppx + ((icos1024(ra) * 18) >> 12);
        int32_t ry = ppy + ((isin1024(ra) * 18) >> 12);
        draw_line(ppx, ppy, rx, ry, rgb(40, 240, 200));
    }
    fill_rect(ppx - 2, ppy - 2, 5, 5, rgb(255, 235, 120));
    px_set(ppx, ppy, rgb(255, 255, 255));
}

static void rc_draw_hud(void) {
    fill_rect(313, 180, 5, 1, rgb(0, 230, 255));
    fill_rect(323, 180, 5, 1, rgb(0, 230, 255));
    fill_rect(320, 173, 1, 5, rgb(0, 230, 255));
    fill_rect(320, 183, 1, 5, rgb(0, 230, 255));
    blend_rect(12, 308, 168, 42, rgb(8, 14, 28), 210);
    rect_outline(12, 308, 168, 42, rgb(0, 198, 240));
    draw_label("OMEGA RAY", 20, 314, 1, rgb(0, 230, 255));
    draw_label("HP 100", 20, 330, 1, rgb(34, 229, 138));
    draw_label("AMMO", 92, 330, 1, rgb(255, 209, 102));
    draw_uint((uint64_t)rc_st[4], 128, 330, 1, rgb(255, 244, 214));
    fill_rect(13, 309, 41, 2, rgb(0, 228, 255));
    fill_rect(54, 309, 41, 2, rgb(255, 209, 102));
    fill_rect(95, 309, 42, 2, rgb(34, 229, 138));
    fill_rect(137, 309, 42, 2, rgb(184, 107, 255));
}

static void rc_render(void) {
    int32_t ang = rc_st[2];
    int64_t dirx = icos1024(ang), diry = isin1024(ang);
    int64_t plx = ((-diry) * 2703LL) >> 12;
    int64_t ply = (dirx * 2703LL) >> 12;
    for (int32_t c = 0; c < 640; c++) rc_cast_col(c, dirx, diry, plx, ply);
    rc_draw_weapon();
    rc_draw_minimap();
    rc_draw_hud();
}

int main(int argc, char **argv) {
    gfx_init();
    int mode = 0; uint64_t param = 300;
    if (!parse_cli(argc, argv, RC_HERO, &mode, &param)) {
        fprintf(stderr, "ERR bad args\n");
        return 2;
    }
    rc_init();
    if (mode == 0) {
        uint64_t t0 = monons();
        uint32_t dce = 0;
        for (uint64_t f = 0; f < param; f++) {
            rc_step(f);
            rc_render();
            dce ^= sample_fb_u32(f);
        }
        uint64_t ns = monons() - t0;
        uint8_t dig[32]; sha_fb_ppm(dig);
        uint64_t us_f = ns ? ns / (param * 1000ull) : 0;
        uint64_t fps100 = ns ? (param * 100000000000ull) / ns : 0;
        uint64_t vmhwm = read_vmhwm_kb();
        printf("GAME=raycast_doom FRAMES=%llu NS=%llu US_PER_FRAME=%llu FPS_X100=%llu ANTI_DCE=%08x VMHWM_KB=%llu FINAL_PPM_SHA256=",
               (unsigned long long)param, (unsigned long long)ns, (unsigned long long)us_f, (unsigned long long)fps100, dce, (unsigned long long)vmhwm);
        print_hex32(dig); printf("\n");
        return 0;
    }
    if (mode == 1) {
        uint64_t hf = RC_HERO >= param ? param - 1 : RC_HERO;
        uint8_t hdig[32], fdig[32], sdig[32];
        for (uint64_t f = 0; f < param; f++) {
            rc_step(f);
            rc_render();
            frame_hashes[f] = hash_fb_u32();
            if (f == hf) sha_fb_ppm(hdig);
        }
        sha_fb_ppm(fdig);
        sha_stream_u32((size_t)param, sdig);
        printf("GAME=raycast_doom FRAMES=%llu HERO_FRAME=%llu HERO_PPM_SHA256=",
               (unsigned long long)param, (unsigned long long)hf);
        print_hex32(hdig); printf(" FINAL_PPM_SHA256=");
        print_hex32(fdig); printf(" STREAM_SHA256=");
        print_hex32(sdig); printf("\n");
        return 0;
    }
    for (uint64_t f = 0; f <= param; f++) { rc_step(f); rc_render(); }
    return write_ppm();
}
