#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Builds Pure-Omega omega-arcade engines on Linux AArch64 (Spark) or macOS ARM64.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
OS="$(uname -s)"
OUT_DIR="${OUT_DIR:-$ROOT/bin}"
mkdir -p "$OUT_DIR"

if [ "$OS" = "Darwin" ]; then
  OMC="${OMC:-$HOME/.local/bin/omc}"
  HDR="$ROOT/om/hdr_macos.om"
  TARGET="a64macos"
else
  OMC="${OMC:-$HOME/canon/omega-mv/seed/out/omc}"
  HDR="$ROOT/om/hdr_linux.om"
  TARGET="a64linux"
  PINNED_SHA="28fca13d44054e26f78c33d1753fd5c005d08e9144d95985c90bd75b60885072"
  GOT_SHA="$(sha256sum "$OMC" | awk '{print $1}')"
  if [ "$GOT_SHA" != "$PINNED_SHA" ]; then
    echo "FAIL: omc sha256 mismatch: got=$GOT_SHA pinned=$PINNED_SHA" >&2
    exit 2
  fi
fi

compile_unit() {
  local name="$1"
  shift
  local tmp="$OUT_DIR/${name}.tmp"
  cat "$HDR" "$@" | "$OMC" > "$tmp"
  chmod +x "$tmp"
  if [ "$OS" = "Darwin" ]; then
    codesign -s - -f "$tmp" >/dev/null 2>&1
  fi
  mv -f "$tmp" "$OUT_DIR/$name"
  local sz
  sz="$(wc -c < "$OUT_DIR/$name" | tr -d ' ')"
  echo "BUILT [$TARGET] $OUT_DIR/$name (${sz} bytes)"
}

compile_unit raycast_doom "$ROOT/om/gfx_common.om" "$ROOT/om/raycast_doom.om"
compile_unit blocks_puzzle "$ROOT/om/gfx_common.om" "$ROOT/om/blocks_puzzle.om"
compile_unit starfighter "$ROOT/om/gfx_common.om" "$ROOT/om/starfighter.om"

if [ -f "$ROOT/om/probe_oob.om" ]; then
  compile_unit probe_oob "$ROOT/om/probe_oob.om"
fi
if [ -f "$ROOT/om/probe_overflow.om" ]; then
  compile_unit probe_overflow "$ROOT/om/probe_overflow.om"
fi
if [ -f "$ROOT/om/probe_loop.om" ]; then
  compile_unit probe_loop "$ROOT/om/probe_loop.om"
fi
if [ -f "$ROOT/om/probe_div0.om" ]; then
  compile_unit probe_div0 "$ROOT/om/probe_div0.om"
fi
if [ "$OS" != "Darwin" ]; then
  if [ -f "$ROOT/om/probe_escape.om" ]; then
    compile_unit probe_escape "$ROOT/om/probe_escape.om"
  fi
  if [ -f "$ROOT/om/gate.om" ]; then
    compile_unit gate "$ROOT/om/gfx_common.om" "$ROOT/om/gate.om"
  fi
fi
