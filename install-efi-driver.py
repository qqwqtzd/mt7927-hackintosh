#!/usr/bin/env python3
"""Add the MT7927 kexts and Bluetooth NVRAM keys to an OpenCore config.plist.

Usage: install-efi-driver.py <EFI/OC directory> <dist directory>
Run as root (the EFI volume is root-owned).
"""
import os
import plistlib
import shutil
import sys

GUID = "7C436110-AB2A-4BBB-A880-FE41995C9F82"
BOOT_ARGS = "mtkfe=native mtknativeif=1"

KEXTS = [
    ("BlueToolFixup.kext", "Contents/MacOS/BlueToolFixup", "BlueToolFixup 2.7.2 (MT7927 BT)"),
    ("MtkBt.kext", "Contents/MacOS/MtkBt", "MediaTek MT7927 Bluetooth firmware"),
    ("MtkWlan.kext", "Contents/MacOS/MtkWlan", "MediaTek MT7927 Wi-Fi (port)"),
    ("MtkWlanNative.kext", "Contents/MacOS/MtkWlanNative", "MediaTek native Wi-Fi menu"),
]


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    oc, dist = sys.argv[1], sys.argv[2]
    cfg = os.path.join(oc, "config.plist")
    kext_dir = os.path.join(oc, "Kexts")
    if not os.path.isfile(cfg):
        print(f"missing {cfg}")
        return 1

    with open(cfg, "rb") as f:
        data = plistlib.load(f)

    kernel_add = data["Kernel"]["Add"]
    existing = {e.get("BundlePath") for e in kernel_add}
    for bundle, exe, comment in KEXTS:
        entry = {
            "Arch": "Any",
            "BundlePath": bundle,
            "Comment": comment,
            "Enabled": True,
            "ExecutablePath": exe,
            "MaxKernel": "",
            "MinKernel": "",
            "PlistPath": "Contents/Info.plist",
        }
        if bundle in existing:
            for i, e in enumerate(kernel_add):
                if e.get("BundlePath") == bundle:
                    kernel_add[i] = entry
                    break
            print(f"updated {bundle}")
        else:
            kernel_add.append(entry)
            print(f"added {bundle}")

    # Keep the map comment accurate.
    for e in kernel_add:
        if e.get("BundlePath") == "UTBMap.kext":
            e["Comment"] = "MT7927 map: 43FC ports 1-10,15-17 (no front USB2.0)"

    nvram = data.setdefault("NVRAM", {})
    add = nvram.setdefault("Add", {}).setdefault(GUID, {})
    add["bluetoothExternalDongleFailed"] = b"\x00"
    add["bluetoothInternalControllerInfo"] = b"\x00" * 14
    old = add.get("boot-args", "")
    if "mtkfe=native" not in old:
        add["boot-args"] = (old + " " + BOOT_ARGS).strip()
    nvram.setdefault("Delete", {}).setdefault(GUID, [])
    delete = nvram["Delete"][GUID]
    for key in ("bluetoothExternalDongleFailed", "bluetoothInternalControllerInfo", "boot-args"):
        if key not in delete:
            delete.append(key)

    tmp = cfg + ".mtk7927.tmp"
    with open(tmp, "wb") as f:
        plistlib.dump(data, f, fmt=plistlib.FMT_XML, sort_keys=False)
    # Validate before replacing.
    with open(tmp, "rb") as f:
        plistlib.load(f)
    os.replace(tmp, cfg)

    print("Kernel.Add:")
    for e in data["Kernel"]["Add"]:
        print(" ", e.get("BundlePath"), e.get("Enabled"))
    print("NVRAM Add:", sorted(add.keys()))
    print("NVRAM Delete:", sorted(delete))
    print("boot-args:", add.get("boot-args"))
    print("kext dir:", kext_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
