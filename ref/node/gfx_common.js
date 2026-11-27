// SPDX-License-Identifier: Apache-2.0
// ref/node/gfx_common.js: Bit-exact Node.js reference graphics & SHA-256 matching om/gfx_common.om.
'use strict';
const crypto = require('crypto');
const fs = require('fs');

function read_vmhwm_kb() {
  try {
    const txt = fs.readFileSync('/proc/self/status', 'utf8');
    const m = txt.match(/VmHWM:\s+(\d+)/);
    return m ? Number(m[1]) : 0;
  } catch (_) { return 0; }
}

const FB_W = 640;
const FB_H = 360;
const FB_N = 230400;

const fb = new Uint32Array(FB_N);
const sin_lut = new Int32Array(256);
const frame_hashes = new Uint32Array(2048);

const SIN_Q1 = [
  0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189, 1285,
  1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191, 2276, 2359,
  2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229,
  3290, 3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822,
  3857, 3889, 3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085,
  4091, 4095, 4096
];

const GFX_FONT5X7 =
  '>ACEIA>4<4444>>A168@ON11>11N26:BO22O@N11A>68@NAA>O124888>AA>AA>>AA?12<' +
  '>AAOAAANAANAAN>A@@@A>LBAAABLO@@N@@OO@@N@@@>A@GAA>AAAOAAA>44444>7222BB<' +
  'ABDHDBA@@@@@@OAKEEAAAAIECAAA>AAAAA>NAAN@@@>AAAEB=NAANDBA?@@>11NO444444' +
  'AAAAAA>AAAAA:4AAAEEKAAA:4:AAAA:4444O1248@O0440440000O0000000000';

function gfx_init() {
  for (let i = 0; i < 64; i++) {
    sin_lut[i] = SIN_Q1[i];
    sin_lut[i + 64] = SIN_Q1[64 - i];
    sin_lut[i + 128] = -SIN_Q1[i];
    sin_lut[i + 192] = -SIN_Q1[64 - i];
  }
}

function iabs(x) { return x < 0 ? -x : x; }
function imin(a, b) { return a < b ? a : b; }
function imax(a, b) { return a > b ? a : b; }
function iclamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }
function uclamp255(v) { return v > 255 ? 255 : v; }
function rgb(r, g, b) { return ((uclamp255(r) << 16) | (uclamp255(g) << 8) | uclamp255(b)) >>> 0; }
function isin1024(a) {
  const m = a & 1023;
  const idx = m >> 2;
  const idx2 = (idx + 1) & 255;
  const frac = m & 3;
  return (sin_lut[idx] * (4 - frac) + sin_lut[idx2] * frac) >> 2;
}
function icos1024(a) { return isin1024(a + 256); }

function px_set(x, y, c) {
  if (x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
  fb[y * 640 + x] = c;
}

function px_add(x, y, dr, dg, db) {
  if (x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
  const idx = y * 640 + x;
  const cur = fb[idx];
  const r = uclamp255(((cur >>> 16) & 255) + dr);
  const g = uclamp255(((cur >>> 8) & 255) + dg);
  const b = uclamp255((cur & 255) + db);
  fb[idx] = ((r << 16) | (g << 8) | b) >>> 0;
}

function px_blend(x, y, c, al) {
  if (x < 0 || x >= FB_W || y < 0 || y >= FB_H) return;
  const idx = y * 640 + x;
  const bg = fb[idx];
  const ia = 256 - al;
  const r = ((((bg >>> 16) & 255) * ia + ((c >>> 16) & 255) * al) >>> 8);
  const g = ((((bg >>> 8) & 255) * ia + ((c >>> 8) & 255) * al) >>> 8);
  const b = (((bg & 255) * ia + (c & 255) * al) >>> 8);
  fb[idx] = ((r << 16) | (g << 8) | b) >>> 0;
}

function fill_rect(x, y, w, h, c) {
  w = imin(x + w, FB_W);
  h = imin(y + h, FB_H);
  x = imax(x, 0);
  y = imax(y, 0);
  if (x >= w || y >= h) return;
  for (let yy = y; yy < h; yy++) {
    const row = yy * 640;
    fb.fill(c, row + x, row + w);
  }
}

function blend_rect(x, y, w, h, c, al) {
  w = imin(x + w, FB_W);
  h = imin(y + h, FB_H);
  x = imax(x, 0);
  y = imax(y, 0);
  if (x >= w || y >= h) return;
  for (let yy = y; yy < h; yy++) {
    for (let xx = x; xx < w; xx++) px_blend(xx, yy, c, al);
  }
}

function rect_outline(x, y, w, h, c) {
  if (w <= 0 || h <= 0) return;
  fill_rect(x, y, w, 1, c);
  fill_rect(x, y + h - 1, w, 1, c);
  fill_rect(x, y, 1, h, c);
  fill_rect(x + w - 1, y, 1, h, c);
}

function draw_line(x0, y0, x1, y1, c) {
  const dx = iabs(x1 - x0), dy = -iabs(y1 - y0);
  const sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  let er = dx + dy;
  for (;;) {
    px_set(x0, y0, c);
    if (x0 === x1 && y0 === y1) break;
    const e2 = er * 2;
    if (e2 >= dy) { er += dy; x0 += sx; }
    if (e2 <= dx) { er += dx; y0 += sy; }
  }
}

function px_glow_pt(cx, cy, c) {
  const r = (c >>> 16) & 255, g = (c >>> 8) & 255, b = c & 255;
  const hr = r >>> 1, hg = g >>> 1, hb = b >>> 1;
  px_add(cx, cy, r, g, b);
  px_add(cx - 1, cy, hr, hg, hb);
  px_add(cx + 1, cy, hr, hg, hb);
  px_add(cx, cy - 1, hr, hg, hb);
  px_add(cx, cy + 1, hr, hg, hb);
}

function draw_glow_line(x0, y0, x1, y1, c) {
  const dx = iabs(x1 - x0), dy = -iabs(y1 - y0);
  const sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  let er = dx + dy;
  for (;;) {
    px_glow_pt(x0, y0, c);
    if (x0 === x1 && y0 === y1) break;
    const e2 = er * 2;
    if (e2 >= dy) { er += dy; x0 += sx; }
    if (e2 <= dx) { er += dx; y0 += sy; }
  }
}

function ring_oct(cx, cy, dx, dy, c) {
  const r = (c >>> 16) & 255, g = (c >>> 8) & 255, b = c & 255;
  px_add(cx + dx, cy + dy, r, g, b);
  px_add(cx - dx, cy + dy, r, g, b);
  px_add(cx + dx, cy - dy, r, g, b);
  px_add(cx - dx, cy - dy, r, g, b);
  px_add(cx + dy, cy + dx, r, g, b);
  px_add(cx - dy, cy + dx, r, g, b);
  px_add(cx + dy, cy - dx, r, g, b);
  px_add(cx - dy, cy - dx, r, g, b);
}

function draw_ring(cx, cy, rad, c) {
  if (rad <= 0) return;
  let x = rad, y = 0, err = 1 - rad;
  while (x >= y) {
    ring_oct(cx, cy, x, y, c);
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}

function font_idx(ch) {
  if (ch >= 48 && ch <= 57) return ch - 48;
  if (ch >= 65 && ch <= 90) return ch - 65 + 10;
  if (ch === 58) return 36;
  if (ch === 45) return 37;
  return 38;
}

function draw_glyph(ch, x, y, sc, c) {
  const gi = font_idx(ch) * 7;
  for (let r = 0; r < 7; r++) {
    const bits = GFX_FONT5X7.charCodeAt(gi + r) - 48;
    for (let col = 0; col < 5; col++) {
      if ((bits >>> (4 - col)) & 1) {
        const px = x + col * sc, py = y + r * sc;
        if (sc === 1) px_set(px, py, c);
        else fill_rect(px, py, sc, sc, c);
      }
    }
  }
}

function draw_label(s, x, y, sc, c) {
  let cx = x;
  const step = 6 * sc;
  for (let i = 0; i < s.length; i++) {
    draw_glyph(s.charCodeAt(i), cx, y, sc, c);
    cx += step;
  }
}

function draw_uint(val, x, y, sc, c) {
  const s = String(val >>> 0);
  let cx = x;
  const step = 6 * sc;
  for (let i = 0; i < s.length; i++) {
    draw_glyph(s.charCodeAt(i), cx, y, sc, c);
    cx += step;
  }
}

function fb_to_ppm_buf() {
  const buf = Buffer.allocUnsafe(15 + FB_N * 3);
  buf.write('P6\n640 360\n255\n', 0, 'ascii');
  let o = 15;
  for (let i = 0; i < FB_N; i++) {
    const c = fb[i];
    buf[o++] = (c >>> 16) & 255;
    buf[o++] = (c >>> 8) & 255;
    buf[o++] = c & 255;
  }
  return buf;
}

function sha_fb_ppm() {
  return crypto.createHash('sha256').update(fb_to_ppm_buf()).digest('hex');
}

function sha_stream_u32(n) {
  const buf = Buffer.allocUnsafe(n * 4);
  for (let i = 0; i < n; i++) buf.writeUInt32BE(frame_hashes[i] >>> 0, i * 4);
  return crypto.createHash('sha256').update(buf).digest('hex');
}

function hash_fb_u32() {
  let h = 2166136261 >>> 0;
  for (let i = 0; i < FB_N; i++) {
    h = Math.imul((h ^ fb[i]) >>> 0, 16777619) >>> 0;
  }
  return h;
}

function sample_fb_u32(fr) {
  let h = 2166136261 >>> 0;
  for (let y = 0; y < 360; y++) {
    const x = (y * 73 + fr * 17) % 640;
    h = Math.imul((h ^ fb[y * 640 + x]) >>> 0, 16777619) >>> 0;
  }
  return h;
}

function parse_cli(argv, def_hero) {
  let mode = 0, param = 300;
  const args = argv.slice(2);
  if (args.length === 0) return { ok: true, mode, param };
  if (args.length > 2) return { ok: false };
  if (args[0] === 'bench') { mode = 0; param = 300; }
  else if (args[0] === 'verify') { mode = 1; param = 300; }
  else if (args[0] === 'ppm') { mode = 2; param = def_hero; }
  else return { ok: false };
  if (args.length === 2) {
    if (!/^[0-9]{1,10}$/.test(args[1])) return { ok: false };
    const v = Number(args[1]);
    if (mode === 2) { if (v >= 2000) return { ok: false }; }
    else { if (v === 0 || v > 2000) return { ok: false }; }
    param = v;
  }
  return { ok: true, mode, param };
}

function run_driver(gname, def_hero, init_fn, step_fn, render_fn) {
  gfx_init();
  const cli = parse_cli(process.argv, def_hero);
  if (!cli.ok) { process.stderr.write('ERR bad args\n'); process.exit(2); }
  init_fn();
  const { mode, param } = cli;
  if (mode === 0) {
    const t0 = process.hrtime.bigint();
    let dce = 0;
    for (let f = 0; f < param; f++) {
      step_fn(f);
      render_fn();
      dce = (dce ^ sample_fb_u32(f)) >>> 0;
    }
    const ns = process.hrtime.bigint() - t0;
    const fsha = sha_fb_ppm();
    const pBig = BigInt(param);
    const us_f = ns > 0n ? ns / (pBig * 1000n) : 0n;
    const fps100 = ns > 0n ? (pBig * 100000000000n) / ns : 0n;
    const vmhwm = read_vmhwm_kb();
    process.stdout.write(
      `GAME=${gname} FRAMES=${param} NS=${ns} US_PER_FRAME=${us_f} FPS_X100=${fps100} ANTI_DCE=${dce.toString(16).padStart(8, '0')} VMHWM_KB=${vmhwm} FINAL_PPM_SHA256=${fsha}\n`
    );
    return;
  }
  if (mode === 1) {
    const hf = def_hero >= param ? param - 1 : def_hero;
    let hsha = '';
    for (let f = 0; f < param; f++) {
      step_fn(f);
      render_fn();
      frame_hashes[f] = hash_fb_u32();
      if (f === hf) hsha = sha_fb_ppm();
    }
    const fsha = sha_fb_ppm();
    const ssha = sha_stream_u32(param);
    process.stdout.write(
      `GAME=${gname} FRAMES=${param} HERO_FRAME=${hf} HERO_PPM_SHA256=${hsha} FINAL_PPM_SHA256=${fsha} STREAM_SHA256=${ssha}\n`
    );
    return;
  }
  for (let f = 0; f <= param; f++) { step_fn(f); render_fn(); }
  process.stdout.write(fb_to_ppm_buf());
}

module.exports = {
  FB_W, FB_H, FB_N, fb, sin_lut, frame_hashes,
  gfx_init, iabs, imin, imax, iclamp, uclamp255, rgb,
  isin1024, icos1024, px_set, px_add, px_blend, fill_rect, blend_rect,
  rect_outline, draw_line, draw_glow_line, draw_ring,
  draw_label, draw_uint, run_driver
};
