# data_fd2_battle_class_promotion_data_table

`.object3 @ 0x615FE`，byte[72] = 36 entries × 2 bytes。位址空間換算見
`assets/tables/_index.md`。table 結束於 `0x61646`，接
`data_fd2_battle_movement_cost_table`。

region 72 bytes（36 entries × 2）；Ghidra 於 `0x615FE` 目前只放 scalar `byte` label（未套
array 型別），實際大小與邊界由 accessor 與相鄰表界定（見下）。每筆描述一個「轉職目標」的結果。

## struct layout（每 entry 2 B）

```
byte[0]   post_promotion_job_id  轉職後的 job_id（0x00..0x1A job-id 空間）
byte[1]   promotion_mv_bonus     升職移動力加成（0 = 無）
```

`byte[1]` 是**升職移動力加成**，不是 learned-spell：非零時顯示 text page 0x254
「移動力增加N點！」並把該值加進 runtime_char `+0x3B` 移動力欄
（`fd2_execute_class_promotion_with_dialog @ 0x31602`，`src/ui_menu/promote.c`）。

Accessor `fd2_get_class_promotion_data_entry @ 0x4E48D` 回傳 `&table[(class_id − 0x20) × 2]`
（stride 2），`class_id` 是轉職目標的 portrait id，範圍 [0x20, 0x43]（→ index 0..35）。上界
0x43 由 `fd2_build_promotion_candidates_with_targets @ 0x31793` 決定（目標 class =
portrait_id + 0x20 / +0x32 / 或 0x34 Lord，portrait_id ∈ [0, 0x12)）。最後兩筆
（class 0x42 / 0x43）是 alt-path 專用的轉職。

## 讀取端（3，皆經 accessor）

`fd2_run_class_promotion_menu_main @ 0x31385`（玩家確認後查新 job）、
`fd2_execute_class_promotion_with_dialog @ 0x31602`（讀新 job + 移動力加成放 stat-gain 對話）、
`fd2_render_promote_candidates_grid @ 0x31019`（選單顯示候選轉職後 class）。

## entry sample

```
index 0  class 0x20 : 09 01   -> 轉職成 job 0x09（劍聖），移動力 +1
index 1  class 0x21 : 0A 00   -> 轉職成 job 0x0A（聖戰士），移動力 +0
index 18 class 0x32 : 11 02   -> 轉職成 job 0x11（英雄），移動力 +2
```

## 全 36 entries

逐轉職路線與職業名稱見 `assets/jobs.md`。
