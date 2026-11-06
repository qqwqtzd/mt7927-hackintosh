#!/bin/bash
# Install the MT7927 Wi-Fi/BT kexts + Bluetooth NVRAM keys into the active OpenCore EFI.
# Run as root:
#   osascript -e 'do shell script "bash 20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/install-efi-driver.sh" with administrator privileges'
set -euo pipefail

EFI="/Volumes/NO NAME 1/EFI"
OC="$EFI/OC"
DIST="20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/dist"
STAMP=$(date +%Y%m%d-%H%M%S)

[ -d "$OC" ] || { echo "OC not found: $OC"; exit 1; }
[ -f "$OC/config.plist" ] || { echo "config.plist not found"; exit 1; }
[ -d "$DIST" ] || { echo "dist not found: $DIST"; exit 1; }

echo "== full EFI backup"
BACKUP="$(dirname "$0")/EFI-before-mt7927-$STAMP"
rm -rf "$BACKUP"
ditto "$EFI" "$BACKUP"
chown -R links:staff "$BACKUP"
echo "backup: $BACKUP"

echo "== config backup"
cp -p "$OC/config.plist" "$OC/config.plist.before-mt7927-$STAMP"

echo "== install kexts"
for k in BlueToolFixup MtkBt MtkWlan MtkWlanNative; do
    if [ -d "$OC/Kexts/$k.kext" ]; then
        mv "$OC/Kexts/$k.kext" "$OC/Kexts/$k.kext.before-mt7927-$STAMP"
        echo "backup $k.kext"
    fi
    ditto "$DIST/$k.kext" "$OC/Kexts/$k.kext"
    echo "installed $k.kext"
done

echo "== edit config.plist"
python3 20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/install-efi-driver.py "$OC" "$DIST"

echo "== validate"
plutil -lint "$OC/config.plist"
for k in BlueToolFixup MtkBt MtkWlan MtkWlanNative; do
    plutil -lint "$OC/Kexts/$k.kext/Contents/Info.plist" >/dev/null
    echo "$k.kext OK"
done
sync
echo "done"
