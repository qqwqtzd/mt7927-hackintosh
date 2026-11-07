#!/bin/bash
# diag.sh: 一键采集 MT7927 Wi-Fi/蓝牙诊断快照并打包成 zip
# 用途：出任何问题（断连/漫游异常/蓝牙消失/性能差）时跑一次，把 zip 交给
#       维护者或 AI 助手分析。不需要重启，通常 <30 秒。
# 用法：
#   bash tools/diag.sh                # 输出到 ~/Desktop/mtkwlan-diag-<时间戳>.zip
#   bash tools/diag.sh /path/to/dir   # 输出到指定目录
# 隐私提示：zip 内包含 SSID / MAC / 内网 IP，分享给他人前注意脱敏。
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
DEST=${1:-"$HOME/Desktop"}
STAGE=$(mktemp -d)/diag-$STAMP
mkdir -p "$STAGE" "$DEST"
ZIP="$DEST/mtkwlan-diag-$STAMP.zip"

echo "=== MT7927 诊断打包 $STAMP ==="
echo "输出: $ZIP"

# ---- 头部信息 ----
{
    echo "=== header ==="
    date
    sw_vers
    hostname
    echo
    echo "--- 漫游计数 ---"
    ioreg -r -c MtkWlan -l -w0 2>/dev/null | grep -E 'MtkW5Roam|MtkW5RoamEvals' || true
} > "$STAGE/00-header.txt"

# ---- Wi-Fi 驱动全量属性（含扫描缓存/状态机/计数器）----
ioreg -r -c MtkWlan -l -w0 2>/dev/null > "$STAGE/01-mtkwlan-full.txt"

# ---- 原生前端（菜单/ioctl 计数）----
ioreg -r -c MtkWlanNative -l -w0 2>/dev/null > "$STAGE/02-native-full.txt"

# ---- 蓝牙驱动 ----
ioreg -rc MtkBt -l -w0 2>/dev/null > "$STAGE/03-mtkbt-full.txt"

# ---- USB 枚举（蓝牙所在端口 / warm path 证据）----
ioreg -p IOUSB -l -w0 2>/dev/null > "$STAGE/04-iousb.txt"

# ---- 网络接口树 ----
{
    ioreg -r -c IONetworkInterface -l -w0 2>/dev/null | head -200
    echo "--- IO80211 ---"
    ioreg -r -c IO80211Interface -l -w0 2>/dev/null | head -120
} > "$STAGE/05-iface-tree.txt"

# ---- 网络状态 ----
{
    echo "--- ifconfig -a ---"
    ifconfig -a
    echo
    echo "--- en1 重点 ---"
    ifconfig en1 2>/dev/null
    echo
    echo "--- 路由 ---"
    netstat -rn -f inet 2>/dev/null | head -20
    echo
    echo "--- DNS ---"
    scutil --dns 2>/dev/null | head -30
} > "$STAGE/06-netstate.txt"

# ---- Wi-Fi 概要（当前网络/历史网络/接口 PHY）----
system_profiler SPAirPortDataType 2>/dev/null > "$STAGE/07-airport.txt" &
SP1=$!

# ---- 蓝牙概要 ----
system_profiler SPBluetoothDataType 2>/dev/null > "$STAGE/08-bluetooth.txt" &
SP2=$!

# ---- kext 加载状态 ----
kmutil showloaded --list-only 2>/dev/null | grep -iE 'mtk|legion|bluetool' > "$STAGE/09-loaded-kexts.txt" || echo none > "$STAGE/09-loaded-kexts.txt"

# ---- 内核日志（mtk 相关，近 5 分钟；需 sudo 的 dmesg 失败则跳过）----
{
    echo "--- log show (last 5m, Mtk/kernel) ---"
    log show --last 5m --style compact --predicate 'eventMessage CONTAINS "Mtk" OR (process == "kernel" AND eventMessage CONTAINS "wlan")' 2>/dev/null | tail -120
    echo
    echo "--- dmesg (sudo, 可能跳过) ---"
    sudo -n dmesg 2>/dev/null | grep -iE 'mtk|wlan|80211|bt' | tail -60 || echo "(无 sudo 权限，跳过 dmesg；log show 已覆盖大部分)"
} > "$STAGE/10-kernel-log.txt"

# ---- 电源/睡眠 ----
pmset -g 2>/dev/null > "$STAGE/11-pmset.txt"

# ---- 最近一次 autotest 快照（若有）----
LATEST=$(ls -t "$ROOT/out/autotest/"*.txt 2>/dev/null | head -1)
if [ -n "$LATEST" ]; then cp "$LATEST" "$STAGE/12-last-autotest.txt"; fi

# ---- 漫游/日志配置回执（驱动参数现状）----
{
    ioreg -r -c MtkWlan -l -w0 2>/dev/null | grep -E 'MtkRoam|MtkRoamDelta|MtkRoamAbsRssi|MtkRoamPreferHe|MtkStall|MtkFrontEnd|MtkState' | head -20
} > "$STAGE/13-roam-config.txt"

wait $SP1 $SP2 2>/dev/null

# 打包
(cd "$(dirname "$STAGE")" && zip -qr "$ZIP" "diag-$STAMP" 2>/dev/null)

echo
echo "=== 快速摘要 ==="
grep -hE 'MtkState|MtkW8Mode' "$STAGE/01-mtkwlan-full.txt" 2>/dev/null | head -3 || true
grep -hE 'MtkW5Roam|MtkW5RoamEvals|MtkRoamDelta|MtkRoamAbsRssi' "$STAGE/13-roam-config.txt" 2>/dev/null | sort -u | head -8 || true
grep -hE 'MtkBtState' "$STAGE/03-mtkbt-full.txt" 2>/dev/null | head -2 || true
echo
echo "已生成: $ZIP"
echo "文件数: $(zipinfo -t "$ZIP" 2>/dev/null | grep -oE '[0-9]+ files' | head -1)"

rm -rf "$(dirname "$STAGE")"
