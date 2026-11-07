#!/bin/bash
# restore the boot EFI from the newest snapshot on this USB. Run after USB boot.
set -e
U=/Volumes/1401
SNAP=$(ls -dt "$U"/efi-snapshots/*/ 2>/dev/null | head -1)
if [ -z "$SNAP" ]; then
  echo "U 盘无快照"
  exit 1
fi
echo "将还原: $SNAP"
TARGET=""
for v in /Volumes/EFI*; do
  if [ -f "$v/EFI/OC/OpenCore.efi" ] && [ -d "$v/EFI/OC/Kexts/MtkWlanNative.kext" ]; then TARGET="$v"; break; fi
done
if [ -z "$TARGET" ]; then
  for v in /Volumes/EFI*; do
    if [ -f "$v/EFI/OC/OpenCore.efi" ]; then TARGET="$v"; break; fi
  done
fi
if [ -z "$TARGET" ]; then
  echo "未找到目标 EFI"
  exit 1
fi
echo "目标 EFI: $TARGET"
TS=$(date +%Y%m%d-%H%M%S)
cp "$TARGET/EFI/OC/config.plist" "$TARGET/EFI/OC/config.plist.before-rescue-$TS"
rm -rf "$TARGET/EFI/OC/Kexts"
cp -R "$SNAP/EFI/OC/Kexts" "$TARGET/EFI/OC/Kexts"
cp "$SNAP/EFI/OC/config.plist" "$TARGET/EFI/OC/config.plist"
for sub in Drivers ACPI Tools; do
  if [ -d "$SNAP/EFI/OC/$sub" ]; then
    cp -R "$SNAP/EFI/OC/$sub" "$TARGET/EFI/OC/"
  fi
done
echo "已还原（原 config 备份: config.plist.before-rescue-$TS）。重启。"
