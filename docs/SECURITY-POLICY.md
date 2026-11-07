# GitHub 推送安全政策（SECURITY-POLICY）

> 用户指令（2026-11-07）：**任何推送到 GitHub 的项目都必须是"公开通用版"，
> 永不含个人敏感信息。**

## 硬性禁止项（推送前必检）
- 用户名 / 主机名 / 本机路径（`/Users/<who>`、`<user>@<host>.local`、git 作者身份）
- 内网 IP / 路由器 IP / 网关 / VLAN（`192.168.*`、`10.*`、`172.*`）
- 硬件 MAC 地址（本机/路由器/对端设备）
- UUID 列表：钥匙串/IDS/设备标识（公开 NVRAM/kext 常量 UUID 除外）
- 密码、令牌（`ghp_*`、API key）、私钥
- 诊断快照（`out/autotest/*` 等含 ifconfig/kextstat 输出的文件）

## 默认动作
- git 作者身份统一为：`MT7927 Studio <dev@mt7927.dev>`（不含任何机器/人名）
- 网络占位一律用：IP `192.0.2.1`（RFC5737 TEST-NET）、MAC `xx:xx:xx:xx:xx:xx`、前缀 `xxxx::/64`
- `out/` `dist/` `REV/` 等产物不入库（.gitignore）
- 推送前扫描：
  ```bash
  grep -rnI -E 'Mac-Pro\.local|links@|/Users/|192\.168\.|([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}|ghp_[A-Za-z0-9]{10,}' \
    --include='*.md' --include='*.sh' --include='*.txt' --include='*.patch' . | grep -v '^\./\.git/'
  ```
  有输出即禁止推送。

## 历史清洗
- 已包含敏感历史的仓：用 `git-filter-repo`（mailmap 作者重写 + replace-text 串替换）清洗后强推；
- 强推注意：旧提交在 GitHub 上可能残留一段时间，公共链接/分叉会失效。
