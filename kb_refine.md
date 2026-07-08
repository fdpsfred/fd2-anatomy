# KB 全面翻新 -- session 交接狀態

> 這是「根據可編譯可玩的 src/ ground truth 對整個 KB 做徹底 review 與翻新」這個長工作的進度交接。
> 下一個 session 從這裡接手。**目前進行到批次 2 的起點（批次 0、1 已完成）。**

## 立即下一步（批次 2）

program_info 改名重寫 + 新檔 + 套用批次 0 裁決 + 2 個 asm addendum。詳細待辦在
`workspace/kb_overhaul/batch2_handoff.md`（含 overview 的 entry-chain 稿、要套的裁決、addendum）。
執行方式沿用批次 1 的模式（見下方「批次執行模式」）。

## 必讀文件（依順序）

1. `CLAUDE.md` — 專案工作規範（工具、scripts 規範、Ghidra 規範、KB 寫作鐵則）
2. auto-memory `MEMORY.md` — 特別是 per-item 逐一處理、no-surface-verify、asm-review、no-hardcoded-count、commit-to-main、繁中白話等 feedback
3. `workspace/kb_overhaul/proposal.md` — 已核可的新結構總提案（資料夾樹、對映、裁決清單、批次計畫）
4. `workspace/kb_overhaul/adjudications_summary.md` — 批次 0 的 14 項裁決結論（要套進 KB 的修正）
5. `workspace/kb_overhaul/side_findings.md` — 8 處 src 註解漂移（asm 已確認）+ 3 處 Ghidra plate 漂移 + 2 個新發現命名錯
6. `workspace/kb_overhaul/batch1_spec.md` — 批次 1 的共用規格（寫作鐵則、正典歸屬表、計數政策、workspace 改寫對照）；批次 2+ 沿用同一套鐵則
7. 不依賴：`legacy/`（凍結過時）、根目錄三個舊 handoff（debug_/src_refine_/test_task_，批次 6 刪）

## 已核可的決策（使用者「全部許可」，不必再問）

1. chapters 兩套合併為頂層 `chapters/`（60 檔→30 檔）
2. program_info 檔名改用 src 模組名（dialog/gfx/anim/rsrc/save/field/table…）
3. emission/ → equivalence/（已於批次 1 完成）
4. workspace 引用規則折衷：KB 禁引 workspace 作事實出處；工具輸出目的地寫在 tools/*/_index.md
5. 三個 root handoff 刪除（耐久事實先落 KB/tools 再刪；debug_handoff 的 86Box 待辦先移 open_issues）
6. matched_function_sources.md 說明改繁中需連動改產生器 script（批次 6）

## 批次進度

| 批次 | 內容 | 狀態 |
|------|------|------|
| 0 | 14 項矛盾裁決 + 8 處 src 註解漂移 asm 定案 + FUN_ 對照 | ✅ 完成（唯讀，無 commit；artifacts 在 workspace/kb_overhaul/）|
| 1 | rebuild_info 重組（emission→equivalence、正典化、計數清理、4 新檔）| ✅ 完成，commit `32380cdf` |
| 2 | program_info 改名+重寫+新檔（spell/pathfind/town_menu/util）+ 裁決套用 + asm addendum | ⏳ 下一步 |
| 3 | resource_info 修繕（codecs/fd2_tmp 新增、tai/fdicon/save_format 收斂）| pending |
| 4 | chapters 合併 60→30（頂層 chapters/）| pending |
| 5 | assets 重寫（tables 橋接層、數值正典、新表文件、FDFIELD 產出灌入）| pending |
| 6 | root/tools/tests 索引 + handoff 收尾 + open_issues 純化 + 產生器繁中 | pending |
| 7 | 全面逐條機械驗證（claim ledger，零抽樣）| pending |

（TaskCreate 看板 id 1-8 對應批次 0-7；批次 0=id1、批次 1=id2…批次 7=id8。）

## 批次執行模式（批次 1 已驗證有效，沿用）

1. 蒐集該批 ground truth（現況檔清單、Ghidra 即時數值、workspace/stale 引用位置）。
2. 寫該批共用 spec 到 `workspace/kb_overhaul/batchN_spec.md`（正典歸屬、寫作鐵則、要套的裁決）。
3. Workflow 平行改寫：**每個 agent 負責一組不重疊的檔案**（單一 owner，不衝突），直接 Read/Write/Edit repo；
   每個 agent 配一個 adversarial reviewer（實際 Read 成品逐條檢查：流水帳/workspace 出處/stale 計數/stale
   路徑/正典引用/與 src-Ghidra 忠實度）。schema 回傳 files_written + issues。
4. 我（主 session）彙整 reviewer issues，**逐一親自核對 ground truth 後修**（不照單全收，reviewer 也會誤判
   中間態）；重要數值一律當場 Ghidra/src 實查。
5. 結構搬移用 `git mv` 保留歷史。
6. grep gate：該批資料夾內零 stale 路徑/計數/workspace 出處。
7. commit 到 main（直接 main，不開分支）；staged 範圍嚴格限該批，`git add -A` 後要 reset 掉不屬本批的
   既有 untracked 檔（debug_handoff.md、tools/fd2_diff/、tests/play/scenarios/spell_probe.json 是批次 6/無關，別 commit）。
8. 每批完成 hard-stop，向使用者回報統計 + 邊界案例後再進下一批（使用者偏好，見 memory）。

要點：workspace/ 是 gitignored，只放中繼產物與 spec；KB/tool/_index 禁引 workspace path 作事實出處。
Bash stdout 是 cp950 會把中文顯示成亂碼，驗中文內容用 Read tool 或 python `U+FFFD count==0`。

## 批次 0 裁決結論摘要（14 項全 confirmed，adversarial 0 推翻；詳見 adjudications_summary.md）

要套進 KB 的修正（批次 2-5 對應系統改寫時套用）：

- **ch01 哈瓦特 offset**：program_info/chapters/chapter_01 的來源端 `+0x94/95/96` 改 `+0x11/12/13`（Ghidra 摺疊 record base +0x83 造成，assets 版對）。目的端 pCombat_aux_block[0xD/E/F] 正確。
- **ch22/26/27/28 char[1]**：ch22=希爾法、ch27/28=悠妮（對）；**ch26 修正 chars[1]=亞奇梅吉、chars[2]=悠妮**（連 src/field/chinit.c:1464 註解也反了）。身分由編成畫面 pin 表決定，非 init handler。
- **FDFIELD header 樣板**：30 章檔的「+51 起 char_spawn_records」全錯；正確 +0x33 tile-step hooks、+0x53 pickup、char_spawn_records 從 **+0x83（131）**起。批次 4 合併 chapters 時整段刪除改引 resource_info/fdfield.md。tools/decoders/fdfield_event_decoder.py:15 docstring 同錯。
- **gfx 0x4DFCC**：是 `fd2_update_palette_cycle_anim`（非「解壓 snapshot」）；graphics.md（→gfx.md）:88-96 整段改七步正確版（詳見 adjudications_summary #4），guard 名 `skip_palette_cycle`。
- **enemy +8 = MV 移動力**（非 magic_resist）；魔抗另查 per-job 表 0x51F96。
- **growth max = exclusive**（實際最大成長 = byte-1；byte==min 固定成長 min）；characters.md 的範圍呈現要換算。
- **chapter_intro bCategory** = intro 外觀變體碼 0/1/2（非 story/battle 旗標；後者查 0x526B9）。
- **敵人 stats × level**：表值全是每等級係數，出場 = 係數 × level（EX 同理，物理路徑再 ÷ 攻擊者 level）。
- **ch20 邊界**正確（chend1=1-19、chend2=20-30）；**ch01 phase** 改採 src Phase A-D 四段，修 Phase D 的 init 順序（先 init chars 再 init_battle_state）。
- **FDTXT entry 32/33** = 第 1 章序章對話（非結局！）；entry 31 = 結局 epilogue，載入呼叫是 `fd2_load_chapter_battle_data(0x1E)`。endgame_text.md 檔名誤導。
- **TAI.DAT** = cinematic 前景 sprite 覆蓋層（與 BG.DAT 配對）；「AI 參數」「palette」兩假說證偽。
- **ch15 凱麗**：「ch12 加入的劍士」全錯 → char_id 0xC、**ch7 章末加入的武者**。
- **譯名**：char28 正名**達克塞**（KB 三種寫法達可塞/達可賽/達克賽全錯）；char15=塞可邦勒、char29=亞齊梅吉（名表正名）；對白 transcript 保留原文（賽/奇變體）不改。批次 5 建 assets/names.md。
- **28 個 FUN_ 字樣**：Ghidra 端全部早已 canonical 命名，純 KB stale 引用，改寫時直接替換（對照見 workspace/kb_overhaul/funmap.md）。另 battle.md:219 `0x1F183` 是 `fd2_check_char_status_immunity`（非 LoS）；battle.md:221/225 `0x15DA2` 呼叫漏 len 引數（實簽名 `(len, char_idx_arr, field_offset, weight)`）。

## side_findings：非 KB 的 ground-truth 修正（asm 已確認，批次 2 對應系統改寫時同步；詳見 side_findings.md）

**8 處 src 註解漂移（改註解不影響 binary，全 asm 級 CONFIRMED）**：
- table.c:40 `MV(=magic_resist for enemies)` → MV 移動力
- rndstat.c:188/251 面板 magic resist → MV；rndstat.c:187 `movement_order(MV)` → **EX 經驗餘額**（面板兩格標籤其實對調：LV/EX/MV = +0x21/+0x3C/+0x3B）
- btltab3.c:372 + promote.c:329-332 promotion byte[1] `learned-spell` → 移動力加成（加到 +0x3B，text page 0x254）
- btltab3.c:809 growth `inclusive max` → exclusive
- chtab3.c:29-31 intro lut `table[metadata[0]-2]` Cat2/3/4 → `table[bCategory]` Cat0/1/2（-2 是 decompiler 假象）
- chinit.c:1464 ch26 lose `悠妮/亞奇梅吉` → 順序對調 `亞奇梅吉/悠妮`

**3 處 Ghidra plate 漂移（需可寫 session；批次 2 對應改寫時同步）**：
- `fd2_init_runtime_char_for_battle @ 0x10C50` plate 欄名 `bMagic_resist(+0x3B)` → 移動力
- `fd2_chapter_transition_menu @ 0x2CAD7` plate `[metadata[0]-2]` Cat2/3/4 → `[bCategory]` Cat0/1/2
- `fd2_chapter_01_init @ 0x3231B` plate 頭「3 階段」→ Phase A-D 四段

**批次 2 ADDENDUM（使用者指定，2 個新發現的 src 命名錯，要 asm 定案並修）**：
- `src/include/types.h:35` 欄位名 `movement_order` + 註解「0=moved 0xFF=not_moved」是誤稱（+0x3C 實為 EX 經驗餘額；真正 acted flag 在 flags(+0x05) bit7，此子項待補一次 asm 確認）
- `src/battle/btl_aisc.c:510` `mp_remaining = pCaster[0x3B]` 疑誤標（+0x3B 是移動力預算，追 0x15A60 資料流定案，對照 btl_aitg.c:430 已正確 `range_rem`）

## 尚未收尾、批次 6 要處理的殘留

- 跨 repo emission/ 路徑：`open_issues.md`、`tools/code_emit/data/routing.md` 仍引 emission/（routing.md 要 content-aware remap：`pipeline_spec.md 模式A rule A-1`→`equivalence/rules.md` fall-through 模式；`calling_convention.md §Decompiler fragments`→`equivalence/watcom_abi.md` epilogue 清單）。
- matched_function_sources.md 的逐列(194)+彙總表待批次 6 產生器重生（正確彙總：CLIB3S 135/159/143/130、MATH387S 33/34/30/29、MATH387R 6/6/4/4、MATH3S 5/5/5/5、MATH3R 3/3/3/3、EMU387 1/1/0/0）；產生器目前不存在需新建並輸出繁中。
- rebuild_info/src_map.md 待批次 2 建好 spell/pathfind/util/town_menu 後回填對應欄。
- 既有未追蹤檔：`debug_handoff.md`、`tools/fd2_diff/`、`tests/play/scenarios/spell_probe.json`（批次 6 / 無關，勿混進其他批 commit）。

## workspace/kb_overhaul/ 檔案清單（gitignored，同機持久，是詳細證據來源）

- `proposal.md` — 核可的新結構總提案
- `adjudications.json` / `adjudications_summary.md` — 批次 0 的 14 項裁決（含 asm 證據）
- `asmverify.json` / `asmverify_summary.md` — 8 處 src 註解的 asm 級交叉比對
- `side_findings.md` — 非 KB 的 ground-truth 修正清單
- `funmap.md` — 28 個 FUN_ ↔ canonical 名對照
- `batch1_spec.md` / `batch1_results.json` / `batch1_issues.md` — 批次 1 規格與 review
- `batch2_handoff.md` — 批次 2 的 entry-chain 稿與待辦
