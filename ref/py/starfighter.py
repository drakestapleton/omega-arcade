# SPDX-License-Identifier: Apache-2.0
# ref/py/starfighter.py: Bit-exact Python 3 reference port of om/starfighter.om.
import gfx_common as G

SF_HERO = 195
sf_st = [0] * 8
st_x = [0] * 64
st_y = [0] * 64
st_sp = [0] * 64
st_c = [0] * 64
en_x = [0] * 24
en_y = [0] * 24
en_hp = [0] * 24
en_kind = [0] * 24
en_ph = [0] * 24
pb_x = [0] * 40
pb_y = [0] * 40
pb_vx = [0] * 40
pb_vy = [0] * 40
pb_on = [0] * 40
pb_c = [0] * 40
eb_x = [0] * 32
eb_y = [0] * 32
eb_vx = [0] * 32
eb_vy = [0] * 32
eb_on = [0] * 32
rg_x = [0] * 12
rg_y = [0] * 12
rg_r = [0] * 12
rg_max = [0] * 12
rg_c = [0] * 12
sp_x = [0] * 96
sp_y = [0] * 96
sp_vx = [0] * 96
sp_vy = [0] * 96
sp_life = [0] * 96
sp_c = [0] * 96


def sf_rand():
    s = (sf_st[0] * 1664525 + 1013904223) & 0xFFFFFFFF
    sf_st[0] = s & 0x7FFFFFFF
    return s


def sf_strand_col(k):
    m = k & 3
    if m == 0:
        return G.rgb(0, 228, 255)
    if m == 1:
        return G.rgb(255, 209, 102)
    if m == 2:
        return G.rgb(34, 229, 138)
    return G.rgb(184, 107, 255)


def sf_spawn_ring(x, y, rmax, c):
    for i in range(12):
        if rg_r[i] == 0:
            rg_x[i], rg_y[i], rg_r[i], rg_max[i], rg_c[i] = x, y, 4, rmax, c
            return


def sf_spawn_sparks(x, y, c):
    n = 0
    for i in range(96):
        if sp_life[i] <= 0:
            ang = n * 85
            spd = 20 + (sf_rand() & 31)
            sp_x[i] = x << 4
            sp_y[i] = y << 4
            sp_vx[i] = (G.icos1024(ang) * spd) >> 12
            sp_vy[i] = (G.isin1024(ang) * spd) >> 12
            sp_life[i] = 18
            sp_c[i] = c
            n += 1
            if n >= 12:
                return


def sf_spawn_wave(wv):
    sf_st[4] = wv
    for i in range(12):
        for s in range(24):
            if en_hp[s] <= 0:
                col = i % 6
                row = i // 6
                en_x[s] = 110 + col * 84
                en_y[s] = 54 + row * 46 - 60
                kind = (i + wv) % 3
                en_kind[s] = kind
                en_hp[s] = 4 if kind == 2 else 2
                en_ph[s] = i * 42
                break


def sf_init():
    sf_st[:] = [987654321, 320, 294, 0, 1, 100, 0, 0]
    for i in range(64):
        st_x[i] = sf_rand() % 640
        st_y[i] = sf_rand() % 360
        layer = i % 3
        st_sp[i] = 1 + layer * 2
        if layer == 0:
            st_c[i] = G.rgb(58, 76, 122)
        elif layer == 1:
            st_c[i] = G.rgb(132, 178, 228)
        else:
            st_c[i] = G.rgb(215, 244, 255)
    en_hp[:] = [0] * 24
    pb_on[:] = [0] * 40
    eb_on[:] = [0] * 32
    rg_r[:] = [0] * 12
    sp_life[:] = [0] * 96
    sf_spawn_wave(1)


def sf_fire_bolt(x, y, vx, vy, c):
    for i in range(40):
        if not pb_on[i]:
            pb_x[i], pb_y[i], pb_vx[i], pb_vy[i], pb_c[i], pb_on[i] = x, y, vx, vy, c, 1
            return


def sf_fire_ebolt(x, y, vx, vy):
    for i in range(32):
        if not eb_on[i]:
            eb_x[i], eb_y[i], eb_vx[i], eb_vy[i], eb_on[i] = x, y, vx, vy, 1
            return


def sf_kill_enemy(idx):
    ex, ey, kd = en_x[idx], en_y[idx], en_kind[idx]
    c = G.rgb(184, 107, 255) if kd == 1 else (G.rgb(255, 209, 102) if kd == 2 else G.rgb(0, 228, 255))
    en_hp[idx] = 0
    sf_st[3] += (kd + 1) * 150
    sf_spawn_ring(ex, ey, 46, c)
    sf_spawn_ring(ex, ey, 28, G.rgb(255, 245, 210))
    sf_spawn_sparks(ex, ey, c)


def sf_step(fr):
    sf_st[7] = fr
    for i in range(64):
        ny = st_y[i] + st_sp[i]
        if ny >= 360:
            ny -= 360
            st_x[i] = sf_rand() % 640
        st_y[i] = ny
    px = 320 + ((G.isin1024(fr * 11) * 185) >> 12)
    py = 292 + ((G.icos1024(fr * 7) * 18) >> 12)
    sf_st[1], sf_st[2] = px, py
    if sf_st[6] > 0:
        sf_st[6] -= 1
    if (fr % 45) == 10:
        sf_st[6] = 12
    if (fr % 5) == 0:
        q = fr // 5
        sf_fire_bolt(px - 14, py - 10, 0, -11, sf_strand_col(q))
        sf_fire_bolt(px + 14, py - 10, 0, -11, sf_strand_col(q + 2))
        if (fr % 15) == 0:
            sf_fire_bolt(px - 22, py - 4, -3, -10, sf_strand_col(1))
            sf_fire_bolt(px + 22, py - 4, 3, -10, sf_strand_col(3))
    for b in range(40):
        if pb_on[b]:
            bx = pb_x[b] + pb_vx[b]
            by = pb_y[b] + pb_vy[b]
            pb_x[b], pb_y[b] = bx, by
            if by < 24 or bx < 0 or bx >= 640:
                pb_on[b] = 0
            else:
                for e in range(24):
                    if en_hp[e] > 0 and abs(bx - en_x[e]) < 18 and abs(by - en_y[e]) < 14:
                        pb_on[b] = 0
                        en_hp[e] -= 1
                        if en_hp[e] <= 0:
                            sf_kill_enemy(e)
                        else:
                            sf_spawn_ring(bx, by, 16, 0xFFFFFF)
                        break
    if sf_st[6] > 0:
        for e in range(24):
            if en_hp[e] > 0 and 36 < en_y[e] < py:
                dx = abs(en_x[e] - px)
                dy = py - en_y[e]
                if dx < 32 + (dy >> 1) and (fr % 3) == 0:
                    en_hp[e] -= 1
                    if en_hp[e] <= 0:
                        sf_kill_enemy(e)
    alive = 0
    for e in range(24):
        if en_hp[e] > 0:
            alive += 1
            ph = en_ph[e] + 12
            en_ph[e] = ph
            ex = en_x[e] + ((G.isin1024(ph) * 3) >> 12)
            ey = (en_y[e] + 2) if en_y[e] < 86 else (en_y[e] + ((G.icos1024(ph) * 2) >> 12))
            ex = G.iclamp(ex, 44, 596)
            ey = G.iclamp(ey, 38, 220)
            en_x[e], en_y[e] = ex, ey
            if ((fr + e * 7) % 37) == 0:
                sf_fire_ebolt(ex, ey + 12, (G.isin1024(ph) * 3) >> 12, 4)
    if alive < 4:
        sf_spawn_wave(sf_st[4] + 1)
    for b in range(32):
        if eb_on[b]:
            bx = eb_x[b] + eb_vx[b]
            by = eb_y[b] + eb_vy[b]
            eb_x[b], eb_y[b] = bx, by
            if by >= 356 or bx < 0 or bx >= 640:
                eb_on[b] = 0
            elif abs(bx - px) < 14 and abs(by - py) < 12:
                eb_on[b] = 0
                if sf_st[5] > 20:
                    sf_st[5] -= 5
                sf_spawn_ring(px, py, 26, G.rgb(255, 96, 120))
    for i in range(12):
        if rg_r[i] > 0:
            nr = rg_r[i] + 3
            rg_r[i] = 0 if nr >= rg_max[i] else nr
    for i in range(96):
        if sp_life[i] > 0:
            sp_x[i] += sp_vx[i]
            sp_y[i] += sp_vy[i]
            sp_life[i] -= 1


def sf_draw_enemy(x, y, kd):
    if kd == 0:
        G.draw_glow_line(x, y + 12, x - 15, y - 8, G.rgb(255, 76, 92))
        G.draw_glow_line(x, y + 12, x + 15, y - 8, G.rgb(255, 76, 92))
        G.draw_line(x - 15, y - 8, x, y - 12, G.rgb(255, 170, 80))
        G.draw_line(x + 15, y - 8, x, y - 12, G.rgb(255, 170, 80))
        G.fill_rect(x - 4, y - 4, 8, 8, G.rgb(255, 190, 92))
        G.fill_rect(x - 2, y - 2, 4, 4, G.rgb(255, 255, 225))
    elif kd == 1:
        G.draw_glow_line(x - 18, y - 4, x - 6, y + 10, G.rgb(190, 92, 255))
        G.draw_glow_line(x + 18, y - 4, x + 6, y + 10, G.rgb(190, 92, 255))
        G.draw_glow_line(x - 18, y - 4, x + 18, y - 4, G.rgb(255, 96, 220))
        G.fill_rect(x - 5, y - 6, 10, 10, G.rgb(128, 52, 216))
        G.fill_rect(x - 2, y - 3, 4, 4, G.rgb(0, 238, 255))
    else:
        G.draw_glow_line(x - 20, y, x, y - 12, G.rgb(38, 232, 142))
        G.draw_glow_line(x + 20, y, x, y - 12, G.rgb(38, 232, 142))
        G.draw_glow_line(x - 20, y, x, y + 12, G.rgb(255, 206, 86))
        G.draw_glow_line(x + 20, y, x, y + 12, G.rgb(255, 206, 86))
        G.fill_rect(x - 8, y - 5, 16, 10, G.rgb(28, 112, 88))
        G.fill_rect(x - 4, y - 2, 8, 4, G.rgb(255, 228, 110))


def sf_draw_player():
    px, py, fr = sf_st[1], sf_st[2], sf_st[7]
    for s in range(4):
        c = sf_strand_col(s)
        ox = s * 6 - 9
        base_ph = fr * 28 + s * 128
        for k in range(14):
            wave = (G.isin1024(base_ph + k * 52) * 5) >> 12
            sx = px + ox + wave
            sy = py + 14 + k * 3
            sc = 14 - k
            rr = G.uclamp255((((c >> 16) & 255) * sc) >> 3)
            gg = G.uclamp255((((c >> 8) & 255) * sc) >> 3)
            bb = G.uclamp255(((c & 255) * sc) >> 3)
            G.px_add(sx, sy, rr, gg, bb)
            G.px_add(sx + 1, sy, rr, gg, bb)
    sw = sf_st[6]
    if sw > 0:
        for s in range(4):
            c = sf_strand_col(s)
            off = s * 2 - 3
            tx = px + off * (58 + sw * 6)
            G.draw_glow_line(px + off * 6, py - 10, tx, 28, c)
            G.draw_line(px + off * 6, py - 10, tx, 28, G.rgb(240, 255, 255))
    G.draw_glow_line(px, py - 18, px - 26, py + 12, G.rgb(0, 228, 255))
    G.draw_glow_line(px, py - 18, px + 26, py + 12, G.rgb(0, 228, 255))
    G.draw_line(px - 26, py + 12, px - 8, py + 6, G.rgb(140, 220, 255))
    G.draw_line(px + 26, py + 12, px + 8, py + 6, G.rgb(140, 220, 255))
    G.fill_rect(px - 5, py - 12, 10, 22, G.rgb(190, 218, 245))
    G.fill_rect(px - 14, py + 2, 28, 6, G.rgb(64, 114, 176))
    G.fill_rect(px - 3, py - 8, 6, 10, G.rgb(0, 232, 255))
    G.fill_rect(px - 1, py - 6, 2, 5, 0xFFFFFF)
    G.fill_rect(px - 24, py + 7, 4, 5, G.rgb(255, 209, 102))
    G.fill_rect(px + 21, py + 7, 4, 5, G.rgb(184, 107, 255))


def sf_draw_hud():
    G.blend_rect(0, 0, 640, 26, G.rgb(6, 10, 24), 220)
    G.fill_rect(0, 26, 160, 2, G.rgb(0, 228, 255))
    G.fill_rect(160, 26, 160, 2, G.rgb(255, 209, 102))
    G.fill_rect(320, 26, 160, 2, G.rgb(34, 229, 138))
    G.fill_rect(480, 26, 160, 2, G.rgb(184, 107, 255))
    G.draw_label("OMEGA STARFIGHTER", 12, 9, 1, G.rgb(0, 230, 255))
    G.draw_label("SCORE", 168, 9, 1, G.rgb(164, 194, 234))
    G.draw_uint(sf_st[3], 210, 9, 1, 0xFFFFFF)
    G.draw_label("WAVE", 300, 9, 1, G.rgb(255, 209, 102))
    G.draw_uint(sf_st[4], 334, 9, 1, G.rgb(255, 244, 200))
    G.draw_label("SHIELD", 424, 9, 1, G.rgb(34, 229, 138))
    G.fill_rect(472, 8, 110, 10, G.rgb(18, 28, 48))
    G.fill_rect(474, 10, G.iclamp(sf_st[5], 0, 106), 6, G.rgb(34, 229, 138))
    G.rect_outline(472, 8, 110, 10, G.rgb(100, 220, 180))


def sf_fade_col(c, fade, urm):
    return G.rgb(
        G.uclamp255((((c >> 16) & 255) * fade) // urm),
        G.uclamp255((((c >> 8) & 255) * fade) // urm),
        G.uclamp255(((c & 255) * fade) // urm),
    )


def sf_render():
    fb = G.fb
    for y in range(360):
        col = G.rgb(3 + ((y * 8) >> 8), 6 + ((y * 12) >> 8), 16 + ((y * 26) >> 8))
        row = y * 640
        fb[row : row + 640] = [col] * 640
    for i in range(64):
        G.px_set(st_x[i], st_y[i], st_c[i])
        if st_sp[i] > 2:
            G.px_set(st_x[i], st_y[i] - 1, st_c[i])
            G.px_set(st_x[i], st_y[i] - 2, G.rgb(80, 140, 200))
    for i in range(12):
        if rg_r[i] > 0:
            fade = rg_max[i] - rg_r[i]
            urm = rg_max[i]
            G.draw_ring(rg_x[i], rg_y[i], rg_r[i], sf_fade_col(rg_c[i], fade, urm))
            G.draw_ring(rg_x[i], rg_y[i], rg_r[i] + 1, sf_fade_col(rg_c[i], fade >> 1, urm))
    for b in range(40):
        if pb_on[b]:
            G.draw_glow_line(pb_x[b], pb_y[b], pb_x[b], pb_y[b] + 10, pb_c[b])
            G.draw_line(pb_x[b], pb_y[b], pb_x[b], pb_y[b] + 8, 0xFFFFFF)
    for b in range(32):
        if eb_on[b]:
            bx, by = eb_x[b], eb_y[b]
            G.px_add(bx, by, 255, 120, 90)
            G.px_add(bx - 1, by, 220, 60, 80)
            G.px_add(bx + 1, by, 220, 60, 80)
            G.px_add(bx, by - 1, 220, 60, 80)
            G.px_add(bx, by + 1, 220, 60, 80)
            G.px_set(bx, by, G.rgb(255, 240, 210))
    for e in range(24):
        if en_hp[e] > 0:
            sf_draw_enemy(en_x[e], en_y[e], en_kind[e])
    for i in range(96):
        lf = sp_life[i]
        if lf > 0:
            sx = sp_x[i] >> 4
            sy = sp_y[i] >> 4
            c = sp_c[i]
            rr = G.uclamp255((((c >> 16) & 255) * lf) >> 4)
            gg = G.uclamp255((((c >> 8) & 255) * lf) >> 4)
            bb = G.uclamp255(((c & 255) * lf) >> 4)
            G.px_add(sx, sy, rr, gg, bb)
            G.px_add(sx + 1, sy, rr, gg, bb)
            G.px_add(sx, sy + 1, rr, gg, bb)
    sf_draw_player()
    sf_draw_hud()


if __name__ == "__main__":
    G.run_driver("starfighter", SF_HERO, sf_init, sf_step, sf_render)
