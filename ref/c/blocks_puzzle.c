/* SPDX-License-Identifier: Apache-2.0 */
/* ref/c/blocks_puzzle.c: Bit-exact C11 reference port of om/blocks_puzzle.om. */
#include "gfx_common.h"

#define BK_HERO 248u
#define BK_OX 240
#define BK_OY 20
#define BK_CELL 16

static const char *BK_SHAPES =
    "0111213120212223021222321011121310201121102011211020112110201121"
    "1001112110112112011121121001111210200111101121221121021200011112"
    "0010112120112112011112221001110200011121102011120111212210110212"
    "20011121101112220111210200101112";

static uint8_t board[200];
static uint8_t bag[7];
static int32_t bk_st[16];
static int32_t pt_x[96], pt_y[96], pt_vx[96], pt_vy[96], pt_life[96];
static uint32_t pt_col[96];

static uint32_t bk_rand(void) {
    uint32_t s = (uint32_t)bk_st[0];
    s = s * 1664525u + 1013904223u;
    bk_st[0] = (int32_t)(s & 2147483647u);
    return s;
}

static inline int32_t bk_dx(int32_t p, int32_t r, int32_t c) { return BK_SHAPES[p * 32 + r * 8 + c * 2] - '0'; }
static inline int32_t bk_dy(int32_t p, int32_t r, int32_t c) { return BK_SHAPES[p * 32 + r * 8 + c * 2 + 1] - '0'; }

static void bk_refill_bag(void) {
    for (int i = 0; i < 7; i++) bag[i] = (uint8_t)i;
    for (int i = 6; i > 0; i--) {
        int j = (int)(bk_rand() % (uint32_t)(i + 1));
        uint8_t t = bag[i]; bag[i] = bag[j]; bag[j] = t;
    }
    bk_st[1] = 0;
}

static int32_t bk_pop_bag(void) {
    if (bk_st[1] >= 7) bk_refill_bag();
    return bag[bk_st[1]++];
}

static int bk_fits(int32_t p, int32_t r, int32_t x, int32_t y) {
    for (int c = 0; c < 4; c++) {
        int32_t gx = x + bk_dx(p, r, c), gy = y + bk_dy(p, r, c);
        if (gx < 0 || gx >= 10 || gy >= 20) return 0;
        if (gy >= 0 && board[gy * 10 + gx] != 0) return 0;
    }
    return 1;
}

static int32_t bk_ghost_y(int32_t p, int32_t r, int32_t x, int32_t y) {
    int32_t gy = y;
    while (bk_fits(p, r, x, gy + 1)) gy++;
    return gy;
}

static int bk_try_rotate(void) {
    int32_t p = bk_st[2], r1 = (bk_st[3] + 1) & 3, x0 = bk_st[4], y0 = bk_st[5];
    static const int32_t kdx[6] = {0, -1, 1, 0, -2, 2};
    static const int32_t kdy[6] = {0, 0, 0, -1, 0, 0};
    for (int k = 0; k < 6; k++) {
        if (bk_fits(p, r1, x0 + kdx[k], y0 + kdy[k])) {
            bk_st[3] = r1; bk_st[4] = x0 + kdx[k]; bk_st[5] = y0 + kdy[k];
            return 1;
        }
    }
    return 0;
}

static void bk_stamp(int32_t p, int32_t r, int32_t x, int32_t y, uint8_t val) {
    for (int c = 0; c < 4; c++) {
        int32_t px = x + bk_dx(p, r, c), py = y + bk_dy(p, r, c);
        if (py >= 0 && py < 20) board[py * 10 + px] = val;
    }
}

static int32_t bk_score_board(void) {
    int32_t cl = 0, holes = 0, agg_h = 0;
    for (int row = 0; row < 20; row++) {
        int full = 1;
        for (int col = 0; col < 10; col++) if (!board[row * 10 + col]) full = 0;
        if (full) cl++;
    }
    for (int col = 0; col < 10; col++) {
        int seen = 0;
        for (int row = 0; row < 20; row++) {
            if (board[row * 10 + col]) { if (!seen) { seen = 1; agg_h += 20 - row; } }
            else if (seen) holes++;
        }
    }
    return cl * 180 - holes * 42 - agg_h * 5;
}

static int32_t bk_eval_place(int32_t p, int32_t r, int32_t x) {
    if (!bk_fits(p, r, x, 0)) return -999999;
    int32_t gy = bk_ghost_y(p, r, x, 0);
    bk_stamp(p, r, x, gy, (uint8_t)(p + 1));
    int32_t sc = bk_score_board();
    bk_stamp(p, r, x, gy, 0);
    return sc + gy * 4;
}

static void bk_plan_ai(void) {
    int32_t p = bk_st[2], best_sc = -999999, best_r = 0, best_x = 3;
    for (int32_t r = 0; r < 4; r++) {
        for (int32_t x = -2; x < 10; x++) {
            int32_t sc = bk_eval_place(p, r, x);
            if (sc > best_sc) { best_sc = sc; best_r = r; best_x = x; }
        }
    }
    bk_st[10] = best_r; bk_st[11] = best_x;
}

static uint32_t bk_piece_color(int32_t cid) {
    if (cid == 1) return rgb(0, 228, 255);
    if (cid == 2) return rgb(255, 212, 82);
    if (cid == 3) return rgb(184, 96, 255);
    if (cid == 4) return rgb(38, 232, 138);
    if (cid == 5) return rgb(255, 72, 104);
    if (cid == 6) return rgb(66, 138, 255);
    return rgb(255, 152, 56);
}

static void bk_spawn_particles(int32_t row) {
    int32_t py = BK_OY + row * BK_CELL + 8;
    for (int col = 0; col < 10; col++) {
        uint32_t c = bk_piece_color(board[row * 10 + col]);
        int32_t px = BK_OX + col * BK_CELL + 8;
        for (int k = 0; k < 96; k++) {
            if (pt_life[k] <= 0) {
                pt_x[k] = px << 4; pt_y[k] = py << 4;
                uint32_t rv = bk_rand();
                pt_vx[k] = (int32_t)(rv & 63u) - 32;
                pt_vy[k] = (int32_t)((rv >> 6) & 31u) - 44;
                pt_life[k] = 22; pt_col[k] = c;
                break;
            }
        }
    }
}

static void bk_spawn_next(void) {
    int32_t p = bk_st[6];
    bk_st[2] = p; bk_st[6] = bk_pop_bag();
    bk_st[3] = 0; bk_st[4] = 3; bk_st[5] = 0;
    if (!bk_fits(p, 0, 3, 0)) memset(board, 0, sizeof(board));
    bk_plan_ai();
}

static void bk_collapse_row(int32_t row) {
    while (row > 0) {
        memcpy(&board[row * 10], &board[(row - 1) * 10], 10);
        row--;
    }
    memset(board, 0, 10);
}

static void bk_lock_piece(void) {
    bk_stamp(bk_st[2], bk_st[3], bk_st[4], bk_st[5], (uint8_t)(bk_st[2] + 1));
    int32_t cleared = 0, fmask = 0, row = 19;
    while (row >= 0) {
        int full = 1;
        for (int col = 0; col < 10; col++) if (!board[row * 10 + col]) full = 0;
        if (full) {
            cleared++; fmask |= (1 << row);
            bk_spawn_particles(row);
            bk_collapse_row(row);
        } else row--;
    }
    if (cleared > 0) {
        bk_st[12] = 10; bk_st[13] = fmask;
        bk_st[8] += cleared;
        bk_st[9] = 1 + bk_st[8] / 4;
        bk_st[7] += cleared * cleared * 100 * bk_st[9];
    } else bk_st[7] += 10;
    bk_spawn_next();
}

static void bk_init(void) {
    memset(board, 0, sizeof(board));
    memset(pt_life, 0, sizeof(pt_life));
    bk_st[0] = 13372026; bk_st[1] = 7; bk_st[7] = 0; bk_st[8] = 0;
    bk_st[9] = 1; bk_st[12] = 0; bk_st[13] = 0; bk_st[14] = 0;
    for (int r = 0; r < 4; r++) {
        int row = 16 + r, gap = 2 + (r * 3) % 6;
        for (int c = 0; c < 10; c++) {
            if (c != gap) board[row * 10 + c] = (uint8_t)(1 + (r * 3 + c) % 7);
        }
    }
    bk_refill_bag();
    bk_st[6] = bk_pop_bag();
    bk_spawn_next();
}

static void bk_step(uint64_t fr) {
    bk_st[14] = (int32_t)fr;
    for (int k = 0; k < 96; k++) {
        if (pt_life[k] > 0) {
            pt_x[k] += pt_vx[k]; pt_y[k] += pt_vy[k];
            pt_vy[k] += 3; pt_life[k]--;
        }
    }
    if (bk_st[12] > 0) bk_st[12]--;
    if (bk_st[3] != bk_st[10]) {
        if (!bk_try_rotate()) bk_st[10] = bk_st[3];
    } else {
        int32_t cx = bk_st[4], tx = bk_st[11];
        if (cx < tx && bk_fits(bk_st[2], bk_st[3], cx + 1, bk_st[5])) bk_st[4] = cx + 1;
        else if (cx > tx && bk_fits(bk_st[2], bk_st[3], cx - 1, bk_st[5])) bk_st[4] = cx - 1;
    }
    int32_t p = bk_st[2], r = bk_st[3], cx = bk_st[4], cy = bk_st[5];
    int32_t step_n = (r == bk_st[10] && cx == bk_st[11]) ? 2 : 1;
    for (int s = 0; s < step_n; s++) {
        if (bk_fits(p, r, cx, cy + 1)) { cy++; bk_st[5] = cy; }
        else { bk_lock_piece(); return; }
    }
}

static void bk_draw_bevel(int32_t x, int32_t y, uint32_t c) {
    uint32_t r = (c >> 16) & 255u, g = (c >> 8) & 255u, b = c & 255u;
    uint32_t hi = rgb(r + 72, g + 72, b + 72);
    uint32_t lo = rgb(r >> 1, g >> 1, b >> 1);
    uint32_t core = rgb(r + 22, g + 22, b + 22);
    fill_rect(x, y, 15, 15, c);
    fill_rect(x, y, 15, 2, hi);
    fill_rect(x, y, 2, 15, hi);
    fill_rect(x, y + 13, 15, 2, lo);
    fill_rect(x + 13, y, 2, 15, lo);
    fill_rect(x + 4, y + 4, 7, 7, core);
    fill_rect(x + 4, y + 4, 3, 2, rgb(255, 255, 255));
}

static void bk_draw_ghost(int32_t x, int32_t y, uint32_t c) {
    blend_rect(x + 1, y + 1, 13, 13, c, 46);
    rect_outline(x, y, 15, 15, c);
    rect_outline(x + 1, y + 1, 13, 13, c);
}

static void bk_draw_bg(void) {
    for (uint32_t y = 0; y < 360; y++) {
        uint32_t col = rgb(7u + ((y * 10u) >> 8), 11u + ((y * 16u) >> 8), 26u + ((y * 30u) >> 8));
        size_t row = (size_t)y * 640u;
        for (size_t x = 0; x < 640; x++) fb[row + x] = col;
    }
    uint32_t gcol = rgb(18, 28, 54);
    for (int32_t gx = 20; gx < 640; gx += 40) fill_rect(gx, 0, 1, 360, gcol);
    for (int32_t gy = 20; gy < 360; gy += 40) fill_rect(0, gy, 640, 1, gcol);
}

static void bk_draw_panels(void) {
    blend_rect(28, 24, 184, 312, rgb(10, 16, 34), 220);
    rect_outline(28, 24, 184, 312, rgb(0, 205, 245));
    fill_rect(29, 25, 45, 3, rgb(0, 228, 255));
    fill_rect(74, 25, 46, 3, rgb(255, 209, 102));
    fill_rect(120, 25, 46, 3, rgb(34, 229, 138));
    fill_rect(166, 25, 45, 3, rgb(184, 107, 255));
    draw_label("OMEGA", 46, 42, 2, rgb(0, 230, 255));
    draw_label("BLOCKS", 46, 64, 2, rgb(255, 224, 130));
    fill_rect(42, 88, 156, 1, rgb(46, 72, 118));
    draw_label("SCORE", 46, 104, 2, rgb(148, 184, 228));
    draw_uint((uint64_t)bk_st[7], 46, 128, 2, rgb(255, 255, 255));
    draw_label("LINES", 46, 176, 2, rgb(148, 184, 228));
    draw_uint((uint64_t)bk_st[8], 46, 200, 2, rgb(34, 229, 138));
    draw_label("LEVEL", 46, 248, 2, rgb(148, 184, 228));
    draw_uint((uint64_t)bk_st[9], 46, 272, 2, rgb(255, 209, 102));
    blend_rect(428, 24, 184, 312, rgb(10, 16, 34), 220);
    rect_outline(428, 24, 184, 312, rgb(184, 107, 255));
    draw_label("NEXT", 452, 42, 2, rgb(0, 230, 255));
    fill_rect(448, 70, 144, 96, rgb(6, 10, 22));
    rect_outline(448, 70, 144, 96, rgb(58, 88, 140));
    int32_t np = bk_st[6]; uint32_t nc = bk_piece_color(np + 1);
    for (int c = 0; c < 4; c++) bk_draw_bevel(488 + bk_dx(np, 0, c) * BK_CELL, 94 + bk_dy(np, 0, c) * BK_CELL, nc);
    draw_label("7-BAG", 452, 194, 2, rgb(255, 209, 102));
    int32_t bp = bk_st[1];
    for (int i = 0; i < 7; i++) {
        int32_t bx = 452 + i * 20;
        uint32_t col = i >= bp ? bk_piece_color(bag[i] + 1) : rgb(28, 38, 62);
        fill_rect(bx, 222, 14, 14, col);
        rect_outline(bx, 222, 14, 14, rgb(180, 210, 255));
    }
    draw_label("FRAME", 452, 264, 2, rgb(148, 184, 228));
    draw_uint((uint64_t)bk_st[14], 452, 288, 2, rgb(220, 238, 255));
}

static void bk_render(void) {
    bk_draw_bg();
    fill_rect(BK_OX - 4, BK_OY - 4, 168, 328, rgb(6, 10, 20));
    rect_outline(BK_OX - 4, BK_OY - 4, 168, 328, rgb(0, 224, 255));
    rect_outline(BK_OX - 2, BK_OY - 2, 164, 324, rgb(38, 62, 104));
    for (int row = 0; row < 20; row++)
        for (int col = 0; col < 10; col++)
            rect_outline(BK_OX + col * BK_CELL, BK_OY + row * BK_CELL, 15, 15, rgb(16, 24, 44));
    for (int row = 0; row < 20; row++)
        for (int col = 0; col < 10; col++)
            if (board[row * 10 + col])
                bk_draw_bevel(BK_OX + col * BK_CELL, BK_OY + row * BK_CELL, bk_piece_color(board[row * 10 + col]));
    int32_t p = bk_st[2], r = bk_st[3], cx = bk_st[4], cy = bk_st[5];
    int32_t gy0 = bk_ghost_y(p, r, cx, cy);
    uint32_t pc = bk_piece_color(p + 1);
    for (int c = 0; c < 4; c++) bk_draw_ghost(BK_OX + (cx + bk_dx(p, r, c)) * BK_CELL, BK_OY + (gy0 + bk_dy(p, r, c)) * BK_CELL, pc);
    for (int c = 0; c < 4; c++) bk_draw_bevel(BK_OX + (cx + bk_dx(p, r, c)) * BK_CELL, BK_OY + (cy + bk_dy(p, r, c)) * BK_CELL, pc);
    if (bk_st[12] > 0) {
        uint32_t al = (uint32_t)bk_st[12] * 22u;
        for (int row = 0; row < 20; row++)
            if ((bk_st[13] >> row) & 1)
                blend_rect(BK_OX, BK_OY + row * BK_CELL, 160, BK_CELL, rgb(230, 252, 255), al);
    }
    for (int k = 0; k < 96; k++) {
        int32_t lf = pt_life[k];
        if (lf > 0) {
            int32_t px = pt_x[k] >> 4, py = pt_y[k] >> 4;
            uint32_t c = pt_col[k], ulf = (uint32_t)lf;
            uint32_t rr = uclamp255((((c >> 16) & 255u) * ulf) >> 4);
            uint32_t gg = uclamp255((((c >> 8) & 255u) * ulf) >> 4);
            uint32_t bb = uclamp255(((c & 255u) * ulf) >> 4);
            px_add(px, py, rr, gg, bb); px_add(px + 1, py, rr, gg, bb);
            px_add(px, py + 1, rr, gg, bb); px_add(px + 1, py + 1, rr, gg, bb);
            px_add(px - 1, py, rr >> 1, gg >> 1, bb >> 1);
            px_add(px + 2, py, rr >> 1, gg >> 1, bb >> 1);
            px_add(px, py - 1, rr >> 1, gg >> 1, bb >> 1);
            px_add(px, py + 2, rr >> 1, gg >> 1, bb >> 1);
        }
    }
    bk_draw_panels();
}

int main(int argc, char **argv) {
    gfx_init();
    int mode = 0; uint64_t param = 300;
    if (!parse_cli(argc, argv, BK_HERO, &mode, &param)) {
        fprintf(stderr, "ERR bad args\n");
        return 2;
    }
    bk_init();
    if (mode == 0) {
        uint64_t t0 = monons(); uint32_t dce = 0;
        for (uint64_t f = 0; f < param; f++) { bk_step(f); bk_render(); dce ^= sample_fb_u32(f); }
        uint64_t ns = monons() - t0;
        uint8_t dig[32]; sha_fb_ppm(dig);
        uint64_t us_f = ns ? ns / (param * 1000ull) : 0;
        uint64_t fps100 = ns ? (param * 100000000000ull) / ns : 0;
        uint64_t vmhwm = read_vmhwm_kb();
        printf("GAME=blocks_puzzle FRAMES=%llu NS=%llu US_PER_FRAME=%llu FPS_X100=%llu ANTI_DCE=%08x VMHWM_KB=%llu FINAL_PPM_SHA256=",
               (unsigned long long)param, (unsigned long long)ns, (unsigned long long)us_f, (unsigned long long)fps100, dce, (unsigned long long)vmhwm);
        print_hex32(dig); printf("\n");
        return 0;
    }
    if (mode == 1) {
        uint64_t hf = BK_HERO >= param ? param - 1 : BK_HERO;
        uint8_t hdig[32], fdig[32], sdig[32];
        for (uint64_t f = 0; f < param; f++) {
            bk_step(f); bk_render(); frame_hashes[f] = hash_fb_u32();
            if (f == hf) sha_fb_ppm(hdig);
        }
        sha_fb_ppm(fdig); sha_stream_u32((size_t)param, sdig);
        printf("GAME=blocks_puzzle FRAMES=%llu HERO_FRAME=%llu HERO_PPM_SHA256=",
               (unsigned long long)param, (unsigned long long)hf);
        print_hex32(hdig); printf(" FINAL_PPM_SHA256=");
        print_hex32(fdig); printf(" STREAM_SHA256=");
        print_hex32(sdig); printf("\n");
        return 0;
    }
    for (uint64_t f = 0; f <= param; f++) { bk_step(f); bk_render(); }
    return write_ppm();
}
