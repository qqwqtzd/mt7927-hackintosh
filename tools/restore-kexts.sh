#!/bin/bash
# restore-kexts.sh: undo a Windows-side DISABLE-WIFI.bat and roll MtkWlan back to
# the newest before-* backup when one exists. Run with sudo on the Mac.
# Usage: sudo bash tools/restore-kexts.sh
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)

# mount every EFI partition that carries OpenCore (same probe as install-efi.sh)
for d in disk0s1 disk1s1 disk2s1 disk3s1; do
    [ -e "/dev/$d" ] && diskutil mount "$d" >/dev/null 2>&1 || true
done
i=0; FOUND=0
for m in /Volumes/EFI0 /Volumes/EFI1 /Volumes/EFI2 /Volumes/EFI3; do
    mkdir -p "$m"
    # ensure a mount point exists even if the volume is unmounted
    [ -f "$m/EFI/OC/OpenCore.efi" ] || continue
    FOUND=1
    K="$m/EFI/OC/Kexts"
    echo "=== $m"
    for name in MtkWlan MtkWlanNative MtkBt BlueToolFixup LegionBtFix; do
        if [ -d "$K/$name.kext.off" ]; then
            rm -rf "$K/$name.kext"
            mv "$K/$name.kext.off" "$K/$name.kext"
            echo "  re-enabled $name"
        fi
    done
    # roll MtkWlan back to the newest before-* backup, if one exists
    NEW=$(ls -1dt "$K"/MtkWlan.kext.before-* 2>/dev/null | head -1)
    if [ -n "$NEW" ] && [ -d "$NEW" ]; then
        rm -rf "$K/MtkWlan.kext"
        cp -R "$NEW" "$K/MtkWlan.kext"
        echo "  MtkWlan rolled back to: $(basename "$NEW")"
        echo "    hash $(shasum -a 256 "$K/MtkWlan.kext/Contents/MacOS/MtkWlan" 2>/dev/null | awk '{print $1}')"
    else
        echo "  no MtkWlan backup found; kept current MtkWlan.kext"
    fi
    sync
done
[ "$FOUND" = 1 ] || echo "no OpenCore EFI found"
echo "now reboot the Mac."
echo "after boot, the assistant reads out/autotest/*.txt automatically."
