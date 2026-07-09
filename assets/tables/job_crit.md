# data_fd2_battle_job_crit_rate_table

`.object2 @ 0x5239B`，byte[28] = 28 bytes。entry 0 起 `05 03 03 05` 可當定位 anchor。
位址空間換算見 `assets/tables/_index.md`。

Ghidra type `byte[28]`。

## struct

```
byte[28]   每 byte = 某職業的暴擊率 %（unsigned）
```

Accessor 為 `table[job_id − 1]`（job_id 1-based），故 index 0 對應 job 0x01。反組譯：
`MOVZX EAX, byte ptr [EAX + 0x5239B]`，EAX = job_id − 1，stride 1。與魔抗表
`data_fd2_battle_job_magic_resist_table` 同 job 域、同 accessor 形式。job 0x00（龍）
無單位使用、永不查表。

## entry / index 對應

index N 對應 job_id (N+1)，服務 class_id 0x01..0x1C，共 28 entries（idx 0..27），與
`job_magic_resist`（dword[28]）平行。例：index 0（job 0x01 劍士）= 5、index 22
（job 0x17 忍者）= 30。idx 24..27（job 0x19 機兵 / 0x1A / 0x1B 村民 / 0x1C 達克塞）
數值為 0，是「不暴擊」的真 entry（非 padding）：達克塞（char 0x1C，job 0x1C）與共用
class 0x1C 的沼澤怪物物理攻擊時實際讀 idx27。idx 28..29 才是表尾對齊 padding，其後
（0x523B9）為另一張指標表。

## 全 28 bytes

每職業暴擊率數值見 `assets/jobs.md`。
