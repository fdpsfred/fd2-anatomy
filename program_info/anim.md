# anim（動畫系統）

戰鬥、法術、必殺技、召喚與過場的所有動畫都由 `anim/` 這組模組驅動。核心是把
FIGANI / 精靈圖 byte-stream 逐格合成到工作緩衝區，再以 mode-13h blit 推到螢幕，並在
特定 frame 插入 SFX、MP 扣除與 VGA palette 特效。

## 驗證對象

**src**：`anim/anicine.c`、`anim/anidec.c`、`anim/aniend.c`、`anim/anispell.c`、
`anim/aniui.c`、`anim/aniwalk.c`、`anim/anicombt.c`、`anim/anisummn.c`

**主要 Ghidra 對象**（名稱@位址，即時核對）：

- `fd2_play_figani_animation_loop @ 0x2B659` — FIGANI pose-stream 主播放器
- `fd2_step_figani_pose_animation @ 0x2B9A1` — 單一 pose 的 sub-frame stepper
- `fd2_load_figani_sfx_bank @ 0x2BC9A` — 由 FIGANI header 取 FDOTHER.DAT SFX bank
- `fd2_play_char_intro_zoom_anim @ 0x29164` — 角色登場 zoom-in
- `fd2_flash_char_hit_sprite @ 0x2A289` — 施法／受擊閃光疊層
- `fd2_animate_spell_impact_per_target @ 0x1C4CC`、`fd2_animate_spell_full_screen_flash @ 0x1CAC7`、
  `fd2_animate_spell_overlay_blink @ 0x1CD17` — 法術視覺三段
- `fd2_execute_summon_spell_cast @ 0x27FC9`、`fd2_interpolate_palette_range_toward_color @ 0x286BD` — 召喚 palette FX
- `fd2_play_death_animation_and_mark_dead @ 0x1DB65` — 死亡／消散動畫
- `fd2_slide_panel_down_step @ 0x1974C`、`fd2_slide_panel_up_partial_step @ 0x1839B`、
  `fd2_render_status_screen_slide_frame @ 0x18409` — 面板滑動
- `fd2_ai_pass_turn_with_heal @ 0x13FD4` — 待機回復動畫

**主要資料表**：`data_fd2_animation_spell_sprite_offset_table @ 0x51F33`、
`data_fd2_animation_spell_frame_count_table @ 0x51F54`、
`data_fd2_animation_spell_sfx_id_table @ 0x51F75`、
`data_fd2_animation_spell_overlay_blink_mask_table @ 0x52006`、
`data_fd2_animation_spell_palette_flash_table @ 0x51AAD`、
`data_fd2_battle_summon_spell_palette_r_table @ 0x5254F`（`_g_table @ 0x52553`、`_b_table @ 0x52557`）、
`data_fd2_battle_summon_spell_sfx_bank_index_table @ 0x5255B`、
`data_fd2_ui_anim_sprite_sheet_ptr @ 0x53A81`、`data_fd2_vga_palette_data_ptr @ 0x53A65`

**相關資源檔**：FIGANI.DAT（格式正典見 `resource_info/figani.md`）、TAI.DAT、BG.DAT、
FDOTHER.DAT、FDSHAP.DAT。

## FIGANI 必殺技／召喚 pose-stream 播放

`fd2_play_figani_animation_loop @ 0x2B659` 是所有特殊攻擊與召喚視覺的核心 pose 迭代器。
它讀 caster FIGANI byte-stream（迴圈上界 = caster_figani 的 header byte +2），逐 pose 播放，
target FIGANI 作為包覆的伴隨 pose：

- pose entry 三個關鍵欄位：`+4` type（1 = 施法幀）、`+5` sfx_hook_id（0 = 無）、`+6` sub-frame 數。
  完整 FIGANI header／pose byte layout、idx 公式（`portrait_id × 3 + {0 basic / 1 extended / 2 placeholder}`）
  與 entry 分布見 `resource_info/figani.md`。
- 當 spell_id 屬 `0x18`（必殺技）或 `0x1C..0x1E`（特殊技）且 pose 有 sfx hook 時，播放該 hook 音效。
- pose type == 1（施法幀）：扣施法者 MP（`fd2_deduct_caster_mp`）、`fd2_flash_char_hit_sprite`
  施法閃光；若 spell_id `< 10` 或 `> 0x1F`，以 remap 表（來源 `data_fd2_tile_anim_table_base @ 0x53A6D`）
  對背景做兩次 palette-remap blit（座標 (0, 0x32) 與 (0xA4, 0x9D)），並把 `palette_write_countdown` 設 6。
- 每個 sub-frame：還原背景 → 依施法者 `bTeam` 決定 caster／target sprite 疊放順序合成 → flush 到 0xA0000。
  `palette_write_countdown > 0` 期間，由 `data_fd2_animation_spell_palette_flash_table @ 0x51AAD`
  取該 spell_id 的 RGB（R 在 +0、G 在 +0x24、B 在 +0x48，共 36×3 = 108 bytes）寫 VGA palette index 0
  作為施法高光，延遲後復原。

| 位址 | 名稱 | 角色 |
|---|---|---|
| `0x2B659` | `fd2_play_figani_animation_loop` | pose 主迭代器 |
| `0x2B9A1` | `fd2_step_figani_pose_animation` | 單 pose 的 sub-frame stepper |
| `0x2BC9A` | `fd2_load_figani_sfx_bank` | FIGANI header +4 byte 經 6-byte lut 換 FDOTHER.DAT entry，載入 SFX bank |
| `0x29164` | `fd2_play_char_intro_zoom_anim` | 9-frame 角色登場（每幀 10px 滑入 + palette 由 0x30 漸暗到 0；依 bTeam 走上半／下半版面）|
| `0x2A289` | `fd2_flash_char_hit_sprite` | 依 team 選螢幕位置（enemy 0xC080／ally 0x5AB，ch24 char 0x11 例外）畫受擊高光 |

## 法術視覺三段管線

大型攻擊法術以三個 animator 疊出完整視覺，皆對 target 陣列逐目標作用：

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x1C4CC` | `fd2_animate_spell_impact_per_target` | 最常用的命中動畫：逐 frame、逐 target 貼精靈幀 |
| `0x1CAC7` | `fd2_animate_spell_full_screen_flash` | 雙緩衝白（變體 0x4A）／彩（0x4B）交替 strobe，4 cycle × 2 blit，每 blit 延遲 0x5A ms，總長約 720ms |
| `0x1CD17` | `fd2_animate_spell_overlay_blink` | 10-frame 逐目標 tint 漸隱（alpha 由 7 遞減到 0）|

`fd2_animate_spell_impact_per_target` 由三張平行 byte-table 驅動（spell_id 0..35 索引）：

- `data_fd2_animation_spell_sprite_offset_table @ 0x51F33` — sprite frame 起始偏移
- `data_fd2_animation_spell_frame_count_table @ 0x51F54` — 動畫 frame 數
- `data_fd2_animation_spell_sfx_id_table @ 0x51F75` — 主 SFX 觸發 frame（frame 0 觸發；值為 0 表無）

`fd2_animate_spell_overlay_blink` 的 tint mask 取自
`data_fd2_animation_spell_overlay_blink_mask_table @ 0x52006`。

impact loop 另對特定法術在特定 frame 排額外 SFX：spell 0x16（frame 7 → SFX 3）、
0x19（frame 3/6 → 5）、0x12（frame 4 → 7）、0x13（frame 3/6 → 8）、0x08（frame 3/6 → 0xA）、
0x09（frame 0xF/0x13 → 0xF）。

## 召喚 palette-cycle FX

`fd2_execute_summon_spell_cast @ 0x27FC9` 處理召喚系法術
0x20（熾天使）／0x21（風妖精）／0x22（破壞神）／0x23（暗邪鬼），有多階段動畫、per-summon
SFX 掛勾與家族專屬結算。每個召喚的目標色由三張表以 `spell_id - 0x20` 索引：

- `data_fd2_battle_summon_spell_palette_r_table @ 0x5254F` = `{3F, 33, 35, 35}`
- `data_fd2_battle_summon_spell_palette_g_table @ 0x52553` = `{3F, 39, 00, 3A}`
- `data_fd2_battle_summon_spell_palette_b_table @ 0x52557` = `{3F, 3F, 00, 09}`

第四張 `data_fd2_battle_summon_spell_sfx_bank_index_table @ 0x5255B` = `{5B, 5C, 5D, 5E}`
不是 RGB 表，而是各召喚 SFX bank 的 FDOTHER.DAT entry index。

`fd2_interpolate_palette_range_toward_color @ 0x286BD` 是一個 palette-range 線性內插原語：
對 palette index 區間 `[start_idx, end_idx)`，把原始 palette（`data_fd2_vga_palette_data_ptr @ 0x53A65`）
朝目標色 (R, G, B) 混合，係數為 `blend / 0x28`（blend = 0x28 → 純原色，blend = 0 → 純目標色），
經 DAC port 0x3C8/0x3C9 寫入。它本身只做一趟寫入，動畫的「逐幀漸變」由呼叫端的迴圈負責：召喚的
strobe 段以 blend 由 0x28 每步 -4 呼叫它，收尾的淡入段以 blend 0..0x28（共 0x29 次、涵蓋整段
palette 範圍 [0, 0xFF)）逐幀呼叫產生召喚色淡入。標題／主選單畫面
`fd2_title_attract_and_main_menu @ 0x1F894`（標題吸引 cinematic + 主選單，重用結局／credit 美術，
main 每次回到頂層都會執行）也用它做紅／青 tint 漸變。

## 死亡／消散動畫

`fd2_play_death_animation_and_mark_dead @ 0x1DB65`：

- 先掃全隊，把「HP 歸零、`bFlags` bit0 尚未設、且位於可視戰鬥視窗內」的角色收進 on-screen 陣列；
  若沒有任何一個在畫面內，直接把所有 HP 歸零者設 `bFlags = 1` 靜默判死並返回。
- 第一段 13-frame 閃爍（blink key = frame % 4）；閃爍結束後把 HP 歸零者 `bFlags = 1`（永久死亡）。
- 第二段 12-frame 消散（分 0..5 與 6..11 兩半），puff 精靈取自
  `data_fd2_ui_anim_sprite_sheet_ptr @ 0x53A81`，index `[6 + (frame + 0x44) * 4]`；
  死亡音效為 `fd2_play_sfx_with_handle(FDOTHER bank, 3, 1)`。

## 面板／對話框滑動動畫

| 位址 | 名稱 | 用途 |
|---|---|---|
| `0x1974C` | `fd2_slide_panel_down_step` | 章節 portrait／對話面板的單幀滿版滑動：還原背景 snapshot、複製至多 0x56 列（每列 310 bytes）、把整張 320x200 workspace flush 到 VRAM；方向無關，caller 逐幀驅動 y_offset 做 slide-in／slide-out。城鎮選單、對話、商店、轉職、status 皆用它 |
| `0x1839B` | `fd2_slide_panel_up_partial_step` | 中央面板的部分步：把至多 0x66 列（每列 310 bytes）複製進 workspace，不還原背景也不 flush，供 status slide frame 內部使用 |
| `0x18409` | `fd2_render_status_screen_slide_frame` | status／選單面板 12-frame 滑動的一幀（frame 0..0xB）：左面板水平滑、右面板垂直滑（frame ≥ 9 全出畫面不畫）、中央面板上滑（frame < 6）；caller 驅動 intro 0xB→0 或 outro 0→0xB |

左／右面板的 layer painter（`fd2_paint_status_panel_layer_left @ 0x182AD`、
`fd2_paint_status_panel_layer_right @ 0x18312`）位於 `gfx/rndstat.c`，屬 gfx 模組，見 `gfx.md`。

## 其他動畫函數族

- `anidec.c`：ANI 檔的 chunk-based frame decoder（`fd2_ani_decoder_*`：palette／row／sparse 各類 chunk opcode）。
- `aniui.c`：城鎮／UI 動畫（金錢增減滾動、商店卷動、螢幕震動、入隊 appear 特效、白光 palette pulse）。
- `aniwalk.c`：角色四方向走一步（`fd2_walk_step_up/down/left/right`）與沿路徑走的
  `fd2_walk_path_animation_loop`，以及 tile 事件動畫 tick。
- `anisummn.c`：召喚動畫各 variant 的 tick state machine（`fd2_tick_summon_anim_variant_a..e` 等）。
- `aniend.c`：標題吸引畫面＋主選單、遊戲結束（戰敗）過場（`fd2_play_game_over_sequence`：主角索爾
  runtime_char[0] 死亡時播 2 幀 game-over sprite）、隱藏關解鎖過場、遊戲結局 cinematic。
- `anicine.c`：戰鬥 hit cinematic、必殺技／召喚前置 cinematic、章節登場 FIGANI 動畫。

## 待機回復動畫

`fd2_ai_pass_turn_with_heal @ 0x13FD4` 是「待機／休息」動作：AI 回合派遣在沒有評分動作可選時的
fall-through（case 1/3/5/7/11），以及玩家該回合未移動而選「待機」時共用。當條件成立
（HP 未滿、`pStatus_flags_block[4]` 中毒旗標為 0、`+0x26` 麻痹旗標為 0）時，鏡頭 pan 到該角色、
播放 glow 疊層與兩幀精靈、放回復音效（idx 4），並回復 `HP_max / 5`（20%，clamp 到上限）後回傳 1。
這是重甲 boss「拖不死」的程式根源。AI 回合派遣脈絡見 `battle.md`。
