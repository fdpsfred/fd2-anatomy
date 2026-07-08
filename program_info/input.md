# input

FD2 的鍵盤輸入不走一般的 INT 16h 阻塞讀取，而是直接讀寫 BIOS data area（segment
0x40）的鍵盤緩衝區 HEAD/TAIL 指標做非阻塞輪詢，讓等待輸入時仍能持續跑閒置動畫，
只有在確認緩衝區有 pending key 之後才用 INT 16h AH=10h 取出 scancode。所有時間節拍
也是靠 BIOS midnight tick（0:046C，18.2 Hz）自行 busy-wait 計算。

## 驗證對象

- src：`src/input/input.c`
- 主要 Ghidra 對象（名稱@位址，逐一 live 核對）：
  - `fd2_check_keyboard_buffer_nonempty` @ 0x10620
  - `fd2_clear_keyboard_buffer` @ 0x4E031
  - `fd2_read_bios_midnight_tick` @ 0x4DFC0
  - `fd2_wait_one_bios_tick` @ 0x13460、`fd2_wait_n_bios_ticks` @ 0x17AA9
  - `fd2_wait_for_input_with_idle` @ 0x11AA8、`fd2_wait_for_input_v2` @ 0x12DAC
  - `fd2_wait_for_action_target_input` @ 0x115B6
  - `fd2_wait_for_input_dialog_with_blink` @ 0x16C57、`fd2_wait_input_with_dialog_repaint` @ 0x17898
  - `fd2_wait_input_with_status_panel_repaint` @ 0x18B84
  - `fd2_wait_input_with_chapter_dialog_blink` @ 0x2D85F、`fd2_wait_input_with_recruitment_repaint` @ 0x32004
  - `fd2_wait_ticks_or_keypress_with_palette` @ 0x1E5C0
  - scancode 消費端 `fd2_game_main_loop` @ 0x117E7（正典歸 ui_menu.md）
- 資源檔：無專屬資源檔（直接讀寫 BIOS data area）；游標移動與選單開啟的音效經 FDOTHER
  SFX bank 播放（見 audio.md）。

## BIOS data area 位址

| 位址 | 內容 |
|---|---|
| `0x040:001A` (0x41A) | 鍵盤 buffer HEAD pointer |
| `0x040:001C` (0x41C) | 鍵盤 buffer TAIL pointer |
| `0x040:006C` (0x46C) | DOS midnight tick counter (18.2 Hz) |

## input.c function 清單

緩衝區與節拍原語：

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x10620` | `fd2_check_keyboard_buffer_nonempty` | 比對 0x41A vs 0x41C，非阻塞判斷 buffer 是否有 pending key |
| `0x4E031` | `fd2_clear_keyboard_buffer` | 寫 `*0x41C = *0x41A`，丟棄所有 pending key |
| `0x4DFC0` | `fd2_read_bios_midnight_tick` | 回傳 0x46C tick 的低 16 bit |
| `0x13460` | `fd2_wait_one_bios_tick` | busy-wait 直到 tick 前進 1 步（~55ms） |
| `0x17AA9` | `fd2_wait_n_bios_ticks` | busy-wait 直到 tick 前進 n 步 |
| `0x1E5C0` | `fd2_wait_ticks_or_keypress_with_palette` | 等 max_ticks 或按鍵（取先到者），期間持續 palette cycling，結束時清空 buffer |

等待輸入迴圈（輪詢 buffer + 跑閒置動畫，取得 pending key 後 INT 16h 讀取並 remap）：

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x11AA8` | `fd2_wait_for_input_with_idle` | 主輸入迴圈：palette cycling + 每 tick 重繪戰場 frame（游標閃爍） |
| `0x12DAC` | `fd2_wait_for_input_v2` | 同 idle 版，但重繪 tick 只存在暫存器內、逐次呼叫各自追蹤 |
| `0x115B6` | `fd2_wait_for_action_target_input` | 選取目標格/角色的游標互動迴圈，回傳 1=確定、-1=ESC 取消 |
| `0x16C57` | `fd2_wait_for_input_dialog_with_blink` | 對話中變體：立繪眨眼 + 可選 ▼ 箭頭動畫 |
| `0x17898` | `fd2_wait_input_with_dialog_repaint` | 設定/選項對話：每 frame 重繪十字選單邊框 |
| `0x18B84` | `fd2_wait_input_with_status_panel_repaint` | 重繪戰場 + 迷你狀態面板，只等按鍵**不讀取** |
| `0x2D85F` | `fd2_wait_input_with_chapter_dialog_blink` | 章節 intro 對話：4-frame 面板循環 + 立繪眨眼 |
| `0x32004` | `fd2_wait_input_with_recruitment_repaint` | 招募畫面節流重繪；額外把 ASCII space (0x20) 也映射成 Enter |

scancode 消費端 `fd2_game_main_loop` @ 0x117E7 每 frame 呼叫 `fd2_wait_for_input_with_idle`
取得 scancode 後做 per-frame 派遣（游標移動、開選單、狀態畫面、切換行動角色等），
其正典說明歸 ui_menu.md。

## fd2_wait_for_input_with_idle 流程

```
loop:
  if fd2_check_keyboard_buffer_nonempty():
    break
  fd2_update_palette_cycle_anim()    // palette cycling 動畫 (水/火炬等)
  if BIOS tick (0x46C) changed:
    fd2_composite_battle_frame(0)     // cursor 閃爍動畫
key_input_mode = 0x10               // INT 16h AH=10h "read enhanced keyboard" preset
int386(0x16, &regs, &regs)         // INT 16h 讀取；回傳後 AH=scancode、AL=ASCII
remap (對 AH scancode):
  0xE0 或 0x52 -> 0x1C  (Enter)
  0x53         -> 0x01  (Esc)
return key_input_mode
```

`key_input_mode`（AH byte）先寫入 0x10 當作 INT 16h 的功能號，呼叫後同一 byte 變成回傳
的 scancode。remap 把擴展鍵前綴 (0xE0) 與 Insert/Numpad 0 (0x52) 標準化為 Enter，把
Numpad `.` (0x53) 標準化為 Esc。Numpad 5 (0x4C) 不在此層 remap，而是由
`fd2_game_main_loop` 當作 ESC-like 的「切換下一個可行動角色」鍵處理。

所有 wait-for-input 迴圈共用同一套 INT 16h + remap 收尾（0xE0/0x52->0x1C、0x53->0x01）。

## fd2_clear_keyboard_buffer

兩 byte BIOS area 寫入：`*0x41C = *0x41A`，等同丟棄所有 pending key，下個
wait_for_input 不會被舊輸入污染。被呼叫時機：cursor 動畫 step 之間、SFX 觸發後、
frame boundary、對話切換之間。

## BIOS tick 節拍：fd2_wait_one_bios_tick / fd2_wait_n_bios_ticks

兩者都以 BIOS midnight tick（0:046C，18.2 Hz，每 tick ~55ms）為單位 busy-wait，並處理
16-bit day-rollover wraparound。tick 在 asm 中是以 `MOVSX EAX,word ptr [0x46C]` 讀成
**sign-extended** 的 int32 再原封存進 cache，因此低字為 0xFFFF 會被 cache 成 0xFFFFFFFF。

- `fd2_wait_one_bios_tick`：spin 直到 tick 前進至少 1 步，reference tick 存
  `data_fd2_engine_wait_one_bios_tick_last_seen` @ 0x53A0C。
- `fd2_wait_n_bios_ticks(n)`：spin 直到 tick 前進至少 n 步，reference tick 存
  `data_fd2_engine_wait_n_bios_ticks_last_seen` @ 0x53A2C。

常用值：
- `fd2_wait_n_bios_ticks(1)` ≈ 55ms（一 BIOS frame，cursor blink、sprite step）
- `fd2_wait_n_bios_ticks(2)` ≈ 110ms（動畫 pacing）
- `fd2_wait_n_bios_ticks(6)` ≈ 330ms（post-cast 等較長停頓）

## Scancode 表 (IBM PC keyboard set 1)

`fd2_wait_for_input_with_idle` 收尾時對 AH scancode 做的 remap，以及
`fd2_game_main_loop` 對回傳 scancode 的派遣動作：

| Scancode | 鍵 | remap / 派遣動作 |
|---|---|---|
| 0x01 | Esc | 切換下一個可行動角色 |
| 0x1C | Enter | 行動 / 確定 |
| 0x22 | B | 保留 no-op（章節特定用途） |
| 0x2C | Z | 切換下一個可行動角色（與 Esc 同一分支） |
| 0x39 | Space | 行動 / 確定 |
| 0x3B | F1 | 戰術俯瞰縮放 |
| 0x3C | F2 | 角色狀態畫面 |
| 0x47 | Home / Numpad 7 | 角色狀態畫面（同 F2） |
| 0x48 | ↑ | 游標上移 |
| 0x49 | PgUp / Numpad 9 | 戰術俯瞰縮放（同 F1） |
| 0x4B | ← | 游標左移 |
| 0x4C | Numpad 5 | 切換下一個可行動角色（與 Esc 同一分支；不在 wait 層 remap） |
| 0x4D | → | 游標右移 |
| 0x50 | ↓ | 游標下移 |
| 0x52 | Numpad 0 / Insert | wait 層 remap 成 0x1C (Enter) |
| 0x53 | Numpad . | wait 層 remap 成 0x01 (Esc) |
| 0xE0 | 擴展鍵前綴 | wait 層 remap 成 0x1C (Enter) |

## 輸入相關 globals

| 位址 | 名稱 | 內容 |
|---|---|---|
| `0x53A8D` | `data_fd2_input_int16_regs` | 共用 union REGS scratch（INT 10h/16h 皆用）；byte 0 (AL) 別名 `data_fd2_input_last_key_pressed`＝最後按鍵的 ASCII |
| `0x53A8E` | `data_fd2_input_key_input_mode` | 上述 union 的 byte 1 (AH)：呼叫前預設 0x10 當 INT 16h 功能號，呼叫後為 scancode（remap 對象、也是各 wait 迴圈的回傳值） |
| `0x539F0` | `data_fd2_input_idle_current_bios_tick_word` | idle 迴圈每輪的 BIOS tick 快照（write-only latch，無 runtime reader） |
| `0x539F2` | `data_fd2_input_idle_last_rendered_tick_word` | 上次重繪游標閃爍 frame 的 tick，與現值不同才重繪（~18.2 Hz） |
| `0x53A0C` | `data_fd2_engine_wait_one_bios_tick_last_seen` | `fd2_wait_one_bios_tick` 的 reference tick |
| `0x53A2C` | `data_fd2_engine_wait_n_bios_ticks_last_seen` | `fd2_wait_n_bios_ticks` 的 reference tick |
| `0x54127` | `data_fd2_ui_recruitment_screen_repaint_tick_latch` | `fd2_wait_input_with_recruitment_repaint` 的節流重繪 tick latch |

`data_fd2_ui_play_active_flag` @ 0x51AAC 是 gameplay-active 閘門，唯一讀者是
`fd2_render_terrain_info_hud_panel`（值為 0 時抑制地形資訊 HUD 面板），在回合切換 /
章節 init-end / 存讀檔轉場前後被清 0 再設回 1，正典歸 gfx.md，非 input 專屬。

## BIOS 輪詢 + INT 16h 讀取的設計

遊戲並非完全繞開 INT 16h，而是分成兩段：等待期間直接讀 BIOS buffer 的 HEAD/TAIL
（0x41A/0x41C）做非阻塞判斷「是否有 pending key」，這樣 idle 迴圈才能在等待時持續跑
palette cycling 與 frame 重繪；一旦確認有鍵，才用 `int386(0x16, ...)`（INT 16h AH=10h）
真正取出 scancode。直接讀 buffer 而非用阻塞式 INT 16h 輪詢的好處：避免每輪 INT 呼叫的
overhead、與同一迴圈讀 tick (0x46C) 的節拍同步較佳。DOS/4GW flat memory model 下
BIOS data area 可經 segment 0 mapping 直接讀寫，這在 90 年代初的 DOS Protected Mode
遊戲很常見。
