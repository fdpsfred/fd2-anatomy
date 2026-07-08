# 第 22 章 — 遠古呼喚

在聖靈之塔頂與敵方支配者的部下決戰，擊敗對手後取得傳送法杖，啟動塔頂的傳送魔法陣，準備直接前往敵人根據地。本章是 FD2 全 30 章中唯一以「全螢幕白屏 → palette 漸暗 → 黑屏」收尾的章節。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| — | 龍騎士莎拉 | 章中 (FDFIELD turn event) | 莎拉須在自行出現 (從牢房脫困) 後才能算入隊；若在莎拉出場前消滅完敵人，莎拉不會加入。 |

## 敵人配置

LV13 黑暗法師×4 (HP507, 炎龍術, 地震術, 毒擊術, 麻痺術) 等。第 3、第 7 回合敵方左下/右下角各 spawn 3 隻魔鬼 (FDFIELD turn-event 0x31 兩次觸發)；第 5 回合 reinforcement (event 0x32)。配置寫在 FDFIELD.DAT entry 64。

## 寶物

待 FDFIELD.DAT entry 64 確認。

## 商店

無章內商店；本章後連戰多場無商店。

## 特殊機制

- **失敗條件**：索爾死亡，或希爾法 (char[1]) 死亡。Post-action handler 為 `fd2_chapter_22_27_28_post_action_shared`，與第 27、28 章共用同一份 slot-1 存活檢查結構 (見下方 Post-action handler)。
- **白屏 fade-to-black 結尾**：FD2 全 30 章中唯一以「全螢幕白屏 → palette fade → 黑屏」收尾的章節。End handler 在 page 6 播完後，先呼 `fd2_cast_screen_wide_spell_with_fade` 播大範圍法術視覺 → 等待 500ms → `memset(0xA0000, 0xFF, 64000)` 把整個 framebuffer 填成白 → `fd2_play_palette_fade_to_black` 漸暗 → `memset(0xA0000, 0, 64000)` 轉黑。
- **連戰無商店**：本章與後續章節之間由 `fd2_chapter_transition_menu` 抑制商店出現，四場連戰後才會恢復。
- **第 3、7 回合三角魔鬼 spawn**：由 FDFIELD turn-event hook 控制 (見下方 FDFIELD event script)。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_22_init @ 0x0003367E` | 34 B |
| End | `fd2_chapter_22_end @ 0x000244B6` | 354 B |
| Post-action | `fd2_chapter_22_27_28_post_action_shared @ 0x00020A87` | 與 ch27/28 共用 |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[21]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[21]` |  |

### Init handler

1. `fd2_init_battle_state_for_chapter`
2. `fd2_pan_cursor_and_window(0x10, 0x1C)`
3. `fd2_cutscene_event_trigger(0x43)` + `fd2_clear_all_chars_facing`
4. `fd2_display_dialog_scene(page=0)` + `fd2_pan_cursor_to_char(0)`

### Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 22 | 0 |
| End | 22 | 4, 5, 6 |

### char_id 初始化序列

無 init handler 內 char init。莎拉由 FDFIELD turn-event 加入 (從牢房脫困)，end handler 不加入新角色。

### Cutscene events

- Init：`0x43`
- End：`0x41`, `0x42`

### Post-action handler

`fd2_chapter_22_27_28_post_action_shared @ 0x20A87` (與 ch27/28 共用)：

- default
- 額外 lose：if `char[1]` 死亡

`char[1]` = 希爾法（編成畫面 pin 表：chapter_id 0x15/0x16 → char 0x18，由 `fd2_pin_required_char_to_party_slot1` 釘進 slot 1）。與 ch27/28 共用的是 post-action handler 的 slot-1 檢查結構，不是同一角色：ch27/28 的 `chars[1]` 是悠妮。

### End handler events

`fd2_chapter_22_end @ 0x244B6` (354 B) — 白屏 fade-to-black 結尾：

1. 從 scene tables (含 facing_table) 讀位置
2. `fd2_setup_chars_and_camera_for_intro(0xF, 0x48, 0x16, 0x19, 2, 0x10, 0x12)`
3. `fd2_display_dialog_scene(page=4)` + `fd2_cutscene_event_trigger(0x41)`
4. `fd2_display_dialog_scene(page=5)` + `fd2_pan_cursor_and_window(0x10, 0x10)` + `fd2_cutscene_event_trigger(0x42)`
5. `fd2_display_dialog_scene(page=6)` + `fd2_pan_cursor_and_window(0x10, 0xE)`
6. `fd2_cast_screen_wide_spell_with_fade(cursor_y+3, ..., 10, 8)` — 大範圍 spell visual
7. 500ms wait
8. `memset(0xA0000, 0xFF, 64000)` — **白屏 (整 framebuffer = 0xFF)**
9. `fd2_play_palette_fade_to_black` — palette 漸暗
10. `memset(0xA0000, 0, 64000)` — 黑屏
11. `fd2_save_runtime_char_to_template` + `current_chapter_id += 1`

「連戰四場後才有商店」屬 `fd2_chapter_transition_menu @ 0x2CAD7` 處理 (與 end handler 解耦)。

## FDFIELD event script

FDFIELD entry idx **64** (= chapter_id × 3 + 1, chapter_id=21)，entry size 1951 bytes，`party_member_count` = 16，`char_spawn_count` = 70。header layout 見 `resource_info/fdfield.md`。

**3/16 active turn-event hooks**：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 3 | 0 (enemy_turn_intro) | 0x31 | `0x000351E9` | 三角魔鬼 spawn (t3/t7 phase 0 reinforcement) |
| 5 | 2 (new_player_turn_intro) | 0x32 | `0x00035261` | reinforcement_spawner |
| 7 | 0 (enemy_turn_intro) | 0x31 | `0x000351E9` | 三角魔鬼 spawn |

## 對話

對話文字 7 pages 來自 FDTXT.DAT entry 22。Init handler 開場引用 page 0；戰鬥中的魔鬼援軍騷擾與龍騎士莎拉脫困加入等 turn-event 過場，以及擊敗支配者部下、取得傳送法杖，分別用 page 1-3；End handler 於塔頂啟動傳送魔法陣的場景依序引用 page 4、5、6。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『一路來到了塔頂，居然都沒
[PAGE_BREAK]
　有遇到敵人，真是奇怪！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『索爾，不要掉以輕心！說不
[PAGE_BREAK]
　定敵人都集中在塔頂等我們
[PAGE_BREAK]
　呢！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『亞雷斯，你好像說對了！塔
[PAGE_BREAK]
　頂上有一大群人耶！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『嗄？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007C]
『呼呼呼，你們來得比我預期
[PAGE_BREAK]
　的還早些，不過也沒關係，
[PAGE_BREAK]
　等我把你們都解決後再來辦
[PARAGRAPH]
　事就好了‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『你們執意要佔領這聖靈之塔
[PAGE_BREAK]
　的理由，想必和你們在此進
[PAGE_BREAK]
　行的勾當有關吧！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007C]
『哈哈哈，反正你們別想活著
[PAGE_BREAK]
　離開這塔頂，所以告訴你們
[PAGE_BREAK]
　也無妨；這座塔是大陸上最
[PARAGRAPH]
　接近天空的地方，我的主人
[PAGE_BREAK]
　要在此建立一個傳送點，如
[PAGE_BREAK]
　此他才能把麾下的軍隊從居
[PARAGRAPH]
　城送到這大陸來‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『居城？你們究竟是來自何處
[PAGE_BREAK]
　？你的主人又是誰？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007C]
『我的主人是支配者，是創造
[PAGE_BREAK]
　者，也是毀滅者‥‥其他的
[PAGE_BREAK]
　你們就不必知道了，準備受
[PARAGRAPH]
　死吧！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『準備要死的是你吧！看劍！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0018]
『對手的底細不明，大家要小
[PAGE_BREAK]
　心為戰！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x006C]
『嘎嘎‥‥‥！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007C]
『來了嗎‥‥，
[PAGE_BREAK]
　宰了這些傢伙！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x006C]
『嘎嘎嘎‥‥‥！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_ID=0x0014]
『咦，這是怎麼回事？塔頂打
[PAGE_BREAK]
　成一片？‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『喂，這裡打的正起勁，如果
[PAGE_BREAK]
　不想受傷就別過來！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0014]
『別把人看扁了！我起碼也是
[PAGE_BREAK]
　個龍騎士耶！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『龍騎士？好久沒聽到這個崇
[PAGE_BREAK]
　高的名號了‥‥妳怎麼會恰
[PAGE_BREAK]
　好來到這裡呢？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0014]
『說來話長，我自從不小心被
[PAGE_BREAK]
　抓之後，就一直被關在所在
[PAGE_BREAK]
　不明的牢房中，幾天前才被
[PARAGRAPH]
　轉送到這塔裡；剛才我聽到
[PAGE_BREAK]
　塔頂有打鬥聲，看守的人又
[PAGE_BREAK]
　不在，就設法打破牢門出來
[PARAGRAPH]
　看看。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『原來如此，現在妳打算怎麼
[PAGE_BREAK]
　辦？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0014]
『當然是加入你們一行，修理
[PAGE_BREAK]
　那個大傢伙啊！我要好好和
[PAGE_BREAK]
　清算這筆帳！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x007C]
『手下敗將，還不認輸嗎？這
[PAGE_BREAK]
　次我不會再客氣了！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『少囉唆！先應付完我這一劍
[PAGE_BREAK]
　再說！』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『‥這些卑賤的蟲蟻‥為何會
[PAGE_BREAK]
　如此厲害‥‥主人‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『唉！又失手了，本來這回我
[PAGE_BREAK]
　有更多話想問他‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『沒關係，我們得到了這傳送
[PAGE_BREAK]
　法杖，只要啟動他們布下的
[PAGE_BREAK]
　這個魔法陣，就可以直接傳
[PARAGRAPH]
　送到他們的根據地了。』
[END]
```

### Page 4

```text
[PORTRAIT_RIGHT_BY_ID=0x0015]
『希爾法，這種單程傳送很可
[PAGE_BREAK]
　能一去不回，你不覺得太冒
[PAGE_BREAK]
　險了嗎？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『沒辦法，眼看著敵人就要大
[PAGE_BREAK]
　舉進攻了，只有前往他們的
[PAGE_BREAK]
　根據地做個徹底解決，才有
[PARAGRAPH]
　可能阻止他們。為了保護精
[PAGE_BREAK]
　靈族和整個馬拉大陸，我已
[PAGE_BREAK]
　有犧牲的決心。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這樣嗎‥‥』
[END]
```

### Page 5

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『等一下我就啟動傳送場，想
[PAGE_BREAK]
　去的就自行走進這魔法陣中
[PAGE_BREAK]
　，我絕不強迫。』
[END]
```

### Page 6

```text
[PORTRAIT_RIGHT_BY_ID=0x0018]
『你‥你們‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『希爾法，如果就這樣讓你一
[PAGE_BREAK]
　個人去，我約拿的名字會被
[PAGE_BREAK]
　今後的世人所恥笑。
[PARAGRAPH]
　我可不是個貪生怕死的人，
[PAGE_BREAK]
　更不會在危難時拋棄朋友！
[PAGE_BREAK]
　』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『既然都到了這裡，那還有什
[PAGE_BREAK]
　麼好怕的！我們相信，幾位
[PAGE_BREAK]
　前輩的智慧和知識，一定能
[PARAGRAPH]
　帶領我們度過難關的。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『就是說啊！馬拉大陸上的強
[PAGE_BREAK]
　者在此濟濟一堂，要擊潰敵
[PAGE_BREAK]
　人根據地有什麼問題！希爾
[PARAGRAPH]
　法大師，你要對我們自己有
[PAGE_BREAK]
　信心才對！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『謝謝‥謝謝大家這麼相信我
[PAGE_BREAK]
　！既然如此，我這就啟動傳
[PAGE_BREAK]
　送魔法！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『哎，我等不及想看看我們會
[PAGE_BREAK]
　去的是個怎麼樣的地方！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0018]
『馬那德拉、亞布利加﹑艾米
[PAGE_BREAK]
　西德拉、伊多卡那瓦！‥‥
[PAGE_BREAK]
　』
[END]
```
