#!/bin/sh
# Release step: publish dist/doorbell-app.bin to the web updater (docs/update/).
# Copies the app image, writes the version label into manifest.json and index.html,
# then runs the host check (sha256 vs dist, offset 0x20000, single part, no erase).
# Usage: sh firmware/tools/sync_web_update.sh   (after tools/build.sh)
set -eu
FW="$(cd "$(dirname "$0")/.." && pwd)"
UP="$FW/../docs/update"
BIN="$FW/dist/doorbell-app.bin"
[ -f "$BIN" ] || { echo "missing $BIN (run tools/build.sh first)" >&2; exit 1; }
mkdir -p "$UP"

# version label: first "## vX.Y" heading in RELEASE-NOTES.md
VER="$(sed -n 's/^## v\([0-9][0-9A-Za-z.+-]*\).*/\1/p' "$FW/RELEASE-NOTES.md" | head -n 1)"
[ -n "$VER" ] || { echo "no version heading in RELEASE-NOTES.md" >&2; exit 1; }
# if HEAD carries a v* tag it must agree
TAG="$(git -C "$FW" describe --tags --exact-match --match 'v*' 2>/dev/null || true)"
if [ -n "$TAG" ] && [ "${TAG#v}" != "$VER" ]; then
  echo "git tag $TAG does not match RELEASE-NOTES v$VER" >&2; exit 1
fi

cp "$BIN" "$UP/doorbell-app.bin"
cat > "$UP/manifest.json" <<JSON
{
  "name": "Bell Aid",
  "version": "$VER",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "doorbell-app.bin", "offset": 131072 }
      ]
    }
  ]
}
JSON
sed -i "s|<!--ver-->[^<]*<!--/ver-->|<!--ver-->$VER<!--/ver-->|" "$UP/index.html"
echo "web updater: v$VER, $(sha256sum "$UP/doorbell-app.bin" | cut -c1-16)..."
python3 "$FW/tools/check_web_update.py"
