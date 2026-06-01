# FD2.SAV 存檔格式

存檔大小固定 22987 bytes (0x59CB)。XOR-style obfuscation 與 4-byte checksum
作為完整性檢查 / cheat deterrent，**非 cryptographic 安全**。

## 整體 layout

```
offset     size     內容
+0x0000    0x8A3    save header (各種 runtime globals)
+0x08A3    0xA00    map / terrain data (DAT_00053BF7 來源)
+0x12A3    N×0x50   runtime_char_array (N = party_member_count)
+0x30A3    0x20     DAT_00053AD5 block
+0x30C3    u8       DAT_00053BEF (全域回合計數 battle_turn_counter)
+0x30C4    u8       party_member_count (DAT_00053BEB)
+0x30C5    u8       current_chapter_id (DAT_00053C03)
+0x30C6    u8       DAT_00053AA9   battle_window_origin_x
+0x30C7    u8       DAT_00053AAD   battle_window_origin_y
+0x30C8    u8       DAT_00053AB1   cursor_world_x
+0x30C9    u8       DAT_00053AB5   cursor_world_y
+0x30CA    u8       DAT_00053AB9   cursor_screen_x
+0x30CB    u8       DAT_00053ABD   cursor_screen_y
+0x30CC    u8       DAT_00053BFB
+0x30CD    u32      DAT_00053BF3
+0x30D1    u8       DAT_00053AF9
+0x30D2    u8       DAT_00051AAB
+0x30D3    u8       DAT_00051E61
+0x30D4    u8       DAT_00051E62
... (header continues)
+0x312B    ─────── SLOT ARRAY 開始 ───────
+0x312B    0xA28    slot 0 (2600 bytes)
+0x3B53    0xA28    slot 1
+0x457B    0xA28    slot 2
+0x4FA3    0xA28    slot 3
+0x59C7    u32      checksum (在 EOF 之前 4 bytes)
+0x59CB    EOF
```

實際大小：`0x312B + 4 × 0xA28 = 0x312B + 0x28A0 = 0x59CB` ✓

## Slot layout (0xA28 bytes per slot)

```
+0x000    0xA00    map / terrain snapshot
+0xA00    u8       chapter
+0xA01    u8       DAT_00053BFB
+0xA02    u32      DAT_00053BF3
+0xA06    u8       DAT_00051AAB
+0xA07    u8       DAT_00053AF9
+0xA08    u8       DAT_00051E61
+0xA09    u8       DAT_00051E62
+0xA0A    0x1E bytes  剩餘 (具體 sub-field 命名待後續分析)
```

## 加密與 checksum

`fd2_save_crypt_buffer @ 0x4DBD8` 是 XOR-based involution（同一 function 加密與解密
雙向使用）。`fd2_save_compute_checksum @ 0x4DBB9` 是 4-byte sum/xor/rot 的快速 checksum
(非 CRC32)。

### 寫流程

```
buffer ← 當前狀態 (22987 bytes 含位置 0x59C7 留 4 byte 給 checksum)
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

加密用途是 cheat deterrent / 完整性檢查，並非安全加密。

## 與 program 端的對應

完整 fopen call sites 與 9 處字串 occurrence 對應的 6 個 function 見
`program_info/save_load.md`。
