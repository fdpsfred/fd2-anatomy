# assets/

從 FD2.LE binary 與資源檔解析出的遊戲內容。資料分三層：可讀數值正典（玩家視角的角色 /
道具 / 法術 / 敵人 / 職業數值）、橋接層資料表（binary 位址 ↔ struct 欄名 ↔ 攻略縮寫），
以及章節與文字。

## 數值正典（可讀）

角色 / 敵 / 道具 / 法術 / 職業的 readable 數值唯一寫在這裡，`tables/` 只放 struct 並引用之。

- `characters.md` — 32 角色：id ↔ 中文名、出場屬性、升級成長、法術習得
- `items.md` — 215 道具（0x00–0xD6）效果一覽
- `spells.md` — 36 法術（攻擊 / 劍技 / 恢復 / 輔助 / 召喚）
- `enemies.md` — 68 敵 / 友軍 unit 屬性（HP/MP/AP/DP/DX/EX 為每等級係數）
- `jobs.md` — 27 職業：魔抗 / 暴擊率 / 轉職物品
- `names.md` — 角色譯名異名收斂（FDTXT 名表正名 ↔ 對白變體 ↔ 攻略誤植 ↔ 各 KB 用字）
- `races.md` — race_id ↔ 種族名稱

## tables/ — 橋接層資料表

binary data table（`.object2` / `.object3`）的 struct layout：把 binary 位址對應到 `src/include/types.h`
的欄名與攻略縮寫，只放 struct / 位址 / 邊界，數值內容一律引用上面的正典檔。涵蓋角色出場屬性、
升級成長、道具、法術、敵人、法術習得、章 intro metadata、職業暴擊率與魔抗，以及地形移動花費
（`movement_cost.md`）、職業可裝備（`job_allowed_items.md`）、轉職資料（`class_promotion.md`）、
story/battle 章別分派（`chapter_category.md`）。逐表位址與欄位對照見 `tables/_index.md`。

## chapters/ — 章節

每章一份檔案，含章名、加入角色、敵人配置、寶物、特殊機制與完整對話內容。跨章機制
（Good/Bad ending fork、conditional recruit 矩陣、reward 鏈）與 30 章對照總表見
`chapters/_index.md`。

## text/ — FDTXT.DAT 文字

entry 0 全遊戲共用文字庫、結局與序章文字、glyph lookup table。清單見 `text/_index.md`。

## 與 program / resource 文件的關係

`assets/` 是「玩家視角」的遊戲內容；`program_info/` 是「程式視角」的執行邏輯；
`resource_info/` 是「檔案視角」的格式說明。
