# tools/ail_extract/

從 FD2.LE 抽出 Miles AIL audio library 重建為 `ailv3.lib` + `ailv3.h` +
`fd2common.lib`，並驗證可播放 FD2 的 MIDI 與 PCM 音效。

## Production pipeline（consolidated mode）

```
Step  Script                         Input                          Output
────  ─────────────────────────────  ─────────────────────────────  ──────────────────────────────
 0    dump_ghidra_supplements.py     Ghidra MCP (FD2.LE open)       raw/ail_fn_metadata.json
      (snippets 1-2, 4-8)                                          raw/ail_data_items.json
                                                                    raw/crt_data_symbols.json
                                                                    raw/fd2common_rel32_sites.json
                                                                    raw/ail_data_owners.json
                                                                    raw/pcrel32_sites_raw.json
                                                                    audit/ghidra_pool_snapshot.json

 1    dump_ail_set.py                raw/ail_fn_metadata.json       ail_inventory.json
                                     raw/ail_data_items.json

 2    extract_ail_bytes.py           ail_inventory.json             ail_code.bin
                                     FD2.LE (binary)                ail_data.bin
                                                                    ail_layout.json

 3    enumerate_pcrel32_sites.py     ail_code.bin                   pcrel32_worklist.jsonl
                                     raw/pcrel32_sites_raw.json
                                     ail_inventory.json

 4    (manual, already done)         pcrel32_worklist.jsonl         ail_fixups_synth.jsonl  ★
                                                                    ail_fixups_midfn.jsonl  ★

 5    bin_to_omf.py --mode consol.   ail_inventory.json             objs/ail_code.obj
                                     ail_layout.json                objs/ail_data.obj
                                     ail_code/data.bin              objs/fd2common_*.obj (×7)
                                     ail_fixups_synth.jsonl
                                     ail_fixups_midfn.jsonl
                                     raw/crt_data_symbols.json
                                     raw/fd2common_rel32_sites.json
                                     audit/ghidra_pool_snapshot.json
                                     le_fixups.json

 6    pack_libs.py                   objs/*.obj                     out/ailv3.lib
                                                                    out/fd2common.lib

 7    gen_ailv3_h.py                 ail_inventory.json             out/ailv3.h

 8    run_test.py                    out/*.lib + out/ailv3.h        BUILD_OK + [FINAL]
                                     tau.c + build_test.bat
                                     game assets (FDMUS/FDOTHER)
```

★ = 手動 audit 產物（1408 rel32 verdicts + 244 mid-fn details），不可自動重建。
    正本存於 `tools/ail_extract/`，workspace/ 下為工作副本。

## 檔案清單

### Pipeline scripts

| 檔案 | 用途 |
|---|---|
| `dump_ail_set.py` | Stage 1: raw dumps → `ail_inventory.json` |
| `extract_ail_bytes.py` | Stage 2: inventory → `ail_code.bin` + `ail_data.bin` + `ail_layout.json` |
| `enumerate_pcrel32_sites.py` | Stage 3: E8/E9/0F8x byte scan → `pcrel32_worklist.jsonl` |
| `bin_to_omf.py` | Stage 5: **核心** — OMF .obj emit（consolidated / per-item 雙模式） |
| `omf_writer.py` | OMF 32-bit record encoder（bin_to_omf 依賴） |
| `pack_libs.py` | Stage 6: DOSBox-X + wlib 9.5a → `.lib` 打包 |
| `gen_ailv3_h.py` | Stage 7: inventory → `ailv3.h`（handle typedef + `#pragma aux ... "*" modify [eax ebx ecx edx]`）。產物同時是 FD2 遊戲建置輸入：置於 `libs/ailv3/ailv3.h`（建置時 DOSBox 掛成 `F:`、compile `-i=F:\ailv3`），由 `src/include/protos.h` `#include`，取代舊的 plain AIL 宣告（缺 clobber pragma 曾導致 SFX 靜音）。`SIGNATURE_OVERRIDE` 修正 Ghidra 把 `AIL_set_sample_address` handle 誤標 int |
| `dump_ghidra_supplements.py` | Stage 0: 7 個 Ghidra inline Java snippet，產生 raw/ + audit/ 下所有 supplement 檔案 |

### Verdict 管理工具（Stage 4 手動分類用）

| 檔案 | 用途 |
|---|---|
| `append_verdict.py` | 逐筆寫入 synth verdict |
| `list_pending_sites.py` | 列出尚未分類的 worklist sites |
| `carryover_synth_verdicts.py` | re-dump 後沿用既有 verdicts |

### 手動 audit 產物（不可自動重建）

| 檔案 | 內容 |
|---|---|
| `ail_fixups_synth.jsonl` | 1408 筆 rel32 cross-fn verdict（5 路分流：AIL→AIL / AIL→CRT / AIL→fd2common / AIL→AIL_mid / unknown） |
| `ail_fixups_midfn.jsonl` | 244 筆 mid-fn alt-entry detail（target fn + offset + label name） |

### Test pipeline

| 檔案 | 用途 |
|---|---|
| `run_test.py` | Host 端 driver：staging + DOSBox-X conf 生成 + 執行 + log 收集 |
| `tau.c` | DOS test client（S0 baseline + S2-S6 scenario），Watcom 9.5a C89 |
| `build_test.bat` | DOSBox-X 內 wcc386 + wlink build script |
| `taudio.lnk` | wlink directive（dos4g + ailv3.lib + fd2common.lib + CLIB3S） |
| `MDI.INI` / `DIG.INI` | Miles driver INI config（IRQ=-1 → BLASTER env autodetect） |

### Legacy / fallback

| 檔案 | 用途 |
|---|---|
| `dpmis.c` | per-item mode 的 DPMI lock/unlock stubs（consolidated mode 不需要） |
| `test_audio.c` | 舊 test client（pre-tau.c） |
| `split_to_objs.py` | per-item mode 的 fn/data grouping |
| `verify_baseline.py` | FD2 原 binary baseline 比對 |

### Unit tests

| 檔案 | 對應 |
|---|---|
| `test_dump_ail_set.py` | dump_ail_set 8 個 sanity check |
| `test_extract_ail_bytes.py` | extract_ail_bytes 9 個 byte-identical check |
| `test_enumerate_pcrel32_sites.py` | enumerate 分布 invariant check |
| `test_omf_writer.py` | OMF record sequence / checksum / PUBDEF check |

## Consolidated mode vs per-item mode

| | Consolidated（production） | Per-item（fallback） |
|---|---|---|
| 指令 | `--mode consolidated` | `--mode per-item` |
| AIL .obj | 2（ail_code + ail_data） | 428 fn + 273 data |
| ailv3.lib | ~210 KB | ~397 KB |
| fd2common .obj | 7（lock+unlock merged） | 8 |
| DPMI stubs | 不需要 | 需要 dpmis.c |
| Intra-AIL fixup | literal disp32（1001 sites） | EXTDEF + FIXUPP |
| ISR stack gap | Ghidra data item 自然保留 | MERGE_GROUPS |
| Vendor adjacency | 自動（compact vendor-order） | MERGE_GROUPS |

## Workspace 結構

Pipeline 中間產物寫入 `workspace/ail_extract/`：

| 路徑 | 內容 | 可重建？ |
|---|---|---|
| `raw/*.json` | Ghidra dump（7 個 supplement） | ✓ dump_ghidra_supplements.py |
| `ail_inventory.json` | 428 fn + 312 data inventory | ✓ dump_ail_set.py |
| `ail_code.bin` / `ail_data.bin` / `ail_layout.json` | Binary extraction | ✓ extract_ail_bytes.py |
| `pcrel32_worklist.jsonl` | 1631 rel32 sites | ✓ enumerate_pcrel32_sites.py |
| `objs/` / `build/` / `out/` / `run_stage/` | Pipeline 各 stage 產出 | ✓ bin_to_omf + pack_libs + run_test |
| `handoff.md` | 狀態紀錄 | — |
