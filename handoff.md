# Handoff — 角色成長數值表（growth_table）+ 成長 roll KB

> 給下一個 session 無縫接續。待辦在最上；其後為必讀文件、任務、網頁當前狀態、已驗證機制、
> 檔案地圖、操作流程與瀏覽器陷阱。本檔暫時性，任務全數收尾後刪除。

## 待辦 / 待決定（最優先）
- [PENDING] **尚未 commit**（使用者要求才做；直接 commit 到 main，不開分支）。本 session 改動檔案：
  `tools/growth_table/page_template.html`（大改；`gen_growth.py`/`build_page.py` 未動）、
  `assets/tables/character_growth.md`（+ 組語證據 + 「升級最大值修改版」off-by-one）、
  `program_info/battle.md`（升級節 + 1 行指標）、`assets/tables/_index.md`（描述更新）。
  另 session 起始已有未 commit 的前置工作（`CLAUDE.md`、`assets/characters.md`、
  `assets/tables/class_promotion.md`、`program_info/_index.md`、`program_info/known_bugs.md`、
  `tools/fd2_diff/`、`tests/play/scenarios/spell_probe.json`），commit 前需一併確認範圍。
- [DORMANT] **命中/迴避是否要獨立欄位**：目前折疊進 DX（未裝備＝DX，已加註）。使用者多輪未再提，暫緩。
- [NOTE] 先前發布過一個 stale Artifact（「炎龍騎士團 2 · 角色成長數值表」，
  https://claude.ai/code/artifact/297d5042-1dee-4157-a108-13883768562b）。使用者要刪；**Artifact 工具無刪除能力**，
  已請使用者自行於 claude.ai Artifacts 圖庫刪除。勿再發布新 Artifact（使用者不用 Artifact）。

## 必讀文件（依序）
1. `CLAUDE.md`、`index.md`（專案規範與 KB 結構）
2. 本檔
3. `assets/tables/character_growth.md`（成長 roll 語意、組語證據、修改版 off-by-one）、
   `program_info/known_bugs.md`（凱拉斯轉職 bug）
4. src ground truth：`src/battle/btl_init.c`（recruit/spawn）、`src/battle/btl_turn.c`（升級/roll）、
   `src/ui_menu/promote.c`（轉職）、`src/battle/battle.c`（`fd2_recalculate_combat_stats`）
5. `tools/growth_table/`（見下方檔案地圖）
6. memory：[[project_user_modded_maxgrowth_exe]]（使用者實機用修改版 exe，成長值每級 +1）

## 任務
把 FD2 每個可加入角色從初始等級到最高等級、每一級的 HP/MP/AP/DP/DX 做成網頁成長表：含「全程最小」
「全程最大」兩界限、轉職角色雙職業段、多轉職線（各列一版），並可多角色比較成長曲線。蓋亞/渥德不能
轉職但可升 LV99。資料流：四表 Ghidra dump → `raw_tables.json` → `gen_growth.py` →
`growth_data.json`＋`growth_compact.json` → `build_page.py` 注入模板 → `fd2_growth_tables.html`。
已跑 6-agent 對抗式驗證（四表 byte-exact、獨立重算 32 角色 0 誤差）；全量 JS==Python **23,700 值 0 不一致**。

## 網頁當前狀態（`page_template.html` → build → `fd2_growth_tables.html`）
三分頁，順序＝**屬性排名**（載入預設頁）、**成長曲線比較**、**角色明細**。整體 `.wrap` 1680px（利用 16:9）。已在使用者 Chrome 實測、0 console 錯誤。

### 共用圖表（`lineChart`）
- **分段生涯軸**：左半基礎職 LV、右半進階職 LV，中間「轉職」虛線（`splitAt=40`）。放置依**職業階級 tier**
  （`tierOf`）：可轉職角色基礎職＝base（左，x=lv）；promotion 與「以進階職入隊的不可轉職 LV40 單位」
  （莎拉/蘭斯洛特…）＝adv（右，x=40+lv）；蓋亞/渥德＝hero（不分段，1..99）。`+40` 右移只在該視圖同時有
  base 與 adv 時啟用（`viewDomain.splitActive`）。英雄與 promo 混選 X 撐 99、右半改標生涯累計級。
- **持平延伸**：英雄混選時，非英雄組合超過 career-80 後畫虛線水平延伸到 xmax；tooltip 也取該 build 末點值
  持平顯示（不加標記）。
- **Y 軸固定用最大成長範圍**（`viewDomain(...,"max")`）：切 min/max 只換線、Y 軸不重縮放。
- **配色** `--s1..--s8`＝`blue,yellow,magenta,red,violet,orange,aqua,green`（用 dataviz skill 8 色相暴力搜尋
  「正常視覺＋protan/deutan」前 N 槽最佳序；兩綠 aqua/green 在 slot 7/8，≤7 條不撞綠；相鄰 CVD ΔE
  light 25 / dark 23.7 PASS）。
- **legend**（`renderLegend`）：每筆文字用該線條顏色；band 圖標「實線＝全程最大　虛線＝全程最小」、line 圖
  標「實線＝<版本>」、有分段再標「轉職分隔」。比較頁名格式 `legendName`「索爾（劍聖）」。
- 相關函式：`buildTiered`/`seriesPts`/`placeX`/`viewDomain`/`lineChart`/`renderLegend`/`attachHover`/
  `drawSpark`/`drawCompare`。

### 版面（兩 tab 選擇區都在左側 sticky 側邊欄，右側放內容/圖表）
- **角色明細** `.detail-grid` grid `392px 1fr`；roster 內 `.rostercols` 把三類（可轉職／進階職業／LV99）排 3 欄；
  段落 badge 依 tier：base＝「基礎職業」、adv＝「轉職」、hero＝「進階職業」。
- **比較頁** `.cmp-layout` grid `440px 1fr`；`.buildgroups` 4 欄、cgroup 緊湊（副標省種族、chip 直排）；
  選取後 chip 職業文字上該線條色（`.pick.on .nm`）。控制列僅「清除所有選擇」；空狀態「請於左方…」。
  蓋亞/渥德 chip 文字＝「機兵」（無「（LV99）」；群組小 pill 保留）。
- **比較 picker sticky** 成立靠：把「推導依據」`.method` 在比較頁用 JS 搬進右欄 `.cmp-right`（明細頁
  `insertBefore` 移回 footer 前）、method 右欄用 2 欄排版（`.cmp-right .method-grid`）、圖表加高
  （`lineChart` H 上限 560），使右欄高度 > picker 才有停留空間。明細 roster 本靠長內容 sticky，未動。
- `≤900px` 兩者改上下堆疊。角色名完整顯示（無 ellipsis）。

### 比較頁選項＝「職業組合」
picker 依角色分組（`.cgroup`），可轉職→每個進階職一 chip、其餘→單一 chip；可同時選同角色不同進階職
（共用左半、右半分岔）。最多 8 條、依序上色。預設全程最大（`cmpVer="max"`；鈕文字「全程最大／最小數值成長」）。
`segTitle` 只回 `job_name`（悠妮召喚師線不再加「（召喚師線）」冗綴）。

### 屬性排名（第三分頁）
`#panel-rank`＝五欄（依屬性數量），每欄一項 HP/MP/AP/DP/DX，把所有「角色 × 職業組合」端點依該屬性
**最高等級最終值**排名為左對齊橫條圖。
- **端點粒度**＝`buildsOf(c)`（與比較頁一致）：可轉職角色每條轉職線各一條、承接基礎職 LV40；不可轉職角色
  用唯一路線。共 **47 條/欄**。標籤：`si===0`（基礎/唯一職）＝「名　職業」；`si>=1`（轉職線）＝「名→進階職」。
- **數值**＝`finalVal(cid,si,stat,ver)`＝`COMPUTED[cid][si].rows` 末列的 `max`/`min[stat]`（一般 LV40／蓋亞・渥德 LV99）。
  沿用已驗證的 `COMPUTED`，未新增資料；獨立 Python 交叉驗算（scratchpad `verify_rank.py`）五欄 TOP5 逐值相符。
- **每欄獨立控制**：`RANK_STATE[stat]={ver:"max"|"min", order:"desc"|"asc"}`，預設 max/desc；切換只重繪該欄
  （`.rcol[data-stat].outerHTML=rankColumn(stat)`，事件委派在 `#rankGrid`）。
- **橫條**：寬度＝`v/該欄目前 ver 的最大值`（正規化到欄最大，非名次）；色＝`STAT_COLOR`（hp→--s4 紅 / mp→--s1 藍 /
  ap→--s6 橙 / dp→--s7 綠 / dx→--s5 紫，用 CSS 變數故主題切換自動變色，排名頁不需重繪）；填色 `.rbar` opacity .5；
  名次＋名字寫在條上靠左（`.rlab`）、數值靠右（`.rval`），兩者用 `--bar-outline`（8 向實心描邊，色＝`var(--surface)`
  隨主題翻淺/深）在任何條色上都保持可讀。
- 相關函式／狀態：`STAT_COLOR`/`RANK_BUILDS`/`rankLabel`/`finalVal`/`RANK_STATE`/`rankColumn`/`buildRanking`。
  `selectTab` 已三分頁化（`isD/isC/isR`），method 只在比較頁搬進 `.cmp-right`、明細與排名頁回 `main.wrap`。

## 已驗證機制（勿重推）
- 初始等級 = `character_base[char_id]` 的 +2 欄（`fd2_init_runtime_char_from_base_growth` 用之；FDFIELD
  戰場 spawn 是另一條、不影響 roster 等級）。
- 登場：`HP/MP = base + min×(LV−1)`；`AP/DP/DX = base + min×LV`。
- 升級每級成長 ∈ `[min, max_byte−1]`（原版真實最大＝上界 byte − 1，組語+C 已驗證）。**使用者實機來自
  「升級最大值修改版」exe，每級誤給 `max_byte`（+1/級），故其實機值較高**——證據與此 off-by-one 已寫入
  `assets/tables/character_growth.md`；背景見 memory [[project_user_modded_maxgrowth_exe]]。工具刻意保持原版 `−1`。
- 轉職：等級歸 1、exp 歸 0、HP/MP 全滿，並立即 +1 次「新職業」成長；entry＝`growth_table[portrait_id]`；
  target＝`char_id+0x20`（default）/ `+0x32`（alt，需 `key_item[portrait]!=0xFF`）/ `0x34`（悠妮召喚師，需 0x5A）。
- 等級上限：portrait `0x1E`/`0x1F`（蓋亞/渥德）→99，其餘→40。
- 命中/迴避＝DX（未裝備；`fd2_recalculate_combat_stats`，裝備才加 HT/EV）；MV＝`character_base+7`，固定不隨
  等級、轉職 +bonus；EX 是經驗 carry 非屬性。
- 凱拉斯（char 16，龍劍士）誤落在可轉職段、排除清單漏了他 → 目標算成 slot `0x30`（殘留「聖戰士」錯配資料）
  → 幽靈轉職實機 crash。已排除該路線（見 `program_info/known_bugs.md`）。

## 檔案地圖
- `tools/growth_table/gen_growth.py` — 產生器（raw_tables.json → growth_data.json + growth_compact.json）
- `tools/growth_table/page_template.html` — 頁面模板（含 `/*__DATA__*/` 佔位；所有 CSS/JS 在此改）
- `tools/growth_table/build_page.py` — 注入 compact → 同時寫 `workspace/growth_table/fd2_growth_tables.html`（預覽）
  與 `docs/character-stat-comparison/fd2_growth_tables.html`（GitHub Pages 發佈檔，會被 commit；docs/ 另有 `.nojekyll`＋`README.md`）
- `tools/growth_table/spot_check.py` — 產生 `spot_check.md`（里程碑值人工對照）
- `tools/growth_table/verify_js_browser.js` — 瀏覽器全量比對測試（貼進載入頁的 javascript_tool 跑）
- `workspace/growth_table/raw_tables.json` — 四表 Ghidra dump（唯一資料來源，勿手改）
- `workspace/growth_table/{growth_data.json, growth_compact.json, fd2_growth_tables.html, spot_check.md}` — 產出

## 改一次資料/頁面的流程
1. 改 `gen_growth.py` 或 `page_template.html`
2. `python tools/growth_table/gen_growth.py`（重生 JSON；只改頁面可略）
3. `python tools/growth_table/build_page.py`（重建 HTML）
4. 瀏覽器重整（http server 讀檔即時，不必重起 server）

## 預覽 / 驅動瀏覽器（關鍵陷阱，務必先讀）
- 指令環境（Bash/PowerShell/python）與使用者本機同一台。
- **server 用 PowerShell Start-Process 起 host 端 python**、bind 127.0.0.1（背景 Bash 起的可能被隔離）：
  `Start-Process python -ArgumentList '-m','http.server','8765','--bind','127.0.0.1' -WorkingDirectory 'C:\Users\fdpsf\Documents\fd2-anatomy\workspace\growth_table' -WindowStyle Hidden`
  （server 不保證跨 session 存活，需重起。）
- **claude-in-chrome extension 可能連到「錯的」Chrome**：帳號下有多個瀏覽器。若導航 `127.0.0.1:8765` 得
  `ERR_CONNECTION_REFUSED`、但使用者手動連得到，用 `list_connected_browsers` → `AskUserQuestion` 列出 →
  `switch_browser` 請使用者在「連得到 8765 的那個 Chrome」按 Connect。**不是 sandbox 網路隔離**，只是連錯瀏覽器。
- 頁面**必須有 `<meta charset="utf-8">`**（模板最前面），否則中文亂碼。
- extension 的 `javascript_tool` 在**隔離世界**：讀不到頁面 JS 變數（COMPUTED 等）。驗證要自足（讀 DOM 內嵌
  `#growthData` JSON + `fetch('growth_data.json')`），用**頂層 await**、最後一行是結果物件（包成 async IIFE 會
  回 Promise、工具顯示 `{}`）。要測 sticky/座標可 dispatch `MouseEvent`、讀 `getBoundingClientRect`。
- `file://` 與 `data:` URL 被 extension 擋；一律用 http server。

## 如何重跑全量比對測試
1. 確認 server 在跑、頁面已在 Chrome 載入。
2. 把 `tools/growth_table/verify_js_browser.js` 內容貼進該分頁的 `javascript_tool` 執行。
3. 期望 `{... valuesChecked:23700, mismatches:0, verdict:"PASS-all-identical" ...}`。
