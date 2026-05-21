# input

FD2 直接讀寫 BIOS data area (segment 0x40) 的鍵盤緩衝區指標，不走 INT 16h。

## BIOS data area 位址

| 位址 | 內容 |
|---|---|
| `0x040:001A` (0x41A) | 鍵盤 buffer HEAD pointer |
| `0x040:001C` (0x41C) | 鍵盤 buffer TAIL pointer |
| `0x040:006C` (0x46C) | DOS midnight tick counter (18.2 Hz) |

## 主要 functions

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x10620` | `fd2_check_keyboard_buffer_nonempty` | 比對 0x41A vs 0x41C 看 buffer 是否非空 |
| `0x4E031` | `fd2_clear_keyboard_buffer` | 寫 `*0x41C = *0x41A`，丟棄所有 pending key |
| `0x11AA8` | `fd2_wait_for_input_with_idle` | 主 poll loop (含 idle 動畫) |
| `0x16C57` | `fd2_wait_for_input_dialog_with_blink` | dialog 中變體 |
| `0x117E7` | `fd2_game_main_loop` | per-frame scancode 分派 (歸 ui_menu) |
| `0x17AA9` | `fd2_wait_n_bios_ticks` | 18.2Hz 為單位的 busy-wait |

## fd2_wait_for_input_with_idle 流程

```
key_input_mode = 0x10
loop:
  if fd2_check_keyboard_buffer_nonempty():
    break
  fd2_update_palette_cycle_anim()    // palette cycling 動畫 (水/火炬等)
  if BIOS tick changed (0x46C):
    fd2_composite_battle_frame()      // cursor 閃爍動畫
int386(0x16, &regs, &out)         // INT 16h BIOS keyboard wait/read scancode
remap_special:
  -0x20 (0xE0) or 'R' (0x52) → 0x1C  (Enter)
  'S' (0x53)                  → 0x01  (Esc)
return key_input_mode
```

擴展鍵 (0xE0 prefix) 標準化為 Enter；Numpad 5 (0x53) 標準化為 Esc。

## fd2_clear_keyboard_buffer

兩 byte BIOS area 寫入：`*0x41C = *0x41A`，等同丟棄所有 pending key，下個
wait_for_input 不會被舊輸入污染。被呼叫時機：cursor 動畫 step 之間、SFX 觸發後、
frame boundary、對話切換之間。

## fd2_wait_n_bios_ticks(n)

Busy-wait 直到 BIOS tick 推進 n ticks (每 tick ~55ms = 1/18.2Hz)。處理 16-bit
wraparound。儲存上次 reference tick 在 `0x53A2C`。

常用值：
- `fd2_wait_n_bios_ticks(1)` ≈ 55ms (一 BIOS frame，cursor blink、sprite step)
- `fd2_wait_n_bios_ticks(2)` ≈ 110ms (動畫 pacing)
- `fd2_wait_n_bios_ticks(6)` ≈ 330ms (post-cast 等較長停頓)

## Scancode 表 (IBM PC keyboard set 1)

`fd2_game_main_loop` 處理的 scancode：

| Scancode | 鍵 |
|---|---|
| 0x01 | Esc |
| 0x1C | Enter |
| 0x2C | Z |
| 0x39 | Space |
| 0x3B | F1 |
| 0x3C | F2 |
| 0x47 | Home / Numpad 7 |
| 0x48 | ↑ |
| 0x49 | F3 |
| 0x4B | ← |
| 0x4C | Numpad 5 (special-mapped to 0x01 Esc) |
| 0x4D | → |
| 0x50 | ↓ |
| 0x52 | Numpad 0 / Insert (mapped to 0x1C Enter) |
| 0x53 | Numpad . (mapped to 0x01 Esc) |
| 0xE0 | Extended-key prefix (mapped to 0x1C Enter) |

## 輸入相關 globals

| 位址 | 名稱 |
|---|---|
| `0x53A8D` | `last_key_pressed` (raw scancode buffer) |
| `0x53A8E` | `key_input_mode` (timeout/mode 值) |
| `0x53A2C` | `fd2_wait_n_bios_ticks` 的 reference tick |
| `0x539F0` | `idle_tick_value` (BIOS tick copy) |
| `0x539F2` | `previous_idle_tick` |
| `0x51AAC` | `data_fd2_ui_play_active_flag` (1=normal play; 0 during transition lockout) |

## 為何用 BIOS 而非 INT 16h

直接寫 BIOS area (而非走 INT 16h) 的常見理由：避免 INT overhead、繞開部分
DOS extender 對 INT 16h 的 bug、與同 frame 讀 tick (0x46C) 有更好的同步性。
DOS/4GW 的 flat memory mode 下 BIOS data area 可直接讀寫 (segment 0 mapping)，
這在 90 年代初的 DOS Protected Mode 遊戲很常見。
