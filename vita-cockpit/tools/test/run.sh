#!/usr/bin/env bash
set -euo pipefail

# Host tests for the sampling policy. Links the real core against the shared
# fake platform. Not part of the app build.
#
#   tools/test/run.sh

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build/test"

mkdir -p "$OUT"

CC="${CC:-cc}"

"$CC" -std=c99 -O1 -g \
  -Wall -Wextra \
  -I"$ROOT/src" \
  -I"$ROOT/tools/fake" \
  "$ROOT/tools/test/test_metrics.c" \
  "$ROOT/tools/fake/fake_platform.c" \
  "$ROOT/src/core/history.c" \
  "$ROOT/src/core/metrics.c" \
  "$ROOT/src/core/plugins.c" \
  -o "$OUT/test_metrics"

"$OUT/test_metrics"
