# 第 19 章 — 黑暗中的狙擊

進入黑森林後遭「死亡骷髏」殘部突襲，龍劍士巴拿羅西亞奉聖寇拉斯王之命前來支援。

## 加入角色

| char_id | 角色 | 加入時機 | 條件 |
|---|---|---|---|
| 0x40 | 龍劍士巴拿羅西亞 | 章中 (FDFIELD turn 6 reinforcement) | 巴拿羅西亞在第 6 回合以援軍登場後，須再消滅完全部敵人才會加入；若在他登場前就打完所有敵人，巴拿羅西亞不會加入。 |

## 敵人配置

本章敵軍為死亡骷髏傭兵團殘部。FDFIELD entry 55 共 49 個會生成的 spawn 記錄（另有 21 筆 race_id 0xFF 保留記錄不生成）。含初始佈署與各回合援軍波次（波次時序見 §FDFIELD event script）。

| enemy_data | 敵人 | 等級 | 數量 | AI |
|---|---|---|---|---|
| 13 | 狂戰士 | LV11 | ×1 | aggressive_physical |
| 15 | 突擊騎兵 | LV21 | ×6 | aggressive_physical |
| 21 | 狙擊手 | LV9 | ×4 | aggressive_physical |
| 11 | 傭兵 | LV11 | ×14 | aggressive_physical |
| 24 | 巫師 | LV10 | ×6 | hard_skip / aggressive_physical |
| 27 | 大祭師 | LV10 | ×6 | hard_skip / aggressive_physical |
| 32 | 武術家 | LV12 | ×8 | aggressive_physical |
| 30 | 影之忍者 | LV8 | ×3 | item_pickup |

## 寶物

地圖寶物（走上寶物 tile 拾取，來源 FDFIELD tile_pickup 表）：

- 道具：力量藥水 (0xC6)、生命之實 (0x5E)、再生藥 (0xC2)、魔力水晶 (0x5F)
- 金錢：10000、5000

敵人掉落（擊殺帶有掉落的敵人可得）：

- 道具：生命之實 (0x5E)、再生藥 (0xC2)、耐力藥水 (0xC7)、速度藥水 (0xC8)、鑽石 (0xCC)
- 金錢：10000

## 商店

無章內商店。

## 特殊機制

- **失敗條件**：索爾死亡；或巴拿羅西亞 (char[0x40]) 在加入後死亡。後者由 `fd2_chapter_19_post_action` 以 `data_fd2_battle_turn_counter > 6` 作時序 gate（代表巴拿羅西亞已登場），在他尚未出現之前，char[0x40] 戰死不算敗。
- **共用 init handler**：本章 init 走 `fd2_chapter_19_20_21_init_shared`，與第 20、21 章共用，三章開場流程完全相同；這是 30 章中唯一的共用 init 例子。
- **回合事件**：FDFIELD turn-event hooks 在第 4、6、10 回合觸發。第 4、10 回合為 AI setup，第 6 回合觸發巴拿羅西亞的 reinforcement spawn（見 FDFIELD event script）。

## Handler 流程

### Function 位址

| 角色 | 位址 | 大小 |
|---|---|---|
| Init | `fd2_chapter_19_20_21_init_shared @ 0x00033674` | 10 B (與 ch20/21 共用) |
| End | `fd2_chapter_19_end @ 0x00023E39` | 59 B (trivial) |
| Post-action | `fd2_chapter_19_post_action @ 0x00020926` | default + gated lose |
| BGM (player turn) | `data_fd2_audio_per_chapter_player_turn_bgm_track[18]` |  |
| BGM (enemy turn) | `data_fd2_audio_per_chapter_enemy_turn_bgm_track[18]` |  |

### Init handler

Shared minimal init (10 B)：

1. `fd2_init_battle_state_for_chapter`
2. `fd2_display_dialog_scene(page=0)` (取自 ch19 的 FDTXT entry)
3. `fd2_pan_cursor_to_char(0)`

ch19/20/21 三章共用同一 init handler，三章間沒有 init 時序差異，所有差異都在 post-action handler 與 FDFIELD.DAT entry。

### Dialog page 引用

| 來源 | FDTXT entry | Pages |
|---|---|---|
| Init | 19 | 0 |
| End | 19 | 3 |

### char_id 初始化序列

無 init handler 內 char init。巴拿羅西亞由 FDFIELD turn 6 reinforcement spawn 加入。

### Cutscene events

無 init / end cutscene。

### Post-action handler

`fd2_chapter_19_post_action @ 0x20926`：

- default 判定（敵全死 = 勝、索爾死 = 負）。
- **gated lose**：若 `data_fd2_battle_turn_counter > 6` 且 `char[0x40]` 死亡 → game_event_flag = 1。`data_fd2_battle_turn_counter > 6` 對應「巴拿羅西亞已登場之後」的時序 gate；在他尚未出現之前，char[0x40] 戰死不算敗。

### End handler events

`fd2_chapter_19_end @ 0x23E39` (59 B) — trivial：

1. `fd2_save_runtime_char_to_template`
2. `fd2_display_dialog_scene(page=3)`
3. `current_chapter_id += 1`

無 cutscene、無加入。

## FDFIELD event script

FDFIELD entry idx **55**（= chapter_id × 3 + 1，chapter_id = 18），entry size 1951 bytes，party_member_count = 16，char_spawn_count = 70。header layout 見 `resource_info/fdfield.md`。

**3/16 active turn-event hooks**：

| turn | phase | event_code | handler 位址 | 語意 |
|---|---|---|---|---|
| 4 | 2 (new_player_turn_intro) | 0x2C | `0x000350A4` | ai_setup |
| 6 | 1 (end_of_player_turn) | 0x2E | `0x000350CC` | reinforcement_spawner (巴拿羅西亞登場) |
| 10 | 2 (new_player_turn_intro) | 0x2D | `0x000350B9` | ai_setup |

## 對話

對話文字 4 pages 來自 FDTXT.DAT entry 19。page 0 為開場（init handler 引用），page 1、2 為戰鬥中巴拿羅西亞率援軍登場與敵將陣亡的插話，page 3 為戰後對話（end handler 引用）。

### Page 0

```text
[PORTRAIT_RIGHT_BY_ID=0x0000]
『真令人不舒服的森林！這裡
[PAGE_BREAK]
　是什麼地方？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『這是大陸北方山脈之麓的黑
[PAGE_BREAK]
　森林，由於山脈和濃密的森
[PAGE_BREAK]
　林阻擋陽光，這一帶看起來
[PARAGRAPH]
　總是陰森森的，一般旅人都
[PAGE_BREAK]
　不敢行經此處。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0008]
『過了此地，也就快到死之沼
[PAGE_BREAK]
　澤了，聽說曾有人在這一帶
[PAGE_BREAK]
　遭怪物攻擊‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0003]
『我有不好的預感，大家要多
[PAGE_BREAK]
　小心！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0001]
『哎呀！老爹你說對啦，前面
[PAGE_BREAK]
　果然有人來了！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x0051]
『約拿老頭，上次在你手裡折
[PAGE_BREAK]
　損了不少弟兄，今天這筆帳
[PAGE_BREAK]
　要一併討回來！準備受死吧
[PARAGRAPH]
　！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0004]
『可惡，又是陰魂不散的死亡
[PAGE_BREAK]
　骷髏！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『不知死活的傢伙，你們的罪
[PAGE_BREAK]
　業就到此為止了！我今天絕
[PAGE_BREAK]
　不放過你們！』
[END]
```

### Page 1

```text
[PORTRAIT_LEFT_BY_ID=0x001B]
『咦！這是惡名昭彰的「死亡
[PAGE_BREAK]
　骷髏」傭兵團啊！他們竟然
[PAGE_BREAK]
　在這裡出現，難道果真是有
[PARAGRAPH]
　人在暗中‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『嘿！那可不是巴拿羅西亞閣
[PAGE_BREAK]
　下嗎？真巧啊！』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001B]
『啊！你是凱拉斯嘛！好一陣
[PAGE_BREAK]
　子沒碰面了，想不到會在這
[PAGE_BREAK]
　裡遇見你‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『巴拿羅西亞閣下，現在可能
[PAGE_BREAK]
　不是敘舊的好時候，先讓我
[PAGE_BREAK]
　們解決了這幫想謀害約拿先
[PARAGRAPH]
　生的惡徒再說。』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001B]
『約拿先生也在這裡？好極了
[PAGE_BREAK]
　，這場仗也算上我一份！』
[END]
```

### Page 2

```text
[PORTRAIT_LEFT_BY_CHAR=0x0010]
『‥為什麼‥為什麼這些小鬼
[PAGE_BREAK]
　竟然如此厲害‥‥』
[END]
```

### Page 3

```text
[PORTRAIT_LEFT_BY_ID=0x001B]
『這場仗打的真是漂亮！對於
[PAGE_BREAK]
　各位的表現，我個人真是非
[PAGE_BREAK]
　常佩服。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『巴拿羅西亞閣下，陛下不是
[PAGE_BREAK]
　和您在一起的嗎？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001B]
『啊，我早該想到的，原來你
[PAGE_BREAK]
　是聖寇拉斯的部下‥‥』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0010]
『是的，我們在行經死亡沼澤
[PAGE_BREAK]
　的途中發現「死亡骷髏」在
[PAGE_BREAK]
　此出沒，陛下擔心他們的目
[PARAGRAPH]
　標可能是你們，所以命令我
[PAGE_BREAK]
　先來查看一番。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『原來如此‥‥那他往哪裡去
[PAGE_BREAK]
　了？』
[PAGE_BREAK]
[PORTRAIT_LEFT_BY_ID=0x001B]
『陛下說他已經發現了一些蛛
[PAGE_BREAK]
　絲馬跡，要趕往西方高塔一
[PAGE_BREAK]
　趟，我想現在應該還在路上
[PARAGRAPH]
　吧。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x000D]
『西方高塔‥‥那不就是「聖
[PAGE_BREAK]
　靈之塔」嗎？那裡是我們精
[PAGE_BREAK]
　靈族的發源聖地，想不到敵
[PARAGRAPH]
　人也看上了那裡，不知道他
[PAGE_BREAK]
　們有何意圖？』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『總之，我們要儘快和聖寇拉
[PAGE_BREAK]
　斯會合，看來他已發現了重
[PAGE_BREAK]
　要的線索，往後的幾天可能
[PARAGRAPH]
　得趕一下路，大家忍耐一點
[PAGE_BREAK]
　。』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0000]
『這有什麼問題！倒是約拿先
[PAGE_BREAK]
　生您年事已大，可不要太勉
[PAGE_BREAK]
　強喔！』
[PAGE_BREAK]
[PORTRAIT_RIGHT_BY_ID=0x0015]
『呵呵‥索爾，謝謝你的關心
[PAGE_BREAK]
　，我會注意的。我們這就上
[PAGE_BREAK]
　路吧！』
[END]
```
