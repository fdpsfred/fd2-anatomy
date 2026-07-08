# data_fd2_battle_job_magic_resist_table

`.object2 @ 0x51F96`，dword[28] = 112 bytes（`[0x51F96, 0x52006)`）。位址空間換算見
`assets/tables/_index.md`。下一張表（供 `fd2_animate_spell_overlay_blink` 用）從 `0x52006`
起。

Ghidra type `dword[28]`；C 端 `const uint32 data_fd2_battle_job_magic_resist_table[28]`。

## struct 與 entry 語意（28 dword = job_id 0x01..0x1C）

```
dword[28]   每個值是該職業承受法術傷害的縮放係數（十分之幾，u32 LE）
```

消費端以 `table[job_id − 1]` 索引，故 28 個 dword 對應 job_id 0x01..0x1C（index 0..27），全部
是真 entry：

- index 0..25 = job_id 0x01..0x1A 的 26 個標準職業。
- index 26 = job_id 0x1B（村民類敵人的 class_id；值 10 → 0% 抗）。
- index 27 = job_id 0x1C（達克塞與一隻沼澤怪物共用的 class_id；值 7 → 30% 抗）。達克塞是可加入的
  玩家角色（char_id 0x1C、class_id 0x1C），受魔法攻擊時確實讀出 index 27，故此筆並非尾端 padding。
- job_id 0x00（職業名稱表作「龍」）沒有任何單位使用，也永不查此表。

（`fd2_calc_magic_damage` 以 `REP MOVSD`，`ECX = 0x1C = 28` 整批複製 28 dword 到堆疊暫存區後再索引，
複製筆數與真 entry 數一致。）

## 消費端與計算公式

唯一消費者 `fd2_calc_magic_damage @ 0x1C75E`：

```
MOV ECX,0x1C ; MOV EDI,ESP ; MOV ESI,0x51F96 ; REP MOVSD   ; 整批複製 28 dword 到 stack
resist = table[job_id − 1]
魔法傷害 = (s32)(spell_base_power × resist) / 10
```

`resist = 10` 表示滿傷（無抗性）；值越低抗性越高，遊戲內魔法抗性 = (10 − resist) × 10 %。
例：job 0x05 法師 → 7 → 30% 抗；job 0x0D 大法師 → 5 → 50% 抗；job 0x1A → 4 → 60% 抗。

## entry sample

```
0x51F96   0A 00 00 00      index 0 (job 0x01) = u32 10
0x51F9A   0A 00 00 00      index 1 (job 0x02) = u32 10
0x51F9E   0A 00 00 00      index 2 (job 0x03) = u32 10
```

## 全 28 dword

每職業魔抗係數數值見 `assets/jobs.md`。
