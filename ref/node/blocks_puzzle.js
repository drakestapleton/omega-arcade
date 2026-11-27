// SPDX-License-Identifier: Apache-2.0
// ref/node/blocks_puzzle.js: Bit-exact Node.js reference port of om/blocks_puzzle.om.
'use strict';
const G = require('./gfx_common');
const { fb, rgb, uclamp255, px_add, fill_rect, blend_rect, rect_outline, draw_label, draw_uint } = G;

const BK_HERO = 248, BK_OX = 240, BK_OY = 20, BK_CELL = 16;
const BK_SHAPES =
  '0111213120212223021222321011121310201121102011211020112110201121' +
  '1001112110112112011121121001111210200111101121221121021200011112' +
  '0010112120112112011112221001110200011121102011120111212210110212' +
  '20011121101112220111210200101112';

const board = new Uint8Array(200);
const bag = new Uint8Array(7);
const bk_st = new Int32Array(16);
const pt_x = new Int32Array(96), pt_y = new Int32Array(96);
const pt_vx = new Int32Array(96), pt_vy = new Int32Array(96), pt_life = new Int32Array(96);
const pt_col = new Uint32Array(96);

function bk_rand() {
  const s = (Math.imul(bk_st[0] >>> 0, 1664525) + 1013904223) >>> 0;
  bk_st[0] = s & 2147483647;
  return s;
}

function bk_dx(p, r, c) { return BK_SHAPES.charCodeAt(p * 32 + r * 8 + c * 2) - 48; }
function bk_dy(p, r, c) { return BK_SHAPES.charCodeAt(p * 32 + r * 8 + c * 2 + 1) - 48; }

function bk_refill_bag() {
  for (let i = 0; i < 7; i++) bag[i] = i;
  for (let i = 6; i > 0; i--) {
    const j = bk_rand() % (i + 1);
    const t = bag[i]; bag[i] = bag[j]; bag[j] = t;
  }
  bk_st[1] = 0;
}

function bk_pop_bag() {
  if (bk_st[1] >= 7) bk_refill_bag();
  return bag[bk_st[1]++];
}

function bk_fits(p, r, x, y) {
  for (let c = 0; c < 4; c++) {
    const gx = x + bk_dx(p, r, c), gy = y + bk_dy(p, r, c);
    if (gx < 0 || gx >= 10 || gy >= 20) return false;
    if (gy >= 0 && board[gy * 10 + gx] !== 0) return false;
  }
  return true;
}

function bk_ghost_y(p, r, x, y) {
  let gy = y;
  while (bk_fits(p, r, x, gy + 1)) gy++;
  return gy;
}

function bk_try_rotate() {
  const p = bk_st[2], r1 = (bk_st[3] + 1) & 3, x0 = bk_st[4], y0 = bk_st[5];
  const kdx = [0, -1, 1, 0, -2, 2], kdy = [0, 0, 0, -1, 0, 0];
  for (let k = 0; k < 6; k++) {
    if (bk_fits(p, r1, x0 + kdx[k], y0 + kdy[k])) {
      bk_st[3] = r1; bk_st[4] = x0 + kdx[k]; bk_st[5] = y0 + kdy[k];
      return true;
    }
  }
  return false;
}

function bk_stamp(p, r, x, y, val) {
  for (let c = 0; c < 4; c++) {
    const px = x + bk_dx(p, r, c), py = y + bk_dy(p, r, c);
    if (py >= 0 && py < 20) board[py * 10 + px] = val;
  }
}

function bk_score_board() {
  let cl = 0, holes = 0, agg_h = 0;
  for (let row = 0; row < 20; row++) {
    let full = true;
    for (let col = 0; col < 10; col++) if (!board[row * 10 + col]) full = false;
    if (full) cl++;
  }
  for (let col = 0; col < 10; col++) {
    let seen = false;
    for (let row = 0; row < 20; row++) {
      if (board[row * 10 + col]) { if (!seen) { seen = true; agg_h += 20 - row; } }
      else if (seen) holes++;
    }
  }
  return cl * 180 - holes * 42 - agg_h * 5;
}

function bk_eval_place(p, r, x) {
  if (!bk_fits(p, r, x, 0)) return -999999;
  const gy = bk_ghost_y(p, r, x, 0);
  bk_stamp(p, r, x, gy, p + 1);
  const sc = bk_score_board();
  bk_stamp(p, r, x, gy, 0);
  return sc + gy * 4;
}

function bk_plan_ai() {
  const p = bk_st[2];
  let best_sc = -999999, best_r = 0, best_x = 3;
  for (let r = 0; r < 4; r++) {
    for (let x = -2; x < 10; x++) {
      const sc = bk_eval_place(p, r, x);
      if (sc > best_sc) { best_sc = sc; best_r = r; best_x = x; }
    }
  }
  bk_st[10] = best_r; bk_st[11] = best_x;
}

function bk_piece_color(cid) {
  if (cid === 1) return rgb(0, 228, 255);
  if (cid === 2) return rgb(255, 212, 82);
  if (cid === 3) return rgb(184, 96, 255);
  if (cid === 4) return rgb(38, 232, 138);
  if (cid === 5) return rgb(255, 72, 104);
  if (cid === 6) return rgb(66, 138, 255);
  return rgb(255, 152, 56);
}

function bk_spawn_particles(row) {
  const py = BK_OY + row * BK_CELL + 8;
  for (let col = 0; col < 10; col++) {
    const c = bk_piece_color(board[row * 10 + col]);
    const px = BK_OX + col * BK_CELL + 8;
    for (let k = 0; k < 96; k++) {
      if (pt_life[k] <= 0) {
        pt_x[k] = px << 4; pt_y[k] = py << 4;
        const rv = bk_rand();
        pt_vx[k] = (rv & 63) - 32;
        pt_vy[k] = ((rv >>> 6) & 31) - 44;
        pt_life[k] = 22; pt_col[k] = c;
        break;
      }
    }
  }
}

function bk_spawn_next() {
  const p = bk_st[6];
  bk_st[2] = p; bk_st[6] = bk_pop_bag();
  bk_st[3] = 0; bk_st[4] = 3; bk_st[5] = 0;
  if (!bk_fits(p, 0, 3, 0)) board.fill(0);
  bk_plan_ai();
}

function bk_collapse_row(row) {
  while (row > 0) {
    board.copyWithin(row * 10, (row - 1) * 10, row * 10);
    row--;
  }
  board.fill(0, 0, 10);
}

function bk_lock_piece() {
  bk_stamp(bk_st[2], bk_st[3], bk_st[4], bk_st[5], bk_st[2] + 1);
  let cleared = 0, fmask = 0, row = 19;
  while (row >= 0) {
    let full = true;
    for (let col = 0; col < 10; col++) if (!board[row * 10 + col]) full = false;
    if (full) {
      cleared++; fmask |= (1 << row);
      bk_spawn_particles(row);
      bk_collapse_row(row);
    } else row--;
  }
  if (cleared > 0) {
    bk_st[12] = 10; bk_st[13] = fmask;
    bk_st[8] += cleared;
    bk_st[9] = 1 + Math.trunc(bk_st[8] / 4);
    bk_st[7] += cleared * cleared * 100 * bk_st[9];
  } else bk_st[7] += 10;
  bk_spawn_next();
}

function bk_init() {
  board.fill(0); pt_life.fill(0);
  bk_st[0] = 13372026; bk_st[1] = 7; bk_st[7] = 0; bk_st[8] = 0;
  bk_st[9] = 1; bk_st[12] = 0; bk_st[13] = 0; bk_st[14] = 0;
  for (let r = 0; r < 4; r++) {
    const row = 16 + r, gap = 2 + (r * 3) % 6;
    for (let c = 0; c < 10; c++) if (c !== gap) board[row * 10 + c] = 1 + (r * 3 + c) % 7;
  }
  bk_refill_bag();
  bk_st[6] = bk_pop_bag();
  bk_spawn_next();
}

function bk_step(fr) {
  bk_st[14] = fr;
  for (let k = 0; k < 96; k++) {
    if (pt_life[k] > 0) {
      pt_x[k] += pt_vx[k]; pt_y[k] += pt_vy[k];
      pt_vy[k] += 3; pt_life[k]--;
    }
  }
  if (bk_st[12] > 0) bk_st[12]--;
  if (bk_st[3] !== bk_st[10]) {
    if (!bk_try_rotate()) bk_st[10] = bk_st[3];
  } else {
    const cx = bk_st[4], tx = bk_st[11];
    if (cx < tx && bk_fits(bk_st[2], bk_st[3], cx + 1, bk_st[5])) bk_st[4] = cx + 1;
    else if (cx > tx && bk_fits(bk_st[2], bk_st[3], cx - 1, bk_st[5])) bk_st[4] = cx - 1;
  }
  const p = bk_st[2], r = bk_st[3], cx = bk_st[4];
  let cy = bk_st[5];
  const step_n = (r === bk_st[10] && cx === bk_st[11]) ? 2 : 1;
  for (let s = 0; s < step_n; s++) {
    if (bk_fits(p, r, cx, cy + 1)) { cy++; bk_st[5] = cy; }
    else { bk_lock_piece(); return; }
  }
}

function bk_draw_bevel(x, y, c) {
  const r = (c >>> 16) & 255, g = (c >>> 8) & 255, b = c & 255;
  const hi = rgb(r + 72, g + 72, b + 72);
  const lo = rgb(r >>> 1, g >>> 1, b >>> 1);
  const core = rgb(r + 22, g + 22, b + 22);
  fill_rect(x, y, 15, 15, c);
  fill_rect(x, y, 15, 2, hi);
  fill_rect(x, y, 2, 15, hi);
  fill_rect(x, y + 13, 15, 2, lo);
  fill_rect(x + 13, y, 2, 15, lo);
  fill_rect(x + 4, y + 4, 7, 7, core);
  fill_rect(x + 4, y + 4, 3, 2, rgb(255, 255, 255));
}

function bk_draw_ghost(x, y, c) {
  blend_rect(x + 1, y + 1, 13, 13, c, 46);
  rect_outline(x, y, 15, 15, c);
  rect_outline(x + 1, y + 1, 13, 13, c);
}

function bk_draw_bg() {
  for (let y = 0; y < 360; y++) {
    const col = rgb(7 + ((y * 10) >>> 8), 11 + ((y * 16) >>> 8), 26 + ((y * 30) >>> 8));
    fb.fill(col, y * 640, (y + 1) * 640);
  }
  const gcol = rgb(18, 28, 54);
  for (let gx = 20; gx < 640; gx += 40) fill_rect(gx, 0, 1, 360, gcol);
  for (let gy = 20; gy < 360; gy += 40) fill_rect(0, gy, 640, 1, gcol);
}

function bk_draw_panels() {
  blend_rect(28, 24, 184, 312, rgb(10, 16, 34), 220);
  rect_outline(28, 24, 184, 312, rgb(0, 205, 245));
  fill_rect(29, 25, 45, 3, rgb(0, 228, 255));
  fill_rect(74, 25, 46, 3, rgb(255, 209, 102));
  fill_rect(120, 25, 46, 3, rgb(34, 229, 138));
  fill_rect(166, 25, 45, 3, rgb(184, 107, 255));
  draw_label('OMEGA', 46, 42, 2, rgb(0, 230, 255));
  draw_label('BLOCKS', 46, 64, 2, rgb(255, 224, 130));
  fill_rect(42, 88, 156, 1, rgb(46, 72, 118));
  draw_label('SCORE', 46, 104, 2, rgb(148, 184, 228));
  draw_uint(bk_st[7], 46, 128, 2, rgb(255, 255, 255));
  draw_label('LINES', 46, 176, 2, rgb(148, 184, 228));
  draw_uint(bk_st[8], 46, 200, 2, rgb(34, 229, 138));
  draw_label('LEVEL', 46, 248, 2, rgb(148, 184, 228));
  draw_uint(bk_st[9], 46, 272, 2, rgb(255, 209, 102));
  blend_rect(428, 24, 184, 312, rgb(10, 16, 34), 220);
  rect_outline(428, 24, 184, 312, rgb(184, 107, 255));
  draw_label('NEXT', 452, 42, 2, rgb(0, 230, 255));
  fill_rect(448, 70, 144, 96, rgb(6, 10, 22));
  rect_outline(448, 70, 144, 96, rgb(58, 88, 140));
  const np = bk_st[6], nc = bk_piece_color(np + 1);
  for (let c = 0; c < 4; c++) bk_draw_bevel(488 + bk_dx(np, 0, c) * BK_CELL, 94 + bk_dy(np, 0, c) * BK_CELL, nc);
  draw_label('7-BAG', 452, 194, 2, rgb(255, 209, 102));
  const bp = bk_st[1];
  for (let i = 0; i < 7; i++) {
    const bx = 452 + i * 20;
    const col = i >= bp ? bk_piece_color(bag[i] + 1) : rgb(28, 38, 62);
    fill_rect(bx, 222, 14, 14, col);
    rect_outline(bx, 222, 14, 14, rgb(180, 210, 255));
  }
  draw_label('FRAME', 452, 264, 2, rgb(148, 184, 228));
  draw_uint(bk_st[14], 452, 288, 2, rgb(220, 238, 255));
}

function bk_render() {
  bk_draw_bg();
  fill_rect(BK_OX - 4, BK_OY - 4, 168, 328, rgb(6, 10, 20));
  rect_outline(BK_OX - 4, BK_OY - 4, 168, 328, rgb(0, 224, 255));
  rect_outline(BK_OX - 2, BK_OY - 2, 164, 324, rgb(38, 62, 104));
  for (let row = 0; row < 20; row++)
    for (let col = 0; col < 10; col++)
      rect_outline(BK_OX + col * BK_CELL, BK_OY + row * BK_CELL, 15, 15, rgb(16, 24, 44));
  for (let row = 0; row < 20; row++)
    for (let col = 0; col < 10; col++)
      if (board[row * 10 + col])
        bk_draw_bevel(BK_OX + col * BK_CELL, BK_OY + row * BK_CELL, bk_piece_color(board[row * 10 + col]));
  const p = bk_st[2], r = bk_st[3], cx = bk_st[4], cy = bk_st[5];
  const gy0 = bk_ghost_y(p, r, cx, cy), pc = bk_piece_color(p + 1);
  for (let c = 0; c < 4; c++) bk_draw_ghost(BK_OX + (cx + bk_dx(p, r, c)) * BK_CELL, BK_OY + (gy0 + bk_dy(p, r, c)) * BK_CELL, pc);
  for (let c = 0; c < 4; c++) bk_draw_bevel(BK_OX + (cx + bk_dx(p, r, c)) * BK_CELL, BK_OY + (cy + bk_dy(p, r, c)) * BK_CELL, pc);
  if (bk_st[12] > 0) {
    const al = bk_st[12] * 22;
    for (let row = 0; row < 20; row++)
      if ((bk_st[13] >> row) & 1)
        blend_rect(BK_OX, BK_OY + row * BK_CELL, 160, BK_CELL, rgb(230, 252, 255), al);
  }
  for (let k = 0; k < 96; k++) {
    const lf = pt_life[k];
    if (lf > 0) {
      const px = pt_x[k] >> 4, py = pt_y[k] >> 4, c = pt_col[k];
      const rr = uclamp255((((c >>> 16) & 255) * lf) >>> 4);
      const gg = uclamp255((((c >>> 8) & 255) * lf) >>> 4);
      const bb = uclamp255(((c & 255) * lf) >>> 4);
      px_add(px, py, rr, gg, bb); px_add(px + 1, py, rr, gg, bb);
      px_add(px, py + 1, rr, gg, bb); px_add(px + 1, py + 1, rr, gg, bb);
      px_add(px - 1, py, rr >>> 1, gg >>> 1, bb >>> 1);
      px_add(px + 2, py, rr >>> 1, gg >>> 1, bb >>> 1);
      px_add(px, py - 1, rr >>> 1, gg >>> 1, bb >>> 1);
      px_add(px, py + 2, rr >>> 1, gg >>> 1, bb >>> 1);
    }
  }
  bk_draw_panels();
}

G.run_driver('blocks_puzzle', BK_HERO, bk_init, bk_step, bk_render);
