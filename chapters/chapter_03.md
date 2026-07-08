# 第 3 章 — 往塞拉村途中

前往以醫術聞名的塞拉村途中，索爾一行人在橋上撞見遭士兵追殺的鐵諾並出手相救。戰勝追兵後，若鐵諾在戰鬥中存活，他便會加入隊伍。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 2 | 鐵諾 | End handler 末段 | char[6] (鐵諾本人) 戰鬥中存活 |

## 敵人配置

- LV3 士兵 × 8 (HP42)
- LV5 精英戰士 (HP90)
- LV3 士兵 × 5
- LV3 士兵 × 6

敵人配置寫在 FDFIELD.DAT entry 7。

## 寶物

由 FDFIELD tile_event_id 觸發 pickup。

## 商店

神秘商店 (Ctrl+F2) — hack table 觸發。

## 特殊機制

- **勝負條件**：本章無自訂勝負，走 default post-action handler `fd2_check_battle_end_default_handler`：team-0 全滅 → 勝；索爾 (char_id 0) 陣亡 → 敗。
- **條件式招募**：char[6] = 鐵諾本人。End handler 以 `fd2_check_char_is_dead(6)` 判定戰鬥結果——鐵諾存活則播 page 7 後呼 `fd2_init_runtime_char_from_base_growth(2)`，讓鐵諾 (char_id 2) 正式加入；鐵諾陣亡則改播 page 6 悼念對話並 skip recruit。
- **後續劇情**：鐵諾遭禁衛軍統領葛雷大人追殺的事件，與後續章節劇情有關連。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_03_init @ 0x00032E8C` | 324 B |
| End | `fd2_chapter_03_end @ 0x000230F2` | 214 B |
| Post-action | `fd2_check_battle_end_default_handler @ 0x000205B4` | (default — 無自訂勝負) |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[2]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[2]` |  |

### Init handler

`fd2_init_battle_state_for_chapter` 進入正式戰鬥模式後，依序：

- `fd2_pan_cursor_and_window(3, 0x11)` 場景
- `fd2_display_dialog_scene(page=0)`
- `fd2_cutscene_event_trigger(0x12)` + `fd2_load_chapter_portraits_and_dump_tmp(race_id=1)`
- `fd2_pan_cursor_and_window(3, 6)` + `fd2_cutscene_event_trigger(0x11)`
- `fd2_display_dialog_scene(page=1)` + `fd2_cutscene_event_trigger(0x13)`
- `fd2_display_dialog_scene(page=2)` + `fd2_pan_cursor_and_window(3, 0x11)`
- `fd2_display_dialog_scene(page=3)` + `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages 順序 |
|---|---|---|
| Init | 3 | 0, 1, 2, 3 |
| End (char[6] 活) | 3 | 7 |
| End (char[6] 死) | 3 | 6 |

### char_id 初始化序列

Init handler 內無 `fd2_init_runtime_char_from_base_growth` 呼叫。

End handler 條件式 `fd2_init_runtime_char_from_base_growth(2)` → 鐵諾加入（僅當 char[6] 存活）。

### Cutscene events

- Init: `0x11, 0x12, 0x13` (3 events)

每 event 對應 `data_fd2_chapter_cutscene_event_script_ptr_table_106[event_id]` 的 walk-animation script。

### Post-action handler

`data_fd2_chapter_post_action_handler_table[2]` 指向 `fd2_check_battle_end_default_handler`，
無自訂勝負條件：所有 team-0 死 → win，索爾 (char_id 0) 死 → lose。

### End handler events

`fd2_chapter_03_end @ 0x000230F2` (214 B)：

1. 從 `data_fd2_chapter_ch03_end_scene_char_pos_x_table` / `pos_y_table` / `facing_table` (各 8 entries @ 0x520BD/0x520C4/0x520CB) 讀 4 entries → local recruit_block × 3
2. `fd2_save_runtime_char_to_template`
3. **Conditional**：`fd2_check_char_is_dead(6)`
   - 若 char[6] 活著 → `fd2_setup_chars_and_camera_for_intro(...)` + `fd2_display_dialog_scene(page=7)` +
     `fd2_init_runtime_char_from_base_growth(2)` — char 2 = 鐵諾加入
   - 若 char[6] 已死 → `fd2_display_dialog_scene(page=6)` (skip recruit)
4. `current_chapter_id += 1`

## FDFIELD event script

FDFIELD entry idx **7** (= chapter_id × 3 + 1, chapter_id = 2)，entry size 1171 bytes；
party_member_count = 6、char_spawn_count = 40。header layout 見 `resource_info/fdfield.md`。

16 個 turn-event hook slot 中 1 個 active（其餘 15 為 sentinel）：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 2 (new_player_turn_intro) | 0x09 | `0x000344C2` | char_conditional; ch3_char_cond |

## 對話

對話文字 10 pages 來自 FDTXT.DAT entry 3。Init handler 依序引用 page 0/1/2/3；End handler 依 char[6]（鐵諾）戰鬥存活與否分支，存活播 page 7、陣亡播 page 6。其餘 pages（page 4/5 追殺者頭目登場與敗亡、page 8/9 短白）屬同一 FDTXT entry 內的戰鬥中 cutscene 對白。

### Page 0

```text
[PORTRAIT_LEFT_BY_ID=0x0008]
『過了這座橋，就快到以醫術
[PAGE_BREAK]
　聞名的賽拉村了。這村子中
[PAGE_BREAK]
　的祭司醫療法術非常神奇，
[PARAGRAPH]
　應該能治好悠妮的失憶症。
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『希望如此‥咦‥』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x0002]
『‥呼，這回大概是追不上了
[PAGE_BREAK]
　‥‥』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0007]
『鐵諾，總算追上你了吧！
[PAGE_BREAK]
　這下子看你怎麼逃！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0002]
『你們到底和我有什麼仇恨，
[PAGE_BREAK]
　非得要殺死我不可？！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_CHAR=0x0007]
『若你能活到我們隊長趕到的
[PAGE_BREAK]
　時候，你再自己去問他吧！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0002]
『可惡！我和你們拼了！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x0008]
『唉呀！看，橋上有人準備要
[PAGE_BREAK]
　打架耶！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『以多打少，這太不公平了！
[PAGE_BREAK]
　走，亞雷斯，我們去幫那個
[PAGE_BREAK]
　落單的人！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0004]
『索爾，我們還沒搞清楚這是
[PAGE_BREAK]
　怎麼一回事，這太‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『看那些士兵蠻不講理的樣子
[PAGE_BREAK]
　，就知道他一定是好人！
[PAGE_BREAK]
　亞雷斯，你到底去不去？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0004]
『去，去！我總不能看著你一
[PAGE_BREAK]
　個人去拼命，認識你算我倒
[PAGE_BREAK]
　楣！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『我也去！我也去！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0001]
『既然如此，那麼大家都去吧
[PAGE_BREAK]
　！走囉！』
[END]
```

### Page 4

```text
[PORTRAIT_LEFT_BY_ID=0x004D]
『鐵諾，你果然很耐命！怪不
[PAGE_BREAK]
　得頭子一定要我親自來看看
[PAGE_BREAK]
　‥‥不過，你的好運也到此
[PARAGRAPH]
　為止了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0002]
『如果不是這些年輕人幫忙的
[PAGE_BREAK]
　話，我早就沒命了！不過既
[PAGE_BREAK]
　然我還活著，我還是要問你
[PARAGRAPH]
　一個問題﹕到底是誰命令你
[PAGE_BREAK]
　來殺我？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x004D]
『告訴你也不要緊，是葛雷大
[PAGE_BREAK]
　人命令我們來除掉你的！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『葛雷？那個心胸狹窄的禁衛
[PAGE_BREAK]
　軍統領？是為了三年前爭奪
[PAGE_BREAK]
　卡蘿那女孩的事吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0002]
『妳‥妳怎麼會知道這件事？
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『秘密！不過你可以放心，我
[PAGE_BREAK]
　們都會幫你的。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x004D]
『可惡！幫他的都得死，何況
[PAGE_BREAK]
　你們已經知道了這個秘密，
[PAGE_BREAK]
　那就別怪我不留情！給我殺
[PAGE_BREAK]
　！』
[END]
```

### Page 5

```text
[PORTRAIT_LEFT_BY_ID=0x004D]
『我竟然會敗在一批小鬼的手
[PAGE_BREAK]
　下‥‥葛雷大人‥‥』
[END]
```

### Page 6

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『真遺憾，我們已經盡力了，
[PAGE_BREAK]
　結果還是幫不了他‥‥』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『也好，讓他到地下和愛人相
[PAGE_BREAK]
　聚，也不用再四處躲避別人
[PAGE_BREAK]
　的追殺‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『希莉亞，妳的真實身份究竟
[PAGE_BREAK]
　是什麼？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0008]
『秘密！總之，我們安葬他之
[PAGE_BREAK]
　後就趕快離開這裡吧，要不
[PAGE_BREAK]
　然的話，麻煩可能還在後頭
[PARAGRAPH]
　呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『就這麼辦吧！』
[END]
```

### Page 7

```text
[PORTRAIT_LEFT_BY_ID=0x0002]
『多謝各位的幫助，我才能逃
[PAGE_BREAK]
　過這次追殺。不過葛雷是不
[PAGE_BREAK]
　會就此罷手的，我在這片土
[PARAGRAPH]
　地上，還是找不到安居之所
[PAGE_BREAK]
　‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『那你可以加入我們啊！像你
[PAGE_BREAK]
　這樣老練的戰士，對我們來
[PAGE_BREAK]
　說是不可多得的伙伴，而我
[PARAGRAPH]
　們一群人在一起，我保證葛
[PAGE_BREAK]
　雷不敢隨便對你下手。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0002]
『小姐，妳為什麼會知道這些
[PAGE_BREAK]
　事，我‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『秘密！反正你加入我們就是
[PAGE_BREAK]
　了。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『哼！一副故作神秘的樣子，
[PAGE_BREAK]
　真是個古靈精怪的丫頭。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『你說什麼？我可是在為你們
[PAGE_BREAK]
　增加有力的伙伴呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『不希罕！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0002]
『我已經決定了，如果各位不
[PAGE_BREAK]
　嫌棄我的話，以後就請多多
[PAGE_BREAK]
　關照。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『歡迎！不過，我們還是趕快
[PAGE_BREAK]
　離開此地，剩下的事以後再
[PAGE_BREAK]
　說了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『說得對，我們上路吧！』
[END]
```

### Page 8

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這‥‥這是什麼碗糕！』
[END]
```

### Page 9

```text
[PORTRAIT_LEFT_BY_CHAR=0x0006]
『卡蘿那‥‥‥！』
[END]
```
