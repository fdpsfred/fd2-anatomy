# data_fd2_battle_job_crit_rate_table

`.object2 @ 0x5239B`，27 entries × 1 byte = 27 bytes。signature `05 03 03 05`。

## struct

```
byte[27]   每 byte 是某職業的暴擊率 %（unsigned）
```

accessor 為 `table[job_id - 1]`（job_id 1-based），故 element 0 對應 job 0x01。
job 0x00（龍）無此資料，永不被查表。consumer 反組譯：
`MOVZX EAX, byte ptr [EAX + 0x5239B]`，EAX = bJob_id - 1，stride 1。

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x5219B` |
| FD2.EXE | `0x773AF` |
| Δ | +0x200 |

`.object2` 小表跨版本偏移為 +0x200，與 `.object3` 大表 +0xC200 不同。

## values

byte index N 對應 job_id (N+1)：

```
idx  0  job 0x01: 5     劍士
idx  1  job 0x02: 3     戰士
idx  2  job 0x03: 3     騎士
idx  3  job 0x04: 5     弓兵
idx  4  job 0x05: 3     法師
idx  5  job 0x06: 3     僧侶
idx  6  job 0x07: 0     盜賊
idx  7  job 0x08: 18    武者
idx  8  job 0x09: 5     劍聖
idx  9  job 0x0a: 3     聖戰士
idx 10  job 0x0b: 3     聖騎士
idx 11  job 0x0c: 12    狙擊手
idx 12  job 0x0d: 3     大法師
idx 13  job 0x0e: 3     祭師
idx 14  job 0x0f: 12    龍劍士
idx 15  job 0x10: 10    鬥士
idx 16  job 0x11: 6     英雄
idx 17  job 0x12: 3     魔戰士
idx 18  job 0x13: 3     龍騎士
idx 19  job 0x14: 7     神射手
idx 20  job 0x15: 3     召喚師
idx 21  job 0x16: 3     聖者
idx 22  job 0x17: 30    忍者
idx 23  job 0x18: 18    武聖
idx 24  job 0x19: 0     機兵
idx 25  job 0x1a: 0     ？？？
idx 26           0     trailing pad（無 job 0x1b）
```

## 全表

完整 readable 表格詳 `assets/jobs.md`。
