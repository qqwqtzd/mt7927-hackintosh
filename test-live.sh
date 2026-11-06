#!/bin/bash
# Live (non-persistent) MT7927 test: loads the kexts into the running kernel, prints diagnostics.
# Nothing is copied to EFI, so a reboot removes the driver and a panic (worst case) is recoverable
# by simply booting again. Run with:
#
#   sudo bash 20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)/test-live.sh
#
set -u
ROOT="20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400cd "20 20 12 61 79 80 81 701 33 98 100 204 250 395 398 399 400dirname "-e")" && pwd)"

if [ "$(id -u)" -ne 0 ]; then
    echo "run with: sudo bash $0"
    exit 1
fi

load_kext() {
    local k="$1"
    echo "kextload $k"
    if kextload "$k"; then
        return 0
    fi
    echo "kextload failed; retry with kmutil load -z"
    if kmutil load -z -p "$k"; then
        return 0
    fi
    echo "retry after ad-hoc signing"
    xattr -cr "$k" 2>/dev/null || true
    codesign --force --sign - "$k" 2>&1 || true
    kextload "$k" || kmutil load -z -p "$k"
}

echo "=== card ==="
ioreg -r -l -w0 -n 'pci14c3,7927' 2>/dev/null | grep -E '^\s*\+-o|"device-id"|"subsystem-id"|"class-code"' | head -8
if ! ioreg -r -l -w0 -n 'pci14c3,7927' >/dev/null 2>&1; then
    echo "pci14c3,7927 not found; Wi-Fi cannot attach. Check the card/slot first."
fi

echo
echo "=== loading MtkWlan ==="
load_kext "$ROOT/dist/MtkWlan.kext"
echo "kextload rc=$?"
sleep 5

echo
echo "=== MtkWlan state ==="
ioreg -r -c MtkWlan -l -w0 2>/dev/null | grep -E 'Mtk(Pci|RawChip|Chip|State|W2|W3|W4|W5|W6|Fw|PcieLinkCtl|BadAccess)' | head -100
echo
echo "=== interfaces ==="
ifconfig -a 2>/dev/null | grep -E '^en[0-9]|status:|inet ' | head -40

echo
echo "=== Bluetooth USB half ==="
if ioreg -r -c IOUSBHostDevice -l -w0 2>/dev/null | grep -qiE 'MediaTek|"idVendor" = (1161|5075|3725)'; then
    echo "MediaTek USB device present; loading BlueToolFixup + MtkBt"
    load_kext "$ROOT/dist/BlueToolFixup.kext" || true
    load_kext "$ROOT/dist/MtkBt.kext" || true
    sleep 5
    ioreg -rc MtkBt -l -w0 2>/dev/null | grep MtkBt | head -100
    echo
    system_profiler SPBluetoothDataType 2>/dev/null | head -30
else
    echo "No MediaTek USB Bluetooth device enumerated; skipping MtkBt."
    echo "Fix the M.2 E-key USB routing / USB map first (see INSTALL.md section 4)."
fi

echo
echo "=== kernel log (last 2 minutes) ==="
log show --last 2m --style compact --predicate 'eventMessage CONTAINS "MtkWlan" OR eventMessage CONTAINS "MtkBt"' 2>/dev/null | tail -100

echo
echo "=== done ==="
echo "Reboot to unload the live kexts (they are not in the EFI)."
