#!/bin/bash
# Rebuild the three MT7927 port kexts with the Command Line Tools and refresh dist/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

echo "== MtkWlan (Wi-Fi core)"
(cd WTLMtkWireless && bash tools/build-mac.sh)

echo "== MtkBt (Bluetooth firmware loader)"
(cd WTLMtkBluetooth && bash tools/build-mac.sh)

echo "== MtkWlanNative (native Wi-Fi menu)"
(cd WTLWifiMenu && NO_LINKCHECK=1 ./build.sh)

mkdir -p dist
rm -rf dist/MtkWlan.kext dist/MtkBt.kext dist/MtkWlanNative.kext
ditto WTLMtkWireless/out/MtkWlan.kext dist/MtkWlan.kext
ditto WTLMtkBluetooth/out/MtkBt.kext dist/MtkBt.kext
ditto WTLWifiMenu/out/MtkWlanNative.kext dist/MtkWlanNative.kext

echo "== firmware symbols"
nm -m dist/MtkWlan.kext/Contents/MacOS/MtkWlan | grep -E 'mtk_fw_(patch|ram)_7927' | head -4
nm -m dist/MtkBt.kext/Contents/MacOS/MtkBt | grep -E 'mtk_bt_fw_7927' | head -4

echo "== sha256"
shasum -a 256 dist/MtkWlan.kext/Contents/MacOS/MtkWlan \
              dist/MtkBt.kext/Contents/MacOS/MtkBt \
              dist/MtkWlanNative.kext/Contents/MacOS/MtkWlanNative
