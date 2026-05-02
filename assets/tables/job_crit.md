# job_crit_table

`.object2 @ 0x5239B`，27 entries × 1 byte = 27 bytes。signature `05 03 03 05`。

## struct

```
byte[27]   每 byte 是 job_id (0x00..0x1A) 對應的暴擊率 %
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x5219B` |
| FD2.EXE | `0x773AF` |
| Δ | +0x200 |

`.object2` 小表跨版本偏移為 +0x200，與 `.object3` 大表 +0xC200 不同。

## values

```
job  0: 5     (龍 — actually unused, 龍職位無此資料；記錄為 0 應被視為 placeholder)
job  1: 5     劍士
job  2: 3     戰士
job  3: 3     騎士
job  4: 5     弓兵
job  5: 3     法師
job  6: 3     僧侶
job  7: 0     盜賊
job  8: 18    武者
job  9: 5     劍聖
job 10: 3     聖戰士
job 11: 3     聖騎士
job 12: 12    狙擊手
job 13: 3     大法師
job 14: 3     祭師
job 15: 12    龍劍士
job 16: 10    鬥士
job 17: 6     英雄
job 18: 3     魔戰士
job 19: 3     龍騎士
job 20: 7     神射手
job 21: 3     召喚師
job 22: 3     聖者
job 23: 30    忍者
job 24: 18    武聖
job 25: 0     機兵
job 26: 0     ？？？
```

(列表顯示的 job 0 數值 `5` 是 binary raw byte；實際遊戲 job 0 = 龍未做暴擊判定，
attack guide 註明從 job 0x01 開始。)

## 全表

完整 readable 表格詳 `assets/jobs.md`。
