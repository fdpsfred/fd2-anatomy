# data_fd2_battle_job_magic_resist_table

`.object2 @ 0x51F96`。可讀區間 `[0x51F96, 0x52006)` = 28 dword = 112 bytes：
前 27 dword 是各職業的魔法抗性原始值，第 28 dword (值 7) 是消費端
`fd2_calc_magic_damage` 的 REP MOVSD (ECX=0x1C) 連帶複製進堆疊暫存區的尾端 dword；
下一張表 (供 `fd2_animate_spell_overlay_blink` 用) 才從 0x52006 開始。
signature `09 0A 00 00 00` (注意：`09` 是表前一個 anchor byte，**不**在表內)。

## struct

```
dword[28]   前 27 dword 是 job_id (0x00..0x1A) 對應的魔法抗性原始值 (u32 LE)；
            第 28 dword 是 REP MOVSD 連帶複製的尾端值，不對應任何職業
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

對應 job 0x00..0x1A (27 個值)。job 0 (龍) 無此資料，binary value 為佔位 placeholder。
可讀區間第 28 dword 的尾端值為 7，不對應任何職業 (僅因 REP MOVSD 複製 28 dword 連帶帶入)。

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
