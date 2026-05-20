# data_fd2_battle_job_magic_resist_table

`.object2 @ 0x51F96`，27 entries × 4 bytes = 108 bytes。signature `09 0A 00 00 00`
(注意：`09` 是表前一個 anchor byte，**不**在表內)。

## struct

```
dword[27]   每 dword 是 job_id (0x00..0x1A) 對應的魔法抗性原始值 (u32 LE)
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x51D96` |
| FD2.EXE | `0x76FAA` |
| Δ | +0x200 |

`.object2` 小表跨版本偏移為 +0x200，與 `.object3` 大表 +0xC200 不同。

## 計算公式

```
魔法抗性 = (10 - 數值) / 10
```

例：值 7 → 30% 抗性；值 4 → 60% 抗性；值 10 → 0% 抗性。

## values (按 job_id)

```
10, 10, 10, 10, 7, 7, 10, 10, 10, 10, 9, 10, 5, 5, 8, 10, 6, 8, 10, 9, 5, 5, 10, 8, 8, 4, 10
```

對應 job 0x00..0x1A。job 0 (龍) 無此資料，binary value 為佔位 placeholder。

## 全表 readable

詳 `assets/jobs.md`。

## entry sample

Anchor + entry 0..2：

```
0x51F95   09               anchor byte (signature 第 1 byte，不屬本表)
0x51F96   0A 00 00 00      entry 0 = u32 10
0x51F9A   0A 00 00 00      entry 1 = u32 10
0x51F9E   0A 00 00 00      entry 2 = u32 10
```
