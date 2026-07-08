# data_fd2_chapter_intro_metadata_table

`.object3 @ 0x6238D`，26 entries × 31 bytes = 806 bytes。位址空間換算見
`assets/tables/_index.md`。table 緊接 `data_fd2_battle_character_growth_table` 最後一筆
entry 之後（`0x6238D`，零 padding），下一張表是
`data_fd2_battle_spell_learning_table @ 0x626B3`。

Ghidra type `chapter_intro_metadata_entry[26]`。每筆對應一個走 intro 畫面的章節，含 intro
外觀變體碼、特殊 hotkey 設定，以及**內嵌的三家商店物品清單**。

## struct layout（31 B）

| offset | size | 欄名 | 意義 |
|---|---|---|---|
| +0  | 1  | bCategory | intro 畫面**外觀變體碼**（0/1/2），非 story/battle 旗標，見下節 |
| +1  | 1  | bHotkey_state | 神秘商店隱藏 hotkey 對應的 `chapter_intro_menu_cursor_state` 值 |
| +2  | 1  | bHotkey_scancode | 開啟神秘商店的鍵盤 scancode（F-key 組合，見下節）|
| +3  | 12 | bWeapons[12] | 武器店 item ID（0xFF = 空 slot）|
| +15 | 8  | bItems[8] | 道具店 item ID（0xFF = 空）|
| +23 | 8  | bMystery[8] | 神秘商店 item ID（0xFF = 空）|

## bCategory 語意（外觀變體碼，非 story/battle 旗標）

`bCategory` 值域 0/1/2，是 intro 畫面的外觀變體碼，**不決定** story/battle 分派（那是另一張表
`data_fd2_chapter_per_chapter_category_table @ 0x526B9` 以 chapter_id 索引，見
`chapter_category.md`）。它有兩個用途：

1. `fd2_chapter_transition_menu` 用 `bCategory` 直接索引
   `data_fd2_chapter_intro_panel_resource_idx_per_metadata_category_table @ 0x526D7`
   = `{0x0B, 0x3D, 0x3E}`（0→0x0B、1→0x3D、2→0x3E），選 FDOTHER.DAT 的 intro panel 背景 RLE。
2. 以 `bCategory × 6 + chapter_intro_menu_cursor_state` 索引 intro portrait pose 座標表
   `[3][6]`（`0x52635` / `0x52647`），決定主角 portrait 繪製位置與 transition zoom 目標座標。

## 商店資料（內嵌，無獨立表）

每章三家商店的物品清單就是本 entry 的 `+3 / +15 / +23` 三段，沒有獨立的 shop 表。
`fd2_load_chapter_shop_item_ids` 依 `chapter_intro_menu_cursor_state` 選段（state==1 → weapons
cap 12 src_offset 0x03；state==3 → items cap 8 src_offset 0x0F；其他 → mystery cap 8
src_offset 0x17），逐 byte 讀到 0xFF terminator，餵給 `fd2_run_buy_item_menu` /
`fd2_run_sell_item_menu` / `fd2_run_give_item_menu`（`src/ui_menu/shop.c`）。逐章商店品項見
各 `chapters/chapter_NN.md` §商店。

- **神秘商店**（mystery，+23 段）由 `+2 bHotkey_scancode` 的隱藏 hotkey 開啟；scancode 是 F-key 組合，
  逐章不同（Shift+F1..F10 = 0x54..0x5D、Ctrl+F1..F10 = 0x5E..0x67、Alt+F1..F10 = 0x68..0x71）。
- **entry ↔ 章映射 = `entry_index = chapter_n − 1`**（entry0 = 第 1 章起始裝備、entry25 = 第 26 章末期裝備）。
- **只有 story 章顯示商店**：`fd2_chapter_transition_menu` 依 `data_fd2_chapter_per_chapter_category_table`
  對 story 章（category = 0）才走 intro 主選單與商店；battle 章（ch23/24/25/28/29/30）跳過 intro，即使 entry
  有值也不出商店。第 22 章雖為 story 章，其 entry 為全零（無商店品項）。

位址 `0x62390`（= `0x6238D + 3`）落在本表 entry[0] 的 bWeapons slot 中段，並非獨立的商店表
——FD2 沒有獨立的 28×28 商店表。商店資料一律以本表 entry 的內嵌欄位（+3 / +15 / +23）為準。

## entry sample

Entry 0（chapter 1）：
`00 00 54 80 81 84 A5 FF FF FF FF FF FF FF FF C0 FF FF FF FF FF FF FF 01 16 35 C0 C1 84 FF FF`

```
bCategory=00（intro 變體 0：panel 資源 0x0B、pose row 0）
bHotkey_state=00  bHotkey_scancode=0x54（Shift+F1，神秘商店 hotkey）
weapons: 0x80 0x81 0x84 0xA5
items:   0xC0
mystery: 0x01 0x16 0x35 0xC0 0xC1 0x84
```

## entry count 與存取

26 entries 對應走 intro 畫面的章節。Accessor
`fd2_get_chapter_intro_metadata_entry @ 0x4E4B9` 公式 `base + (chapter_id − 1) × 0x1F`。
部分章節的 entry 為全零（未填 intro / 商店資料）。**是否走 intro 與 story/battle 分派無關**：
story/battle 只由 `data_fd2_chapter_per_chapter_category_table @ 0x526B9`（chapter_id 索引，見
`chapter_category.md`）決定，不能用「entry 是否全零」判斷章節屬性——例如第 22 章是 story 章
（category=0），而某些 battle 章反而帶有已填但 intro 流程不讀取的 entry。

## 全 26 entries

逐章 intro 商店品項見對應 `chapters/chapter_NN.md`。
