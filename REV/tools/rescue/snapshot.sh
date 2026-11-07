#!/bin/bash
# Snapshot the boot EFI (config.plist + Kexts + Drivers + ACPI + Tools), skipping stale backups.
set -e
U=/Volumes/1401
E=$(mount | awk '$3 ~ /^\/Volumes\/EFI/ {print $3; exit}')
if [ -z "$E" ]; then echo "未找到已挂载 EFI"; exit 1; fi
TS=$(date +%Y%m%d-%H%M%S)
D="$U/efi-snapshots/$TS"
mkdir -p "$D/EFI/OC"
cp "$E/EFI/OC/config.plist" "$D/EFI/OC/config.plist"
for sub in ACPI Drivers Kexts Tools; do
  if [ -d "$E/EFI/OC/$sub" ]; then
    mkdir -p "$D/EFI/OC/$sub"
    find "$E/EFI/OC/$sub" -maxdepth 1 -mindepth 1 \
      ! -name '*.before-*' ! -name '*.bak-*' ! -name '*broken*' ! -name '*.kext.kext' \
      -exec cp -R {} "$D/EFI/OC/$sub/" \;
  fi
done
echo "快照完成: $D"
ls -dt "$U"/efi-snapshots/*/ 2>/dev/null | tail -n +4 | xargs -r rm -rf
echo "保留快照: $(ls -dt "$U"/efi-snapshots/*/ | wc -l | tr -d ' ') 份"
