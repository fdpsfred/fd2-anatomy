# 第 25 章 — 火焰的審判

著陸火焰之谷熔岩洞窟，與龍人族王聖寇拉斯會師對抗火魔神；戰後惡魔族王家魔法師亞齊梅吉自願加入。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 26 (0x1A) | 龍劍士聖寇拉斯 | end handler | 無條件 (在 `fd2_save_runtime_char_to_template` 之前 init) |
| 29 (0x1D) | 大法師亞齊梅吉 | end handler | 無條件 (在 save 之後 init — 不被 template 保留) |

## 敵人配置

本章 FDFIELD entry 73 共 55 個會生成的 spawn 記錄（另有 15 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 39 | 龍人法師 | LV24 | ×4 | aggressive_physical |
| 38 | 龍人戰士 | LV25 | ×15 | aggressive_physical / defensive_kiter |
| 18 | 龍騎士 | LV24 | ×2 | default_attacker |
| 41 | 惡魔 | LV14 | ×9 | default_attacker / defensive_kiter |
| 42 | 大惡魔 | LV13 | ×2 | defensive_kiter |
| 38 | 龍人戰士 | LV26 | ×1 | defensive_kiter |
| 16 | 地獄騎士 | LV17 | ×4 | aggressive_physical |
| 57 | 火魔神 | LV19 | ×1 | aggressive_physical |
| char 0x1D | 亞齊梅吉（玩家職模板） | LV21 | ×1 | default_attacker |

火魔神 (enemy_data 57) 為 boss。表末 char 0x1D 為以亞齊梅吉角色模板生成的敵方單位；亞齊梅吉本人於戰後自願加入我方（見 §加入角色）。

友軍 NPC（team 1，戰場自走）：

| enemy_data | 單位 | 等級 | 數量 |
|---|---|---|---|
| 36 | 火龍 | LV16 | ×1 |
| 5 | 龍劍士 | LV16 | ×14 |

友軍為龍人族生力軍（火龍 ×1、龍劍士 ×14），與龍人族王聖寇拉斯會師後投入戰場自走。

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：大地裝甲 (0xBF)、風精之羽 (0x60)、魔力水晶 (0x5F)、衝擊手臂 (0x4B)、神聖之水 (0xC3)
- 金錢：20000

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：再生藥 (0xC2)、神聖之水 (0xC3)
- 金錢：10000、15000

## 商店

無章內商店（battle 章，不走 intro 商店選單）。

## 特殊機制

- **Init 開場 4 連震 cutscene**：本章是 FD2 唯一在 init 階段自製程式化 cutscene + sfx 的章節。init 用 `fd2_load_dat_resource` 載入 FDOTHER[0x58] 地震音效 wave，接著 `fd2_play_sfx_with_handle` + `fd2_animate_screen_shake` × 4 連震：前 3 次強度 0x14，第 4 次強度 0x3C（三倍長度的 climax 大地震），每次之間等待約 600ms。
- **Init 從 page 1 開始**：`fd2_display_dialog_scene` 直接由 page 1 起，跳過 page 0。page 0 由 tile-step handler `fd2_chapter_event_handler_37__ch25_first_time @0x353DA` 引用（領主首次踩上 gated tile → page 0 對白 → 觸發對 char 0x11 的 scripted 戰鬥）；page 0 transcript 為火魔神被打擾的睡眠對白。
- **Save-split 加入（聖寇拉斯進 template、亞齊梅吉不進）**：end handler 內聖寇拉斯 (char 0x1A) 在 `fd2_save_runtime_char_to_template` 之前 init，因此進入 saved template；亞齊梅吉 (char 0x1D) 在 save 之後才 init，屬 runtime-only，不被 saved template 保留（下章 init 時可能重新加入）。
- **勝負條件**：`fd2_chapter_25_post_action` 走標準 default 判定（全敵死 = 勝、索爾死 = 負），並加一條額外 lose：聖寇拉斯 (chars[0x10]) 死 → `game_event_flag = 1`（負）。
- **Turn 6 dialog event**：FDFIELD tile_event 在第 6 回合玩家 turn 結束時（phase 1 end_of_player_turn）觸發 `dialog_with_state` 對話事件，handler @ 0x00035487。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_25_init @ 0x0003396A` | 324 B |
| End | `fd2_chapter_25_end @ 0x00024DF2` | 142 B |
| Post-action | `fd2_chapter_25_post_action @ 0x00020B14` | default + lose if char[0x10] dead |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[24]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[24]` |  |

### Init handler

開場 4 連震 cutscene + game state buffer 清空：

1. `fd2_init_battle_state_for_chapter`
2. `data_fd2_audio_status_effect_sfx_handle_ptr = NULL` 清空
3. `data_fd2_audio_status_effect_sfx_handle_ptr = fd2_load_dat_resource("FDOTHER.DAT", idx=0x58)` — 載入地震 sfx wave
4. `fd2_pan_cursor_and_window(5, 0)`
5. `fd2_display_dialog_scene(page=1)`（從 page 1 起，非 page 0）
6. `memset(large_game_state_buffer, 0, 0x25680)` — 清 game state buffer（0x25680 bytes ≈ 150 KiB）
7. **4× 連續地震**：`fd2_play_sfx_with_handle(data_fd2_audio_status_effect_sfx_handle_ptr)` + `fd2_animate_screen_shake(strength)` + 約 600ms 等待
   - 第 1、2、3 次：strength = 0x14
   - 第 4 次：strength = 0x3C（三倍長度 climax）
8. `fd2_display_dialog_scene(page=2)`
9. `fd2_pan_cursor_to_char(0)` + `fd2_stop_and_free_status_effect_sfx`

Page 0 在此 init 路徑未被引用，實際由 tile-step handler `fd2_chapter_event_handler_37__ch25_first_time @0x353DA` 引用（領主首次踩 gated tile 觸發）；page 0 transcript 為火魔神被打擾的睡眠對白。

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 25 | 1, 2 |
| End | 25 | 6, 7 |

### char_id 初始化序列

End handler 內：
- char_id 26 (0x1A) → 龍劍士聖寇拉斯（在 `fd2_save_runtime_char_to_template` 之前 init）
- char_id 29 (0x1D) → 大法師亞齊梅吉（在 `fd2_save_runtime_char_to_template` 之後 init — 不被 saved template 包含）

### Cutscene events

- Init：無 `fd2_cutscene_event_trigger`（用 `fd2_play_sfx_with_handle` + `fd2_animate_screen_shake` × 4 自製 cutscene）
- End：`fd2_cutscene_event_trigger(0x4B)`

### Post-action handler

`fd2_chapter_25_post_action @ 0x00020B14`：
- 標準 default 判定（全敵死 = 勝、索爾死 = 負）
- **額外 lose 條件**：if chars[0x10]（聖寇拉斯）死 → `game_event_flag = 1`

### End handler events

`fd2_chapter_25_end @ 0x00024DF2` (142 B) — 2 chars 加入（其中 1 在 save 後）：

1. `fd2_display_dialog_scene(page=6)`
2. `fd2_pan_cursor_and_window(4, 0x10)` + `fd2_load_chapter_portraits_and_dump_tmp(2)`
3. `fd2_cutscene_event_trigger(0x4B)`
4. `fd2_display_dialog_scene(page=7)`
5. `fd2_init_runtime_char_from_base_growth(0x1A)` — 聖寇拉斯加入 (char 26)
6. `fd2_save_runtime_char_to_template`  ← save 在此
7. `fd2_init_runtime_char_from_base_growth(0x1D)` — 亞齊梅吉加入 (char 29)，**在 save 之後，不被 template 保留**
8. `current_chapter_id += 1`

亞齊梅吉每次進入 ch26 時可能由 ch26 init 重新初始化，屬有意設計的「runtime-only」加入。

### DAT resources loaded (init)

| File | idx | 用途 |
|---|---|---|
| FDOTHER.DAT | 0x58 | 地震 sfx wave（chapter 25 開場 4 連震） |

## FDFIELD event script

FDFIELD tile_event entry idx **73**（= chapter_id × 3 + 1，chapter_id=24），entry size 1951 bytes，party_member_count = 16，char_spawn_count = 70。header layout 見 `resource_info/fdfield.md`。

turn-event hook：1 / 16 active（其餘 15 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 6 | 1 (end_of_player_turn) | 0x38 | `0x00035487` | dialog_with_state (ch25 對話事件) |

## 對話

對話文字 8 pages 來自 FDTXT.DAT entry 25。Init 引用 page 1、2，End 引用 page 6、7；page 0 由 tile-step handler @0x353DA 引用（火魔神被打擾的睡眠對白）；page 5 由 turn-event handler @0x35487 引用；page 3/4 不由任何 ch25 handler 或 post-action 引用。roster／機制欄的名表正名「亞齊梅吉」與對白 transcript 內的「亞奇梅吉」為同一角色的遊戲內部變體。

### Page 0

```text
[PORTRAIT_RIGHT_BY_CHAR=0x0011]
『是誰‥‥
[PAGE_BREAK]
　打擾了我的睡眠‥‥！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x007D]
『果然不愧是龍人族之長，終
[PAGE_BREAK]
　於讓你找到這裡了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『你竟敢利用惡魔族來逞一己
[PAGE_BREAK]
　的野心私欲，實在是不可原
[PAGE_BREAK]
　諒！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007D]
『惡魔族本來就是我們創造的
[PAGE_BREAK]
　，要如何使用他們是我們的
[PAGE_BREAK]
　事！而且，你既然知道了這
[PARAGRAPH]
　些事，我就不會讓你活著離
[PAGE_BREAK]
　開這裡‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『我手下的援軍馬上就趕到了
[PAGE_BREAK]
　，今天要讓你看看龍人族戰
[PAGE_BREAK]
　士的力量‥‥‥！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x007D]
『真驚人的聲勢，你到底派來
[PAGE_BREAK]
　了什麼援軍？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『這‥好像不是‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『終於停了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『嗚‥真爛的著陸法！還沒和
[PAGE_BREAK]
　敵人交戰就摔掉半條命了‥
[PAGE_BREAK]
　‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『悠妮，悠妮！妳沒有受傷吧
[PAGE_BREAK]
　？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0009]
『還好，一點傷都沒有‥索爾
[PAGE_BREAK]
　你也還好吧？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哼‥‥差別待遇喔！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『別吵了，大家都沒事吧！這
[PAGE_BREAK]
　裡好像就是敵方的根據地之
[PAGE_BREAK]
　一，我們要特別小心‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『咦！那邊有人在對峙著‥好
[PAGE_BREAK]
　像有我們一族的龍人呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x001B]
『真的？那陛下人一定在裡面
[PAGE_BREAK]
　！聖寇拉斯陛下！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『有人認識我嗎？啊！是巴拿
[PAGE_BREAK]
　羅西亞和凱拉斯！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『陛下，我們總算找到您了！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『聖寇拉斯，好久不見了，沒
[PAGE_BREAK]
　想到會和你在這裡會師！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『啊！原來是約拿先生，好極
[PAGE_BREAK]
　啦，有你手下這批生力軍，
[PAGE_BREAK]
　這下子就沒問題了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007D]
『哼，果然是你找來的援軍沒
[PAGE_BREAK]
　錯，你們都準備死在這裡吧
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這怪傢伙還真囂張，亞雷斯
[PAGE_BREAK]
　，我們合力宰了他！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『對！送他去和他的前三個伙
[PAGE_BREAK]
　伴相會！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007D]
『什麼！難道‥難道地水風三
[PAGE_BREAK]
　個魔神都已經被你們打倒了
[PAGE_BREAK]
　？可惡，我火魔神要替他們
[PARAGRAPH]
　一併把這筆血債討回來！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『少囉唆！有種就放馬過來吧
[PAGE_BREAK]
　！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『約拿‥你要代替我‥打倒這
[PAGE_BREAK]
　幫傢伙，要不然‥這世界會
[PAGE_BREAK]
　‥‥』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_CHAR=0x003D]
『‥可惡，主人不會放過你們
[PAGE_BREAK]
　的‥‥到時候你們就會和這
[PAGE_BREAK]
　世界一起毀滅‥‥』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_CHAR=0x003E]
『聖寇拉斯陛下，我們來遲了
[PAGE_BREAK]
　！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『還不晚，先把火魔神打倒再
[PAGE_BREAK]
　說吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x003E]
『遵命！』
[END]
```

### Page 6

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『真是驚險的一戰！約拿老頭
[PAGE_BREAK]
　，你從哪裡找來這些生力軍
[PAGE_BREAK]
　？如果沒有他們，這一戰的
[PARAGRAPH]
　結果就會改觀了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『他們是一群富正義感的年輕
[PAGE_BREAK]
　勇士，因為機緣巧合同聚一
[PAGE_BREAK]
　堂，之後為了打倒暗中威脅
[PARAGRAPH]
　這整個大陸的敵人而和我一
[PAGE_BREAK]
　起奮戰至今。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『好極了，我正愁來不及召集
[PAGE_BREAK]
　一支夠強的軍隊！據我調查
[PAGE_BREAK]
　的結果，這熔岩洞窟中有通
[PARAGRAPH]
　道通往一個古代人的遺跡，
[PAGE_BREAK]
　據說從那裡可以直達黃金的
[PAGE_BREAK]
　城堡。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『所謂黃金的城堡，就是指敵
[PAGE_BREAK]
　人的根據地嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『很難說，我們要去確認之後
[PAGE_BREAK]
　才知道。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『敵人似乎怕我們發現這一點
[PAGE_BREAK]
　，所以剛才那傢伙企圖施用
[PAGE_BREAK]
　魔法讓火山爆發，徹底毀滅
[PARAGRAPH]
　這條通道，但這樣也會毀了
[PAGE_BREAK]
　惡魔族的棲息之地‥幸好你
[PAGE_BREAK]
　們及時抵達，才沒有讓那傢
[PARAGRAPH]
　伙得逞。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『好極了，咱們趕快前往遺跡
[PAGE_BREAK]
　吧，這次絕不能再讓敵人搶
[PAGE_BREAK]
　先了！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_ID=0x001D]
『等等！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『咦，敵方還有漏網之魚嗎？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001D]
『別誤會，我是來加入你們一
[PAGE_BREAK]
　行的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『你會說人類的語言？這可有
[PAGE_BREAK]
　趣了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001D]
『我是當今惡魔族之王卡爾根
[PAGE_BREAK]
　的弟弟，王家魔法師亞奇梅
[PAGE_BREAK]
　吉。我平日對人類和大陸上
[PARAGRAPH]
　的其他種族有很深的研究，
[PAGE_BREAK]
　所以通曉你們的語言。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『你的種族剛剛才聯合火魔神
[PAGE_BREAK]
　與我們為敵，為何現在你又
[PAGE_BREAK]
　要加入我們？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001D]
『我族並不是故意要與你們為
[PAGE_BREAK]
　敵，因為敵人以邪法控制了
[PAGE_BREAK]
　王兄的神智，藉以命令我族
[PARAGRAPH]
　的戰士來抵擋你們的攻勢。
[PAGE_BREAK]
　我看穿這一點之後，敵人便
[PAGE_BREAK]
　假王兄之手將我監禁起來，
[PARAGRAPH]
　還好你們阻止了他們，不然
[PAGE_BREAK]
　整個惡魔族可能會就此滅亡
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『原來如此，你對敵人的所知
[PAGE_BREAK]
　有多少？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001D]
『我相信他們和古代人的遺跡
[PAGE_BREAK]
　以及黃金城絕對有關連，他
[PAGE_BREAK]
　們可能是從附近的遺跡來到
[PARAGRAPH]
　這個世界的，所以當務之急
[PAGE_BREAK]
　是經由本地的通道前往遺跡
[PAGE_BREAK]
　，以免他們再度增援。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0010]
『好極了，那接下來便由你來
[PAGE_BREAK]
　帶路了。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001D]
『樂意之至！我們這就走吧！
[PAGE_BREAK]
　』
[END]
```
