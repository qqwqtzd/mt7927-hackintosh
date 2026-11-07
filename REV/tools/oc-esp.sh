#!/bin/bash
# oc-esp.sh — mount the OpenCore ESP robustly (by content, not disk number).
# Usage: oc-esp.sh            -> mount OC ESP at /Volumes/OCESP (or reuse existing mount)
#        oc-esp.sh path       -> print path of mounted OC ESP
# Fish-rule: never trust disk numbers; always probe for EFI/OC/OpenCore.efi.
set -e
# 1) already mounted somewhere?
for m in /Volumes/OCESP /Volumes/EFI0 /Volumes/EFI1; do
  [ -f "$m/EFI/OC/OpenCore.efi" ] && { echo "$m"; exit 0; }
done
# 2) probe every msdos/EFI partition on every disk (by UUID would be nicer, but content works after any reorder)
FOUND=""
for d in $(diskutil list 2>/dev/null | awk '/EFI|MS-DOS|FAT/{print $NF}' | grep -E '^disk[0-9]+s[0-9]+$'); do
  M=/tmp/ocprobe-$d
  mkdir -p "$M"
  umount "$M" 2>/dev/null || true
  if mount -t msdos /dev/$d "$M" 2>/dev/null; then
    if [ -f "$M/EFI/OC/OpenCore.efi" ]; then
      FOUND="$d"
      mv "$M" /Volumes/OCESP 2>/dev/null || true
      if [ -f /Volumes/OCESP/EFI/OC/OpenCore.efi ]; then
        echo "/Volumes/OCESP ($d)"
        exit 0
      fi
      # if move failed, keep probing and unmount
    fi
    umount "$M" 2>/dev/null || true
  fi
done
echo "OC ESP 未找到（U 盘未插/系统异常）" >&2
exit 1
