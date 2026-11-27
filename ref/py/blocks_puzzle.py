# SPDX-License-Identifier: Apache-2.0
# ref/py/blocks_puzzle.py: Bit-exact Python 3 reference port of om/blocks_puzzle.om.
import gfx_common as G

BK_HERO = 248
BK_OX = 240
BK_OY = 20
BK_CELL = 16

BK_SHAPES_STR = (
    "0111213120212223021222321011121310201121102011211020112110201121"
    "1001112110112112011121121001111210200111101121221121021200011112"
    "0010112120112112011112221001110200011121102011120111212210110212"
    "20011121101112220111210200101112"
)
BK_SHAPES = [ord(c) - 48 for c in BK_SHAPES_STR]

board = [0] * 200
bag = [0] * 7
bk_st = [0] * 16
pt_x = [0] * 96
pt_y = [0] * 96
pt_vx = [0] * 96
pt_vy = [0] * 96
pt_life = [0] * 96
pt_col = [0] * 96


def bk_rand():
    s = (bk_st[0] * 1664525 + 1013904223) & 0xFFFFFFFF
    bk_st[0] = s & 0x7FFFFFFF
    return s


def bk_dx(p, r, c):
    return BK_SHAPES[p * 32 + r * 8 + c * 2]


def bk_dy(p, r, c):
    return BK_SHAPES[p * 32 + r * 8 + c * 2 + 1]


def bk_refill_bag():
    for i in range(7):
        bag[i] = i
    for i in range(6, 0, -1):
        j = bk_rand() % (i + 1)
        bag[i], bag[j] = bag[j], bag[i]
    bk_st[1] = 0


def bk_pop_bag():
    if bk_st[1] >= 7:
        bk_refill_bag()
    p = bag[bk_st[1]]
    bk_st[1] += 1
    return p


def bk_fits(p, r, x, y):
    base = p * 32 + r * 8
    for c in range(4):
        gx = x + BK_SHAPES[base + c * 2]
        gy = y + BK_SHAPES[base + c * 2 + 1]
        if gx < 0 or gx >= 10 or gy >= 20:
            return False
        if gy >= 0 and board[gy * 10 + gx] != 0:
            return False
    return True


def bk_ghost_y(p, r, x, y):
    gy = y
    while bk_fits(p, r, x, gy + 1):
        gy += 1
    return gy


def bk_try_rotate():
    p = bk_st[2]
    r1 = (bk_st[3] + 1) & 3
    x0 = bk_st[4]
    y0 = bk_st[5]
    for dx, dy in ((0, 0), (-1, 0), (1, 0), (0, -1), (-2, 0), (2, 0)):
        if bk_fits(p, r1, x0 + dx, y0 + dy):
            bk_st[3] = r1
            bk_st[4] = x0 + dx
            bk_st[5] = y0 + dy
            return True
    return False


def bk_stamp(p, r, x, y, val):
    base = p * 32 + r * 8
    for c in range(4):
        px = x + BK_SHAPES[base + c * 2]
        py = y + BK_SHAPES[base + c * 2 + 1]
        if 0 <= py < 20:
            board[py * 10 + px] = val


def bk_score_board():
    cl = 0
    for row in range(20):
        base = row * 10
        if all(board[base + col] for col in range(10)):
            cl += 1
    holes = 0
    agg_h = 0
    for col in range(10):
        seen = False
        for row in range(20):
            if board[row * 10 + col]:
                if not seen:
                    seen = True
                    agg_h += 20 - row
            elif seen:
                holes += 1
    return cl * 180 - holes * 42 - agg_h * 5


def bk_eval_place(p, r, x):
    if not bk_fits(p, r, x, 0):
        return -999999
    gy = bk_ghost_y(p, r, x, 0)
    bk_stamp(p, r, x, gy, p + 1)
    sc = bk_score_board()
    bk_stamp(p, r, x, gy, 0)
    return sc + gy * 4


def bk_plan_ai():
    p = bk_st[2]
    best_sc, best_r, best_x = -999999, 0, 3
    for r in range(4):
        for x in range(-2, 10):
            sc = bk_eval_place(p, r, x)
            if sc > best_sc:
                best_sc, best_r, best_x = sc, r, x
    bk_st[10] = best_r
    bk_st[11] = best_x


def bk_piece_color(cid):
    if cid == 1:
        return G.rgb(0, 228, 255)
    if cid == 2:
        return G.rgb(255, 212, 82)
    if cid == 3:
        return G.rgb(184, 96, 255)
    if cid == 4:
        return G.rgb(38, 232, 138)
    if cid == 5:
        return G.rgb(255, 72, 104)
    if cid == 6:
        return G.rgb(66, 138, 255)
    return G.rgb(255, 152, 56)


def bk_spawn_particles(row):
    py = BK_OY + row * BK_CELL + 8
    for col in range(10):
        c = bk_piece_color(board[row * 10 + col])
        px = BK_OX + col * BK_CELL + 8
        for k in range(96):
            if pt_life[k] <= 0:
                pt_x[k] = px << 4
                pt_y[k] = py << 4
                rv = bk_rand()
                pt_vx[k] = (rv & 63) - 32
                pt_vy[k] = ((rv >> 6) & 31) - 44
                pt_life[k] = 22
                pt_col[k] = c
                break


def bk_spawn_next():
    p = bk_st[6]
    bk_st[2] = p
    bk_st[6] = bk_pop_bag()
    bk_st[3] = 0
    bk_st[4] = 3
    bk_st[5] = 0
    if not bk_fits(p, 0, 3, 0):
        for i in range(200):
            board[i] = 0
    bk_plan_ai()


def bk_collapse_row(row):
    while row > 0:
        board[row * 10 : row * 10 + 10] = board[(row - 1) * 10 : row * 10]
        row -= 1
    board[0:10] = [0] * 10


def bk_lock_piece():
    bk_stamp(bk_st[2], bk_st[3], bk_st[4], bk_st[5], bk_st[2] + 1)
    cleared = 0
    fmask = 0
    row = 19
    while row >= 0:
        base = row * 10
        if all(board[base + col] for col in range(10)):
            cleared += 1
            fmask |= 1 << row
            bk_spawn_particles(row)
            bk_collapse_row(row)
        else:
            row -= 1
    if cleared > 0:
        bk_st[12] = 10
        bk_st[13] = fmask
        bk_st[8] += cleared
        bk_st[9] = 1 + (bk_st[8] // 4)
        bk_st[7] += cleared * cleared * 100 * bk_st[9]
    else:
        bk_st[7] += 10
    bk_spawn_next()


def bk_init():
    for i in range(200):
        board[i] = 0
    for i in range(96):
        pt_life[i] = 0
    bk_st[0] = 13372026
    bk_st[1] = 7
    bk_st[7] = 0
    bk_st[8] = 0
    bk_st[9] = 1
    bk_st[12] = 0
    bk_st[13] = 0
    bk_st[14] = 0
    for r in range(4):
        row = 16 + r
        gap = 2 + (r * 3) % 6
        for c in range(10):
            if c != gap:
                board[row * 10 + c] = 1 + (r * 3 + c) % 7
    bk_refill_bag()
    bk_st[6] = bk_pop_bag()
    bk_spawn_next()


def bk_step(fr):
    bk_st[14] = fr
    for k in range(96):
        if pt_life[k] > 0:
            pt_x[k] += pt_vx[k]
            pt_y[k] += pt_vy[k]
            pt_vy[k] += 3
            pt_life[k] -= 1
    if bk_st[12] > 0:
        bk_st[12] -= 1
    if bk_st[3] != bk_st[10]:
        if not bk_try_rotate():
            bk_st[10] = bk_st[3]
    else:
        cx, tx = bk_st[4], bk_st[11]
        if cx < tx and bk_fits(bk_st[2], bk_st[3], cx + 1, bk_st[5]):
            bk_st[4] = cx + 1
        elif cx > tx and bk_fits(bk_st[2], bk_st[3], cx - 1, bk_st[5]):
            bk_st[4] = cx - 1
    p, r, cx, cy = bk_st[2], bk_st[3], bk_st[4], bk_st[5]
    step_n = 2 if (r == bk_st[10] and cx == bk_st[11]) else 1
    for _ in range(step_n):
        if bk_fits(p, r, cx, cy + 1):
            cy += 1
            bk_st[5] = cy
        else:
            bk_lock_piece()
            return


def bk_draw_bevel(x, y, c):
    r = (c >> 16) & 255
    g = (c >> 8) & 255
    b = c & 255
    hi = G.rgb(r + 72, g + 72, b + 72)
    lo = G.rgb(r >> 1, g >> 1, b >> 1)
    core = G.rgb(r + 22, g + 22, b + 22)
    G.fill_rect(x, y, 15, 15, c)
    G.fill_rect(x, y, 15, 2, hi)
    G.fill_rect(x, y, 2, 15, hi)
    G.fill_rect(x, y + 13, 15, 2, lo)
    G.fill_rect(x + 13, y, 2, 15, lo)
    G.fill_rect(x + 4, y + 4, 7, 7, core)
    G.fill_rect(x + 4, y + 4, 3, 2, 0xFFFFFF)


def bk_draw_ghost(x, y, c):
    G.blend_rect(x + 1, y + 1, 13, 13, c, 46)
    G.rect_outline(x, y, 15, 15, c)
    G.rect_outline(x + 1, y + 1, 13, 13, c)


def bk_draw_bg():
    fb = G.fb
    for y in range(360):
        col = G.rgb(7 + ((y * 10) >> 8), 11 + ((y * 16) >> 8), 26 + ((y * 30) >> 8))
        row = y * 640
        fb[row : row + 640] = [col] * 640
    gcol = G.rgb(18, 28, 54)
    for gx in range(20, 640, 40):
        G.fill_rect(gx, 0, 1, 360, gcol)
    for gy in range(20, 360, 40):
        G.fill_rect(0, gy, 640, 1, gcol)


def bk_draw_panels():
    G.blend_rect(28, 24, 184, 312, G.rgb(10, 16, 34), 220)
    G.rect_outline(28, 24, 184, 312, G.rgb(0, 205, 245))
    G.fill_rect(29, 25, 45, 3, G.rgb(0, 228, 255))
    G.fill_rect(74, 25, 46, 3, G.rgb(255, 209, 102))
    G.fill_rect(120, 25, 46, 3, G.rgb(34, 229, 138))
    G.fill_rect(166, 25, 45, 3, G.rgb(184, 107, 255))
    G.draw_label("OMEGA", 46, 42, 2, G.rgb(0, 230, 255))
    G.draw_label("BLOCKS", 46, 64, 2, G.rgb(255, 224, 130))
    G.fill_rect(42, 88, 156, 1, G.rgb(46, 72, 118))
    G.draw_label("SCORE", 46, 104, 2, G.rgb(148, 184, 228))
    G.draw_uint(bk_st[7], 46, 128, 2, 0xFFFFFF)
    G.draw_label("LINES", 46, 176, 2, G.rgb(148, 184, 228))
    G.draw_uint(bk_st[8], 46, 200, 2, G.rgb(34, 229, 138))
    G.draw_label("LEVEL", 46, 248, 2, G.rgb(148, 184, 228))
    G.draw_uint(bk_st[9], 46, 272, 2, G.rgb(255, 209, 102))
    G.blend_rect(428, 24, 184, 312, G.rgb(10, 16, 34), 220)
    G.rect_outline(428, 24, 184, 312, G.rgb(184, 107, 255))
    G.draw_label("NEXT", 452, 42, 2, G.rgb(0, 230, 255))
    G.fill_rect(448, 70, 144, 96, G.rgb(6, 10, 22))
    G.rect_outline(448, 70, 144, 96, G.rgb(58, 88, 140))
    np = bk_st[6]
    nc = bk_piece_color(np + 1)
    for c in range(4):
        bk_draw_bevel(488 + bk_dx(np, 0, c) * BK_CELL, 94 + bk_dy(np, 0, c) * BK_CELL, nc)
    G.draw_label("7-BAG", 452, 194, 2, G.rgb(255, 209, 102))
    bp = bk_st[1]
    for i in range(7):
        bx = 452 + i * 20
        col = bk_piece_color(bag[i] + 1) if i >= bp else G.rgb(28, 38, 62)
        G.fill_rect(bx, 222, 14, 14, col)
        G.rect_outline(bx, 222, 14, 14, G.rgb(180, 210, 255))
    G.draw_label("FRAME", 452, 264, 2, G.rgb(148, 184, 228))
    G.draw_uint(bk_st[14], 452, 288, 2, G.rgb(220, 238, 255))


def bk_render():
    bk_draw_bg()
    G.fill_rect(BK_OX - 4, BK_OY - 4, 168, 328, G.rgb(6, 10, 20))
    G.rect_outline(BK_OX - 4, BK_OY - 4, 168, 328, G.rgb(0, 224, 255))
    G.rect_outline(BK_OX - 2, BK_OY - 2, 164, 324, G.rgb(38, 62, 104))
    grid_c = G.rgb(16, 24, 44)
    for row in range(20):
        for col in range(10):
            G.rect_outline(BK_OX + col * BK_CELL, BK_OY + row * BK_CELL, 15, 15, grid_c)
    for row in range(20):
        for col in range(10):
            cid = board[row * 10 + col]
            if cid:
                bk_draw_bevel(BK_OX + col * BK_CELL, BK_OY + row * BK_CELL, bk_piece_color(cid))
    p, r, cx, cy = bk_st[2], bk_st[3], bk_st[4], bk_st[5]
    gy0 = bk_ghost_y(p, r, cx, cy)
    pc = bk_piece_color(p + 1)
    for c in range(4):
        bk_draw_ghost(BK_OX + (cx + bk_dx(p, r, c)) * BK_CELL, BK_OY + (gy0 + bk_dy(p, r, c)) * BK_CELL, pc)
    for c in range(4):
        bk_draw_bevel(BK_OX + (cx + bk_dx(p, r, c)) * BK_CELL, BK_OY + (cy + bk_dy(p, r, c)) * BK_CELL, pc)
    if bk_st[12] > 0:
        al = bk_st[12] * 22
        fm = bk_st[13]
        for row in range(20):
            if (fm >> row) & 1:
                G.blend_rect(BK_OX, BK_OY + row * BK_CELL, 160, BK_CELL, G.rgb(230, 252, 255), al)
    for k in range(96):
        lf = pt_life[k]
        if lf > 0:
            px = pt_x[k] >> 4
            py = pt_y[k] >> 4
            c = pt_col[k]
            rr = G.uclamp255((((c >> 16) & 255) * lf) >> 4)
            gg = G.uclamp255((((c >> 8) & 255) * lf) >> 4)
            bb = G.uclamp255(((c & 255) * lf) >> 4)
            G.px_add(px, py, rr, gg, bb)
            G.px_add(px + 1, py, rr, gg, bb)
            G.px_add(px, py + 1, rr, gg, bb)
            G.px_add(px + 1, py + 1, rr, gg, bb)
            hr, hg, hb = rr >> 1, gg >> 1, bb >> 1
            G.px_add(px - 1, py, hr, hg, hb)
            G.px_add(px + 2, py, hr, hg, hb)
            G.px_add(px, py - 1, hr, hg, hb)
            G.px_add(px, py + 2, hr, hg, hb)
    bk_draw_panels()


if __name__ == "__main__":
    G.run_driver("blocks_puzzle", BK_HERO, bk_init, bk_step, bk_render)
