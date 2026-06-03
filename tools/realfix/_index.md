# tools/realfix/ — 真遊戲檔 ground-truth 查詢工具

從 `fd2_game_files/` 的真實遊戲檔解析出資源 loader 期望值，供逆向理解與測試設計參考。

## 元件

| 檔案 | 用途 |
|---|---|
| `dump_real.py` | dump 真 DAT 資源（FDOTHER/FDFIELD/FDTXT/FDSHAP/FDMUS）的 offset/size/首 bytes、FDICON.B24 portrait sprite-header、FD2.SAV 欄位。FD2.SAV 磁碟上是 XOR 加密（`sav_decrypt` 鏡像 `fd2_save_crypt_buffer @ 0x4dbd8`），工具會先解密再讀欄位並印 `checksum_ok` 自驗。用法：`python tools/realfix/dump_real.py [chapter]`（chapter 預設 1，決定 FDFIELD/FDTXT 的索引基底）。 |

DAT/SAV/FDICON 的 layout 常數（6-byte prefix、u32 offset table、SAV 欄位 offset、stride）是 loader 實作的 on-disk 格式，註解標明來源（`src/rsrc/rsrc.c` / `src/life/main.c` / `src/save/save.c`）。

## 與測試的關係

resource-loader 的 unit test（`tests/rsrc`/`audio`/`battle`/`life`）不依賴本工具的輸出值，而是用 `tests/include/realfile.h` 在測試內獨立 parse 同一份 staged 真檔做 cross-check。本工具供人工查真值、理解格式、規畫測試時參考。
