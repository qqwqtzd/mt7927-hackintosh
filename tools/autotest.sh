#!/bin/bash
# autotest.sh: 开机自动采集 Wi-Fi/BT 诊断快照
# ROUTER 环境变量可覆盖默认网关（用于触发流量采样）: after a reboot, collect the Wi-Fi/BT diagnostic snapshot automatically.
# Designed for unattended iterations: boot -> LaunchDaemon or manual run -> the
# snapshot lands in out/autotest/<stamp>.txt and the assistant reads it directly.
# Usage: sudo tools/autotest.sh   (writes to <repo>/out/autotest/, root-owned dir)
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
OUT="$ROOT/out/autotest"
mkdir -p "$OUT"
STAMP=$(date +%Y%m%d-%H%M%S)
F="$OUT/$STAMP.txt"
exec > "$F" 2>&1

echo "=== autotest $STAMP $(hostname) ==="
date

# wait for the WiFi driver fully up (the LaunchAgent can fire before core brings it up)
UP=0
for i in $(seq 1 120); do
    if ioreg -r -c MtkWlan -l -w0 2>/dev/null | grep -q 'W6 pass'; then UP=1; break; fi
    sleep 2
done
if [ "$UP" = 0 ]; then
    echo "NOTE: WiFi not up after 240 s; collecting driver state anyway"
fi
sleep 3

# force a little traffic so TXS helpers have data to show
( curl --noproxy '*' --interface en1 -sS -m 5 -o /dev/null http://\${ROUTER:-192.0.2.1}/ 2>/dev/null; true ) &

echo "=== MtkWlan state ==="
ioreg -r -c MtkWlan -l -w0 2>/dev/null | grep -E 'MtkState|MtkTargetSsid|MtkW8Mode|MtkW8He|MtkW6TxFailed|MtkW6TxPackets|MtkW6RxPackets|MtkW6Link|MtkW2Sec|MtkW9Txs|MtkW9TxsLast|MtkW9TxFree|MtkW9Ba|MtkStall|MtkIRQ|MtkIrq' | head -40

echo "=== en1 ==="
ifconfig en1 2>/dev/null | grep -E 'status|inet |inet6 '

echo "=== current network ==="
system_profiler SPAirPortDataType 2>/dev/null | sed -n '/Current Network Information/,+8p'

echo "=== native front ==="
ioreg -r -c MtkWlanNative -l -w0 2>/dev/null | grep -E 'MtkNativeRejected|MtkNativeRejectedCodes|MtkNativeTx|MtkNativeRx|MtkNativeScan' | head -10

echo "=== bluetooth ==="
ioreg -rc MtkBt -l -w0 2>/dev/null | grep -E 'MtkBtState|MtkBtStep|MtkBtWarmPath|MtkBtBytesSent|MtkBtChipFamily' | head -10
system_profiler SPBluetoothDataType 2>/dev/null | sed -n '1,14p'

echo "=== kernel log tail (mtk) ==="
log show --last 2m --style compact --predicate 'eventMessage CONTAINS "Mtk" OR process == "kernel"' 2>/dev/null | tail -30

echo "=== load ==="
kmutil showloaded --list-only 2>/dev/null | grep -E 'Mtk|Legion|BlueTool' || echo none
echo "=== done $STAMP ==="
echo "snapshot: $F"
