#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Newest versioned VPK. build.sh names them cockpit-v<version>-b<build>.vpk so
# several builds can sit in ux0:data without overwriting each other.
VPK="$(ls -t "$ROOT"/build/cockpit-v*.vpk 2>/dev/null | head -1 || true)"
if [[ -z "$VPK" ]]; then
  VPK="$ROOT/build/cockpit.vpk"
fi

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <vita-ip>" >&2
  echo "Example: $0 192.168.1.42" >&2
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
echo "  3. Select cockpit.vpk and press Cross"
echo "  4. Launch the COCKPIT bubble"
