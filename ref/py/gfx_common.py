# SPDX-License-Identifier: Apache-2.0
# ref/py/gfx_common.py: Bit-exact Python 3 reference graphics & SHA-256 matching om/gfx_common.om.
import hashlib
import struct
import sys
import time

FB_W = 640
FB_H = 360
FB_N = 230400

fb = [0] * FB_N
sin_lut = [0] * 256
frame_hashes = [0] * 2048

SIN_Q1 = (
    0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189, 1285,
    1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191, 2276, 2359,
    2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229,
    3290, 3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822,
    3857, 3889, 3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085,
    4091, 4095, 4096,
)

GFX_FONT5X7 = (
    ">ACEIA>4<4444>>A168@ON11>11N26:BO22O@N11A>68@NAA>O124888>AA>AA>>AA?12<"
    ">AAOAAANAANAAN>A@@@A>LBAAABLO@@N@@OO@@N@@@>A@GAA>AAAOAAA>44444>7222BB<"
    "ABDHDBA@@@@@@OAKEEAAAAIECAAA>AAAAA>NAAN@@@>AAAEB=NAANDBA?@@>11NO444444"
    "AAAAAA>AAAAA:4AAAEEKAAA:4:AAAA:4444O1248@O0440440000O0000000000"
)
GFX_FONT_BYTES = [ord(c) - 48 for c in GFX_FONT5X7]


def gfx_init():
    for i in range(64):
        sin_lut[i] = SIN_Q1[i]
        sin_lut[i + 64] = SIN_Q1[64 - i]
        sin_lut[i + 128] = -SIN_Q1[i]
        sin_lut[i + 192] = -SIN_Q1[64 - i]


def tdiv(a, b):
    # Truncate-toward-zero division for b > 0 matching Omega/C/JS.
    return -((-a) // b) if a < 0 else a // b


def iclamp(v, lo, hi):
    return lo if v < lo else (hi if v > hi else v)


def uclamp255(v):
    return 255 if v > 255 else v


def rgb(r, g, b):
    if r > 255:
        r = 255
    if g > 255:
        g = 255
    if b > 255:
        b = 255
    return (r << 16) | (g << 8) | b


def isin1024(a):
    m = a & 1023
    idx = m >> 2
    idx2 = (idx + 1) & 255
    frac = m & 3
    return (sin_lut[idx] * (4 - frac) + sin_lut[idx2] * frac) >> 2


def icos1024(a):
    return isin1024(a + 256)


def px_set(x, y, c):
    if 0 <= x < 640 and 0 <= y < 360:
        fb[y * 640 + x] = c


def px_add(x, y, dr, dg, db):
    if 0 <= x < 640 and 0 <= y < 360:
        idx = y * 640 + x
        cur = fb[idx]
        r = ((cur >> 16) & 255) + dr
        g = ((cur >> 8) & 255) + dg
        b = (cur & 255) + db
        if r > 255:
            r = 255
        if g > 255:
            g = 255
        if b > 255:
            b = 255
        fb[idx] = (r << 16) | (g << 8) | b


def px_blend(x, y, c, al):
    if 0 <= x < 640 and 0 <= y < 360:
        idx = y * 640 + x
        bg = fb[idx]
        ia = 256 - al
        r = (((bg >> 16) & 255) * ia + ((c >> 16) & 255) * al) >> 8
        g = (((bg >> 8) & 255) * ia + ((c >> 8) & 255) * al) >> 8
        b = ((bg & 255) * ia + (c & 255) * al) >> 8
        fb[idx] = (r << 16) | (g << 8) | b


def fill_rect(x, y, w, h, c):
    x1 = x + w
    y1 = y + h
    if x1 > 640:
        x1 = 640
    if y1 > 360:
        y1 = 360
    if x < 0:
        x = 0
    if y < 0:
        y = 0
    if x >= x1 or y >= y1:
        return
    span = [c] * (x1 - x)
    for yy in range(y, y1):
        row = yy * 640
        fb[row + x : row + x1] = span


def blend_rect(x, y, w, h, c, al):
    x1 = x + w
    y1 = y + h
    if x1 > 640:
        x1 = 640
    if y1 > 360:
        y1 = 360
    if x < 0:
        x = 0
    if y < 0:
        y = 0
    if x >= x1 or y >= y1:
        return
    ia = 256 - al
    cr = ((c >> 16) & 255) * al
    cg = ((c >> 8) & 255) * al
    cb = (c & 255) * al
    for yy in range(y, y1):
        row = yy * 640
        for xx in range(x, x1):
            idx = row + xx
            bg = fb[idx]
            r = (((bg >> 16) & 255) * ia + cr) >> 8
            g = (((bg >> 8) & 255) * ia + cg) >> 8
            b = ((bg & 255) * ia + cb) >> 8
            fb[idx] = (r << 16) | (g << 8) | b


def rect_outline(x, y, w, h, c):
    if w <= 0 or h <= 0:
        return
    fill_rect(x, y, w, 1, c)
    fill_rect(x, y + h - 1, w, 1, c)
    fill_rect(x, y, 1, h, c)
    fill_rect(x + w - 1, y, 1, h, c)


def draw_line(x0, y0, x1, y1, c):
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    er = dx + dy
    while True:
        if 0 <= x0 < 640 and 0 <= y0 < 360:
            fb[y0 * 640 + x0] = c
        if x0 == x1 and y0 == y1:
            break
        e2 = er * 2
        if e2 >= dy:
            er += dy
            x0 += sx
        if e2 <= dx:
            er += dx
            y0 += sy


def px_glow_pt(cx, cy, r, g, b, hr, hg, hb):
    px_add(cx, cy, r, g, b)
    px_add(cx - 1, cy, hr, hg, hb)
    px_add(cx + 1, cy, hr, hg, hb)
    px_add(cx, cy - 1, hr, hg, hb)
    px_add(cx, cy + 1, hr, hg, hb)


def draw_glow_line(x0, y0, x1, y1, c):
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    er = dx + dy
    r = (c >> 16) & 255
    g = (c >> 8) & 255
    b = c & 255
    hr = r >> 1
    hg = g >> 1
    hb = b >> 1
    while True:
        px_glow_pt(x0, y0, r, g, b, hr, hg, hb)
        if x0 == x1 and y0 == y1:
            break
        e2 = er * 2
        if e2 >= dy:
            er += dy
            x0 += sx
        if e2 <= dx:
            er += dx
            y0 += sy


def draw_ring(cx, cy, rad, c):
    if rad <= 0:
        return
    r = (c >> 16) & 255
    g = (c >> 8) & 255
    b = c & 255
    x = rad
    y = 0
    err = 1 - rad
    while x >= y:
        px_add(cx + x, cy + y, r, g, b)
        px_add(cx - x, cy + y, r, g, b)
        px_add(cx + x, cy - y, r, g, b)
        px_add(cx - x, cy - y, r, g, b)
        px_add(cx + y, cy + x, r, g, b)
        px_add(cx - y, cy + x, r, g, b)
        px_add(cx + y, cy - x, r, g, b)
        px_add(cx - y, cy - x, r, g, b)
        y += 1
        if err < 0:
            err += 2 * y + 1
        else:
            x -= 1
            err += 2 * (y - x) + 1


def font_idx(ch):
    if 48 <= ch <= 57:
        return ch - 48
    if 65 <= ch <= 90:
        return ch - 65 + 10
    if ch == 58:
        return 36
    if ch == 45:
        return 37
    return 38


def draw_glyph(ch, x, y, sc, c):
    gi = font_idx(ch) * 7
    for r in range(7):
        bits = GFX_FONT_BYTES[gi + r]
        for col in range(5):
            if (bits >> (4 - col)) & 1:
                px = x + col * sc
                py = y + r * sc
                if sc == 1:
                    px_set(px, py, c)
                else:
                    fill_rect(px, py, sc, sc, c)


def draw_label(s, x, y, sc, c):
    cx = x
    step = 6 * sc
    for ch in s:
        draw_glyph(ord(ch), cx, y, sc, c)
        cx += step


def draw_uint(val, x, y, sc, c):
    draw_label(str(val), x, y, sc, c)


def fb_to_ppm_bytes():
    out = bytearray(15 + FB_N * 3)
    out[0:15] = b"P6\n640 360\n255\n"
    o = 15
    for c in fb:
        out[o] = (c >> 16) & 255
        out[o + 1] = (c >> 8) & 255
        out[o + 2] = c & 255
        o += 3
    return bytes(out)


def sha_fb_ppm():
    return hashlib.sha256(fb_to_ppm_bytes()).hexdigest()


def sha_stream_u32(n):
    data = struct.pack(f">{n}I", *frame_hashes[:n])
    return hashlib.sha256(data).hexdigest()


def hash_fb_u32():
    h = 2166136261
    for c in fb:
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


def sample_fb_u32(fr):
    h = 2166136261
    f17 = fr * 17
    for y in range(360):
        x = (y * 73 + f17) % 640
        h = ((h ^ fb[y * 640 + x]) * 16777619) & 0xFFFFFFFF
    return h


def parse_cli(argv, def_hero):
    args = argv[1:]
    if len(args) == 0:
        return True, 0, 300
    if len(args) > 2:
        return False, 0, 0
    if args[0] == "bench":
        mode, param = 0, 300
    elif args[0] == "verify":
        mode, param = 1, 300
    elif args[0] == "ppm":
        mode, param = 2, def_hero
    else:
        return False, 0, 0
    if len(args) == 2:
        s = args[1]
        if not s or len(s) > 10 or not s.isdigit():
            return False, 0, 0
        v = int(s)
        if mode == 2:
            if v >= 2000:
                return False, 0, 0
        else:
            if v == 0 or v > 2000:
                return False, 0, 0
        param = v
    return True, mode, param


def read_vmhwm_kb():
    try:
        with open("/proc/self/status", "r") as fp:
            for line in fp:
                if line.startswith("VmHWM:"):
                    return int(line.split()[1])
    except OSError:
        return 0
    return 0


def run_driver(gname, def_hero, init_fn, step_fn, render_fn):
    gfx_init()
    ok, mode, param = parse_cli(sys.argv, def_hero)
    if not ok:
        sys.stderr.write("ERR bad args\n")
        sys.exit(2)
    init_fn()
    if mode == 0:
        t0 = time.perf_counter_ns()
        dce = 0
        for f in range(param):
            step_fn(f)
            render_fn()
            dce ^= sample_fb_u32(f)
        ns = time.perf_counter_ns() - t0
        fsha = sha_fb_ppm()
        us_f = ns // (param * 1000) if ns > 0 else 0
        fps100 = (param * 100000000000) // ns if ns > 0 else 0
        vmhwm = read_vmhwm_kb()
        sys.stdout.write(
            f"GAME={gname} FRAMES={param} NS={ns} US_PER_FRAME={us_f} FPS_X100={fps100} ANTI_DCE={dce:08x} VMHWM_KB={vmhwm} FINAL_PPM_SHA256={fsha}\n"
        )
        return
    if mode == 1:
        hf = param - 1 if def_hero >= param else def_hero
        hsha = ""
        for f in range(param):
            step_fn(f)
            render_fn()
            frame_hashes[f] = hash_fb_u32()
            if f == hf:
                hsha = sha_fb_ppm()
        fsha = sha_fb_ppm()
        ssha = sha_stream_u32(param)
        sys.stdout.write(
            f"GAME={gname} FRAMES={param} HERO_FRAME={hf} HERO_PPM_SHA256={hsha} FINAL_PPM_SHA256={fsha} STREAM_SHA256={ssha}\n"
        )
        return
    for f in range(param + 1):
        step_fn(f)
        render_fn()
    sys.stdout.buffer.write(fb_to_ppm_bytes())
