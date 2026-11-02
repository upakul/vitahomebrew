#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Newest versioned VPK (AGENTS.md: artifact naming).
VPK="$(ls -t "$ROOT"/build/cap-test-v*.vpk 2>/dev/null | head -1 || true)"
if [[ -z "$VPK" ]]; then
  VPK="$ROOT/build/vita_cap_test.vpk"
fi

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <vita-ip>" >&2
  exit 1
fi

if [[ ! -f "$VPK" ]]; then
  echo "VPK not found: $VPK" >&2
  echo "Run ./build.sh first." >&2
  exit 1
fi

VITA_IP="$1"

echo "Uploading to ftp://$VITA_IP:1337/ux0:/data/ ..."
curl -T "$VPK" "ftp://$VITA_IP:1337/ux0:/data/"

echo
echo "Uploaded. On the Vita:"
echo "  1. Open VitaShell"
echo "  2. Go to ux0:data"
echo "  3. Select vita_cap_test.vpk and press Cross"
echo "  4. Launch the Vita Cap Test bubble"
