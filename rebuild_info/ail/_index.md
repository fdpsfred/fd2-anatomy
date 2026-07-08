# rebuild_info/ail/

Miles AIL audio library (AIL3DIG + AIL3MDI) 在 FD2.LE 內的逆向工程資料與
重建 pipeline 所需規約。

## 檔案

| 檔案 | 內容 |
|---|---|
| `inventory.md` | AIL function/data inventory：分類規則、命名規約、dead 判定、thunk pair、mixer dispatch table、library 邊界 |
| `calling_convention.md` | AIL ABI 契約（clobber `#pragma aux` 唯一正典，playtest_bugs A 類引用）：`__watcall` / EBX-clobber、handle 與 buffer-pointer 型別、fd2common pool |
| `public_api_semantics.md` | AIL public API runtime 行為：driver init 流程、AIL_delay 單位、sequence_status bitflag、SFX sample setup、FD2 game 的 API 使用模式 |
| `omf_emit_rules.md` | OMF emit 6 條規約（R-1~R-6）、method B mid-fn PUBDEF、CRT EXTDEF |
| `build_quirks.md` | AIL 專屬的 build/runtime 陷阱（BLASTER IRQ / AIL_DEBUG long path / wlib OMF record buffer）；通用 toolchain 陷阱見 `../build_test/toolchain_quirks.md` |

## Pipeline 與工具

pipeline scripts 和完整用法見 `tools/ail_extract/_index.md`。
