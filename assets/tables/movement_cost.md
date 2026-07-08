# data_fd2_battle_movement_cost_table

`.object3 @ 0x61646`，byte[580] = 29 rows × 20 bytes。位址空間換算見
`assets/tables/_index.md`。table 結束於 `0x6188A`，接
`data_fd2_battle_job_allowed_items_table`。

Ghidra type `byte[580]`。每列是某職業對 20 種 tile 屬性類別的移動花費向量。

## struct layout（每 row 20 B）

```
byte[0..19]   對 tile-attribute class 0..19 的移動花費（單 byte，stride 1）
```

Accessor `fd2_get_movement_cost_table_for_job @ 0x4E555` 回傳 `&table[job_id × 0x14]`
（stride 0x14 = 20）。job_id 範圍 0..0x1A（27 個邏輯職業 = rows 0..26）；實體 symbol 配置
29 rows（580 B）到下一張表邊界，rows 27..28 是 accessor 範圍外的保留 slot。

## 消費端語意

取得的 row 指標被當參數傳給 floodfill / pathfind（`fd2_init_movement_range_floodfill`、
`fd2_pathfind_to_destination`），這些函數不自己呼叫 accessor。實際扣點在
`fd2_flood_fill_neighbor_step @ 0x4E16E`：`SUB CL, byte ptr [ESI + EAX]` 從剩餘移動預算
（runtime +0x3B，見 `enemy_data.md`）減去 `cost_table[tile_attr]`。值意義：1 = 一般格、
2..3 = 較難地形、20（0x14）= 幾乎不可通行（一步耗盡預算）。呼叫端對飛行類 0x13（固定 cost）
與 unit 0x10（換用另一張 cost 表）有特例覆寫。

## 讀取端（8）

`fd2_player_action_menu_loop @ 0x18890`、`fd2_wait_for_action_target_input @ 0x115B6`、
`fd2_ai_score_physical_attack @ 0x14237`、`fd2_ai_score_item_use @ 0x1567E`、
`fd2_ai_score_offensive_spell @ 0x1598A`、`fd2_ai_seek_optimal_position @ 0x14121`、
`fd2_ai_walk_to_target_tile @ 0x14B78`、`fd2_compute_aoe_targets @ 0x14818`。

## entry sample

```
job 0x00 (龍) : 01×20                         -> 所有地形皆 cost 1（龍飛行）
job 0x01      : 01 14 01 02 02 14 01 01 ...    -> tile class 1/5 cost 20（不可行）、3/4 cost 2
```

## 全 29 rows

逐職業移動特性見 `assets/jobs.md`。
