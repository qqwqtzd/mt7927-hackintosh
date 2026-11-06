#!/usr/bin/env python3
"""Print the current macOS USB controllers, root ports and attached devices.

Run after plugging a device into a physical port to see which controller/port it lands on:

    python3 usb-list.py

No sudo needed; read-only.
"""
import plistlib
import subprocess
import sys


def load():
    raw = subprocess.check_output(
        ["ioreg", "-a", "-p", "IOService", "-r", "-c", "AppleUSBXHCIPCI", "-l", "-w0"]
    )
    return plistlib.loads(raw)


def props(node):
    return {k: v for k, v in node.items() if not k.startswith("IORegistryEntry")}


def name(node):
    return node.get("IORegistryEntryName") or node.get("IOObjectClass") or "?"


def acpi(node):
    dp = node.get("device-properties")
    return dp.get("acpi-path") if isinstance(dp, dict) else ""


def dev_line(node, depth):
    desc = node.get("USB Product Name") or node.get("USB Vendor Name") or name(node)
    vid = node.get("idVendor")
    pid = node.get("idProduct")
    ids = f" [{vid:04x}:{pid:04x}]" if isinstance(vid, int) and isinstance(pid, int) else ""
    loc = node.get("locationID")
    locs = f" loc=0x{loc:08x}" if isinstance(loc, int) else ""
    print("  " * depth + f"DEV  {desc}{ids}{locs}")


def walk_device(node, depth):
    if node.get("IOObjectClass") == "IOUSBHostDevice":
        dev_line(node, depth)
    for child in node.get("IORegistryEntryChildren", []):
        walk_device(child, depth + (1 if node.get("IOObjectClass") == "IOUSBHostDevice" else 0))


def main():
    controllers = load()
    if not controllers:
        print("no AppleUSBXHCIPCI controllers found")
        return 1
    for c in controllers:
        print("=" * 78)
        print(f"Controller {name(c)}")
        path = acpi(c)
        if path:
            print(f"  ACPI: {path}")
        pc = c.get("port-count")
        if isinstance(pc, bytes):
            print(f"  port-count: {int.from_bytes(pc, 'little')}")
        ports = [n for n in c.get("IORegistryEntryChildren", [])
                 if n.get("IOObjectClass") in ("AppleUSB20XHCIPort", "AppleUSB30XHCIPort")]
        if not ports:
            print("  (no port nodes)")
            continue
        for p in ports:
            port = p.get("port")
            pn = int.from_bytes(port, "little") if isinstance(port, bytes) else "?"
            u = p.get("usb-port-number")
            un = int.from_bytes(u, "little") if isinstance(u, bytes) else pn
            print(f"  PORT {name(p):7} port={pn:2} usb-port-number={un:2} "
                  f"UsbConnector={p.get('UsbConnector')}")
            has = False
            for ch in p.get("IORegistryEntryChildren", []):
                if ch.get("IOObjectClass") == "IOUSBHostDevice":
                    dev_line(ch, 2)
                    for sub in ch.get("IORegistryEntryChildren", []):
                        walk_device(sub, 3)
                    has = True
            if not has:
                print("    (empty)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
