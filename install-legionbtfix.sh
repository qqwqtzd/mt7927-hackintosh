#!/bin/bash
# Install LegionBtFix.kext and enable the A2DP HCI-logging workaround.
set -euo pipefail
EFI_DISK=disk0s1
SRC="/Users/links/mt7927-port/dist/LegionBtFix.kext"
STAMP=$(date +%Y%m%d-%H%M%S)

diskutil mount "$EFI_DISK" >/dev/null
MOUNT=$(diskutil info "$EFI_DISK" | awk -F': *' '/Mount Point/ {print $2}')
[ -d "$MOUNT/EFI/OC" ] || { echo "OC not found at $MOUNT"; exit 1; }
K="$MOUNT/EFI/OC/Kexts"

if [ -d "$K/LegionBtFix.kext" ]; then
    cp -R "$K/LegionBtFix.kext" "$K/LegionBtFix.kext.before-$STAMP"
fi
ditto "$SRC" "$K/LegionBtFix.kext"
python3 /Users/links/mt7927-port/add-legionbtfix.py "$MOUNT/EFI/OC"
plutil -lint "$MOUNT/EFI/OC/config.plist"
plutil -lint "$K/LegionBtFix.kext/Contents/Info.plist"

echo "== enable A2DP HCI-logging workaround"
bash /Users/links/mt7927-port/third-party/legionbtfix/bt-hci-logging.sh on

sync
diskutil unmount "$EFI_DISK"
echo "done"
