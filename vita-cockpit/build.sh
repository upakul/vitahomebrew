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
  echo "Install it first, or export VITASDK=/path/to/vitasdk" >&2
  exit 1
fi

if ! command -v arm-vita-eabi-gcc >/dev/null 2>&1; then
  echo "arm-vita-eabi-gcc not found. Is \$VITASDK/bin on PATH?" >&2
  exit 1
fi

# A build number identifies a *distinct* build, not a compiler invocation.
#
# Rebuilding unchanged sources -- which happens constantly while checking for
# warnings -- reuses the previous number instead of burning a new one. Without
# this the counter tracked keystrokes rather than builds, and the build
# directory filled with near-identical artifacts (D46).
STAMP="$ROOT/BUILD_NUMBER"
STATE="$ROOT/build/.source-stamp"

SOURCE_HASH=$(cd "$ROOT" && find src tools CMakeLists.txt build.sh -type f \
  -print0 | sort -z | xargs -0 cat | shasum | cut -d' ' -f1)

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

# --safe builds the restricted eboot. The default is UNSAFE (D42): uma0:
# capacity needs a devctl the safe authid refuses.
SAFE=OFF
VARIANT=""
if [[ "${1:-}" == "--safe" ]]; then
  SAFE=ON
  VARIANT="-safe"
fi

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release \
  -DCOCKPIT_BUILD="$BUILD" -DCOCKPIT_SAFE="$SAFE"
cmake --build "$ROOT/build" --parallel

# Keep exactly one artifact. A superseded VPK is never useful -- the console has
# the copy that matters -- and a directory of forty near-identical files is
# worse than none (D46).
NEWEST=""
for f in $(ls -t "$ROOT"/build/*.vpk 2>/dev/null); do
  if [[ -z "$NEWEST" ]]; then
    NEWEST="$f"
  else
    rm -f "$f"
  fi
done

echo
echo "Built: $ROOT/build/cockpit-v${VERSION}-b${BUILD}${VARIANT}.vpk"
