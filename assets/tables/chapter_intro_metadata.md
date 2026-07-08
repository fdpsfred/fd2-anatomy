# data_fd2_chapter_intro_metadata_table

`.object3 @ 0x6238D`，26 entries × 31 bytes = 806 bytes。

每筆對應一個 story 章節 (chapters 1..26) 的 intro 階段資料，含 chapter category
旗標、特殊 hotkey 設定、以及 3 個商店 (weapons / items / mystery) 的物品 ID 列表。

table 接在 `data_fd2_battle_character_growth_table` (尾端 padding 0x6238D 之前)
正後方，下一張表是 `data_fd2_battle_spell_learning_table @ 0x626B3`。

## struct layout (chapter_intro_metadata_entry, 31 B)

```
offset  size  field             意義
+0      1     bCategory         chapter category byte
                                 0 = story chapter (走 intro panel + shop menu)
                                 非 0 = battle chapter (跳過 intro，直接 transition)
+1      1     bHotkey_state     觸發特殊 hotkey commit 的 data_fd2_chapter_intro_menu_cursor_state 值
                                 (0x5412B)；hotkey 命中後該 state 跳為 5
+2      1     bHotkey_scancode  特殊 commit hotkey 的鍵盤 scancode
+3      12    bWeapons[12]      武器店 item IDs (0xFF = 空 slot)
                                 對應 fd2_load_chapter_shop_item_ids 的 state==1 路徑
                                 (cap 12, src_offset 0x03)
+15     8     bItems[8]         道具店 item IDs (0xFF = 空)
                                 state==3 (cap 8, src_offset 0x0F)
+23     8     bMystery[8]       神秘商店 item IDs (0xFF = 空)
                                 state==其他 (cap 8, src_offset 0x17)
```

## entry sample

Entry 0 (chapter 1)：

```
00 00 54 80 81 84 A5 FF FF FF FF FF FF FF FF C0 FF FF FF FF FF FF FF 01 16 35 C0 C1 84 FF FF
```

- `bCategory=00` (story chapter)
- `bHotkey_state=00, bHotkey_scancode=0x54` (F11)
- weapons: `0x80, 0x81, 0x84, 0xA5` (4 種武器)
- items:   `0xC0`
- mystery: `0x01, 0x16, 0x35, 0xC0, 0xC1, 0x84` (6 個物品)

Entry 1 (chapter 2)：

```
02 01 5F 00 01 20 84 A5 FF FF FF FF FF FF FF C0 FF FF FF FF FF FF FF C1 CE FF FF FF FF FF FF
```

- `bCategory=02`, `bHotkey_state=01`, `bHotkey_scancode=0x5F` (F1)
- weapons: `0x00, 0x01, 0x20, 0x84, 0xA5`
- items:   `0xC0`
- mystery: `0xC1, 0xCE`

## entry count

26 (chapters 1..26)。Accessor `fd2_get_chapter_intro_metadata_entry @ 0x4E4B9`
的公式 `base + (chapter_id-1) * 0x1F` 對 chapter_id > 26 會回傳指到
`data_fd2_battle_spell_learning_table` 內部的指標；遊戲不會這樣呼叫，因為
chapters 27..30 屬 endgame / 非 story chapter，其 transition dispatch 不走
intro panel 流程 (詳 `chapters/chapter_29.md` /
`chapters/chapter_30.md`)。

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x6238D` |

FD2 strategy guide 文件以 28-byte 為單位描述 entry 內 `+3..+30` 的商店物品區段，
實際 entry stride 為 31 B (含 +0..+2 的 chapter category / hotkey header)。

## 存取路徑

`fd2_get_chapter_intro_metadata_entry @ 0x4E4B9` 是唯一直接 reader。Indirect
callers 透過 `chapter_transition_resume_metadata @ 0x54137` (全域變數) 緩存
entry pointer，後續以 state-based offset 抓 weapons / items / mystery slice 給
`fd2_run_buy_item_menu` / `fd2_run_sell_item_menu` / `fd2_run_equip_member_menu`
/ `fd2_run_give_item_menu`。詳 `program_info/chapter.md` (or chapter intro
dispatch doc)。
