# 未解問題與未做分析

本檔列出比對全知識庫後仍未確定 / 待驗證 / 待做的項目。開放項在最上面、依領域標記；
已調查定案的結論已整合進對應 KB，文末只留基線索引避免重開。每條開放項描述：現狀 + 為何未解 + 解需做什麼。

## 開放項目

### [測試系統]

**ch7 / ch22（0-based）save-jump 載入 hang** — P2 chapter-init sweep 30 章中 28 章 headless 載入乾淨，
ch7/ch22 卡在 `fd2_load_save_and_init_engine` 載入期無窮迴圈（heartbeat 停在載入確認鍵後、0 dump、無 GP fault）。
極可能是 `gen_scenario.py` 的 jump-save 只改 chapter_id、其餘（地圖快照 / tile-event / 單位）仍是第 1 章資料的
不一致 artifact（28/30 佐證 loader 本身正常），但未驗證前不下定論。解法任一即可分類 / 解決：(a) 母本鏈——逐章
一致的 entry SAV 重測；(b) 同一 jump-save 餵原版 `~FD2.EXE`，若亦 hang 即確認為 artifact；(c) `replay.c` 加
INITCH 命令直接呼叫 `data_fd2_chapter_init_handler_table[N]()` 做 fresh init，繞過存檔不一致。詳見 `tests/play/_index.md`。

**測試內容擴充（P3-E / P4 / P5）** — 引擎與各核心機制已驗證並 blessed（狀態表見 `tests/play/_index.md`），剩內容量
工作：P3-E 擴充（12 種 AI behavior class / 法術傷害 / 命中 / 狀態效果，依 `combat_attack` 範式）、P4（招募 / 兌換 /
結局 / 商店 / options 的狀態斷言，需母本鏈到達章節點）、P5（cinematic golden + 原版差分背書；`diff_original.py`
未建，原版差分基礎見 `tools/fd2_diff/`）。

**src_refine item-1 的 tests/ 同步** — `tests/` 約 40+ 檔仍引用已死的舊 symbol 名，對現行 `src/include/` 必編不過
（例：舊 global `data_fd2_chapter_portrait_load_buffer` 約 71 處跨 16 檔）。使用者已批准、時機待指示，勿自行開工。
recipe 見 `tools/src_refine/data/rename_explain.md`，old→new 對照用同目錄 `rename_old2new.json`。

### [重建 / 建置]

**86Box-mac 實機驗證（等使用者）** — 白光柱 crash 根因已找到並修正（`__NO_MATH_OPS`，commit `15d32073`；完整結論見
`rebuild_info/build_test/playtest_bugs.md` F 類）。唯一待辦＝使用者拿新建置的 `FD2.EXE` 放進 86Box-mac 重跑白光柱
場景（治療 / 傳送 / 第 30 章召喚任一），確認不再 crash。

**#32 gfx/blitspr.c blit-leaf coordinated landing** — 19 個 blit-leaf function 無法逐一落地：其 spy-mock
（`fd2_blit_indexed_sprite` dispatcher + `fd2_rle_blit_sprite` leaf）住在所有分支共用的 `tests/testglob.c`，被約
325 處引用、橫跨約 8 個套件且跨分支。emit 真 body 會與同名 spy 形成 Watcom W1027 redefinition，刪 spy 又會弄壞
依賴它的跨分支套件。需在所有並行分支合併成單一樹後，把整個 blit 子系統當一個 coordinated unit 落地（emit 19 個真
body + 刪 2 個 spy-mock + 把約 325 處斷言改成真實像素輸出驗證 + 修 `src/include/protos.h` 的參數名）。逐 function
disasm / 分類證據見 `tools/code_emit/data/emit_issues.json` 的 `0002935b` 條目。

**#33 util/pathfnd.c pathfind entry coordinated landing** — 6 個 bottom-up helper 已 emit（done），剩 2 個 entry
（`fd2_init_movement_range_floodfill @ 0x4E040`、`fd2_pathfind_to_destination @ 0x4E1A6`）不能單獨落地：spy / stub
在共用 `testglob.c`、被約 25 個跨分支套件依賴。需合併後當 coordinated unit 落地（emit 2 個真 body + 刪 spy / stub
+ 把約 25 個依賴套件改成用真實演算法結果斷言）。orchestrator 在 entry 寫入的 secondary cost-table base 由
`data_fd2_battle_pathfind_move_cost_table_ptr @ 0x6006A` 交回。

**#35 le_layout DPMI extender 偵測路徑 runtime 確認 + signature 對映補全** — `rebuild_info/link/le_layout.md`
§入口點目前泛述 `_cstart_` 的 extender 偵測（DOS/4G、Phar Lap 386|DOS、Intel Code Builder），並由綁定的 DOS4GW
stub 與 `RATIONAL DOS/4G` 字串推論「FD2 走 DOS/4G 這條路徑」，此推論尚未經 emulator / DOSBox trace 實機確認。另
Watcom 9.5a `CSTART3S.ASM` 的 `_cstart_` 對 INT 21h AX=3000h 高 16-bit signature 與 AX=FF00h/DX=78h 的三路
signature→extender 對映（`'DX'`=Phar Lap 386|DOS、`'BC'`=Intel CodeBuilder、FF00h/78h=Rational DOS/4G）尚未寫入
le_layout.md。純文字精確性，不影響 emit / link。解：emulator / DOSBox trace 確認 FD2 runtime 走 FF00h 路徑，並把
CSTART3S.ASM 三路 signature 對映補進 le_layout.md。

**#29 3 個無法 import 的 .obj** — 770 個 dedup 後的 Watcom CRT .obj 中 3 個觸發 Ghidra OmfLoader 的 EOF bug 而 import
失敗（`fpeinth.obj`、`font8x8.obj` ×2）。FD2 都不連結這 3 個，對版本判定與 CRT 識別無影響，但理論完整度仍是缺口。
解：trace OmfLoader 為何在處理完 MODEND 後仍多讀 1 byte，patch loader 或重組 .obj 結構。

**#31 crt_lookup byte_match 全表 false-positive re-verification** — 已修 0x46b41（原 `_Not_Enough_Memory` 實為
Watcom `__FpAbort`，`current_name` 更正、`verified` 標 `byte_match_disputed`、Ghidra 同步 rename），但未對全部 52 個
`verified: byte_match` entry 做 system-wide re-verification。解：寫 audit script 把每個 byte_match entry body 內的
hardcoded immediate（字串 / data / call target ptr）抽出，比對對應 lib `.obj` 相同 offset FIXUPP 後的真實 target，
找其他 false positive；可作為 lookup 維護的 regression script。

### [資源檔格式未解]（data-only；caller 不對 unknown byte 做條件分支，不影響 emission）

- **TAI.DAT payload** — 每 entry `+0x00..0x03` = width/height（u16 LE ×2），後續 opcode / payload 序列未解碼。
- **FDOTHER nested sub-entry** — 29 個 outer 各為 sub-archive、共 176 個 sub-entries；多數已對應具體 caller，
  各 sub-entry 的 payload 內容（RLE sprite / SFX 樣本）未逐一 dump 分析。
- **ANI.DAT header** — 0xAD-byte entry header 只解出 `+0xA5..0xA6` = frame_count（其餘無條件分支）；per-frame
  header `+0x00..0x03` = data_size + opcode_count 已解，`+0x04..0x07`（4 bytes）用途未確認。
- **FD2.SAV slot trailer `+0xA0A..0xA28`（30 bytes）** — 未細分 sub-field；save / load 走 memcpy 整段保留。
- **tile_attribute_flags `+1` / `+3` byte** — `+0` flag bitfield（0x04/0x08/0x10 anim、0x80 renderable、0x20/0x40 event）與 `+2` terrain/anim-group enum（值域 0..0x37）已解；`+1`（值域 0..5）與 `+3`（恆 0）未解讀。
- **FDOTHER 12 個 confirmed_dead idx content** — no-ref proof 確認 dead（已排除 8 個 table/LUT-driven live idx），
  內容未解看有無 cut content 線索。
- **ANI.DAT 9 個 entry 對應的 in-game cinematic 場景** — idx 1 = intro animation，其餘 8 個未對應。

（各項多需 in-game trace caller 消費 buffer 時的行為 / 統計 byte 分布推測，屬 backlog。）

### [Data audit deferred]

- **D8-1 `data_crt_emu387_internal_constant_database_174b @ 0x49a06`** — 174B multi-region const / state table，14 個
  __int7 DATA read site 分布 9 個 sub-offset 已命名，但每個 sub-region 的具體 semantic（control word / status word /
  exception mask / precision mode / opcode dispatch...）未逐一拆解。屬 emu387 sub-system 深度 audit。
- **D8-2 `data_crt_emu387_x87_opcode_dispatch_table_176ptrs @ 0x49ab4`** — 176-slot x87 opcode dispatch table（704B），
  主 access site `CALL [EBX*4 + 0x49ab4]` 已識別，per-slot opcode → handler mapping 未逐一解碼。屬 emu387 深度 audit。
- **D7-2 `0x527B0..0x527B7` Watcom near-heap descriptor 前 8 bytes** — `+0x8..0x24` 已對應（head_search_ptr /
  max_free / node_count / sentinel...），前 8 bytes（推測 heap_top / heap_limit）無直接 asm 訪問；需 Watcom 9.5a RTL
  source 或 emulator-level dump 確認。

### [與攻略本對照]

- **#21 ch23 羅德曼「15 回合內」vs binary `< 15`** — binary gate `< 15` 表 turn 1..14 可加入，攻略「15 回合內」可能對應
  turn 1..15（攻略筆誤應為「14 回合內」，或 binary off-by-one）。解：emulator 實測 turn 14 與 turn 15 的羅德曼加入結果。

## 已解結論（基線索引）

以下問題均已調查定案、結論已整合進知識庫，僅列索引避免重開；細節見對應 KB 與 Ghidra plate（歷史過程見 git log）。

**遊戲機制／內容**（見 `chapters/`、`program_info/battle.md`·`field.md`、`resource_info/`、`assets/`）：
ch1 哈瓦特暴走（char_spawn record[8]/[9] + `ai_target_id` 護子）/ ch9 援軍波次 / ch15·17 條件對白 / ch20 沼澤怪物排除·
達克塞限時招募 / ch23 mid-handler reload / 各章寶物·敵人配置（批次 5D byte-verified FDFIELD 表灌入）/ char_id
namespace（`< 0x44` player class）/ FDFIELD entry layout（0x83 header + N×0x1A record）/ FDSHAP idx 公式 /
11 個 LLLLLL DAT 統一格式 / chapter event 非 bytecode（直接函數 dispatch）/ battle drop path（`pickup_kind=2`）/
runtime_char 80B layout（+0x09·+0x27 為 reserved padding、+0x4C 命中 / +0x4E 迴避）/ FD2.SAV layout /
Enemy AI 12 種 behavior class·kill-shot 加權·20% idle heal·two-pass caster / endgame char_spawn_count 控制載入範圍 /
chinese_glyph 1824 字對照 / 攻略筆誤：ch13「哈瓦諾」＝哈瓦特。

**重建分類／audit**（見 `rebuild_info/crt`·`ail`·`equivalence`·`link` 與 Ghidra plate）：
全 1342 個 function 端到端 re-review（name / cc / plate）/ 四 pool + emit_action 分類架構 / CRT byte_match lookup
（見 `crt/matched_function_sources.md`、`lookup_9.5a.json`；產生器 `tools/program_analysis/crt_fid_match/gen_matched_sources.py`）/
AIL 422 個 function 分類 / jump-table·fall-through·全 binary 完整化 audit（見 `tools/program_analysis/jump_table_audit/`；
2 個 5-byte JMP-thunk 例外 0x4A8E8 / 0x3CBD1 留 Ghidra label 交 vendor relink）/ Decompiler fragment·shared epilogue
（見 `equivalence/watcom_abi.md`）/ 0-arg dispatch callee signature / 28 個 unref chapter event handler（cut content 分類）/
soft-FP long-double primitive / XI ctor table·CRT ctor 邊界（見 `crt/symbol_inventory.md`）/ AIL timer 16-vs-15 slot
overflow-array（見 `ail/inventory.md`）/ unaligned 4-byte global emit（見 `equivalence/rules.md`）/ wlink 連結設定·
le_layout（見 `link/`）/ `tools/decoders/` round-trip 修正。
