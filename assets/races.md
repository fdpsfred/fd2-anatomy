# 種族 (race_id)

`race_id` 是 `data_fd2_battle_enemy_data_table @ 0x61AF9`（enemy_data +0）與
`data_fd2_battle_character_base_table @ 0x61DA1`（character_base +0）的第一個 byte，代表單位的種族。
`fd2_init_runtime_char_for_battle @ 0x10C50` 生成單位時把這個 byte 複製到
`runtime_char +0x1F`（archetype_flag），玩家與敵人兩條分支皆然。

## race_id ↔ 種族

值域：玩家角色（character_base）用到 1..6；敵人（enemy_data）用到 1..0x0A。種族名取自各 id 下
單位的名稱。

| race_id | 種族 | 代表單位 |
|---|---|---|
| 1  | 人類 | 索爾等多數玩家角色；士兵 / 傭兵 / 騎兵 / 弓箭手 / 魔法師 / 僧侶 / 盜賊 / 村民；薩卡、瑪爾 |
| 2  | 精靈 | 貝克威、珊、希爾法；敵方精靈 |
| 3  | 豹人 | 塞可邦勒、謝多；友軍 / 敵方豹人 |
| 4  | 龍人 | 凱拉斯、米亞斯多德、聖寇拉斯、巴拿羅西亞；龍劍士、龍人戰士 / 龍人法師 |
| 5  | 魔族 | 達克塞、亞齊梅吉；魔鬼 / 惡魔 / 大惡魔 |
| 6  | 機械 | 蓋亞、渥德；機甲兵 / 機甲射手 / 機甲守衛等 |
| 7  | 魔神 | 地魔神 / 水魔神 / 風魔神 / 火魔神（四元素魔神） |
| 8  | 獸人 | 獸人、獸人隊長 |
| 9  | 魔物 | 沼澤怪物、空魔神 |
| 0A | 龍   | 火龍、雷龍、暗黑龍 |

空魔神名字雖含「魔神」，race_id 卻是 9（魔物）而非 7（元素魔神），是資料本身的分類。

## 與 FDFIELD 「race」欄位的區別

FDFIELD 的 char_spawn_record 裡另有一個 loader 稱作「race / target_race_id」的 byte
（record +0x15，entry-absolute +0x98），`fd2_load_chapter_portraits_and_dump_tmp` 依它篩選某個
過場時刻要載入哪些單位，值如 1 / 2 / 3 / 6 / 7。它是**登場分組（過場波次）選擇碼**，和本檔的種族
`race_id` 是不同欄位：同一角色兩處的值並不一致（例如哈瓦特種族為人類 = 1，其 spawn record 的
race = 7）。char_spawn_record layout 見 `resource_info/fdfield.md`，實際用法見各章 `chapters/`。
