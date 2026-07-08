# data_fd2_battle_item_effect_table

`.object3 @ 0x602AC`，215 entries × 23 bytes = 4945 bytes。entry 0（短劍）前 6 byte
`0B 01 0A 00 5F 00` 可當定位 anchor。位址空間換算見 `assets/tables/_index.md`。

Ghidra type `item_entry[215]`；C struct `item_effect` 定義在 `src/include/types.h`。
Accessor `fd2_get_item_effect_entry(item_id)` 回傳 entry 指標。

## struct layout（23 B）

| offset | size | types.h 欄名 | 攻略縮寫 | 意義 |
|---|---|---|---|---|
| +0  | 1 | unknown_00 | — | prefix byte；entry 0 = 0x0B，其餘 entry = 0x00 |
| +1  | 1 | type | TY | 物品類型（裝備類 / 消耗品分類；見 `assets/items.md`）|
| +2  | 2 | ap | AP | 攻擊力增值（u16 LE）|
| +4  | 2 | ht | HT | 命中率增值 |
| +6  | 2 | dp | DP | 防禦力增值 |
| +8  | 2 | ev | EV | 速度 / 迴避增值 |
| +10 | 1 | special_type | S1 | 附加屬性（02=中毒 03=雙擊 04=暴擊）|
| +11 | 1 | special_chance | S2 | 附加屬性機率 % |
| +12 | 1 | range_min | R1 | 最小攻擊距離 |
| +13 | 1 | range_max | R2 | 最大攻擊距離 |
| +14 | 1 | use_effect | K1 | 使用效果碼 |
| +15 | 1 | use_param_lo | K2 | 使用參數 low（數量 / 法術 ID）|
| +16 | 1 | use_param_hi | K3 | 使用參數 high |
| +17 | 1 | cast_range_flags | K4 | 施放距離 flags（bit4 = 直線，低 4 位 = 距離）|
| +18 | 1 | target_side | K5 | 作用對象（00 = 敵，01 = 己）|
| +19 | 1 | area | K6 | 影響範圍（最大 3）|
| +20 | 2 | price | MM | 價格（u16 LE）；買價 = price，賣價 = price × 3 / 4 |
| +22 | 1 | trailing_22 | — | 尾端 byte（多為 0x05）|

`type(+1)` 是裝備限制的比對鍵：`fd2_check_job_can_equip_item @ 0x1C1C3` 拿它去比對
`job_allowed_items` 表的白名單（見 `job_allowed_items.md`）。`price(+0x14)` 是商店買 / 賣
流程讀的價格欄（`src/ui_menu/shop.c`）。

## entry sample

| entry | bytes（節錄）| 解析 |
|---|---|---|
| 00 短劍 | `0B 01 0A 00 5F 00 ...` | type=1 ap=10 ht=95 price=50 |
| 01 闊劍 | `00 01 14 00 5F 00 ...` | type=1 ap=20 ht=95 price=250 |

## 全 215 entries

數值見 `assets/items.md`。
