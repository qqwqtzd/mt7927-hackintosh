# U 盘救援盘（rescue USB）

设计：主 EFI 更新（kext/config）一旦进不了系统，用 U 盘恢复。

- snapshot.sh  在正常系统执行：抓当前 EFI 快照到 U 盘（保留 3 份）
- restore.sh   从 U 盘启动后执行：用最新快照还原主 EFI（还原前自动备份当前 config）
- check.sh     状态自检
- U 盘自身 EFI = 干净基线（Mtk 系条目 Enabled=false，可独立启动）
