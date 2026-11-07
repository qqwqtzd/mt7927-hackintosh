=== U 盘救援盘 ===
用途：主系统 EFI 更新(kext/config)后万一进不了系统，用它恢复。

日常（正常系统里）：
  bash /Volumes/1401/rescue/snapshot.sh    # 抓当前 EFI 状态(自动保留 3 份)

救援（进不了系统时）：
  1) 插上本 U 盘，开机选从 U 盘启动
  2) 进系统后：
       bash /Volumes/1401/rescue/restore.sh   # 用最新快照还原主 EFI
  3) 重启回主盘

说明：
- 本 U 盘的 EFI 为“干净基线”（不含 MTK 系驱动/插件），可独立正常启动；
  每次主盘 EFI 有更新后，建议重跑 snapshot.sh 存新快照。
- restore 前会把主盘 config 备份为 config.plist.before-rescue-<时间>。
