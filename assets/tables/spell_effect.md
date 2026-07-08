# data_fd2_battle_spell_effect_table

`.object3 @ 0x619FD`，36 entries × 7 bytes = 252 bytes。entry 0（火炎術）前 4 byte
`32 00 5A 05` 可當定位 anchor。位址空間換算見 `assets/tables/_index.md`。

Ghidra type `spell_effect[36]`；C struct `spell_effect` 定義在 `src/include/types.h`。
Accessor `fd2_get_spell_effect_entry(spell_id)` 回傳 entry 指標；消費端
`fd2_calc_magic_damage @ 0x1C75E` 讀 `damage` 與 `hit_rate`（傷害再乘 per-job 魔抗係數 / 10，
見 `job_magic_resist.md`）。

## struct layout（7 B）

| offset | size | types.h 欄名 | 攻略縮寫 | 意義 |
|---|---|---|---|---|
| +0 | 2 | damage | DA | 最大傷害 / 恢復力（u16 LE）|
| +2 | 1 | hit_rate | HT | 命中率 % |
| +3 | 1 | cast_range_flags | DS | 施放距離 flags（bit4 = 直線，低 4 位 = 距離）|
| +4 | 1 | area | RN | 影響範圍（最大 3）|
| +5 | 1 | mp_cost | MP | 消耗法力 |
| +6 | 1 | target_side | WH | 作用對象（00 = 敵，01 = 己）|

## entry sample

| spell_id | 名稱 | bytes | 解析 |
|---|---|---|---|
| 00 | 火炎術 | `32 00 5A 05 00 02 00` | damage=50 hit_rate=90 cast_range_flags=5 area=0 mp_cost=2 |
| 03 | 天火術 | `F4 01 55 05 01 2A 00` | damage=500 hit_rate=85 cast_range_flags=5 area=1 mp_cost=42 |

## 邊界

表結束於 `0x619FD + 252 = 0x61AF9`，剛好接 `data_fd2_battle_enemy_data_table` 起點，
兩表零 padding。

## 全 36 entries

數值見 `assets/spells.md`。
