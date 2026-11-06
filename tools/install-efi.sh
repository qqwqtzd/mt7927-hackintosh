#!/bin/sh
# install-efi.sh: install the built kexts into every EFI partition that carries
# OpenCore, independent of the macOS disk order (disk0/disk1/disk2 changes across
# boots on this machine). Run with root: sudo tools/install-efi.sh
#
# Layout expected: <repo>/dist/<Name>.kext next to this tools/ directory.
# Only kexts that exist in dist are touched; every replaced bundle is copied to
# <Name>.kext.before-<stamp> first. APFS/FAT EFI partitions are verified with
# fsck_msdos before mounting.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
DIST="$ROOT/dist"
[ -d "$DIST" ] || { echo "no $DIST"; exit 1; }

# Keep the FAT volume name collision (all EFI partitions are "NO NAME") away.
diskutil unmount disk3s1 >/dev/null 2>&1 || true

i=0
for d in disk0s1 disk1s1 disk2s1 disk3s1; do
    [ -e "/dev/$d" ] || continue
    m="/Volumes/EFI$i"; i=$((i+1))
    mkdir -p "$m"
    /sbin/fsck_msdos -y "/dev/r$d" >/dev/null 2>&1 || true
    /sbin/mount -t msdos "/dev/$d" "$m" 2>/dev/null || true
done

STAMP=$(date +%Y%m%d-%H%M%S)
FOUND=0
REC="$ROOT/tools/recovery"
for m in /Volumes/EFI0 /Volumes/EFI1 /Volumes/EFI2 /Volumes/EFI3; do
    [ -f "$m/EFI/OC/OpenCore.efi" ] || continue
    FOUND=1
    K="$m/EFI/OC/Kexts"
    echo "=== $m"
    for name in MtkWlan MtkWlanNative MtkBt BlueToolFixup LegionBtFix; do
        src="$DIST/$name.kext"
        [ -d "$src" ] || continue
        if [ -d "$K/$name.kext" ]; then
            cp -R "$K/$name.kext" "$K/$name.kext.before-$STAMP"
        fi
        rm -rf "$K/$name.kext"
        ditto "$src" "$K/$name.kext"
        echo "  $name $(shasum -a 256 "$K/$name.kext/Contents/MacOS/$name" 2>/dev/null | awk '{print $1}')"
    done
    # recovery kit + version manifest land beside the kexts on the same EFI
    R="$(echo "$K" | sed 's|/Kexts$||')/RECOVERY"
    mkdir -p "$R"
    cp -R "$REC/." "$R/"
    {
        echo "installed at: $STAMP"
        echo "boot-args (from config NVRAM): mtkfe=native mtknativeif=1"
        echo "---"
        for name in MtkWlan MtkWlanNative MtkBt BlueToolFixup LegionBtFix; do
            if [ -d "$K/$name.kext" ]; then
                echo "$name $(shasum -a 256 "$K/$name.kext/Contents/MacOS/$name" 2>/dev/null | awk '{print $1}')"
            fi
        done
        echo "---"
        echo "rollback: run RECOVERY/RESTORE-LAST.bat (Windows) or restore-kexts.sh (macOS)"
    } > "$R/CURRENT.txt"
    echo "  recovery kit -> $R"
done
[ "$FOUND" = 1 ] || { echo "no OpenCore EFI found"; exit 1; }
sync
echo "installed; backups use .before-$STAMP"
