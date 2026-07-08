# 第 27 章 — 命運的交會點

抵達古代人遺跡的傳送站，悠妮被機甲的傳導衝擊喚醒了失去的記憶。本章是 GOOD / BAD ENDING 的分歧點：隊伍持有「天空之鑰」時全員一同傳送上黃金城、續往 ch28+；沒有天空之鑰則悠妮獨自傳送離去，遊戲在此結束。

## 加入角色

兩條 ending 路徑皆無角色加入。

## 敵人配置

FDFIELD.DAT[27]：機甲衛兵 + ASR-07 控制單元。

## 寶物

待解：寶物清單需從 FDFIELD.DAT tile_event 解析。

## 商店

連戰最後章節（本章戰前為最後可買賣處，ch28 之後進入黃金城內部不再有商店機會）。

## 特殊機制

- **Init 條件式對話**：init 以 `fd2_any_char_has_item(100)`（天空之鑰）判定，持有時在 page 0 之後額外插播 page 3（希爾法說明中央平台的傳送需要天空之鑰）。
- **Init 三段小範圍施法過場**：init 於三個不同游標位置各呼叫一次 `fd2_cast_screen_wide_spell_with_fade`，演出劇情中三方人物施放魔法／技能的小範圍光效。
- **GOOD / BAD ENDING 分歧**：end handler 用 `fd2_any_char_has_item(100)`（天空之鑰）判定：
  - **GOOD（持有天空之鑰）**：全員一同傳送上黃金城 → 正常 return 進入 ch28+。
  - **BAD（無天空之鑰）**：悠妮獨自傳送離去（`fd2_animate_warp_teleport_char(1, ...)`）→ `fd2_play_game_ending_cinematic` 後進入無限迴圈，遊戲鎖死於此、無法繼續。
- **跨章天空之鑰兌換鏈（ch21 → ch23 → ch27 → ch28+）**：ch21 結束可用集齊的 6 件物品兌換天空之鑰；ch23 持鑰則卡里斯加入；本章持鑰觸發 GOOD ENDING 進入 ch28+，無鑰則強制 BAD ENDING。
- **石碑階梯 tile-step 援軍**：踩到石碑下方階梯的 tile 時，FDFIELD tile-step handler 動態改寫 turn-event hook table，使下回合敵方 turn 觸發援軍 spawn 事件（event 0x3F / 0x41）。詳見下方 FDFIELD event script 節。
- **勝負條件**：標準判定（全敵死＝勝、索爾死＝負）之外，chars[1]（悠妮）陣亡亦判負。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_27_init @ 0x00033AF1` | 428 B（第二大 init） |
| End | `fd2_chapter_27_end @ 0x000250CC` | 920 B |
| Post-action | `fd2_chapter_22_27_28_post_action_shared @ 0x00020A87` | default + lose if char[1] dead（與 ch22/28 共用） |
| BGM（player turn） | `data_fd2_audio_per_chapter_player_turn_bgm_track[26]` | |
| BGM（enemy turn） | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[26]` | |

### Init handler

含 item-conditional dialog branching + 三次小範圍 spell cast cinematic：

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(9, 0x31)` + `fd2_cutscene_event_trigger(0x4C)`
3. `fd2_display_dialog_scene(page=0)`
4. **Conditional**：`fd2_any_char_has_item(100)`（天空之鑰）
   - 找到 → `fd2_display_dialog_scene(page=3)` 額外對話
   - 沒找到 → 跳過
5. `fd2_display_dialog_scene(page=4)`
6. `fd2_pan_cursor_and_window(9, 0x31)` + `fd2_cast_screen_wide_spell_with_fade(cursor_x, cursor_y+3, 2, 2)`
7. palette restore + `fd2_display_dialog_scene(page=5)`
8. `fd2_cast_screen_wide_spell_with_fade(cursor_x, cursor_y, 2, 2)` + palette + `fd2_cutscene_event_trigger(0x51)` + `fd2_display_dialog_scene(page=6)`
9. `fd2_cast_screen_wide_spell_with_fade(cursor_x+2, cursor_y, 2, 2)` + palette + `fd2_display_dialog_scene(page=7)`
10. `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init（固定） | 27 | 0, 4, 5, 6, 7 |
| Init（有天空之鑰） | 27 | 3（插在 page 0 與 page 4 之間） |
| End（固定） | 27 | 8 |
| End GOOD（有天空之鑰） | 27 | 9, 10, 0xB, 0xC |
| End BAD（無天空之鑰） | 27 | 0xD, 0xE, 0xF, 0x10 |

### char_id 初始化序列

無 `fd2_init_runtime_char_from_base_growth` 呼叫；本章不加入新角色，兩條 ending 路徑皆無加入。

### Cutscene events

- Init：`0x4C, 0x51`
- End GOOD：`0x52, 0x53, 0x54`
- End BAD：`0x52`（×2）, `0x54`

### Post-action handler

`fd2_chapter_22_27_28_post_action_shared @ 0x00020A87`（與 ch22/28 共用）：

- 標準 default 判定（全敵死＝勝、索爾死＝負）
- **額外 lose 條件**：if chars[1] dead → `game_event_flag = 1`

本章 chars[1] = 悠妮（編成畫面 pin 表：chapter_id > 0x19 → char 9）。

### End handler events

`fd2_chapter_27_end @ 0x000250CC`（920 B）— GOOD / BAD ENDING fork：

1. 從 `chapter_27_end_scene_pos_x/y_table` 讀位置（含 1-byte vestigial facing）
2. **Revive all**：`chars[0..0xF].bFlags = 0`（16 chars 復活）
3. `fd2_setup_chars_and_camera_for_intro(0xF, 0, 9, 8)`
4. `fd2_display_dialog_scene(page=8)` + `fd2_cutscene_event_trigger(0x52)`
5. **GOOD / BAD FORK**：`fd2_any_char_has_item(100)`（天空之鑰）

**GOOD PATH（有天空之鑰）— 進 ch28+**

- `fd2_display_dialog_scene(page=9)` + `fd2_cutscene_event_trigger(0x53)`
- `fd2_display_dialog_scene(page=10)` + `pan` + `fd2_cutscene_event_trigger(0x54)`
- `fd2_display_dialog_scene(page=0xB)`
- **6× `fd2_palette_overbright_settle_step_loop`**：(0x50,5) / (0x50,4) / (0x50,3) / (0x50,2) / (0x50,2) / (0x50,2) + 不同等待時間（additive over-bright 白閃後 settle 回 base palette，非黑屏；真正黑屏在下一行的 memset 0）
- `fd2_display_dialog_scene(page=0xC)`
- `fd2_cast_screen_wide_spell_with_fade`（大範圍）
- 500ms wait + `memset(0xA0000, 0xFF, 64000)`（白屏）+ `fd2_play_palette_fade_to_black` + `memset(0xA0000, 0, 64000)`（黑屏）
- `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`
- `fd2_restore_all_chars_full_hp_mp`
- `return` ← 正常 return 進入 chapter 28

**BAD PATH（無天空之鑰）— Bad ending**

- `fd2_display_dialog_scene(page=0xD)` + `fd2_cutscene_event_trigger(0x54)`
- `fd2_display_dialog_scene(page=0xE)` + `fd2_cutscene_event_trigger(0x52)`
- `fd2_display_dialog_scene(page=0xF)`
- `fd2_animate_status_effect_overlay_flicker(0, 0x13, 1)` — chars[1] 悠妮 status effect 閃爍
- `fd2_animate_warp_teleport_char(1, 0xFF, 0xFF, ...)` — char[1] 悠妮 teleport away
- `fd2_display_dialog_scene(page=0x10)`
- `fd2_restore_all_chars_full_hp_mp`
- `fd2_play_game_ending_cinematic`
- **infinite loop** — 程式鎖死於此

## FDFIELD event script

FDFIELD entry idx **79**（= chapter_id × 3 + 1，chapter_id=26），entry size 2211 bytes，party_member_count = 16，char_spawn_count = 80。header layout 見 `resource_info/fdfield.md`。

turn-event hook table：2 / 16 active hooks（其餘 14 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 0xFF | 0（enemy_turn_intro） | 0x3F | `0x000358C7` | sentinel-like（final boss event reference） |
| 0xFF | 0（enemy_turn_intro） | 0x41 | `0x0003599B` | sentinel-like |

`turn=0xFF` 不會等於回合計數，初始 hook table 不會 fire。實際觸發機制：tile-step-event handler 在某些劇本 tile 被踩到時，動態 rewrite 該 chapter turn-event hook table 的 turn byte（0xFF → `data_fd2_battle_turn_counter`（當前回合計數）或 +1），把原本 sentinel 的 entry 啟動為下一回合 fire 的 event，形成 cinematic chain（tile-step → handler 寫入 turn = N 或 N+1 → 該回合 turn-event 自動 fire → 連鎖播放劇情）。上方特殊機制的「石碑階梯下回合敵援軍」即屬此類 tile-step → turn-event 連鎖。

## 對話

對話文字 24 pages 來自 FDTXT.DAT entry 27。Init 引用 page 0 /（3 conditional）/ 4 / 5 / 6 / 7；End 依 ending 分歧引用 GOOD：8, 9, 10, 0xB, 0xC，或 BAD：8, 0xD, 0xE, 0xF, 0x10。Page 1 / 2 / 11-23 由 FDFIELD turn-event / tile-step handler 引用。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這裡就是遺跡了嗎？果然是
[PAGE_BREAK]
　個很詭異的地方！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『此地不僅詭異，還有大批敵
[PAGE_BREAK]
　人呢！準備迎敵吧！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『嗶！緊集狀況，請求支援！
[PAGE_BREAK]
　』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0074]
『‥警告，任務中止‥ASR一07
[PAGE_BREAK]
　出現異常現象，原因無法解
[PAGE_BREAK]
　析‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『中央的平台似乎就是傳送的
[PAGE_BREAK]
　地點，就看「天空之鑰」能
[PAGE_BREAK]
　否發生作用了！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x0024]
『嗶！敵人及目標出現，準備
[PAGE_BREAK]
　進行第6號戰鬥指令。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0025]
『命令確認。目標位置標定，
[PAGE_BREAK]
　系統作用功率全開。2秒後
[PAGE_BREAK]
　動作開始。』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『呀一一一一一一』
[END]
```

### Page 6

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！可惡‥竟敢對她下手
[PAGE_BREAK]
　！我饒不了你們！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0024]
『制御及管理中樞ASR一07，完
[PAGE_BREAK]
　成記憶庫連接。進行控制程
[PAGE_BREAK]
　式更新及損壞資料排除。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『啊！我的頭！‥我的頭好像
[PAGE_BREAK]
　要炸開來了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_CHAR=0x0024]
『故障排除完成，初步檢查對
[PAGE_BREAK]
　應﹕各系統完好無損。ASR一
[PAGE_BREAK]
　07，繼續執行你的任務。完
[PAGE_BREAK]
　畢。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『我‥我‥為什麼‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！悠妮！妳還好吧！‥
[PAGE_BREAK]
　‥咦，妳‥妳為什麼哭了？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，別管我，‥不管怎樣
[PAGE_BREAK]
　，先打倒這些傢伙，別的以
[PAGE_BREAK]
　後再說‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0024]
『ASR一07，你有敵對反應！你
[PAGE_BREAK]
　是我們的中樞之一，不該對
[PAGE_BREAK]
　我們有敵對意志！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0025]
『緊急狀況，ASR一07出現預期
[PAGE_BREAK]
　外現象，快通知最高控制中
[PAGE_BREAK]
　樞‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『敵人就在眼前了，趕快發動
[PAGE_BREAK]
　攻擊啊！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『對啊！我在發什麼呆，大家
[PAGE_BREAK]
　上！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『一切的謎底就在眼前了！』
[END]
```

### Page 8

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『就是這個了，轉送站‥好久
[PAGE_BREAK]
　沒看到這種東西了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮？妳還好吧？』
[END]
```

### Page 9

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，所有過去的事，我都
[PAGE_BREAK]
　想起來了。請原諒我騙了你
[PAGE_BREAK]
　，可是那時我真的不知道‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，不管是為了什麼事，
[PAGE_BREAK]
　我都不會怪妳。我只是想送
[PAGE_BREAK]
　妳回家‥就這樣而已，相信
[PARAGRAPH]
　我吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『好，那麼‥我的家就在這上
[PAGE_BREAK]
　面，你願意‥送我回家嗎？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『當然！這不是我們的約定嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『索爾，你要小心！這女孩變
[PAGE_BREAK]
　得很奇怪，說不定她原本是
[PAGE_BREAK]
　敵方的人‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『約拿先生，希爾法大師，你
[PAGE_BREAK]
　們所謂的敵人就在這轉送站
[PAGE_BREAK]
　之後，被你們稱為「黃金城
[PARAGRAPH]
　」的第一空中要塞上‥‥如
[PAGE_BREAK]
　果你們想打倒他，就跟著我
[PAGE_BREAK]
　來，不然的話，就我和索爾
[PARAGRAPH]
　上去也可以‥‥你們意下如
[PAGE_BREAK]
　何呢？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥‥悠妮小姐，妳的記憶全
[PAGE_BREAK]
　部甦醒了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『是的。我在受到剛才那個機
[PAGE_BREAK]
　兵的傳導衝擊之後，受損的
[PAGE_BREAK]
　記憶部份強制還原了，它們
[PARAGRAPH]
　認為我會回到它們那一方，
[PAGE_BREAK]
　可惜的是，我這段日子以來
[PAGE_BREAK]
　的記憶並沒有因此消失‥‥
[PARAGRAPH]
　我已經不是以前的我了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『為什麼？‥我知道妳並不是
[PAGE_BREAK]
　這個世界的人，但為何妳會
[PAGE_BREAK]
　為了我們‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥自私一點來說，我並不是
[PAGE_BREAK]
　為了你們，我是為了一個一
[PAGE_BREAK]
　直保護我、照顧我的人‥‥
[PARAGRAPH]
　像我這樣的人是很悲哀的，
[PAGE_BREAK]
　我們擁有人的一切，卻不能
[PAGE_BREAK]
　做任何平凡人所能做的事情
[PARAGRAPH]
　，所以，我很珍惜這次的經
[PAGE_BREAK]
　歷‥‥我不容許那個無視這
[PAGE_BREAK]
　一切的傢伙傷害他和他所愛
[PARAGRAPH]
　的這個世界。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『‥我明白了。悠妮小姐，我
[PAGE_BREAK]
　很高興妳自始至終都會是我
[PAGE_BREAK]
　們的伙伴‥要對付那個傢伙
[PARAGRAPH]
　，光妳和索爾是不行的，就
[PAGE_BREAK]
　讓我們大家一起來吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『約拿先生，謝謝你‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，你們在說些什麼啊！
[PAGE_BREAK]
　我怎麼聽不太懂‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥索爾，等到上了黃金之城
[PAGE_BREAK]
　，我會讓你知道所有你想知
[PAGE_BREAK]
　道的，在此之前就不要再問
[PARAGRAPH]
　起這些事情了‥答應我好嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『當‥當然！我也只是隨口問
[PAGE_BREAK]
　問而已，就當我沒說好啦！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『好啦！大家可以整裝待發了
[PAGE_BREAK]
　，準備迎向最後的敵人！悠
[PAGE_BREAK]
　妮小姐，請妳啟動轉送裝置
[PARAGRAPH]
　吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『好的，請大家在這平台上站
[PAGE_BREAK]
　好，索爾，請把「天空之鑰
[PAGE_BREAK]
　」給我。』
[END]
```

### Page 10

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『是這個吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『謝謝。大家站好了‥』
[END]
```

### Page 11

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥01327一一344621073。識別
[PAGE_BREAK]
　碼辨識完成‥第一空中要塞
[PAGE_BREAK]
　，請解除出入區防護障壁，
[PARAGRAPH]
　預定五秒後啟動轉送程序。
[PAGE_BREAK]
　』
[END]
```

### Page 12

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『唔哇啊啊啊！‥‥』
[END]
```

### Page 13

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，所有過去的事，我都
[PAGE_BREAK]
　想起來了。請原諒我騙了你
[PAGE_BREAK]
　，可是那時我真的不知道‥
[PARAGRAPH]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，不管是為了什麼事，
[PAGE_BREAK]
　我都不會怪妳，我只是想送
[PAGE_BREAK]
　妳回家‥相信我吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『‥索爾，謝謝你‥一路從羅
[PAGE_BREAK]
　特帝亞到這裡，你一直都那
[PAGE_BREAK]
　麼照顧我、保護我‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這是我該做的，你知道‥我
[PAGE_BREAK]
　一直對妳‥對妳‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『我明白。索爾，但現在是我
[PAGE_BREAK]
　們分離的時刻了，如果我現
[PAGE_BREAK]
　在不走，就會太遲了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『為什麼？不是約好要我送你
[PAGE_BREAK]
　回家的嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒有天空之鑰，只有我能直
[PAGE_BREAK]
　接從此地前往黃金城。索爾
[PAGE_BREAK]
　，請忘了我吧，我並不是一
[PARAGRAPH]
　般的人類，既不能也沒有資
[PAGE_BREAK]
　格接受你的感情，這是命運
[PAGE_BREAK]
　的安排吧‥‥』
[END]
```

### Page 14

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『系統權限強制插入，A1型分
[PAGE_BREAK]
　解傳送啟動待命，座標1一1一
[PAGE_BREAK]
　72‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！不要做傻事！』
[END]
```

### Page 15

```text
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，不要過來！為了你，
[PAGE_BREAK]
　也為了這個世界‥‥讓我為
[PAGE_BREAK]
　你做這最後的一件事吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『悠妮小姐！有關這一切事件
[PAGE_BREAK]
　的來龍去脈，我們還需要向
[PAGE_BREAK]
　妳請教‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒有必要了，我會帶走這代
[PAGE_BREAK]
　表災厄的黃金之城，就像很
[PAGE_BREAK]
　久很久以前一般，讓它沈睡
[PARAGRAPH]
　在一個誰也找不到的地方‥
[PAGE_BREAK]
　時間的洪流會把它的痕跡從
[PAGE_BREAK]
　人們的記憶中抹去，我想，
[PARAGRAPH]
　這會是最好的結果吧‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這樣嗎？‥也許妳的決定是
[PAGE_BREAK]
　對的，但沒有其他方法了嗎
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『沒有，我必須自己去面對最
[PAGE_BREAK]
　後的一戰‥各位，再見了，
[PAGE_BREAK]
　和大家這段日子的相處，讓
[PARAGRAPH]
　我永難忘懷‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！不要！不要走！讓我
[PAGE_BREAK]
　幫妳‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『索爾，永別了，我永遠永遠
[PAGE_BREAK]
　也不會忘記你的，希望你以
[PAGE_BREAK]
　後幸福快樂‥‥』
[END]
```

### Page 16

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮！悠妮！‥‥』
[END]
```

### Page 17

```text
『看！是‥是黃金城！』
[END]
```

### Page 18

```text
『啊！又‥又消失了！』
[END]
```

### Page 19

```text
『這次‥是永遠的消失了吧！
[PAGE_BREAK]
　』
[END]
```

### Page 20

```text
『是的！就像她所承諾的，到
[PAGE_BREAK]
　一個沒有人找得到的地方去
[PAGE_BREAK]
　了‥』
[END]
```

### Page 21

```text
『黃金城傳說‥看來永遠都會
[PAGE_BREAK]
　是個解不開的謎了‥‥』
[END]
```

### Page 22

```text
『索爾‥‥』
[END]
```

### Page 23

```text
『‥再見了，悠妮！雖然妳的
[PAGE_BREAK]
　離去只留下了更多的謎題，
[PAGE_BREAK]
　但在未來的歲月中，我仍會
[PAGE_BREAK]
　永遠記得妳‥和這段非凡的
[PAGE_BREAK]
　冒險‥‥』
[END]
```
