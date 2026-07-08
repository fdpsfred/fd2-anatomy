# 法術系統 (spell)

戰鬥中法術與道具「使用效果」的完整處理鏈：從選單選出一個法術、選定目標，到播放運鏡動畫、
套用實際數值效果。分三層：**派遣層**（依 spell_id 查表叫出對應 handler）、**運鏡層**
（spellcin.c 的全螢幕過場動畫）、**效果套用層**（spelleff.c 依 effect_code 分派實際傷害／
治療／狀態）。本檔也收錄「移動 XOR 施法」規則。

## 驗證對象

- 對應 src：`src/spell/spell.c`（24 個 spell handler wrapper）、`src/spell/spellsel.c`
  （選單／目標選取）、`src/spell/spellcin.c`（運鏡動畫 worker）、`src/spell/spelleff.c`
  （effect_code 分派與各效果 worker）。
- 主要 Ghidra 對象（位址皆即時核對）：
  - 派遣表 `data_fd2_battle_spell_handler_table` @ 0x51D01（`pointer[28]`，索引 spell_id 0x00..0x1B）
  - 選單：`fd2_spell_selection_menu_main` @ 0x1CFF0、`fd2_build_usable_spell_list` @ 0x1C269、
    `fd2_grant_spell_to_char` @ 0x1D79C、`fd2_play_spell_palette_flash_with_sfx` @ 0x1D6C8
  - 運鏡：`fd2_cast_earthquake_spell_with_screen_shake` @ 0x21548、
    `fd2_animate_warp_teleport_char` @ 0x22253、`fd2_cast_screen_wide_spell_with_fade` @ 0x24618、
    `fd2_execute_special_attack_skill` @ 0x276EC、`fd2_execute_summon_spell_cast` @ 0x27FC9
  - 效果套用：`fd2_apply_use_effect_dispatch` @ 0x20C6F、
    `fd2_apply_item_stat_modifier_with_anim` @ 0x21082、`fd2_cast_spell_17_teleport` @ 0x2218A
  - 跨模組但本檔會引用（位址已核）：`fd2_play_spell_cast_sequence` @ 0x2A6BD（運鏡總指揮，正典見
    anim.md）、`fd2_execute_ai_offensive_spell` @ 0x15311（AI 施法路徑，battle.md）、
    `fd2_player_action_menu_loop` @ 0x18890 與 `fd2_player_inline_action_menu_dispatch` @ 0x18D8C
    （玩家行動選單，ui_menu.md）、`fd2_mark_char_acted_this_turn` @ 0x13512、
    `fd2_get_spell_effect_entry` @ 0x4E516（table accessor，table.md）
- 相關資源檔：FDTXT（法術名文字，page = spell_id + 0x1B9）、FDOTHER.DAT（狀態／召喚 SFX bank、
  召喚 sprite、warp SFX bank 索引 0x51）、FIGANI.DAT（必殺技／召喚的角色 pose）、
  TAI.DAT／BG.DAT（必殺技／召喚背景層）、FDSHAP.DAT（戰場 tile 圖）、spell_effect 資料表
  （每 spell 一筆，欄位細節見 assets/tables 與 table.md）。

## 一、派遣層：spell_id -> handler

核心是函數指標表 `data_fd2_battle_spell_handler_table` @ 0x51D01，共 28 個 slot，以 spell_id
（0x00..0x1B）直接索引。spell.c 定義其中 24 個 handler wrapper；spell_id 9、0x17、0x19 的 handler
定義在 spelleff.c。slot 0x18 與 slot 0x10 指向同一個 handler（`fd2_cast_spell_10_variant_b`
@ 0x22153）；0x18 是「淒煌斬」的特技槽，正常由 `fd2_execute_special_attack_skill` 走，這個表
內的 0x18 只是備援。

多數 handler 是薄 wrapper，把 spell_id／effect_id 硬編後轉呼叫共用 worker，可分成幾族：

| spell_id | handler 族 | 委派的 worker |
|---|---|---|
| 0-3, 8 | targeted-blink 攻擊 | `fd2_execute_offensive_targeted_spell` @ 0x21227 |
| 4-7 | full-screen-flash 攻擊 | `fd2_execute_offensive_full_screen_flash_spell` @ 0x213B7 |
| 9 | 單體攻擊（咒殺術，AoE=0） | `fd2_execute_offensive_single_target_spell_id_9` @ 0x214AD |
| 0xA-0xC | 地震（0xB/0xC 前置升起特效） | `fd2_cast_earthquake_spell_with_screen_shake` @ 0x21548 |
| 0xD-0x10 | variant-B 治療 | `fd2_execute_variant_b_heal_cast` @ 0x21B18 |
| 0x11-0x13 | AP／DP／速度 buff | `fd2_cast_ap_boost_spell` / `_dp_boost_spell` / `_speed_boost_spell` |
| 0x14-0x15 | 狀態解除（解毒／祛麻） | `fd2_cast_status_cure_spell` @ 0x22AF6 |
| 0x16, 0x1A, 0x1B | 狀態施加（封咒／毒擊／麻痺） | `fd2_cast_status_inflict_spell` @ 0x22D1B |
| 0x17 | 傳送 | `fd2_cast_spell_17_teleport` @ 0x2218A |
| 0x19 | 行動術（再行動） | `fd2_execute_reactivate_spell_id_25` @ 0x22C04 |

### 兩個派遣入口與兩條路徑

這張表有兩個消費端，各自對 spell_id 分流成「大運鏡」與「查表 handler」兩條路：

- 玩家端 `fd2_spell_selection_menu_main` @ 0x1CFF0：`spell_id < 9 || spell_id == 0x18 ||
  spell_id > 0x1B` 走 `fd2_play_spell_cast_sequence`（大運鏡總指揮）；其餘（狀態／輔助帶 9..0x1B，
  0x18 除外）先載入狀態 SFX、播放 `fd2_play_spell_palette_flash_with_sfx`，再呼叫
  `handler_table[spell_id]`。
- 敵方 AI `fd2_execute_ai_offensive_spell` @ 0x15311：`spell_id < 10 && game_speed_flag
  (@0x53AF9) == 0` 走 `fd2_play_spell_cast_sequence`；否則走 `handler_table[spell_id]`。因此
  快速模式（game_speed_flag != 0）下連基本攻擊法術（0-8）也改走表內的簡化 handler（略過大運鏡），
  這正是 slot 0-8 存在的原因——玩家端永遠給基本法術完整運鏡，AI 快速模式則用表內精簡版。

`fd2_play_spell_cast_sequence` 自身再做第二層分派：`spell_id >= 0x20` -> 召喚系
（`fd2_execute_summon_spell_cast`）；`spell_id == 0x18` 或 0x1C..0x1F -> 必殺技
（`fd2_execute_special_attack_skill`）；其餘走內建的通用施法序列。此函數的細節屬 anim.md 正典，
本檔只記其分派出口。

## 二、選單與目標選取（spellsel.c）

- **已學法術枚舉** `fd2_build_usable_spell_list(char_idx, out_buf)` @ 0x1C269：掃 runtime_char
  的 5-byte `spells_known_bitmap`（+0x1A，40 bit，只有 id 0x00..0x23 是真法術），每個 set bit
  產出 `spell_id = byte*8 + bit`。`out_buf == NULL` 時只回數量（供先算緩衝大小）。此處純枚舉，不查
  MP 是否足夠。
- **授予法術** `fd2_grant_spell_to_char(char_idx, spell_id)` @ 0x1D79C：把 spell_id 對應的 bit
  set 進同一張 `spells_known_bitmap`；由升級處理在該級可習得新法術時呼叫。
- **繪製清單** `fd2_draw_spell_selection_list` @ 0x1CEED：把已學法術排成 4 欄格狀清單。每格畫
  法術名（all_game_text page = spell_id + 0x1B9）、MP 圖示 sprite 0x5C、以及 2 位數 MP 消耗
  （spell_effect 記錄的 +5 byte）。被 highlight 的格用色 0xC9（黃），其餘 0xCD（紅）。
- **輸入迴圈** `fd2_spell_select_input_loop` @ 0x1D51D：一幀選單輸入。上下左右移動游標
  （游標存共用的 `data_fd2_ui_menu_cursor_idx` @ 0x53C57，上下會 wrap、左右每步 4 格）；
  Enter/Space 確認時重建 id 清單，只有「選中法術的 MP 消耗（record +5）<= 施法者 mp_current
  （+0x44）」才回 1（提交），否則留在選單；Esc 回 -1。
- **選單主體** `fd2_spell_selection_menu_main` @ 0x1CFF0：玩家在戰鬥中選 spell 後開的 modal。
  配置三個 64000-byte 緩衝（accumulator／snapshot／composed），快照 VGA 畫面、疊上狀態版面與法術
  清單，做 12 幀滑入；反覆呼叫輸入迴圈直到提交或取消；再 12 幀滑出、還原畫面、釋放緩衝。
  提交後讀 spell_effect 記錄決定目標選取模式：
  - `pSpell[3]` = aoe_kind（0 = 單體、非 0 = AoE 範圍）
  - `pSpell[4]` = 施法半徑／形狀（同時決定動畫凍結期 `battle_anim_phase = pSpell[4] + 2`）
  - `pSpell[6]` = AoE 等待模式參數
  目標分三支：**傳送類**（aoe_kind != 0 且 spell_id == 0x17）做兩段選取（先來源範圍、再自由目的
  地，wait mode 6），記下傳送目的座標；**單體**（aoe_kind == 0 或 spell_id == 0x17）在射程內選點
  （射程內有敵 wait mode 4、無敵 5）；**AoE**（其餘）選 AoE 錨點（wait mode = pSpell[6]），
  spell 0x1E 用直線掃描解算命中名單，其餘做第二次 AoE 計算。命中後依前述兩條路派遣，最後收集
  死亡掉落、播死亡動畫、處理掉落。
- **狀態法術色閃** `fd2_play_spell_palette_flash_with_sfx` @ 0x1D6C8：狀態類法術的施法特效，
  播放狀態 SFX 後把 VGA DAC index 0 用該法術的招牌色閃 4 次。RGB 來自 108-byte（36×3）色表，
  以三個相連 36-byte 平面（R/G/B）依 spell_id 索引。

## 三、運鏡層（spellcin.c）

全螢幕過場動畫 worker，多半自行 malloc 一塊 0x25680 背景快照、逐幀還原並疊繪、blit 到 mode-13h
framebuffer（0xA0504／0xA0000），完成後 recomposite 戰場。實際傷害由呼叫端或下述效果 worker
負責，運鏡本身只管畫面。

- `fd2_cast_earthquake_spell_with_screen_shake` @ 0x21548：地震。3 幀大位移慢震 + 60 幀快抖
  （每 6 幀播 SFX），之後對每個目標 `fd2_calc_magic_damage` 並顯示傷害／miss。震動參數表
  int[9] @ 0x52096（3 組 X 偏移／Y 偏移／縮放）。
- `fd2_play_rising_pre_cast_effect` @ 0x2189A：10 幀「升起火花」前置特效（spell 0xB/0xC 及召喚
  ／劇情共用）。
- `fd2_execute_variant_b_heal_cast` @ 0x21B18：variant-B 治療執行器（spell 0xD-0x10），對每個
  目標套 `fd2_apply_heal_spell_to_target` 並顯示治療量（glyph 0x69）。
- `fd2_play_variant_b_slide_pre_effect` @ 0x21EB1：variant-B 的 16 幀圓環「滑動」前置動畫，四個
  0xD/0xE/0xF/0x10 thunk 各給不同 (offset0, step) 參數。
- `fd2_cast_screen_wide_spell_with_fade` @ 0x24618：Boss／章末全螢幕徑向衝擊波（9 幀擴張 + 長停
  + palette flash 淡入），供終極／龍息類法術與章節過場用。
- **角色傳送三段動畫** `fd2_animate_warp_teleport_char` @ 0x22253：把單位從現位置搬到目的 tile，
  含來源側開門＋崩解、目的側逐列 pop-in、目的側展開。拆成
  `fd2_animate_warp_portal_open_at` @ 0x22470（11 幀開門）、
  `fd2_animate_warp_out_collapse` @ 0x22547（6 幀崩解，回傳最終 sprite 供展開用）、
  `fd2_animate_warp_in_expand` @ 0x22656（10 幀展開）。真正改寫 pos_x/pos_y 是在來源側崩解之後。
  用 FDOTHER.DAT[0x51] 的 warp SFX bank。
- **必殺技** `fd2_execute_special_attack_skill` @ 0x276EC：角色專屬武技（spell 0x18 淒煌斬、
  0x1C 熾炎刀多段、0x1D 音速刃、其餘 fallback）。傷害倍率 {0x18:15, 0x1C:20, 0x1D:12,
  default:18}，`raw = (int16)caster.ap * 倍率 / 10`，per target 傷害 =
  `clamp(target.hp, fd2_apply_damage_and_award_xp(tid, raw - dp))`，HP 隨動畫逐段套用
  （0x1C 分 8 段命中，其餘 1 段）。用 BG.DAT／TAI.DAT／FIGANI.DAT 做全角色對角色 FIGANI 疊繪。
- **召喚系** `fd2_execute_summon_spell_cast` @ 0x27FC9：spell 0x20（熾天使）／0x21（風妖精）／
  0x22（破壞神）／0x23（暗邪鬼）的多段運鏡：施法 pose、sprite 滑入滑出、逐 pose 幀召喚動畫（含
  per-summon SFX hook）、可選閃頻 palette 循環、淡回戰場，最後依 spell_id 做玩法效果：
  - 0x20 -> `fd2_apply_attack_spell_damage`（攻擊魔法傷害）
  - 0x21 -> 清目標狀態 byte [+0x25..+0x27] 後群體補血 800
  - 0x22 -> AP + DP + 速度三 buff（各之間 reset queue idx）
  - 0x23 -> 三次狀態施加（spell 0x1A/0x16/0x1B，byte offset 0x25/0x27/0x26）
  每 summon 的 palette R/G/B 與 SFX bank index 存四張 4-byte 表 @ 0x5254F/53/57/5B，以
  `spell_id - 0x20` 索引；召喚 sprite 從 FDOTHER.DAT[spell_id + 0x21] 載入。

## 四、效果套用層（spelleff.c）

`fd2_apply_use_effect_dispatch(caster, inv_slot, target_count, targets)` @ 0x20C6F 是道具／
法術「使用效果」的頂層分派。先載狀態 SFX、reset AoE 計數，讀 item_effect 記錄：
`effect_code = item_entry[0xD]`、`effect_param = *(u16*)(item_entry + 0xE)`，依 effect_code
（涵蓋 0x05..0x18）分派，最後 `pending_xp_credit = 0`、停 SFX、收集掉落＋死亡動畫＋處理掉落。

| effect_code | worker | 說明 |
|---|---|---|
| 0x05 | `fd2_cast_group_hp_heal_spell(param)` | 群體 HP 補；用畢消耗道具 |
| 0x06 | `fd2_cast_status_cure_spell(0x14, byte +0x25)` | 解毒（清 status_flags_block[4]），消耗 |
| 0x07 | `fd2_cast_status_cure_spell(0x15, byte +0x26)` | 解麻痺（清 status_paralysis_flag），消耗 |
| 0x08 | `fd2_apply_item_stat_modifier_with_anim(field +0x37, anim 0x11)` | 永久屬性卷軸（寫 combat_aux_block[0x10]）|
| 0x09 | 同上（field +0x39, anim 0x12） | 永久屬性卷軸（寫 combat_aux_block[0x12]）|
| 0x0A | 同上（field +0x3E, anim 0x13） | 永久屬性卷軸（寫 dx_total，+0x3E）|
| 0x0B | 手寫迴圈 | MP 補充：mp_max==0 顯示 miss，否則 `fd2_apply_mp_heal_and_award_xp`，消耗 |
| 0x0C | `fd2_cast_speed_boost_spell` | 加速 buff |
| 0x0D | `fd2_cast_group_hp_heal_spell(param)` | 群體 HP 補；此路**不**消耗道具 |
| 0x0E | `fd2_cast_status_inflict_spell(0x1B, byte +0x26)` | 施加麻痺 |
| 0x0F | `fd2_cast_dp_boost_spell` | 防禦 buff |
| 0x10 | `fd2_cast_ap_boost_spell` | 攻擊 buff |
| 0x11 | `fd2_apply_item_stat_modifier_with_anim(field +0x42, anim 0x0D)` | 永久 +HP 上限（hp_max）|
| 0x12 | 同上（field +0x46, anim 0x0D） | 永久 +MP 上限（mp_max）|
| 0x13 | 同上（field +0x3B, anim 0x13） | +移動力（MV 預算，見下）|
| 0x14 / 0x18 | 手寫迴圈 | 攻擊魔法傷害（impact + overlay-blink + `fd2_calc_magic_damage`）|
| 0x15 | `fd2_apply_attack_spell_damage` | 攻擊魔法傷害（impact + full-screen-flash）|
| 0x16 | `fd2_cast_status_inflict_spell(0x16, byte +0x27)` | 施加封咒（combat_aux_block[0]）|
| 0x17 | `fd2_cast_spell_17_teleport` | 傳送 |

### effect_code 0x13：移動力修改要保住 exp_carry

永久屬性卷軸都經 `fd2_apply_item_stat_modifier_with_anim` @ 0x21082，它對 `field_offset` 做
**16-bit** 加法（asm `ADD word ptr [EAX], DX`）。effect_code 0x13 的 field 是 +0x3B（移動力
MV 預算，即 combat_aux_block[0x14]），16-bit 寫入會一併動到相鄰的 +0x3C（`exp_carry`，升級用的
經驗餘額）。因此 0x13 分支在呼叫前先快照目標的 exp_carry、呼叫後再還原，避免加移動力時破壞 XP
carry byte。（runtime_char 佈局正典見 overview.md；+0x3B = MV 移動力、+0x3C = exp_carry。）

### 各 worker 的判定與 XP 給法

狀態／buff 類 worker 皆有共通結構：`level_mod = status_flags_block[0]`（等級 byte），若
`job_id` 落在進階職業區間（9..0x18）再 +30，然後把 `level_mod × 倍率` 累加進
`data_fd2_battle_pending_xp_credit` @ 0x53EC8。倍率分層：

- buff（AP／DP／速度）：×2
- 狀態解除（解毒／祛麻）：×4
- 狀態施加、行動術：×8（最高層）
- 傳送 `fd2_cast_spell_17_teleport`：×10

AP／DP／速度 buff（`fd2_cast_ap_boost_spell` @ 0x22721、`_dp_boost_spell` @ 0x22866、
`_speed_boost_spell` @ 0x22997）都是一次性、不疊加：各自有一個 buff 計時 byte
（status_flags_block[1]/[2]/[3]），計時非 0（已上過 buff）就顯示 miss；為 0 才生效，並把計時設成
`(rng % 4) + 2` 回合。AP/DP 加量 = `(int)(1.0 + stat × 0.15)`（Watcom 向零截斷），速度則固定
+15（同時加 dx_current 與 stat4_current）。狀態施加 `fd2_cast_status_inflict_spell` @ 0x22D1B
還要過三關才命中：目標尚無該狀態、job_id 不是 boss/免疫職（0x19/0x1A）、且約 50% RNG
（`rng % 100 < 0x32`）。（RNG 演算法正典見 battle.md；`fd2_advance_rng_state` 回 16-bit
零擴展值，故各 signed modulo 恆非負。）

## 五、移動 XOR 施法

法師這回合**若移動了就不能施法**，必須原地不動才能放；物理攻擊沒有這個限制（移動後仍可打）。
此規則不是在 spell 模組內做，而是由玩家行動選單 `fd2_player_action_menu_loop` @ 0x18890 透過選單
可用性旗標實作：

- 玩家選好目的 tile 後 `fd2_pathfind_to_destination` 算路徑步數。若步數非 0（實際走了路），
  動畫走完後對 `portrait_id` 不在 {0x12, 0x13, 0x22} 的角色設 `menu_state[1] = 1`，然後以
  `have_moved = 1` 呼叫 `fd2_player_inline_action_menu_dispatch` @ 0x18D8C。
- `menu_state[1]` 對映內層選單的 Spell 槽（`pSlot_disable_arr[1]`）；設 1 即把 Spell 灰掉。
- 因此「移動過的角色」Spell 選項不可選，只有 portrait_id 0x12/0x13/0x22 三種角色豁免（可移動後
  再施法）。若角色**原地不動**（路徑步數 0），派遣時不強制 `menu_state[1]`，Spell 維持可選。
- Attack 槽（slot 0）不受此移動旗標影響，移動後仍可攻擊。

內層派遣另有兩道與此獨立的 Spell 禁用條件：`fd2_build_usable_spell_list(char_idx, 0) == 0`
（無已學法術）、或 `combat_aux_block[0] != 0`（被封咒／沉默）。

### acted 旗標（本回合已行動）

角色完成動作後由 `fd2_mark_char_acted_this_turn` @ 0x13512 設 `flags`（+0x05）**bit7**
（asm `OR byte ptr [rc + 0x5], 0x80`）；此旗標是「本回合已行動」，與 +0x3C 的 exp_carry 無關。
回合階段起始時由 `fd2_clear_all_chars_acted_flag`（btl_turn.c）清掉。行動術（spell 0x19，
`fd2_execute_reactivate_spell_id_25` @ 0x22C04）的效果就是對「已行動」的目標清掉 bit7，讓它可以
再行動一次——若目標尚未行動（bit7 未設）則無事可做、顯示 miss。

## 共用 FX 狀態

- `data_fd2_battle_spell_aoe_count_and_fx_queue_idx` @ 0x53EC4：雙用途 32-bit cell。每次施法起始
  reset 為 0；治療／狀態 worker 以其 != 0 判斷是否重播投射物路徑動畫；同時也是傷害數字／miss 指示
  寫入 FX queue 的游標（每批 +4）。
- `data_fd2_battle_pending_xp_credit` @ 0x53EC8：戰鬥全域待結算經驗累加器。各效果 worker 把該次
  行動賺得的 XP 加進去，升級處理再抽乾。玩家端在 Spell 提交後，會把累積值除以「施法者等級（+30
  若為進階職業）」做節流（見 ui_menu.md 的行動選單）。
