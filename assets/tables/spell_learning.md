# data_fd2_battle_spell_learning_table

`.object3 @ 0x626B3`，20 entries × 12 bytes = 240 bytes。entry 0 前 4 byte `05 11 09 01`
可當定位 anchor。位址空間換算見 `assets/tables/_index.md`。

Ghidra type `spell_learning_entry[20]`。每個 entry 由 `character_growth.spell_learning_idx`
指向使用。

## struct layout（12 B）

每 entry = 6 × `spell_learn_pair`（2 byte 一組）：

```c
struct spell_learn_pair {
    uint8 lv;        /* 學得等級（0xFF = 空 slot）*/
    uint8 spell_id;  /* 法術 ID（參 spell_effect.md）*/
};
```

## 用途

`character_growth.spell_learning_idx` 指向本表。角色升到 `lv` 時，遊戲把 `spell_id`
加進 `runtime_char.spells_known_bitmap`。

## entry sample

Entry 0：`05 11 09 01 0F 1A 15 02 1A 1B FF FF`

```
pair 0: LV  5 → spell 0x11
pair 1: LV  9 → spell 0x01
pair 2: LV 15 → spell 0x1A
pair 3: LV 21 → spell 0x02
pair 4: LV 26 → spell 0x1B
pair 5: 空
```

## 邊界

表結束於 `0x626B3 + 240 = 0x627A3`；其後 `0x627A3` 起是 `glyph_blit_state`（1bpp glyph
blitter 的共用 scratch render-state，17 B，見 `src/include/types.h`），與本表無關。

## 全 20 entries

逐職業習得路線見 `assets/characters.md`（依 character_growth.spell_learning_idx 串接）。
