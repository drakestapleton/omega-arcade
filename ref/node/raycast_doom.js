// SPDX-License-Identifier: Apache-2.0
// ref/node/raycast_doom.js: Bit-exact Node.js reference port of om/raycast_doom.om.
'use strict';
const G = require('./gfx_common');
const { fb, rgb, iabs, imin, imax, isin1024, icos1024, px_set, fill_rect, blend_rect, rect_outline, draw_line, draw_glow_line, draw_ring, draw_label, draw_uint } = G;

const RC_HERO = 185;
const RC_MAP_STR =
  '1111111111111111100000200000000110300020040400011000000000000301' +
  '1000200000000001120020030004000110000000000000011040000000020021' +
  '1000003000000001100000000000030110200000040000011020000000002001' +
  '1000030000002001100000000000000110040002003000011111111111111111';

const rmap = new Uint8Array(256);
const bg_row = new Uint32Array(360);
const rc_st = new Int32Array(8);

function rc_init() {
  for (let i = 0; i < 256; i++) rmap[i] = RC_MAP_STR.charCodeAt(i) - 48;
  rc_st[0] = 10240; rc_st[1] = 10240; rc_st[2] = 110;
  rc_st[3] = 0; rc_st[4] = 99; rc_st[5] = 0;
  for (let y = 0; y < 180; y++) {
    const d = 179 - y;
    bg_row[y] = rgb(8 + ((d * 14) >>> 8), 14 + ((d * 24) >>> 8), 30 + ((d * 48) >>> 8));
  }
  for (let y = 0; y < 180; y++) {
    const band = ((y >>> 3) & 1) === 0 ? 6 : 0;
    bg_row[y + 180] = rgb(14 + ((y * 40) >>> 8) + band, 18 + ((y * 48) >>> 8) + band, 28 + ((y * 64) >>> 8) + band);
  }
}

function rc_blocked(x, y) {
  if (x < 4096 || y < 4096) return true;
  const mx = x >> 12, my = y >> 12;
  if (mx >= 16 || my >= 16) return true;
  return rmap[my * 16 + mx] !== 0;
}

function rc_try_move(dx, dy) {
  let px = rc_st[0], py = rc_st[1];
  const nx = px + dx, ny = py + dy, m = 900;
  if (!rc_blocked(nx + m, py) && !rc_blocked(nx - m, py)) { rc_st[0] = nx; px = nx; }
  if (!rc_blocked(px, ny + m) && !rc_blocked(px, ny - m)) rc_st[1] = ny;
}

function rc_step(fr) {
  let turn = 2, spd = 165;
  if (fr < 60) { turn = 1; spd = 175; }
  else if (fr < 130) { turn = 4; spd = 155; }
  else if (fr < 210) { turn = -3; spd = 165; }
  else { turn = 3; spd = 170; }
  const ang = (rc_st[2] + turn) & 1023;
  rc_st[2] = ang;
  rc_try_move((icos1024(ang) * spd) >> 12, (isin1024(ang) * spd) >> 12);
  rc_st[5] += 28;
  if (rc_st[3] > 0) rc_st[3]--;
  if ((fr % 25) === 10) { rc_st[3] = 6; if (rc_st[4] > 0) rc_st[4]--; }
}

function rc_wall_color(wtype, side, tx, ty, fog) {
  let r = 96, g = 118, b = 146;
  if (wtype === 1) {
    r = 92; g = 116; b = 152;
    const stagger = ((ty >> 3) & 1) * 8;
    if ((ty & 7) === 0) { r = 36; g = 48; b = 68; }
    else if (((tx + stagger) & 15) === 0) { r = 40; g = 52; b = 72; }
    else if ((ty & 7) === 1) { r = 122; g = 148; b = 188; }
  } else if (wtype === 2) {
    r = 28; g = 72; b = 112;
    if (ty >= 13 && ty <= 18) { r = 18; g = 232; b = 255; }
    else if ((tx & 7) === 0) { r = 20; g = 118; b = 160; }
  } else if (wtype === 3) {
    r = 196; g = 136; b = 44;
    if (ty < 3 || ty > 28) { r = 94; g = 64; b = 26; }
    else if (tx >= 12 && tx <= 19) { r = 255; g = 206; b = 88; }
  } else {
    r = 104; g = 62; b = 164;
    if (ty >= 14 && ty <= 17) { r = 38; g = 236; b = 148; }
    else if ((ty & 7) === 0) { r = 56; g = 34; b = 92; }
  }
  if (side === 1) { r = (r * 180) >>> 8; g = (g * 180) >>> 8; b = (b * 180) >>> 8; }
  const inv = 256 - fog;
  r = (r * inv + 10 * fog) >>> 8;
  g = (g * inv + 16 * fog) >>> 8;
  b = (b * inv + 30 * fog) >>> 8;
  return ((r << 16) | (g << 8) | b) >>> 0;
}

function rc_cast_col(col, dirx, diry, plx, ply) {
  const camx = Math.trunc(((col * 2 - 640) * 4096) / 640);
  const rdx = dirx + Math.floor((plx * camx) / 4096);
  const rdy = diry + Math.floor((ply * camx) / 4096);
  const px = rc_st[0], py = rc_st[1];
  let mx = px >> 12, my = py >> 12;
  const ddx = rdx !== 0 ? Math.trunc(16777216 / (rdx < 0 ? -rdx : rdx)) : 1000000000;
  const ddy = rdy !== 0 ? Math.trunc(16777216 / (rdy < 0 ? -rdy : rdy)) : 1000000000;
  const stx = rdx < 0 ? -1 : 1, sty = rdy < 0 ? -1 : 1;
  let sdx = rdx < 0 ? Math.floor(((px - mx * 4096) * ddx) / 4096) : Math.floor((((mx + 1) * 4096 - px) * ddx) / 4096);
  let sdy = rdy < 0 ? Math.floor(((py - my * 4096) * ddy) / 4096) : Math.floor((((my + 1) * 4096 - py) * ddy) / 4096);
  let side = 0, wtype = 1;
  for (let step = 0; step < 32; step++) {
    if (sdx < sdy) { sdx += ddx; mx += stx; side = 0; }
    else { sdy += ddy; my += sty; side = 1; }
    const cell = (mx < 0 || mx >= 16 || my < 0 || my >= 16) ? 1 : rmap[my * 16 + mx];
    if (cell !== 0) { wtype = cell; break; }
  }
  let pdist = side === 0 ? (sdx - ddx) : (sdy - ddy);
  if (pdist < 64) pdist = 64;
  const wpos = side === 0 ? (py + Math.floor((pdist * rdy) / 4096)) : (px + Math.floor((pdist * rdx) / 4096));
  const tx = (wpos & 4095) >> 7;
  let lineh = Math.trunc(1474560 / pdist);
  if (lineh < 2) lineh = 2;
  const dstart = 180 - (lineh >> 1), dend = 180 + (lineh >> 1);
  const cs = imax(dstart, 0), ce = imin(dend, 359);
  let fog = Math.floor((pdist - 3072) / 128);
  if (fog < 0) fog = 0; if (fog > 220) fog = 220;
  for (let y = 0; y < cs; y++) fb[y * 640 + col] = bg_row[y];
  const tstep = Math.trunc(2097152 / lineh);
  let tpos = (cs - dstart) * tstep;
  for (let y = cs; y <= ce; y++) {
    const ty = Math.floor(tpos / 65536) & 31;
    tpos += tstep;
    fb[y * 640 + col] = rc_wall_color(wtype, side, tx, ty, fog);
  }
  for (let y = ce + 1; y < 360; y++) fb[y * 640 + col] = bg_row[y];
}

function rc_draw_weapon() {
  const bp = rc_st[5];
  const bx = (icos1024(bp) * 8) >> 12, by = (iabs(isin1024(bp)) * 6) >> 12;
  const ft = rc_st[3], cx = 320 + bx, base_y = 246 + by + ft * 4;
  if (ft > 2) {
    const my = base_y - 14;
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

function rc_draw_minimap() {
  const mx0 = 522, my0 = 10;
  blend_rect(mx0 - 4, my0 - 4, 106, 106, rgb(6, 10, 22), 205);
  rect_outline(mx0 - 4, my0 - 4, 106, 106, rgb(0, 212, 255));
  for (let gy = 0; gy < 16; gy++) {
    for (let gx = 0; gx < 16; gx++) {
      const cell = rmap[gy * 16 + gx];
      let col = rgb(16, 24, 40);
      if (cell === 1) col = rgb(104, 134, 176);
      else if (cell === 2) col = rgb(0, 216, 255);
      else if (cell === 3) col = rgb(245, 186, 72);
      else if (cell === 4) col = rgb(156, 96, 236);
      fill_rect(mx0 + gx * 6, my0 + gy * 6, 5, 5, col);
    }
  }
  const ppx = mx0 + ((rc_st[0] * 6) >> 12), ppy = my0 + ((rc_st[1] * 6) >> 12), ang = rc_st[2];
  for (let r = 0; r < 5; r++) {
    const ra = ang + r * 32 - 64;
    draw_line(ppx, ppy, ppx + ((icos1024(ra) * 18) >> 12), ppy + ((isin1024(ra) * 18) >> 12), rgb(40, 240, 200));
  }
  fill_rect(ppx - 2, ppy - 2, 5, 5, rgb(255, 235, 120));
  px_set(ppx, ppy, rgb(255, 255, 255));
}

function rc_draw_hud() {
  fill_rect(313, 180, 5, 1, rgb(0, 230, 255));
  fill_rect(323, 180, 5, 1, rgb(0, 230, 255));
  fill_rect(320, 173, 1, 5, rgb(0, 230, 255));
  fill_rect(320, 183, 1, 5, rgb(0, 230, 255));
  blend_rect(12, 308, 168, 42, rgb(8, 14, 28), 210);
  rect_outline(12, 308, 168, 42, rgb(0, 198, 240));
  draw_label('OMEGA RAY', 20, 314, 1, rgb(0, 230, 255));
  draw_label('HP 100', 20, 330, 1, rgb(34, 229, 138));
  draw_label('AMMO', 92, 330, 1, rgb(255, 209, 102));
  draw_uint(rc_st[4], 128, 330, 1, rgb(255, 244, 214));
  fill_rect(13, 309, 41, 2, rgb(0, 228, 255));
  fill_rect(54, 309, 41, 2, rgb(255, 209, 102));
  fill_rect(95, 309, 42, 2, rgb(34, 229, 138));
  fill_rect(137, 309, 42, 2, rgb(184, 107, 255));
}

function rc_render() {
  const ang = rc_st[2];
  const dirx = icos1024(ang), diry = isin1024(ang);
  const plx = Math.floor(((-diry) * 2703) / 4096);
  const ply = Math.floor((dirx * 2703) / 4096);
  for (let c = 0; c < 640; c++) rc_cast_col(c, dirx, diry, plx, ply);
  rc_draw_weapon();
  rc_draw_minimap();
  rc_draw_hud();
}

G.run_driver('raycast_doom', RC_HERO, rc_init, rc_step, rc_render);
