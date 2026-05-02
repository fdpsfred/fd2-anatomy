# enemy_data_table

`.object3 @ 0x61AF9`，68 entries × 10 bytes = 680 bytes。signature
`01 02 12 00 00 05`。

## struct layout

```
offset  size  field  意義
+0      1     RA     race ID
+1      1     CL     class / job ID
+2      2     HP     該等級 HP (u16 LE)
+4      1     MP
+5      1     AP     攻擊
+6      1     DP     防禦
+7      1     DX     敏捷 / 迴避
+8      1     MV     移動力
+9      1     EX     擊敗時獲得的經驗值
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x558F9` |
| FD2.EXE | `0x7AB0D` |
| Δ | +0xC200 |

## char_id namespace

`enemy_id = char_id - 0x44`，**address-based**：

```
enemy_id = (table_address - 0x7AB0D) / 10
```

`init_runtime_char_for_battle @ 0x10C50`：char_id < 0x44 走
`get_char_base_entry`，≥ 0x44 走 `get_enemy_data_entry(char_id - 0x44)`。

## 全 68 entries

詳 `assets/enemies.md`。

## sample

Entry 0 (士兵 友方)：`01 02 12 00 00 05 02 01 04 1E`

```
RA=1 CL=2 HP=18 MP=0 AP=5 DP=2 DX=1 MV=4 EX=30
```

## 邊界精確驗證

`character_base_table` 在 `0x61DA1`，所以 enemy_data 大小 =
`0x61DA1 - 0x61AF9 = 0x2A8 = 680 bytes = 68 × 10`。表之間零 padding，緊密排列。
