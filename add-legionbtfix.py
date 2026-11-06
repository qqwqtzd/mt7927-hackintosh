#!/usr/bin/env python3
"""Add LegionBtFix.kext to an OpenCore config.plist (after Lilu/BlueToolFixup)."""
import os
import plistlib
import sys

BUNDLE = "LegionBtFix.kext"
ENTRY = {
    "Arch": "Any",
    "BundlePath": BUNDLE,
    "Comment": "WTLLegionBtFix 0.3.0 (bluetoothd LE address + BT clock patches)",
    "Enabled": True,
    "ExecutablePath": "Contents/MacOS/LegionBtFix",
    "MaxKernel": "25.99.99",
    "MinKernel": "25.0.0",
    "PlistPath": "Contents/Info.plist",
}


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    cfg = os.path.join(sys.argv[1], "config.plist")
    with open(cfg, "rb") as f:
        data = plistlib.load(f)
    add = data["Kernel"]["Add"]
    for i, e in enumerate(add):
        if e.get("BundlePath") == BUNDLE:
            del add[i]
            break
    # Insert after BlueToolFixup if present, else append.
    idx = len(add)
    for i, e in enumerate(add):
        if e.get("BundlePath") == "BlueToolFixup.kext":
            idx = i + 1
            break
    add.insert(idx, dict(ENTRY))
    tmp = cfg + ".legion.tmp"
    with open(tmp, "wb") as f:
        plistlib.dump(data, f, fmt=plistlib.FMT_XML, sort_keys=False)
    with open(tmp, "rb") as f:
        plistlib.load(f)
    os.replace(tmp, cfg)
    print("Kernel.Add:")
    for e in add:
        print(" ", e.get("BundlePath"), e.get("Enabled"), e.get("MinKernel"), e.get("MaxKernel"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
