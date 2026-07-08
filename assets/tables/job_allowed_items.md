# data_fd2_battle_job_allowed_items_table

`.object3 @ 0x6188A`，byte[203] = 29 rows × 7 bytes。位址空間換算見
`assets/tables/_index.md`。table 結束於 `0x61955`，接
`data_fd2_battle_weapon_attack_anim_pattern_ptr_table_21`。

Ghidra type `byte[203]`。每列是某職業的可裝備 item type 白名單。

## struct layout（每 row 7 B）

```
byte[0..5]   最多 6 個可裝備的 item type ID（0xFF = 未用 slot）
byte[6]      固定 0x01 row marker（accessor / 消費端不讀）
```

Accessor `fd2_get_job_allowed_items_table_entry @ 0x4E53E` 回傳 `&table[job_id × 7]`
（stride 7）。job_id 範圍 0..0x1A（27 個邏輯職業 = rows 0..26）；實體 symbol 配置 29 rows
（203 B）到下一張表邊界，rows 27..28 是 accessor 範圍外的保留 slot。

## 消費端語意

唯一消費者 `fd2_check_job_can_equip_item @ 0x1C1C3`：取得 row 指標後，以單 byte 讀掃描
offset 0..5（迴圈上界 `5 < type_iter`），把每個 allowed type 比對目標 item 的 `type` 欄
（`item_effect` struct +1，`fd2_get_item_effect_entry` 回傳指標指到的分類 byte）。命中即可
裝備。買 / 裝備 / 使用道具路徑都經此檢查。

## entry sample

（type ID 對映的武器 / 防具類別見 `assets/items.md` 的 type 分類）

```
job 0x00 (龍) : FF FF FF FF FF FF 01   -> 無可裝備 type（龍不裝備）
job 0x01      : 01 15 16 FF FF FF 01   -> 允許 type {0x01, 0x15, 0x16}
job 0x02      : 04 15 16 17 18 FF 01   -> 允許 type {0x04, 0x15, 0x16, 0x17, 0x18}
```

## 全 29 rows

逐職業可裝備清單見 `assets/jobs.md`。
