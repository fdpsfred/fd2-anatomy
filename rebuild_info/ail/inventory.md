# AIL library inventory

FD2 的音效系統使用 Sonic Foundry 的 **Miles Sound System** (AIL = Audio
Interface Library)，是 1990 年代 DOS 遊戲業界標準 (Warcraft II / Fallout /
Command & Conquer 等用同一套)。所有 `AIL_*` 函式從 AIL3DIG / AIL3MDI 靜態
library 連進 FD2.LE。

`SETSOUND.EXE`（FLAME2 目錄裡的工具）就是 AIL 的設定程式，會產生 `.MDI` /
`.DIG` driver 設定。

本檔記錄 FD2.LE 內 AIL ecosystem 的 inventory 分類規則、靜態鏈接 thunk
配對、命名規約、library 邊界、dead 判定方法。FD2 自寫的 game-side audio 層
（BGM dispatcher、SFX trigger）與 DOS-side driver / patch 檔清單見
`program_info/audio.md`。

## 重建目標：完整 AIL library reproduction

ailv3.lib rebuild 的目標是**完整重現 Miles AIL3DIG / AIL3MDI vendor lib
功能**，而不是「functional-identical 到 FD2.LE binary」。這代表：

- **所有 Ghidra 內 `AIL_*` 命名的 function 都納入 lib**（含 FD2 game 沒呼叫
  的 vendor public API）
- **不做 dead-fn 排除**——即使某 fn entry/body 在 FD2.LE 內 0 xref，仍是
  vendor lib export surface 的一部分，client 可呼叫
- AIL data items 同原則：全部納入 vendor lib，不依 FD2 reachability 排除

## AIL function inventory（透過 Ghidra MCP 即時取得）

當前 AIL ecosystem 的完整 function set 透過 Ghidra MCP 即時 dump：

```
mcp__ghidra__search_functions(name_pattern="^AIL_")
```

不在 KB 中維護人寫的具體 count / 具體 fn name list — 任何具體列舉都會隨
Ghidra 後續 audit / rename / boundary fix 失效。命名前綴分群：

| 群組 | 命名規約 | 性質 |
|---|---|---|
| 公開 API | `AIL_*`（不含 `AIL_internal_` 前綴）| client 端 `extern` 可呼叫；多數含 self-print `fprintf(log, "AIL_xxx(...)\n", ...)` debug log gate |
| 內部 helper / inner worker | `AIL_internal_*` | vendor lib 內部，不出 public header；多數為 `AIL_xxx_inner` 對應某個 public wrapper 的實作 |
| DIG mixer dispatch table callbacks | `AIL_internal_mix_finalize_<idx_hex>` / `AIL_internal_mix_loop_<idx_hex>` | indirect dispatch via 兩個 pointer table（見下方 DIG mixer dispatch tables 段）|

`AIL_*` 命名涵蓋整個 vendor lib code (AIL3DIG + AIL3MDI 兩個靜態 .obj 合
集)。pool 歸屬透過 `tools/program_analysis/build_call_graph.py`
`categorise()` 機械決定。

## AIL 函式辨識方式（self-print log gate pattern）

每個 **AIL 公開 entry-point** 啟動時會走 `AIL_DEBUG` flag 檢查並印出
`"AIL_<name>(args)\n"` 的字串，binary 內這些 fprintf format strings 可精確
定位每個 entry function 的命名。vendor library 內部 helper 也有部分含 log
print（vendor-internal logged API），其 fmt string 含 `"AIL_xxx(...)\n"`
模式但 FD2 source 端不直接呼叫。

每個 entry-point 命名都透過 `AIL_internal_log_print_timestamp_prefix(...)`
+ `fprintf` 的 debug printf 親自驗證；當函式 body 含多個 AIL 字串引用時，
**以 entry-point 第一條 printf 為準**。

AIL 內部 helper（ISR / timer / mixer / sequence worker 等沒有 debug printf
字串者）命名依 callees / data ref / 結構推敲決定，全部命名為
`AIL_internal_<descriptor>` 或 `AIL_internal_<X>_inner`——不留
`_helper_<addr>` 形式 placeholder。

逆向目標是辨認 AIL 層的邊界：看到 FD2 遊戲邏輯呼叫
`AIL_start_sequence(seq_handle)` 就理解意圖即可，AIL 內部邏輯不深究
(vendor SDK 文件可查)。

## Dead 判定方法（資訊性，不影響 lib 納入）

「dead」這個詞在本 KB 內定義為「FD2.LE binary 內無任何 instruction / data
fixup 引用該 function 的 entry 或 body」。透過 Ghidra ReferenceManager
4-axis 判定：

| Axis | 來源 |
|---|---|
| entry xref | `mcp__ghidra__get_function_xrefs(addr=entry)` 計入 call / jump / data / indirect |
| body inbound xref | iterate `getReferencesTo(addr)` for addr ∈ (body − entry)，filter external source（src ∉ body）|
| mid-fn rel32 jump | `tools/ail_extract/ail_fixups_midfn.jsonl` 內 `target_within_fn == fn_name` 的 entry |
| LE FIXUP data ref to fn ptr | `workspace/data_audit/le_fixups.json` `target_addr_to_sources` filter target ∈ fn body |

**4 軸全 0 = dead**；任一軸 > 0 = alive。判定純資訊性——dead fn 仍納入
ailv3.lib（per 「完整 lib reproduction」goal）。Ghidra plate 含
`sub-case dead_code_stub` 標記是過去某次 audit 的結論，與當前判定可能不
一致；若 plate 與 4-axis 結果衝突，**以 4-axis 為準**。

## Alignment NOP fn boundary（Watcom 9.5a hot-fn alignment）

Watcom 9.5a 對某些 hot function（多為 ISR / driver entry）的 entry 對齊到
16-byte 邊界（`entry & 0xF == 0`），在前一個 fn 結束處與該 entry 之間
插入 alignment NOP padding。常見 NOP encoding 見
`rebuild_info/emission/pool_routing.md`「Watcom compiler alignment NOP」
段（6-byte `8D 80 00 00 00 00` / 6-byte `8D 92 00 00 00 00` / 3-byte
`8D 40 00` / 2-byte `8B C0` 等）。

當 Ghidra 自動分析誤把 alignment NOP 包進 AIL fn body（造成 entry 假性
落在 padding 開始位址），須拆 fn boundary：

1. `delete_function(padding_entry_addr)` 移除誤包的 fn
2. `create_function(real_aligned_entry_addr, name=AIL_internal_<original_name>)` 新建真實 entry 的 fn
3. 對 alignment block 各 `create_function(nop_addr, name=binary_artifact_align_nop_<addr>)`
   歸 `binary_artifact` pool
4. plate transfer 到新 entry + alignment block 各 plate 寫 NOP encoding + 對齊目的

判定 alignment NOP 須以 `read_memory` 的實際 byte 對照 NOP encoding 表，
**不可從 Ghidra mnemonic 推**——同 mnemonic 可能對應多種 encoding（2/3/6
byte），只有特定 encoding 才是 Watcom alignment NOP。

## Static-link thunk + body pairs

部分 AIL helper 因 AIL3DIG 與 AIL3MDI 兩個靜態 .obj 各帶一份而被連結兩次。
一個是 **5-byte JMP thunk**（指向另一個的位址），一個是**完整 body**
（PUSH/INC global/POP/RET）。兩份位於不同 address，bodies 不同
（thunk vs full impl）。當前已知的 thunk + body pair list 透過 Ghidra MCP
即時取得（搜尋 `AIL_internal_log_lock_*` / `AIL_internal_get_isr_lock_count*`
等系列即可看到 `_<addr>` 後綴的 body 與無後綴的 thunk）。

Body 端加 `_<addr>` 後綴是為了在 Ghidra 命名空間中區分 thunk 與 body
（兩者都歸 `AIL_internal_*` 而非公開 `AIL_*`，因為 vendor 內部 helper
性質）。emit pipeline 連結 Watcom AIL3DIG/AIL3MDI 後，這兩份依靜態 link
順序自然出現 in-binary，FD2 source 端不重 emit。

## Vendor-internal AIL public API (logged wrapper + inner pair)

AIL3DIG / AIL3MDI 靜態 library 內含大量「FD2 source 端不直接呼叫，但
vendor library 內部呼叫鏈會走到」的 logged public API，每個 wrapper 自
帶 `"AIL_<name>(args)\n"` log printf + 對應 `AIL_<name>_inner` 實作。

判定方式：搜尋 `AIL_*` 命名且 body 內含 `fprintf(log, "AIL_xxx(...)\n",
...)` 的 self-print pattern，即為 logged public API。其對應 inner 命名為
`AIL_<name>_inner`（如有獨立 inner function）或共享某個已命名 vendor
helper。

完整 pair list 透過 Ghidra MCP 即時 dump：

```
mcp__ghidra__search_functions(name_pattern="^AIL_.*_inner$")
```

外層 logged public wrapper 與 inner 配對由各自 plate comment 紀錄
（wrapper plate 標 fprintf 模板與 inner addr / inner plate 反向指向
wrapper）。emit pipeline 階段這些函式全由 vendor relink 解析，不需要
FD2 自寫實作。

## AIL 內部 helper 函式（命名分群）

AIL library 內部 helper、ISR / timer / mixer / sequence worker。命名以
「entry function 對應 worker → `_inner` 後綴」與「私有功能 → 動詞短語」
為原則。所有這些 helper 在 emit pipeline 階段不會以 FD2 source 形式重新
輸出，**Watcom linker 直接 link AIL3DIG / AIL3MDI** 即可解析。下列分類用
於閱讀導引，非 strict subsystem boundary（多數 helper 會被多個 entry-point
共用）。

### Public entry-point inner workers (`*_inner` 後綴)

每個 public AIL function 多半把實作切到一個 inner worker，public function
只負責 debug printf + 參數轉發。常見 pattern：

```c
ret_type AIL_xxx(args) {
    if (AIL_DEBUG) fprintf(log, "AIL_xxx(args)\n", ...);
    return AIL_xxx_inner(args);
}
```

涵蓋 sample / sequence / timer / driver / channel / preference / install
各類 setter / getter / lifecycle API 的 inner pair。

### Initialization & state globals

- once-only init guard `AIL_internal_init_globals_once`
- runtime config / ISR lock counter / state-array 初始化 helpers
  (`AIL_internal_init_runtime_defaults` / `_init_state_arrays` /
  `_init_mdi_state_arrays`)
- state global register helpers (`AIL_internal_register_state_globals` /
  `_register_mix_globals`)
- INI driver-config 解析路徑 (`AIL_API_read_INI` +
  `AIL_internal_API_read_INI_inner`)
- Timer-slot 配發 (`AIL_register_timer` + slot table)

### Logging & ISR re-entry guard

- log lock acquire / release 對 (有 thunk + body pair，static-link 帶 2 份)
- log timestamp prefix / nesting decrement
- ISR re-entry counter

### Timer / PIT helpers

- 8254 PIT divisor 設定
- frequency → divisor 轉換 inner (`AIL_internal_set_timer_divisor_inner`)
- VGA vertical retrace 同步 busy-wait
- IRQ vector 還原

### DIG mixer / playback engine

- DIG driver 一次性設定 chain (`AIL_internal_dig_driver_setup_full` 等)
- per-sample runtime 控制 (pitch bend / volume / pan)
- DMA buffer 清 0
- pan + volume → 8-bit/16-bit lookup table 建表
- mixer dispatch (`AIL_internal_mix_dispatch_format` /
  `AIL_internal_mix_dispatch_sample`)

### DIG mixer dispatch tables + callbacks

兩個 function-pointer dispatch table 各 128 entries × 4 bytes，被
`AIL_internal_register_mix_globals` 用 `fd2_dpmi_lock_region` lock 住整個
mix-loop code+data 區段（DPMI page-lock 保證 ISR ctx 不缺頁）。Layout：

| Table | base 標籤 | 角色 |
|---|---|---|
| A | `data_ail_dig_mixer_format_finaliser_dispatch_table_a` | 把 mix accumulator 寫到 driver 輸出 buffer 的 per-output-format finaliser；entry size 4B; 部分 entry NULL（不支援格式組合）|
| B | `data_ail_dig_mixer_per_sample_dispatch_table_b` | 讀 source PCM、依 per-channel volume table 縮放、累加進 mix accumulator 的 per-sample-format inner mixer；entry size 4B; 部分 entry NULL |

兩 table 共用 `data_ail_mix_format_flags` 為 7-bit index。當前 lock 區段
範圍透過 Ghidra `get_function_callers(AIL_internal_register_mix_globals)`
+ disasm 即時取得（不在 KB 維護具體 byte range，因 layout 可隨 Ghidra
audit 變動）。

callback 命名規約：

- Table A entry [idx]: `AIL_internal_mix_finalize_<idx_2digit_hex>`
- Table B entry [idx]: `AIL_internal_mix_loop_<idx_2digit_hex>`

每個 callback 的 plate comment 註明所屬 table / idx / dispatch 函式 /
DPMI lock 範圍。callback semantic 為 inline PCM format converter (XOR
0x80/0x8000 sign flip、saturation clip、stereo↔mono pack/unpack、
8↔16 bit 轉換、stereo volume table lookup + accumulate)。

### MDI / sequence engine

- MDI driver 設定 chain
- MIDI byte 送出 driver (`AIL_internal_midi_send_message` /
  `_midi_flush_pending`)
- sequence event 處理 (`AIL_internal_sequence_controller_write` /
  `_sequence_handle_midi_event`)
- channel state 操作 (silence / reset / restore / release inner)

### XMIDI 解析

- EVNT chunk 搜尋
- variable-length quantity 解
- meta-event (tempo / loop) 處理

## DOS-side driver 檔案

FD2 隨片附的 `.MDI` / `.DIG` driver 與 `AILDRVR.LST` / `SAMPLE.AD/OPL/BNK`
instrument patch 屬於 game-shipped runtime 資源，清單見
`program_info/audio.md`。

## Library 邊界

AIL 函式在 `.object1` 內 **不是** 連續區段——Watcom linker 把 AIL code 與
FD2 game / Watcom CRT interleave，[[project_function_interleave]] 描述同
現象。分類只能靠命名前綴 + caller/callee + content evidence，**不能用
address range 判定**。具體 AIL fn 邊界透過 Ghidra MCP
`search_functions(name_pattern="^AIL_")` 即時取得。

## ailv3.lib / fd2common.lib build artifact

`tools/ail_extract/pack_libs.py` 把 `workspace/ail_extract/objs/*.obj`
打包成兩份 lib，產出在 `workspace/ail_extract/out/`：

| 檔案 | 內容 |
|---|---|
| `ailv3.lib` | 所有 AIL fn `.obj`（含 dead public + dead internal stub）+ 所有 shared-data `.obj`；全 PUBDEF（`AIL_*` + `AIL_internal_*`）以滿足 lib 內部 cross-`.obj` EXTDEF 解析 |
| `fd2common.lib` | `fd2_dpmi_*` wrapper + `crt_equivalent_get_eflags` + `_thunk`；全 PUBDEF |
| `ailv3.lst` / `fd2common.lst` | wlib `-l` 列出的 module + dictionary symbol（含 mid-fn alt-entry `L_*_alt_*` label）|

具體 module count / lib size / dictionary symbol count 透過
`out/pack_libs_summary.json` + `out/*.lst` 即時取得（rebuild 後會自動
更新），不在 KB 維護人寫的數字（per [[feedback_no_kb_hardcoded_values]]）。

lib 自洽性 — `pack_libs.py` 後置 sanity 對 `ail_fixups_synth.jsonl` 內 4
類 verdict 全做 PUBDEF dictionary 對比，要求 0 missing：

- `AIL_to_AIL` rel32 EXTDEF target → `ailv3.lib` PUBDEF
- `AIL_to_fd2common` rel32 EXTDEF target → `fd2common.lib` PUBDEF
- mid-fn alt-entry label (`L_<fn>_alt_<off>`) → `ailv3.lib` PUBDEF
- 全 `AIL_to_CRT` extern 與 `data_crt_*` extern 留 wlink 階段以
  CLIB3S 9.5a 解析（不在 lib 自洽範圍內）
