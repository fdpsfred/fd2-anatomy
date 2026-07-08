# data_fd2_battle_job_crit_rate_table

`.object2 @ 0x5239B`，byte[27] = 27 bytes。entry 0 起 `05 03 03 05` 可當定位 anchor。
位址空間換算見 `assets/tables/_index.md`。

Ghidra type `byte[27]`。

## struct

```
byte[27]   每 byte = 某職業的暴擊率 %（unsigned）
```

Accessor 為 `table[job_id − 1]`（job_id 1-based），故 index 0 對應 job 0x01。反組譯：
`MOVZX EAX, byte ptr [EAX + 0x5239B]`，EAX = job_id − 1，stride 1。job 0x00（龍）無此
資料，永不查表。

## entry / index 對應

index N 對應 job_id (N+1)：index 0..25 = job 0x01..0x1A（26 個職業），index 26 為尾端
padding（無 job 0x1B）。例：index 0（job 0x01 劍士）= 5、index 22（job 0x17 忍者）= 30。

## 全 27 bytes

每職業暴擊率數值見 `assets/jobs.md`。
