#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -z "${VITASDK:-}" ]]; then
  if [[ -d "$ROOT/../vitasdk" ]]; then
    VITASDK="$ROOT/../vitasdk"
  else
    VITASDK="$HOME/vitasdk"
  fi
fi
export VITASDK
export PATH="$VITASDK/bin:$PATH"

if [[ ! -d "$VITASDK" ]]; then
  echo "VitaSDK not found at: $VITASDK" >&2
  exit 1
fi

if ! command -v arm-vita-eabi-gcc >/dev/null 2>&1; then
  echo "arm-vita-eabi-gcc not found. Is \$VITASDK/bin on PATH?" >&2
  exit 1
fi

# Monotonic build number (AGENTS.md: artifact naming). Kept outside build/ so
# wiping the build directory cannot restart numbering into a collision with VPKs
# already copied to the console.
# A build number identifies a *distinct* build, not a compiler invocation.
# Rebuilding unchanged sources reuses the previous number instead of burning a
# new one (AGENTS.md: artifact naming).
STAMP="$ROOT/BUILD_NUMBER"
STATE="$ROOT/build/.source-stamp"

SOURCE_HASH=$(cd "$ROOT" && find . -path ./build -prune -o -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.yml' -o -name 'CMakeLists.txt' \
     -o -name 'build.sh' \) -print0 | sort -z | xargs -0 cat \
  | shasum | cut -d' ' -f1)

BUILD=$(cat "$STAMP" 2>/dev/null || echo 0)
case "$BUILD" in
  ''|*[!0-9]*) BUILD=0 ;;
esac

if [[ -f "$STATE" && "$(cat "$STATE")" == "$SOURCE_HASH" && "$BUILD" -gt 0 ]]; then
  echo "Sources unchanged since build $BUILD; rebuilding under the same number."
else
  BUILD=$((BUILD + 1))
  printf '%s\n' "$BUILD" > "$STAMP"
  mkdir -p "$ROOT/build"
  printf '%s\n' "$SOURCE_HASH" > "$STATE"
fi

VERSION=$(sed -n 's/^set(VITA_VERSION[[:space:]]*"\([^"]*\)").*/\1/p' "$ROOT/CMakeLists.txt")

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release -DKERNEL_BUILD="$BUILD"
cmake --build "$ROOT/build" --parallel

# Keep exactly one artifact; superseded VPKs are never useful.
NEWEST=""
for f in $(ls -t "$ROOT"/build/*.vpk 2>/dev/null); do
  if [[ -z "$NEWEST" ]]; then NEWEST="$f"; else rm -f "$f"; fi
done

echo
echo "Built:"
echo "  $ROOT/build/kernel_demo.skprx"
echo "  $ROOT/build/kernel-demo-v${VERSION}-b${BUILD}.vpk"
