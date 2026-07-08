# save

FD2.SAV 檔案的存讀寫，4-slot 選擇器，game-time 與 main-menu 兩條入口。

## 驗證對象

- **src 檔**：`save/save.c`（章末角色狀態回存、checksum、XOR 加解密、4-slot 存讀寫與 slot 選擇器 UI）。存讀入口與 portrait 快取 swap helper 另散於 lifecycle / rsrc 模組。
- **主要 Ghidra 對象**（函數名 @ 位址，皆即時核對）：
  - `fd2_load_save_and_init_engine` @ 0x10010、`fd2_field_menu_status_save_load_quit_dispatch` @ 0x19DF7、`fd2_main_menu_dispatcher` @ 0x25EBB — 三條存讀入口
  - `fd2_save_current_state_to_slot` @ 0x30012、`fd2_load_state_from_selected_slot` @ 0x301F4、`fd2_save_slot_selector_ui` @ 0x30550 — slot 寫入/讀取/選擇器
  - `fd2_save_compute_checksum` @ 0x4DBB9、`fd2_save_crypt_buffer` @ 0x4DBD8 — checksum 與 XOR involution
  - `fd2_title_attract_and_main_menu` @ 0x1F894 — 讀檔判通關（lifecycle，正典見 overview.md）
  - `fd2_load_chapter_portraits_and_dump_tmp` @ 0x10B4E、`fd2_restore_portrait_cache_from_tmp` @ 0x29117 — FD2.TMP portrait 快取 swap
  - `data_fd2_portrait_sprite_cache` @ 0x53A61（0x32A00-byte portrait 快取區）
- **相關資源檔**：`FD2.SAV`（存檔，完整 byte-level layout 見 resource_info/save_format.md）、`FD2.TMP`（換章 portrait swap 檔）、`FDICON.B24`（讀檔時重建 portrait 快取的來源）。

## 主要 functions

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x00010010` | `fd2_load_save_and_init_engine` | 主 LOAD：read FD2.SAV → 驗證 checksum → 填 runtime_char_array 與所有狀態 globals → 載入所有 DAT |
| `0x00019DF7` | `fd2_field_menu_status_save_load_quit_dispatch` | 遊戲中選單「存檔/讀檔/新遊戲」dispatcher |
| `0x00025EBB` | `fd2_main_menu_dispatcher` | 主選單「NEW GAME / CONTINUE」dispatcher |
| `0x00030012` | `fd2_save_current_state_to_slot` | 寫當前狀態到選定 slot |
| `0x000301F4` | `fd2_load_state_from_selected_slot` | 從選定 slot 載入 |
| `0x00030550` | `fd2_save_slot_selector_ui` | 4-slot 選擇器（上下鍵、Enter/Space 確定、ESC 取消） |
| `0x0004DBB9` | `fd2_save_compute_checksum` | 計算 4-byte checksum |
| `0x0004DBD8` | `fd2_save_crypt_buffer` | XOR-scramble 加解密 (involution，同 function 做雙向) |

`fd2_title_attract_and_main_menu @ 0x1F894` 屬 lifecycle，但會讀 FD2.SAV
確認通關狀態並寫 clear flag，是跨系統的特例 — 結局動畫播放、與
`main` 退出條件直接耦合。

## FD2.SAV 檔案結構

完整 byte-level layout 見 `resource_info/save_format.md`。摘要：

- 22987 bytes (0x59CB)
- `+0x0000` 起 0x8A3 bytes 是 save header（runtime globals）
- `+0x08A3` 起 0xA00 bytes 是 map/terrain data
- `+0x12A3` 起 N × 0x50 bytes 是 runtime_char_array (N = party_member_count)
- `+0x312B` 起是 4 個 slot snapshot (各 0xA28 bytes)
- `+0x59C7` 是 4-byte checksum (在 EOF 之前 4 byte)

## 加密與 checksum

`fd2_save_crypt_buffer` 是 XOR-based involution；同一 function 加密與解密。
`fd2_save_compute_checksum` 把 buffer[0..len-5]（排除末 4 byte 的 checksum 欄位本身）逐 byte
累加成一個 32-bit 整數，是純加總 (additive byte-sum)，不是 CRC32，也不含 xor/rotate。

### 寫流程

```
buffer ← 當前狀態
compute_checksum(buffer, 0x59CB) → 寫進 buffer[0x59C7]   // 內部自動排除末 4 byte
crypt_buffer(buffer, 0x59CB)
fwrite(FD2.SAV, buffer)
```

### 讀流程

```
buffer ← fread(FD2.SAV)
crypt_buffer(buffer, 0x59CB)               // 解密
computed = compute_checksum(buffer, 0x59CB)
if (computed != buffer[0x59C7]) → 顯示錯誤
```

加密與 checksum 用途是 cheat deterrent / 完整性檢查，**非 cryptographic 安全**。

## fopen call sites

「FD2.SAV」字串在 binary 出現 9 次，對應 6 個 function 的 fopen：
- `0x5001B` → `fd2_load_save_and_init_engine`
- `0x5016F, 0x5017A, 0x50185` → `fd2_field_menu_status_save_load_quit_dispatch` (3 次：save / re-verify / load)
- `0x501BC` → `fd2_title_attract_and_main_menu`
- `0x50223` → `fd2_main_menu_dispatcher`
- `0x5026F, 0x5027A` → `fd2_save_current_state_to_slot` (read + write-back)
- `0x50285` → `fd2_load_state_from_selected_slot`

## FD2.TMP swap file

`fd2_load_chapter_portraits_and_dump_tmp @ 0x10B4E` 與
`fd2_restore_portrait_cache_from_tmp @ 0x29117` 兩個 helper 用 FD2.TMP 暫存
data_fd2_portrait_sprite_cache (`0x53A61` 起的 0x32A00-byte 快取區)。換章節時把當前 portrait
set dump 到 FD2.TMP，之後再 restore。
