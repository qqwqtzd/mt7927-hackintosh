#!/bin/bash
# Install LegionBtFix.kext and enable the A2DP HCI-logging workaround.
set -euo pipefail
EFI_DISK=disk0s1
SRC="20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/dist/LegionBtFix.kext"
STAMP=$(date +%Y%m%d-%H%M%S)

diskutil mount "$EFI_DISK" >/dev/null
MOUNT=$(diskutil info "$EFI_DISK" | awk -F': *' '/Mount Point/ {print $2}')
[ -d "$MOUNT/EFI/OC" ] || { echo "OC not found at $MOUNT"; exit 1; }
K="$MOUNT/EFI/OC/Kexts"

if [ -d "$K/LegionBtFix.kext" ]; then
    cp -R "$K/LegionBtFix.kext" "$K/LegionBtFix.kext.before-$STAMP"
fi
ditto "$SRC" "$K/LegionBtFix.kext"
python3 20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/add-legionbtfix.py "$MOUNT/EFI/OC"
plutil -lint "$MOUNT/EFI/OC/config.plist"
plutil -lint "$K/LegionBtFix.kext/Contents/Info.plist"

echo "== enable A2DP HCI-logging workaround"
bash 20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/third-party/legionbtfix/bt-hci-logging.sh on

sync
diskutil unmount "$EFI_DISK"
echo "done"
