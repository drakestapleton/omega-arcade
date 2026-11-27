// SPDX-License-Identifier: Apache-2.0
// ref/node/starfighter.js: Bit-exact Node.js reference port of om/starfighter.om.
'use strict';
const G = require('./gfx_common');
const { fb, rgb, iabs, iclamp, uclamp255, isin1024, icos1024, px_set, px_add, fill_rect, blend_rect, rect_outline, draw_line, draw_glow_line, draw_ring, draw_label, draw_uint } = G;

const SF_HERO = 195;
const sf_st = new Int32Array(8);
const st_x = new Int32Array(64), st_y = new Int32Array(64), st_sp = new Int32Array(64), st_c = new Uint32Array(64);
const en_x = new Int32Array(24), en_y = new Int32Array(24), en_hp = new Int32Array(24), en_kind = new Int32Array(24), en_ph = new Int32Array(24);
const pb_x = new Int32Array(40), pb_y = new Int32Array(40), pb_vx = new Int32Array(40), pb_vy = new Int32Array(40), pb_on = new Int32Array(40), pb_c = new Uint32Array(40);
const eb_x = new Int32Array(32), eb_y = new Int32Array(32), eb_vx = new Int32Array(32), eb_vy = new Int32Array(32), eb_on = new Int32Array(32);
const rg_x = new Int32Array(12), rg_y = new Int32Array(12), rg_r = new Int32Array(12), rg_max = new Int32Array(12), rg_c = new Uint32Array(12);
const sp_x = new Int32Array(96), sp_y = new Int32Array(96), sp_vx = new Int32Array(96), sp_vy = new Int32Array(96), sp_life = new Int32Array(96), sp_c = new Uint32Array(96);

function sf_rand() {
  const s = (Math.imul(sf_st[0] >>> 0, 1664525) + 1013904223) >>> 0;
  sf_st[0] = s & 2147483647;
  return s;
}

function sf_strand_col(k) {
  const m = k & 3;
  if (m === 0) return rgb(0, 228, 255);
  if (m === 1) return rgb(255, 209, 102);
  if (m === 2) return rgb(34, 229, 138);
  return rgb(184, 107, 255);
}

function sf_spawn_ring(x, y, rmax, c) {
  for (let i = 0; i < 12; i++) {
    if (rg_r[i] === 0) { rg_x[i] = x; rg_y[i] = y; rg_r[i] = 4; rg_max[i] = rmax; rg_c[i] = c; return; }
  }
}

function sf_spawn_sparks(x, y, c) {
  let n = 0;
  for (let i = 0; i < 96; i++) {
    if (sp_life[i] <= 0) {
      const ang = n * 85, spd = 20 + (sf_rand() & 31);
      sp_x[i] = x << 4; sp_y[i] = y << 4;
      sp_vx[i] = (icos1024(ang) * spd) >> 12;
      sp_vy[i] = (isin1024(ang) * spd) >> 12;
      sp_life[i] = 18; sp_c[i] = c;
      if (++n >= 12) return;
    }
  }
}

function sf_spawn_wave(wv) {
  sf_st[4] = wv;
  for (let i = 0; i < 12; i++) {
    for (let s = 0; s < 24; s++) {
      if (en_hp[s] <= 0) {
        const col = i % 6, row = Math.trunc(i / 6);
        en_x[s] = 110 + col * 84; en_y[s] = 54 + row * 46 - 60;
        const kind = (i + wv) % 3;
        en_kind[s] = kind; en_hp[s] = kind === 2 ? 4 : 2; en_ph[s] = i * 42;
        break;
      }
    }
  }
}

function sf_init() {
  sf_st[0] = 987654321; sf_st[1] = 320; sf_st[2] = 294;
  sf_st[3] = 0; sf_st[4] = 1; sf_st[5] = 100; sf_st[6] = 0; sf_st[7] = 0;
  for (let i = 0; i < 64; i++) {
    st_x[i] = sf_rand() % 640; st_y[i] = sf_rand() % 360;
    const layer = i % 3;
    st_sp[i] = 1 + layer * 2;
    if (layer === 0) st_c[i] = rgb(58, 76, 122);
    else if (layer === 1) st_c[i] = rgb(132, 178, 228);
    else st_c[i] = rgb(215, 244, 255);
  }
  en_hp.fill(0); pb_on.fill(0); eb_on.fill(0); rg_r.fill(0); sp_life.fill(0);
  sf_spawn_wave(1);
}

function sf_fire_bolt(x, y, vx, vy, c) {
  for (let i = 0; i < 40; i++) if (!pb_on[i]) { pb_x[i] = x; pb_y[i] = y; pb_vx[i] = vx; pb_vy[i] = vy; pb_c[i] = c; pb_on[i] = 1; return; }
}

function sf_fire_ebolt(x, y, vx, vy) {
  for (let i = 0; i < 32; i++) if (!eb_on[i]) { eb_x[i] = x; eb_y[i] = y; eb_vx[i] = vx; eb_vy[i] = vy; eb_on[i] = 1; return; }
}

function sf_kill_enemy(idx) {
  const ex = en_x[idx], ey = en_y[idx], kd = en_kind[idx];
  const c = kd === 1 ? rgb(184, 107, 255) : (kd === 2 ? rgb(255, 209, 102) : rgb(0, 228, 255));
  en_hp[idx] = 0;
  sf_st[3] += (kd + 1) * 150;
  sf_spawn_ring(ex, ey, 46, c);
  sf_spawn_ring(ex, ey, 28, rgb(255, 245, 210));
  sf_spawn_sparks(ex, ey, c);
}

function sf_step(fr) {
  sf_st[7] = fr;
  for (let i = 0; i < 64; i++) {
    let ny = st_y[i] + st_sp[i];
    if (ny >= 360) { ny -= 360; st_x[i] = sf_rand() % 640; }
    st_y[i] = ny;
  }
  const px = 320 + ((isin1024(fr * 11) * 185) >> 12);
  const py = 292 + ((icos1024(fr * 7) * 18) >> 12);
  sf_st[1] = px; sf_st[2] = py;
  if (sf_st[6] > 0) sf_st[6]--;
  if ((fr % 45) === 10) sf_st[6] = 12;
  if ((fr % 5) === 0) {
    const q = Math.trunc(fr / 5);
    sf_fire_bolt(px - 14, py - 10, 0, -11, sf_strand_col(q));
    sf_fire_bolt(px + 14, py - 10, 0, -11, sf_strand_col(q + 2));
    if ((fr % 15) === 0) {
      sf_fire_bolt(px - 22, py - 4, -3, -10, sf_strand_col(1));
      sf_fire_bolt(px + 22, py - 4, 3, -10, sf_strand_col(3));
    }
  }
  for (let b = 0; b < 40; b++) {
    if (pb_on[b]) {
      const bx = pb_x[b] + pb_vx[b], by = pb_y[b] + pb_vy[b];
      pb_x[b] = bx; pb_y[b] = by;
      if (by < 24 || bx < 0 || bx >= 640) pb_on[b] = 0;
      else {
        for (let e = 0; e < 24; e++) {
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
    for (let e = 0; e < 24; e++) {
      if (en_hp[e] > 0 && en_y[e] > 36 && en_y[e] < py) {
        const dx = iabs(en_x[e] - px), dy = py - en_y[e];
        if (dx < 32 + (dy >> 1) && (fr % 3) === 0) {
          if (--en_hp[e] <= 0) sf_kill_enemy(e);
        }
      }
    }
  }
  let alive = 0;
  for (let e = 0; e < 24; e++) {
    if (en_hp[e] > 0) {
      alive++;
      const ph = en_ph[e] + 12; en_ph[e] = ph;
      let ex = en_x[e] + ((isin1024(ph) * 3) >> 12);
      let ey = en_y[e] < 86 ? (en_y[e] + 2) : (en_y[e] + ((icos1024(ph) * 2) >> 12));
      ex = iclamp(ex, 44, 596); ey = iclamp(ey, 38, 220);
      en_x[e] = ex; en_y[e] = ey;
      if (((fr + e * 7) % 37) === 0) sf_fire_ebolt(ex, ey + 12, (isin1024(ph) * 3) >> 12, 4);
    }
  }
  if (alive < 4) sf_spawn_wave(sf_st[4] + 1);
  for (let b = 0; b < 32; b++) {
    if (eb_on[b]) {
      const bx = eb_x[b] + eb_vx[b], by = eb_y[b] + eb_vy[b];
      eb_x[b] = bx; eb_y[b] = by;
      if (by >= 356 || bx < 0 || bx >= 640) eb_on[b] = 0;
      else if (iabs(bx - px) < 14 && iabs(by - py) < 12) {
        eb_on[b] = 0;
        if (sf_st[5] > 20) sf_st[5] -= 5;
        sf_spawn_ring(px, py, 26, rgb(255, 96, 120));
      }
    }
  }
  for (let i = 0; i < 12; i++) if (rg_r[i] > 0) { const nr = rg_r[i] + 3; rg_r[i] = nr >= rg_max[i] ? 0 : nr; }
  for (let i = 0; i < 96; i++) if (sp_life[i] > 0) { sp_x[i] += sp_vx[i]; sp_y[i] += sp_vy[i]; sp_life[i]--; }
}

function sf_draw_enemy(x, y, kd) {
  if (kd === 0) {
    draw_glow_line(x, y + 12, x - 15, y - 8, rgb(255, 76, 92));
    draw_glow_line(x, y + 12, x + 15, y - 8, rgb(255, 76, 92));
    draw_line(x - 15, y - 8, x, y - 12, rgb(255, 170, 80));
    draw_line(x + 15, y - 8, x, y - 12, rgb(255, 170, 80));
    fill_rect(x - 4, y - 4, 8, 8, rgb(255, 190, 92));
    fill_rect(x - 2, y - 2, 4, 4, rgb(255, 255, 225));
  } else if (kd === 1) {
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

function sf_draw_player() {
  const px = sf_st[1], py = sf_st[2], fr = sf_st[7];
  for (let s = 0; s < 4; s++) {
    const c = sf_strand_col(s), ox = s * 6 - 9, base_ph = fr * 28 + s * 128;
    for (let k = 0; k < 14; k++) {
      const wave = (isin1024(base_ph + k * 52) * 5) >> 12;
      const sx = px + ox + wave, sy = py + 14 + k * 3, sc = 14 - k;
      const rr = uclamp255((((c >>> 16) & 255) * sc) >>> 3);
      const gg = uclamp255((((c >>> 8) & 255) * sc) >>> 3);
      const bb = uclamp255(((c & 255) * sc) >>> 3);
      px_add(sx, sy, rr, gg, bb); px_add(sx + 1, sy, rr, gg, bb);
    }
  }
  const sw = sf_st[6];
  if (sw > 0) {
    for (let s = 0; s < 4; s++) {
      const c = sf_strand_col(s), off = s * 2 - 3, tx = px + off * (58 + sw * 6);
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

function sf_draw_hud() {
  blend_rect(0, 0, 640, 26, rgb(6, 10, 24), 220);
  fill_rect(0, 26, 160, 2, rgb(0, 228, 255));
  fill_rect(160, 26, 160, 2, rgb(255, 209, 102));
  fill_rect(320, 26, 160, 2, rgb(34, 229, 138));
  fill_rect(480, 26, 160, 2, rgb(184, 107, 255));
  draw_label('OMEGA STARFIGHTER', 12, 9, 1, rgb(0, 230, 255));
  draw_label('SCORE', 168, 9, 1, rgb(164, 194, 234));
  draw_uint(sf_st[3], 210, 9, 1, rgb(255, 255, 255));
  draw_label('WAVE', 300, 9, 1, rgb(255, 209, 102));
  draw_uint(sf_st[4], 334, 9, 1, rgb(255, 244, 200));
  draw_label('SHIELD', 424, 9, 1, rgb(34, 229, 138));
  fill_rect(472, 8, 110, 10, rgb(18, 28, 48));
  fill_rect(474, 10, iclamp(sf_st[5], 0, 106), 6, rgb(34, 229, 138));
  rect_outline(472, 8, 110, 10, rgb(100, 220, 180));
}

function sf_fade_col(c, fade, urm) {
  return rgb(uclamp255(Math.trunc((((c >>> 16) & 255) * fade) / urm)),
             uclamp255(Math.trunc((((c >>> 8) & 255) * fade) / urm)),
             uclamp255(Math.trunc(((c & 255) * fade) / urm)));
}

function sf_render() {
  for (let y = 0; y < 360; y++) {
    const col = rgb(3 + ((y * 8) >>> 8), 6 + ((y * 12) >>> 8), 16 + ((y * 26) >>> 8));
    fb.fill(col, y * 640, (y + 1) * 640);
  }
  for (let i = 0; i < 64; i++) {
    px_set(st_x[i], st_y[i], st_c[i]);
    if (st_sp[i] > 2) {
      px_set(st_x[i], st_y[i] - 1, st_c[i]);
      px_set(st_x[i], st_y[i] - 2, rgb(80, 140, 200));
    }
  }
  for (let i = 0; i < 12; i++) {
    if (rg_r[i] > 0) {
      const fade = rg_max[i] - rg_r[i], urm = rg_max[i];
      draw_ring(rg_x[i], rg_y[i], rg_r[i], sf_fade_col(rg_c[i], fade, urm));
      draw_ring(rg_x[i], rg_y[i], rg_r[i] + 1, sf_fade_col(rg_c[i], fade >>> 1, urm));
    }
  }
  for (let b = 0; b < 40; b++) {
    if (pb_on[b]) {
      draw_glow_line(pb_x[b], pb_y[b], pb_x[b], pb_y[b] + 10, pb_c[b]);
      draw_line(pb_x[b], pb_y[b], pb_x[b], pb_y[b] + 8, rgb(255, 255, 255));
    }
  }
  for (let b = 0; b < 32; b++) {
    if (eb_on[b]) {
      const bx = eb_x[b], by = eb_y[b];
      px_add(bx, by, 255, 120, 90);
      px_add(bx - 1, by, 220, 60, 80); px_add(bx + 1, by, 220, 60, 80);
      px_add(bx, by - 1, 220, 60, 80); px_add(bx, by + 1, 220, 60, 80);
      px_set(bx, by, rgb(255, 240, 210));
    }
  }
  for (let e = 0; e < 24; e++) if (en_hp[e] > 0) sf_draw_enemy(en_x[e], en_y[e], en_kind[e]);
  for (let i = 0; i < 96; i++) {
    if (sp_life[i] > 0) {
      const sx = sp_x[i] >> 4, sy = sp_y[i] >> 4, c = sp_c[i], lf = sp_life[i];
      const rr = uclamp255((((c >>> 16) & 255) * lf) >>> 4);
      const gg = uclamp255((((c >>> 8) & 255) * lf) >>> 4);
      const bb = uclamp255(((c & 255) * lf) >>> 4);
      px_add(sx, sy, rr, gg, bb); px_add(sx + 1, sy, rr, gg, bb); px_add(sx, sy + 1, rr, gg, bb);
    }
  }
  sf_draw_player();
  sf_draw_hud();
}

G.run_driver('starfighter', SF_HERO, sf_init, sf_step, sf_render);
