#!/bin/bash
# Install the MT7927 UTBMap into the active OpenCore EFI (disk0s1).
# Run as root, e.g. via:
#   osascript -e 'do shell script "bash 20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/install-efi-map.sh" with administrator privileges'
set -euo pipefail

EFI="/Volumes/NO NAME 1/EFI/OC"
KEXTS="$EFI/Kexts"
SRC="20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/dist/UTBMap-MT7927.kext"
STAMP=$(date +%Y%m%d-%H%M%S)
BACKUP="$KEXTS/UTBMap.kext.before-mt7927-$STAMP"

[ -d "$EFI" ] || { echo "EFI not found: $EFI"; exit 1; }
[ -f "$EFI/config.plist" ] || { echo "config.plist not found"; exit 1; }
[ -d "$KEXTS" ] || { echo "Kexts not found"; exit 1; }
[ -d "$SRC" ] || { echo "source map not found: $SRC"; exit 1; }

if [ -d "$KEXTS/UTBMap.kext" ]; then
    mv "$KEXTS/UTBMap.kext" "$BACKUP"
    echo "backup: $BACKUP"
else
    echo "no existing UTBMap.kext; nothing to back up"
fi

ditto "$SRC" "$KEXTS/UTBMap.kext"
sync

plutil -lint "$KEXTS/UTBMap.kext/Contents/Info.plist"
echo "installed map ports:"
python3 - "$KEXTS/UTBMap.kext/Contents/Info.plist" <<'PY'
import plistlib, sys
p = sys.argv[1]
d = plistlib.load(open(p, 'rb'))
pr = d['IOKitPersonalities']['XH00']['IOProviderMergeProperties']
print('entries', len(pr['ports']), 'port-count', int.from_bytes(pr['port-count'], 'little'))
print(sorted(int.from_bytes(e['port'], 'little') for e in pr['ports'].values()))
PY

echo "config UTBMap entry:"
grep -n -A6 'UTBMap.kext' "$EFI/config.plist" | head -10
echo "done"
