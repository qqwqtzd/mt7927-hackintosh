#!/bin/bash
# Install the MT7927 Wi-Fi/BT kexts + Bluetooth NVRAM keys into the active OpenCore EFI.
# Run as root:
#   osascript -e 'do shell script "bash /Users/links/mt7927-port/install-efi-driver.sh" with administrator privileges'
set -euo pipefail

EFI="/Volumes/NO NAME 1/EFI"
OC="$EFI/OC"
DIST="/Users/links/mt7927-port/dist"
STAMP=$(date +%Y%m%d-%H%M%S)

[ -d "$OC" ] || { echo "OC not found: $OC"; exit 1; }
[ -f "$OC/config.plist" ] || { echo "config.plist not found"; exit 1; }
[ -d "$DIST" ] || { echo "dist not found: $DIST"; exit 1; }

echo "== full EFI backup"
BACKUP="/Users/links/EFI-before-mt7927-$STAMP"
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
python3 /Users/links/mt7927-port/install-efi-driver.py "$OC" "$DIST"

echo "== validate"
plutil -lint "$OC/config.plist"
for k in BlueToolFixup MtkBt MtkWlan MtkWlanNative; do
    plutil -lint "$OC/Kexts/$k.kext/Contents/Info.plist" >/dev/null
    echo "$k.kext OK"
done
sync
echo "done"
