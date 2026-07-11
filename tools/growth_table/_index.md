# tools/growth_table/

由 FD2.LE 的角色基礎表與成長表推導「每位可加入角色從初始等級到最高等級、每一級的
HP/MP/AP/DP/DX」，產生一個自足的互動網頁（角色屬性數值比較），發佈於 GitHub Pages。

## 產物

- 本機預覽：`workspace/growth_table/fd2_growth_tables.html`（gitignored）。
- 發佈檔：`docs/character-stat-comparison/fd2_growth_tables.html`（committed；GitHub Pages
  來源 `/docs`）。兩份由 `build_page.py` 同一次建置雙輸出、逐 byte 相同。

## 資料流

```
四張 Ghidra MCP dump 表 → workspace/growth_table/raw_tables.json
  → gen_growth.py → growth_data.json（完整逐級展開）＋ growth_compact.json（頁面內嵌精簡版）
  → build_page.py（注入 page_template.html 的 /*__DATA__*/ 佔位）
  → fd2_growth_tables.html（workspace 預覽 ＋ docs 發佈，雙輸出）
```

`raw_tables.json` 是唯一資料來源，含四張表的原始 hex／stride／count：`character_base_table`、
`character_growth_table`、`class_promotion_table`、`portrait_class_change_key_item_table`。勿手改；
要更新從 Ghidra 重 dump 這四表。

## 檔案

| 檔 | 用途 |
|---|---|
| `gen_growth.py` | `raw_tables.json` → `growth_data.json` + `growth_compact.json`（套用出場／升級／轉職公式）|
| `page_template.html` | 頁面模板（含 `/*__DATA__*/` 佔位；所有 CSS/JS 都在此改）|
| `build_page.py` | 注入 compact JSON、雙輸出 HTML（workspace 預覽 + docs 發佈）|
| `spot_check.py` | 產生里程碑值人工對照 `spot_check.md` |
| `verify_js_browser.js` | 瀏覽器端全量比對測試（貼進載入頁的 console 執行）|

## 網頁設計

自足單檔（資料／CSS／JS 全內嵌、零外部引用、字型用系統字），`.wrap` 最大 1680px。淺／深主題可切
（`data-theme`），categorical 配色用 dataviz 驗證過的 8 色相 palette（`--s1..--s8`，相鄰色在
protan/deutan 下 CVD ΔE 通過）。三分頁，順序＝**屬性排名**（載入預設）、**成長曲線比較**、**角色明細**。

### 共用折線圖（`lineChart`）

分段生涯軸：左半基礎職 LV、右半進階職 LV，中間「轉職」虛線（`splitAt=40`）。放置依職業階級 tier
（`tierOf`）：可轉職角色基礎職＝base（左，x=lv）；轉職段與「以進階職入隊的不可轉職 LV40 單位」
（莎拉、蘭斯洛特…）＝adv（右，x=40+lv）；蓋亞／渥德＝hero（不分段，1..99）。`+40` 右移只在該視圖同時
有 base 與 adv 時啟用。Y 軸固定用最大成長範圍，切 min/max 只換線、不重縮放。相關函式：
`buildTiered`／`seriesPts`／`placeX`／`viewDomain`／`lineChart`／`renderLegend`／`attachHover`／
`drawSpark`／`drawCompare`。

### 角色明細

左側 sticky roster（`.rostercols` 三欄：可轉職／進階職業／LV99），右側 id card ＋ 每屬性 spark 折線
＋ 各職業段的逐級成長表（每格 min–max）。

### 成長曲線比較

左側 sticky picker（`.buildgroups` 依角色分組，每個「職業組合」一個 chip），右側折線圖。最多同時 8 條、
依序上色，可同時選同角色不同進階職（共用左半、右半分岔）。預設全程最大。「推導依據」區塊在此頁用 JS
搬進右欄以撐高，讓左側 picker 得以 sticky。

### 屬性排名

五欄（＝屬性數量）HP/MP/AP/DP/DX，把所有「角色 × 職業組合」端點依該屬性在各自最高等級的**最終值**排成
左對齊橫條圖（一般 LV40、蓋亞／渥德 LV99；共 47 條／欄，端點粒度＝比較頁的 `buildsOf`）。每欄可獨立切換
最大／最小成長與升／降冪（`RANK_STATE[stat]`）；橫條寬度正規化到該欄目前最大值（依數值大小、非名次）。
每屬性一色，名次＋名字寫在條上靠左、數值靠右，兩者用 `--bar-outline`（隨主題翻色的 8 向實心描邊）在任何
條色上都保持可讀。相關函式：`RANK_BUILDS`／`finalVal`／`rankColumn`／`buildRanking`。

## 建置與本機預覽

1. 改 `gen_growth.py` 或 `page_template.html`。
2. `python tools/growth_table/gen_growth.py`（重生 JSON；只改頁面可略）。
3. `python tools/growth_table/build_page.py`（重建 HTML，同時更新 workspace 預覽與 docs 發佈檔）。
4. 起 http server（`file://` 會被瀏覽器 extension 擋）：cwd 設 `workspace/growth_table/`、
   `python -m http.server 8765 --bind 127.0.0.1`，開 `127.0.0.1:8765/fd2_growth_tables.html`，改檔重整即可。

## 驗證

- 產生器對四表 byte-exact、獨立重算 32 角色 0 誤差。
- 全量 JS==Python：把 `verify_js_browser.js` 貼進載入頁的 console 執行，期望
  `valuesChecked:23700, mismatches:0`。頁面 JS 變數在瀏覽器 isolated world 讀不到，故驗證腳本自足讀
  內嵌的 `#growthData` JSON。

## 資料依據（機制不在此重述）

頁面套用的公式與機制已在 KB，本工具只引用：

- 成長 roll（`_max` 為 exclusive 上界、原版最大成長＝`_max−1`）與「升級最大值修改版」exe 每級 +1 的
  off-by-one：`assets/tables/character_growth.md`。
- 出場公式（HP/MP＝base+min×(LV−1)、AP/DP/DX＝base+min×LV）、升級逐級 roll、命中／迴避由 DX 導出、
  MV 來源：`program_info/battle.md`。
- 轉職目標 slot（portrait+0x20 default／+0x32 alt 需 key item／0x34 悠妮召喚師）與資格：
  `assets/tables/class_promotion.md`。
- 凱拉斯（char 16）幽靈轉職「聖戰士」實機 crash、排除為有效路線：`program_info/known_bugs.md`。
- 逐角色數值與升級範圍：`assets/characters.md`。
- src ground truth：`src/battle/btl_init.c`（recruit/spawn）、`src/battle/btl_turn.c`（升級／roll）、
  `src/ui_menu/promote.c`（轉職）、`src/battle/battle.c`（`fd2_recalculate_combat_stats`）。

## 補充機制（工具推導用，KB 他處未單列）

- 招募進隊的**初始等級**＝`character_base[char_id]` 的 +2 欄（recruit 路徑
  `fd2_init_runtime_char_from_base_growth` 用之）；此與 FDFIELD 戰場 spawn 是兩條路，不影響 roster 等級。
- **轉職第二段成長**：等級歸 1、屬性沿用基礎職 LV40 結果、立即獲得一次新職業成長，再 LV1..40。成長 entry
  ＝`growth_table[portrait_id]`（基礎職 portrait==char_id）。
- **等級上限**：portrait `0x1E`／`0x1F`（蓋亞／渥德）→ 99，其餘 → 40。

## GitHub Pages 發佈

`docs/` 是 Pages 發佈目錄（不是專案文件；文件在 repo 根的 `assets/`、`chapters/`、`*_info/`）。
`Settings → Pages` → Deploy from a branch → `main` 的 `/docs`；發佈網址
`https://<帳號>.github.io/<repo>/character-stat-comparison/fd2_growth_tables.html`。`docs/.nojekyll` 停用
Jekyll，讓自足 HTML 原樣送出。詳 `docs/README.md`。
