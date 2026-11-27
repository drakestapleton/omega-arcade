# SPDX-License-Identifier: Apache-2.0
# ref/py/raycast_doom.py: Bit-exact Python 3 reference port of om/raycast_doom.om.
import gfx_common as G

RC_HERO = 185
RC_MAP_STR = (
    "1111111111111111100000200000000110300020040400011000000000000301"
    "1000200000000001120020030004000110000000000000011040000000020021"
    "1000003000000001100000000000030110200000040000011020000000002001"
    "1000030000002001100000000000000110040002003000011111111111111111"
)

rmap = [0] * 256
bg_row = [0] * 360
rc_st = [0] * 8


def rc_init():
    for i in range(256):
        rmap[i] = ord(RC_MAP_STR[i]) - 48
    rc_st[0] = 10240
    rc_st[1] = 10240
    rc_st[2] = 110
    rc_st[3] = 0
    rc_st[4] = 99
    rc_st[5] = 0
    for y in range(180):
        d = 179 - y
        bg_row[y] = G.rgb(8 + ((d * 14) >> 8), 14 + ((d * 24) >> 8), 30 + ((d * 48) >> 8))
    for y in range(180):
        band = 6 if ((y >> 3) & 1) == 0 else 0
        bg_row[y + 180] = G.rgb(
            14 + ((y * 40) >> 8) + band,
            18 + ((y * 48) >> 8) + band,
            28 + ((y * 64) >> 8) + band,
        )


def rc_blocked(x, y):
    if x < 4096 or y < 4096:
        return True
    mx = x >> 12
    my = y >> 12
    if mx >= 16 or my >= 16:
        return True
    return rmap[my * 16 + mx] != 0


def rc_try_move(dx, dy):
    px = rc_st[0]
    py = rc_st[1]
    nx = px + dx
    ny = py + dy
    m = 900
    if not rc_blocked(nx + m, py) and not rc_blocked(nx - m, py):
        rc_st[0] = nx
        px = nx
    if not rc_blocked(px, ny + m) and not rc_blocked(px, ny - m):
        rc_st[1] = ny


def rc_step(fr):
    if fr < 60:
        turn, spd = 1, 175
    elif fr < 130:
        turn, spd = 4, 155
    elif fr < 210:
        turn, spd = -3, 165
    else:
        turn, spd = 3, 170
    ang = (rc_st[2] + turn) & 1023
    rc_st[2] = ang
    rc_try_move((G.icos1024(ang) * spd) >> 12, (G.isin1024(ang) * spd) >> 12)
    rc_st[5] += 28
    if rc_st[3] > 0:
        rc_st[3] -= 1
    if (fr % 25) == 10:
        rc_st[3] = 6
        if rc_st[4] > 0:
            rc_st[4] -= 1


def rc_draw_weapon():
    bp = rc_st[5]
    bx = (G.icos1024(bp) * 8) >> 12
    by = (abs(G.isin1024(bp)) * 6) >> 12
    ft = rc_st[3]
    cx = 320 + bx
    base_y = 246 + by + ft * 4
    if ft > 2:
        my = base_y - 14
        G.draw_ring(cx, my, 24, G.rgb(0, 210, 255))
        G.draw_ring(cx, my, 16, G.rgb(255, 215, 90))
        G.draw_ring(cx, my, 9, G.rgb(255, 255, 240))
        G.draw_glow_line(cx - 30, my, cx + 30, my, G.rgb(0, 220, 255))
        G.draw_glow_line(cx, my - 26, cx, my + 26, G.rgb(255, 220, 110))
        G.draw_glow_line(cx - 20, my - 20, cx + 20, my + 20, G.rgb(180, 110, 255))
        G.draw_glow_line(cx - 20, my + 20, cx + 20, my - 20, G.rgb(40, 240, 150))
    G.fill_rect(cx - 38, base_y + 22, 76, 96, G.rgb(22, 28, 42))
    G.fill_rect(cx - 30, base_y + 8, 60, 110, G.rgb(32, 42, 60))
    G.fill_rect(cx - 18, base_y - 6, 36, 124, G.rgb(44, 56, 78))
    G.rect_outline(cx - 38, base_y + 22, 76, 96, G.rgb(76, 96, 130))
    G.rect_outline(cx - 18, base_y - 6, 36, 124, G.rgb(96, 122, 162))
    G.fill_rect(cx - 10, base_y - 12, 20, 12, G.rgb(18, 22, 34))
    G.fill_rect(cx - 6, base_y - 10, 12, 8, G.rgb(0, 230, 255))
    G.fill_rect(cx - 26, base_y + 14, 5, 88, G.rgb(0, 228, 255))
    G.fill_rect(cx - 14, base_y + 4, 4, 98, G.rgb(255, 209, 102))
    G.fill_rect(cx + 10, base_y + 4, 4, 98, G.rgb(34, 229, 138))
    G.fill_rect(cx + 21, base_y + 14, 5, 88, G.rgb(184, 107, 255))
    G.draw_glow_line(cx - 24, base_y + 16, cx - 24, base_y + 96, G.rgb(0, 180, 220))
    G.draw_glow_line(cx - 12, base_y + 6, cx - 12, base_y + 98, G.rgb(220, 170, 60))
    G.draw_glow_line(cx + 12, base_y + 6, cx + 12, base_y + 98, G.rgb(25, 190, 110))
    G.draw_glow_line(cx + 23, base_y + 16, cx + 23, base_y + 96, G.rgb(150, 80, 220))


def rc_draw_minimap():
    mx0, my0 = 522, 10
    G.blend_rect(mx0 - 4, my0 - 4, 106, 106, G.rgb(6, 10, 22), 205)
    G.rect_outline(mx0 - 4, my0 - 4, 106, 106, G.rgb(0, 212, 255))
    c0, c1, c2, c3, c4 = (
        G.rgb(16, 24, 40),
        G.rgb(104, 134, 176),
        G.rgb(0, 216, 255),
        G.rgb(245, 186, 72),
        G.rgb(156, 96, 236),
    )
    pal = (c0, c1, c2, c3, c4)
    for gy in range(16):
        for gx in range(16):
            G.fill_rect(mx0 + gx * 6, my0 + gy * 6, 5, 5, pal[rmap[gy * 16 + gx]])
    ppx = mx0 + ((rc_st[0] * 6) >> 12)
    ppy = my0 + ((rc_st[1] * 6) >> 12)
    ang = rc_st[2]
    for r in range(5):
        ra = ang + r * 32 - 64
        G.draw_line(
            ppx,
            ppy,
            ppx + ((G.icos1024(ra) * 18) >> 12),
            ppy + ((G.isin1024(ra) * 18) >> 12),
            G.rgb(40, 240, 200),
        )
    G.fill_rect(ppx - 2, ppy - 2, 5, 5, G.rgb(255, 235, 120))
    G.px_set(ppx, ppy, G.rgb(255, 255, 255))


def rc_draw_hud():
    G.fill_rect(313, 180, 5, 1, G.rgb(0, 230, 255))
    G.fill_rect(323, 180, 5, 1, G.rgb(0, 230, 255))
    G.fill_rect(320, 173, 1, 5, G.rgb(0, 230, 255))
    G.fill_rect(320, 183, 1, 5, G.rgb(0, 230, 255))
    G.blend_rect(12, 308, 168, 42, G.rgb(8, 14, 28), 210)
    G.rect_outline(12, 308, 168, 42, G.rgb(0, 198, 240))
    G.draw_label("OMEGA RAY", 20, 314, 1, G.rgb(0, 230, 255))
    G.draw_label("HP 100", 20, 330, 1, G.rgb(34, 229, 138))
    G.draw_label("AMMO", 92, 330, 1, G.rgb(255, 209, 102))
    G.draw_uint(rc_st[4], 128, 330, 1, G.rgb(255, 244, 214))
    G.fill_rect(13, 309, 41, 2, G.rgb(0, 228, 255))
    G.fill_rect(54, 309, 41, 2, G.rgb(255, 209, 102))
    G.fill_rect(95, 309, 42, 2, G.rgb(34, 229, 138))
    G.fill_rect(137, 309, 42, 2, G.rgb(184, 107, 255))


def rc_render():
    fb = G.fb
    l_bg = bg_row
    l_rmap = rmap
    ang = rc_st[2]
    dirx = G.icos1024(ang)
    diry = G.isin1024(ang)
    plx = ((-diry) * 2703) >> 12
    ply = (dirx * 2703) >> 12
    px = rc_st[0]
    py = rc_st[1]
    mx0 = px >> 12
    my0 = py >> 12
    # Pre-fill ceiling & floor rows once per frame (exact match to per-col fill)
    for y in range(360):
        c = l_bg[y]
        row = y * 640
        fb[row : row + 640] = [c] * 640
    for col in range(640):
        cx_num = (col * 2 - 640) * 4096
        camx = -((-cx_num) // 640) if cx_num < 0 else cx_num // 640
        rdx = dirx + ((plx * camx) >> 12)
        rdy = diry + ((ply * camx) >> 12)
        mx = mx0
        my = my0
        ddx = 16777216 // (-rdx if rdx < 0 else rdx) if rdx != 0 else 1000000000
        ddy = 16777216 // (-rdy if rdy < 0 else rdy) if rdy != 0 else 1000000000
        if rdx < 0:
            stx = -1
            sdx = ((px - (mx << 12)) * ddx) >> 12
        else:
            stx = 1
            sdx = ((((mx + 1) << 12) - px) * ddx) >> 12
        if rdy < 0:
            sty = -1
            sdy = ((py - (my << 12)) * ddy) >> 12
        else:
            sty = 1
            sdy = ((((my + 1) << 12) - py) * ddy) >> 12
        side = 0
        wtype = 1
        for _ in range(32):
            if sdx < sdy:
                sdx += ddx
                mx += stx
                side = 0
            else:
                sdy += ddy
                my += sty
                side = 1
            cell = 1 if (mx < 0 or mx >= 16 or my < 0 or my >= 16) else l_rmap[my * 16 + mx]
            if cell != 0:
                wtype = cell
                break
        pdist = (sdx - ddx) if side == 0 else (sdy - ddy)
        if pdist < 64:
            pdist = 64
        wpos = (py + ((pdist * rdy) >> 12)) if side == 0 else (px + ((pdist * rdx) >> 12))
        tx = (wpos & 4095) >> 7
        lineh = 1474560 // pdist
        if lineh < 2:
            lineh = 2
        dstart = 180 - (lineh >> 1)
        dend = 180 + (lineh >> 1)
        cs = 0 if dstart < 0 else dstart
        ce = 359 if dend > 359 else dend
        fog = (pdist - 3072) >> 7
        if fog < 0:
            fog = 0
        elif fog > 220:
            fog = 220
        inv = 256 - fog
        tstep = 2097152 // lineh
        tpos = (cs - dstart) * tstep
        idx = cs * 640 + col
        for _y in range(cs, ce + 1):
            ty = (tpos >> 16) & 31
            tpos += tstep
            if wtype == 1:
                stagger = ((ty >> 3) & 1) * 8
                if (ty & 7) == 0:
                    r, g, b = 36, 48, 68
                elif ((tx + stagger) & 15) == 0:
                    r, g, b = 40, 52, 72
                elif (ty & 7) == 1:
                    r, g, b = 122, 148, 188
                else:
                    r, g, b = 92, 116, 152
            elif wtype == 2:
                if 13 <= ty <= 18:
                    r, g, b = 18, 232, 255
                elif (tx & 7) == 0:
                    r, g, b = 20, 118, 160
                else:
                    r, g, b = 28, 72, 112
            elif wtype == 3:
                if ty < 3 or ty > 28:
                    r, g, b = 94, 64, 26
                elif 12 <= tx <= 19:
                    r, g, b = 255, 206, 88
                else:
                    r, g, b = 196, 136, 44
            else:
                if 14 <= ty <= 17:
                    r, g, b = 38, 236, 148
                elif (ty & 7) == 0:
                    r, g, b = 56, 34, 92
                else:
                    r, g, b = 104, 62, 164
            if side == 1:
                r = (r * 180) >> 8
                g = (g * 180) >> 8
                b = (b * 180) >> 8
            r = (r * inv + 10 * fog) >> 8
            g = (g * inv + 16 * fog) >> 8
            b = (b * inv + 30 * fog) >> 8
            fb[idx] = (r << 16) | (g << 8) | b
            idx += 640
    rc_draw_weapon()
    rc_draw_minimap()
    rc_draw_hud()


if __name__ == "__main__":
    G.run_driver("raycast_doom", RC_HERO, rc_init, rc_step, rc_render)
