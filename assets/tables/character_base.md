# data_fd2_battle_character_base_table

`.object3 @ 0x61DA1`，32 entries × 24 bytes = 768 bytes。entry 0（索爾）前 4 byte
`01 01 01 2A` 可當定位 anchor。位址空間換算見 `assets/tables/_index.md`。

Ghidra type `char_base_entry[32]`；C struct `character_base` 定義在
`src/include/types.h`。玩家角色（char_id < 0x44）出場時由
`fd2_init_runtime_char_for_battle @ 0x10C50` 讀本表建立 runtime 單位；敵 / 友軍 NPC
（char_id ≥ 0x44）改讀 `enemy_data`（見 `enemy_data.md`）。

## struct layout（24 B）

| offset | size | types.h 欄名 | 攻略縮寫 | 意義 |
|---|---|---|---|---|
| +0  | 1 | race_id | RA | 種族 ID（見 `assets/races.md`）|
| +1  | 1 | class_id | CL | 職業 ID |
| +2  | 1 | level | LV | 出場時等級 |
| +3  | 2 | hp | HP | 基礎 HP（u16 LE）|
| +5  | 2 | mp | MP | 基礎 MP（u16 LE）|
| +7  | 1 | mv | MV | 移動力（複製進 runtime +0x3B，不乘等級）|
| +8  | 4 | initial_spells[4] | MG | 初始已會法術（4 個 byte slot；0 = 無）|
| +12 | 6 | initial_items[6] | IT | 初始裝備 item ID（0xFF = 空 slot）|
| +18 | 2 | ap | AP | 基礎攻擊力（u16 LE）|
| +20 | 2 | dp | DP | 基礎防禦力（u16 LE）|
| +22 | 2 | dx | DX | 基礎敏捷 / 迴避（u16 LE）|

## 出場數值計算

玩家角色出場屬性 = base + growth 成長：HP / MP = base + growth_min ×（LV−1），
AP / DP / DX = base + growth_min × LV（成長 min 欄取自 character_growth）。完整公式與
逐角色數值見 `assets/characters.md`；成長表 struct 見 `character_growth.md`。

## entry sample

Entry 0（索爾）：`01 01 01 2A 00 00 00 04 00 00 00 00 00 84 C0 FF FF FF 00 00 00 00 00 00`

```
race_id=01 class_id=01 level=01
hp=42 mp=0 mv=4
initial_spells = 00 00 00 00        (出場無已會法術)
initial_items  = 00 84 C0 FF FF FF  (短劍 / item 0x84 / item 0xC0 + 3 空 slot)
ap=0 dp=0 dx=0                       (基礎全 0，實際靠 LV × growth + 裝備)
```

## 全 32 entries

數值見 `assets/characters.md`。
