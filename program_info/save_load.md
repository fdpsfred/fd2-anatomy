# save_load

FD2.SAV 檔案的存讀寫，4-slot 選擇器，game-time 與 main-menu 兩條入口。

## 主要 functions

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x00010010` | `load_save_and_init_engine` | 主 LOAD：read FD2.SAV → 驗證 checksum → 填 runtime_char_array 與所有狀態 globals → 載入所有 DAT |
| `0x00019DF7` | `menu_confirm_save_load_newgame` | 遊戲中選單「存檔/讀檔/新遊戲」dispatcher |
| `0x00025EBB` | `main_menu_continue_dispatcher` | 主選單「NEW GAME / CONTINUE」dispatcher |
| `0x00030012` | `save_current_state_to_slot` | 寫當前狀態到選定 slot |
| `0x000301F4` | `load_state_from_selected_slot` | 從選定 slot 載入 |
| `0x00030550` | `save_slot_selector_ui` | 4-slot 選擇器（上下鍵、Enter/Space 確定、ESC 取消） |
| `0x0004DBB9` | `save_compute_checksum` | 計算 4-byte checksum |
| `0x0004DBD8` | `save_crypt_buffer` | XOR-scramble 加解密 (involution，同 function 做雙向) |

`play_ending_and_record_clear` 屬 lifecycle，但會讀 FD2.SAV 確認通關狀態並寫
clear flag，是跨系統的特例。

## FD2.SAV 檔案結構

完整 byte-level layout 見 `resource_info/save_format.md`。摘要：

- 22987 bytes (0x59CB)
- `+0x0000` 起 0x8A3 bytes 是 save header（runtime globals）
- `+0x08A3` 起 0xA00 bytes 是 map/terrain data
- `+0x12A3` 起 N × 0x50 bytes 是 runtime_char_array (N = party_member_count)
- `+0x312B` 起是 4 個 slot snapshot (各 0xA28 bytes)
- `+0x59C7` 是 4-byte checksum (在 EOF 之前 4 byte)

## 加密與 checksum

`save_crypt_buffer` 是 XOR-based involution；同一 function 加密與解密。
`save_compute_checksum` 是 4-byte sum/xor/rot 的快速 checksum (不是 CRC32)。

### 寫流程

```
buffer ← 當前狀態
compute_checksum(buffer, 0x59CB-4) → 寫進 buffer[0x59C7]
crypt_buffer(buffer, 0x59CB)
fwrite(FD2.SAV, buffer)
```

### 讀流程

```
buffer ← fread(FD2.SAV)
crypt_buffer(buffer, 0x59CB)               // 解密
computed = compute_checksum(buffer, 0x59CB-4)
if (computed != buffer[0x59C7]) → 顯示錯誤
```

加密與 checksum 用途是 cheat deterrent / 完整性檢查，**非 cryptographic 安全**。

## fopen call sites

「FD2.SAV」字串在 binary 出現 9 次，對應 6 個 function 的 fopen：
- `0x5001B` → `load_save_and_init_engine`
- `0x5016F, 0x5017A, 0x50185` → `menu_confirm_save_load_newgame` (3 次：save / re-verify / load)
- `0x501BC` → `play_ending_and_record_clear`
- `0x50223` → `main_menu_continue_dispatcher`
- `0x5026F, 0x5027A` → `save_current_state_to_slot` (read + write-back)
- `0x50285` → `load_state_from_selected_slot`

## FD2.TMP swap file

`load_chapter_portraits_and_dump_tmp @ 0x10B4E` 與
`restore_portrait_cache_from_tmp @ 0x29117` 兩個 helper 用 FD2.TMP 暫存
portrait_sprite_cache (200 KB region from `0x53A61`)。換章節時把當前 portrait
set dump 到 TMP，之後再 restore。
