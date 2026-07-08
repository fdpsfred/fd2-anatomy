# data_fd2_chapter_per_chapter_category_table

`.object2 @ 0x526B9`，byte[30] = 30 bytes。位址空間換算見 `assets/tables/_index.md`。

Ghidra type `byte[30]`；C 端 `const uint8 data_fd2_chapter_per_chapter_category_table[30]`。

## struct 與語意

```
byte[30]   以 chapter_id（0..29，= 章號 − 1）索引：0 = story 章、非 0 = battle 章
```

story 章走 intro panel + 電台式主選單（含商店 / 存讀檔）；battle 章直接進戰鬥、跳過 intro。
這才是 story/battle 的分派來源；`chapter_intro_metadata` 的 `bCategory` 是另一回事（intro 外觀
變體碼，見 `chapter_intro_metadata.md`）。

## 消費端

- `fd2_chapter_transition_menu`（`src/field/chtrans.c`）：
  `if (table[current_chapter_id] == 0)` → 走 story intro 分支。
- `fd2_save_current_state_to_slot` / `fd2_load_state_from_selected_slot`
  （`src/save/save.c`）：以本表判斷該章是否允許 story 章的存讀檔行為。

## 全表值（byte[30]，以 chapter_id 索引）

```
chapter_id  00..15  全 0（章 1..16 皆 story）
chapter_id  16..21  全 0（章 17..22 story）
chapter_id  22 23 24 = 1 1 1  （章 23 24 25 = battle）
chapter_id  25 26    = 0 0    （章 26 27 = story）
chapter_id  27 28 29 = 1 1 1  （章 28 29 30 = battle）
```

即 battle 章為第 23、24、25、28、29、30 章，其餘為 story 章。逐章分類與流程見
`chapters/_index.md`（消費端 `src/field/chtrans.c`）。
