#!/bin/bash
# Rescue USB quick check
echo "snapshots:"
ls -dt /Volumes/1401/efi-snapshots/*/ 2>/dev/null
echo "usb-kexts:"
ls /Volumes/1401/EFI/OC/Kexts/ 2>/dev/null | grep -v before
echo "usb-config-mtk-entries:"
/usr/libexec/PlistBuddy -c "Print :Kernel:Add" /Volumes/1401/EFI/OC/config.plist 2>/dev/null | grep -iE 'mtk|legion' | head -6 || echo "none"
