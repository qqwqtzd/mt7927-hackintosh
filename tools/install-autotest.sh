#!/bin/bash
# install-autotest.sh: install the LaunchAgent that snapshots Wi-Fi/BT diagnostics
# after every boot, so iterations do not need manual log copying.
# Run with sudo: bash tools/install-autotest.sh
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
PLIST="$HERE/com.mtkwlan.autotest.plist"
AGENT="$HOME/Library/LaunchAgents/com.mtkwlan.autotest.plist"
mkdir -p "$HOME/Library/LaunchAgents"
cp "$PLIST" "$AGENT"
chmod 644 "$AGENT"
launchctl bootout gui/"$(id -u)"/com.mtkwlan.autotest 2>/dev/null || true
launchctl bootstrap gui/"$(id -u)" "$AGENT" 2>/dev/null || true
launchctl enable gui/"$(id -u)"/com.mtkwlan.autotest
echo "LaunchAgent installed: $AGENT"
echo "Snapshots land in: $(cd "$HERE/.." && pwd)/out/autotest/"
