# rebuild_info/ail/

Miles AIL audio library (AIL3DIG + AIL3MDI) 在 FD2.LE 內的逆向工程資料與
重建 pipeline 所需規約。

## 檔案

| 檔案 | 內容 |
|---|---|
| `inventory.md` | AIL function/data inventory：分類規則、命名規約、dead 判定、thunk pair、mixer dispatch table、library 邊界 |
| `calling_convention.md` | Calling convention 例外（`__watcall` / EBX-clobber / `#pragma aux`）、handle typedef、fd2common pool |
| `public_api_semantics.md` | AIL public API runtime 行為：driver init 流程、AIL_delay 單位、sequence_status bitflag、SFX bank format、FD2 game 的 API 使用模式 |
| `omf_emit_rules.md` | OMF emit 6 條規約（R-1~R-6）、method B mid-fn PUBDEF、CRT EXTDEF |
| `build_quirks.md` | Watcom 9.5a / DOSBox-X / DOS toolchain 已知陷阱 |

## Pipeline 與工具

pipeline scripts 和完整用法見 `tools/ail_extract/_index.md`。

目前狀態和 open issues 見 `workspace/ail_extract/handoff.md`。
