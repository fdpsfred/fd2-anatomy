# data_fd2_battle_character_growth_table

`.object3 @ 0x620A1`，68 entries × 11 bytes = 748 bytes (+3 bytes padding 至
0x62390)。signature `06 08 04 06`。

## struct layout

```
offset  size  field                 意義
+0      1     AP_min                每級 AP 最小成長
+1      1     AP_max                每級 AP 最大成長 +1 (exclusive)
+2      1     DP_min
+3      1     DP_max
+4      1     DX_min
+5      1     DX_max
+6      1     HP_min
+7      1     HP_max
+8      1     MP_min
+9      1     MP_max
+10     1     spell_learning_idx    data_fd2_battle_spell_learning_table 的 index；0xFF = 無
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x55EA1` |
| FD2.EXE | `0x7B0B5` |
| Δ | +0xC200 |

## entry sample

Entry 0 (索爾基礎)：`06 08 04 06 02 03 08 0C 00 00 FF`

```
AP min/max = 6 / 8
DP min/max = 4 / 6
DX min/max = 2 / 3
HP min/max = 8 / 12
MP min/max = 0 / 0
spell_learning_idx = 0xFF (無)
```

## entry 數量考量

68 entries 包含 32 角色的基礎 growth + 32 角色的轉職後 growth + 4 reserved /
unused（per actual binary table size）。詳 `assets/characters.md` 完整對照。

## padding

table 尾端 3 bytes (`00 00 54` @ `0x6238D-0x6238F`) 是對齊 padding，與本表 entry
內容無關。

## 對應 spell_learning

`spell_learning_idx` 指向 `data_fd2_battle_spell_learning_table @ 0x626B3` 的對應 entry，描述
此職業在哪些等級學什麼 spell。詳 `assets/tables/spell_learning.md`。
