#!/usr/bin/env bash
set -euo pipefail

# Host preview: renders the shipped composition through a software vita2d shim
# and the shared fake platform, then writes PNGs. Not part of the app build.
#
#   tools/preview/run.sh [output-dir]

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/build/preview}"

mkdir -p "$OUT"

CC="${CC:-cc}"
SRC="$ROOT/src"

"$CC" -std=c99 -O1 -g \
  -Wall -Wextra \
  -I"$ROOT/tools/preview/stub" \
  -I"$ROOT/tools/fake" \
  -I"$SRC" \
  "$ROOT/tools/preview/preview.c" \
  "$ROOT/tools/preview/vita2d_soft.c" \
  "$ROOT/tools/fake/fake_platform.c" \
  "$SRC/app/screen.c" \
  "$SRC/gfx/draw.c" \
  "$SRC/gfx/font.c" \
  "$SRC/gfx/theme.c" \
  "$SRC/core/history.c" \
  "$SRC/core/input.c" \
  "$SRC/core/metrics.c" \
  "$SRC/core/plugin_editor.c" \
  "$SRC/core/plugins.c" \
  "$SRC/panels/core.c" \
  "$SRC/panels/frame.c" \
  "$SRC/panels/plugins.c" \
  "$SRC/panels/plugins_summary.c" \
  "$SRC/panels/power.c" \
  "$SRC/panels/registry.c" \
  "$SRC/panels/storage.c" \
  "$SRC/panels/system.c" \
  "$SRC/ui/header.c" \
  "$SRC/ui/modal.c" \
  "$SRC/ui/plugin_flow.c" \
  "$SRC/ui/tabbar.c" \
  -lz -lm \
  -o "$OUT/preview"

"$OUT/preview" "$OUT"
