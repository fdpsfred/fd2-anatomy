# FD2 測試重寫 — 交接文件

`tests/` 從「1758 個 per-function spy 單元測試」完全重寫成「決定論 playthrough 整合測試系統」。
本檔讓新 session 零 context 接續。**最新測試里程碑 commit:`9dc4dc3`(P3-E 戰鬥傷害 oracle)**。
未追蹤檔 `.claude/skills/anthropic_agent_sdk/` 與本任務無關,別提交。

---

## 0. 下一步該做什麼(P3-E 傷害 oracle 已完成,等你選下一步)

**已完成(本批次):P3-E 戰鬥傷害 oracle**。`combat_attack` scenario 驅動玩家 idx0 攻擊敵人
idx14,固定 seed 0x1234,`tools/fd2_play/expect.py` 用同一 LFSR + 物理公式預算傷害(此 seed
觸發爆擊 = 12)並斷言 idx14 `hp_current` 減少量。決定論雙跑 byte-identical、blessed、`run_all`
5/5(oracle 自動納入)。expect.py 三方驗證(手算 LFSR / 實機 / port 皆得 12),且 seed 敏感、
雙擊/爆擊路徑皆測過,靜態表即時從 `FD2.LE` 解析(不硬編)。

**下一步選項(等你決定)**:
- (a) **擴充戰鬥 oracle**(最自然的延續):依 `combat_attack` 範式加更多攻擊者/武器/地形、
  **法術傷害**(`fd2_calc_magic_damage`)、**12 種 AI behavior class**、命中/狀態效果。
- (b) **建母本鏈**:從 ch1 連續通關逐章擷取一致 entry SAV,當跨章期望值母本,順帶解 §3 的
  ch7/ch22 open issue。這是 P4(招募/兌換/結局/商店)的前置。
- (c) **P4 分歧 + 村莊**:招募/兌換/結局 fork + 商店金額/save round-trip 的狀態斷言。

範式已備齊:驅動 UI 看 `combat_attack`/`atk_probe`、狀態斷言看 `st_dump.py`、公式 oracle 看
`expect.py`。

## 0a. 注意:expect.py 的 RNG 對齊(踩過的雷)

玩家攻擊的 RNG 序列從 seed 起算是 **雙擊(1) → 命中(1) → [命中]爆擊(1) → [命中]jitter(1)**:
- cinematic(`fd2_execute_combat_hit_cinematic`)在傷害計算「前」先抽 1 次做 3% 雙擊判定;
  intro-zoom 不抽 RNG(實證:commit 後首個擷取 seed 未變)。
- 玩家攻擊走 `fd2_calculate_combat_hit_outcome`(算 outcome,cinematic 跨 frame 套用 HP),
  **不是** `fd2_execute_attack_damage_calculation`(後者自己寫 HP,1 caller 非玩家路徑)。
- 小 AP/DP 時地形修正整除歸零 → 傷害與地形無關(expect.py 會檢查並在可能非零時擋下)。
- weapon `item_effect.special_type`(struct +10)決定爆擊/毒/雙擊類;`range_min`(+12)==1 才
  能反擊。注意 `fd2_get_item_effect_entry` 回傳 `&entry.type`(+1),故 battle.c 的
  `weapon_entry[9]` = struct +10。

---

## 1. 必讀文件(依順序)

1. `CLAUDE.md`、`index.md` — 專案規範與知識庫結構
2. 計畫全文:`C:\Users\fdpsf\.claude\plans\src-code-fd2-exe-tests-deep-hamming.md`(7-phase roadmap、
   oracle 策略、內容覆蓋地圖、舊測試遷移判準)
3. memory:`project_fd2_play_test_system`、`feedback_no_workaround_root_cause_only`、
   `feedback_const_data_never_demote_for_tests`、`feedback_script_review_then_test`、
   `feedback_no_unverified_claim`、`feedback_long_workflow_checkpoints`、`feedback_chinese_prose_readability`
4. 本系統索引:`tests/play/_index.md`(含各 phase 狀態表)、`tools/fd2_play/_index.md`(工具 + 決定論要點)
5. 期望值來源:`program_info/battle.md`(傷害/命中/AI 公式)、`assets/chapters/_index.md`(招募/兌換/
   結局矩陣)、`assets/tables/`、`resource_info/save_format.md`(存檔 layout)、`fd2-knowledge` skill
   (`python .claude/skills/fd2-knowledge/query.py formula|item|spell|char|enemy`)
6. 待解問題:`open_issues.md`(最上面有「測試系統」段)

---

## 2. 怎麼操作(指令速查)

```
# 改了 src/ 或 tests/play/ 後重建測試 EXE(前景跑,別背景)
python tools/fd2_play/build_replay.py

# 跑單一 scenario(輸出到 workspace/fd2_play/run/<out>/)
python tools/fd2_play/run_play.py --scenario <name> [--out <dir>]

# 把擷取的畫面還原成可看的 PNG(編寫腳本時必用)
python tools/fd2_play/fb2png.py --scenario <name> [--out <dir>]

# 第一次凍結 golden;之後 regression 比對
python tools/fd2_play/compare.py --scenario <name> [--out <dir>] --bless
python tools/fd2_play/compare.py --scenario <name> [--out <dir>]

# determinism 雙跑:同 scenario 跑兩個不同 --out,再 --against 比對
python tools/fd2_play/compare.py --scenario <name> --out A --against <full path to run/B>

# 跑整個 regression 套件(只跑有 golden 者)
python tools/fd2_play/run_all.py

# 編正式遊戲(不帶測試開關;驗證生產純度)
python tools/fd2_build/build_fd2.py
```

工具確認(每個新 session 起手依 CLAUDE.md 先確認):Ghidra MCP 已開 FD2.LE、`dosbox-x` 在 PATH
(`C:\DOSBox-X\dosbox-x.exe`)、Watcom 9.5a 在 `C:\Users\fdpsf\Documents\WATCOM_9.5a\BIN`、python 可用。

---

## 3. 待解問題

### chapter 7 / chapter 22(0-based)save-jump 載入 hang [P2]

- 詳見 `open_issues.md`。P2 sweep 30 章中 28 章 headless 載入乾淨,ch7/ch22 hang 在
  `fd2_load_save_and_init_engine` 載入期間(heartbeat 停在載入確認鍵後、0 dump、無 GP fault)。
- 極可能是 `gen_scenario.py` 的 jump-save 只改 chapter_id、其餘仍是第 1 章資料(地圖快照/
  tile-event/單位)的不一致 artifact,**非重建 bug**(28/30 佐證),但未驗證前不下定論。
- 三條解法(任一即可分類/解決):
  (a) 母本鏈:用一致的逐章 entry SAV 重測;
  (b) 原版差分:同一 jump-save 餵原版 `~FD2.EXE`,若亦 hang 即確認為 artifact;
  (c) **INITCH replay 命令**(最省事):在 `replay.c` 加一個命令,模仿 NEW GAME(`main.c:191-203`)
      直接設 `data_fd2_chapter_current_chapter_id=N`、`data_fd2_shared_menu_party_member_count=0`、
      呼叫 `data_fd2_chapter_init_handler_table[N]()` 做 fresh init,繞過存檔不一致,給 30 章乾淨
      的 P2 init 覆蓋。

---

## 4. 目前進度(各 phase 機制驗證狀態)

引擎與每個核心機制都已驗證並 commit;其餘為依既有範式擴充內容的工作量。

| Phase 機制 | 狀態 | 證據 / blessed scenario |
| --- | --- | --- |
| P0 引擎(注入/擷取/比對/決定論/生產純度) | ✅ | `boot`;雙跑 byte-identical;鑑別力測到差異;FD2.EXE byte-identical |
| P1 NEW GAME → 章節 | ✅ | `ch01_intro`(第一章 prologue) |
| P2 任意章 init/render(save-jump) | ✅ 機制 | `ch05_jump`;sweep 28/30(ch7/ch22 見 §3) |
| P3 戰鬥操作(游標/選取/動作選單) | ✅ 機制 | `combat_move`(游標座標斷言) |
| P4 存檔載入(CONTINUE) | ✅ | `continue_load` |
| P3-E 傷害數值斷言(host LFSR 預算) | ✅ | `combat_attack`(物理攻擊 oracle,固定 seed 預算定值;`expect.py`) |
| P3-E 其餘(12 class AI / 法術傷害 / 命中 / 狀態) | ⬜ | 依 `combat_attack` 範式擴充 |
| P4 招募/兌換/結局/商店/options | ⬜ | 需到達章節點 + 狀態斷言 |
| P5 cinematic golden + 原版差分背書 | ⬜ | dialog/FIGANI/spell/ending;`diff_original.py` 未建 |
| P6 舊 spy 測試退役 + 文件收斂 | ✅ 退役 | per-function spy suite(70 檔)搬 `legacy/tests_unit_spy/`(gitignored 凍結副本);保留凍結 logic net `battle/btlrng.c`(rng)/`save/savecsum.c`(checksum+crypt)/`table/table.c`(accessor),`build_test` 29/29;`tests/_index.md` 重寫成兩層。spy 失效根因=coordinated-landing(#32/#33),整合測試取代覆蓋 |

`run_all` 目前 **5/5 PASS**(boot、ch01_intro、combat_move、continue_load、combat_attack
[含 oracle]),連跑穩定。

---

## 5. Code 現況

### 開關
測試碼全部由編譯參數 **`-DFD2_REPLAY`** 控制。`build_fd2.py`(正式)不帶 → 測試碼全消失,
`FD2.EXE` SHA256 = `AB5F110AF02608462A1DA464732C098EF4DC8E17E6DC43252B535ED1CBF3BD9B`(349189 bytes,
加 hook 前後相同,已驗)。`build_replay.py`(測試)帶 → 啟用 + 連入 `tests/play/*.c` → `FD2RP.EXE`。

### src/ 內的掛鉤(只有小掛鉤,全 `#ifdef FD2_REPLAY`)
| 檔案 | 內容 |
| --- | --- |
| `src/life/main.c` | warmup 固定為 0;`fd2_replay_init()` 呼叫 |
| `src/input/input.c` | `fd2_check_keyboard_buffer_nonempty()` 內呼叫 `fd2_replay_pump()` |
| `src/anim/aniend.c` | 主選單迴圈讀鍵前呼叫 `fd2_replay_pump()` |
| `src/save/save.c` | 存檔選單讀鍵前呼叫 `fd2_replay_pump()` |
| `src/include/protos.h` | `fd2_replay_init/pump` 宣告 |
| `src/include/consts.h` | `BIOS_TICK_*` 改成虛擬時鐘巨集(見 §6) |

### 測試主體(在 src/ 外,只進測試 EXE)
- `tests/play/replay.c` — 讀 `SCRIPT.TXT`、注入 BIOS 鍵盤環、虛擬時鐘 `fd2_replay_tick()`、
  `CAP`/`SEED`/`END`、寫 `DONE.TXT`/`HB.TXT`
- `tests/play/capture.c` — dump `FBnn.BIN`(framebuffer 64000B)+ `PALnn.BIN`(DAC 768B)+
  `STnn.BIN`(狀態)
- `tests/play/playharn.h`、`tests/play/scenarios/*.json`、`tests/play/golden/<name>/`

### host 工具(`tools/fd2_play/`)
`build_replay.py`、`run_play.py`、`compare.py`、`fb2png.py`、`run_all.py`、`gen_scenario.py`、
`sweep_chapters.py`、`expect.py`(戰鬥傷害 oracle)、`st_dump.py`(ST blob 解讀)、`_index.md`。
帶 `oracle` 區塊的 scenario 由 `run_all` 在 golden 比對外自動跑 `expect.py`。

### SCRIPT.TXT 命令格式
`SEED <hex16>` / `KEY <hexSc> [hexAsc]` / `CAP` / `END`(EOF 等同 END;`#` 註解)。

### STnn.BIN 格式(host 比對/斷言用)
開頭 16 個 little-endian int32,順序:
`[0]chapter_id [1]event_flag [2]turn [3]party_count [4]gold [5]cursor_world_x [6]cursor_world_y
[7]cursor_screen_x [8]cursor_screen_y [9]view_origin_x [10]view_origin_y [11]rng_seed
[12]ai_phys_score [13]ai_spell_score [14]ai_item_score [15]reserved`
之後接 `party_count × 0x50` bytes 的 runtime_char 陣列(layout 見 `src/include/types.h`:
`hp_current`@+0x40、`hp_max`@+0x42、`mp_current`@+0x44、`ap`@+0x48、`dp`@+0x4A …)。

---

## 6. 重要踩雷與設計決策(沒讀會卡住)

- **選單/對話「確認鍵」看 ASCII 不是 scancode**(`aniend.c:302`:`last_key_pressed=='\r'||' '`)。
  確認要送 `KEY 1C 0D`(Enter)或 `KEY 39 20`(Space);只送 scancode 沒反應。上下移動用 scancode
  `KEY 48`/`KEY 50`(不需 ascii)。
- **決定論靠虛擬時鐘**:`BIOS_TICK_*` 在 replay 下改成每讀遞增的計數器(`consts.h` gated +
  `replay.c:fd2_replay_tick`)。否則動畫相位隨 wall-clock 飄,framebuffer 不決定論。**新增會讀
  BIOS tick 的程式碼時不必擔心**,虛擬時鐘已覆蓋。
- **每次 run 必須 pristine 起始**:`run_play.py` 已清掉遊戲生成的 `FD2.TMP`/`AUDDBG.TXT` 並一律
  覆寫 `FD2.SAV`。新增 scenario 不必處理,但若發現重用目錄結果飄動,先查是不是又有新的遊戲生成檔
  沒被清。
- **開機→選單流程**:title 過場有約 3 個輸入跳過點,之後才到 START/LOAD/CONTINUE 選單
  (`active_idx` 0=START/1=LOAD/2=CONTINUE,預設 0)。
  - NEW GAME(第一章 prologue):`KEY 1C 0D` × 4(前 3 個跳過、第 4 個選 START)。
  - CONTINUE(載入內建存檔):`1C 0D ×3` + `KEY 50` + `KEY 50` + `1C 0D`。
- **save-jump 限制**:`jump_chapter` 只改 chapter_id,其餘是第 1 章資料 → 適合測「該章地圖/loader
  能否載入」(P2 init),**不適合**斷言「該章正確隊伍」。正確跨章狀態要靠母本鏈。
- **clang 紅線是假象**:IDE 找不到 Watcom 的 `D:\H`/`src/include`,會對 `tests/play/*.c` 與 src
  報一堆 `file not found`。以 DOSBox 內 Watcom 編譯結果為準(`build_replay.py` 印 0 errors 才算數)。
- **commit 規範**:直接 commit 到 main(不開分支);emit/build 一律前景跑;訊息結尾帶
  Co-Authored-By 與 Claude-Session(見既有 commit)。

---

## 7. 已 commit 的里程碑

| commit | 內容 |
| --- | --- |
| `9eda340` | P0 walking skeleton(引擎 + 注入 + 擷取 + 比對 + 生產純度) |
| `825fcef` | P1 ch1 + 虛擬時鐘決定論 + fb2png + run_all |
| `fe72d84` | P3 戰鬥操作 + P4 存檔載入(continue_load / combat_move) |
| `86815a2` | P2 save-jump + 30 章 sweep(28/30)+ gen_scenario + open_issues |
| `9dc4dc3` | P3-E 戰鬥傷害 oracle(combat_attack + expect.py LFSR/公式斷言 + st_dump) |
