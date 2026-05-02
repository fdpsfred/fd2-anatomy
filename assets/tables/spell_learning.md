# spell_learning_table

`.object3 @ 0x626B3`，20 entries × 12 bytes = 240 bytes。signature `05 11 09 01`。

## struct layout

每 entry = 6 × `spell_learn_pair` (2 bytes 一組，共 12 bytes)：

```c
struct spell_learn_pair {
    u8 lv;         // 學得等級 (0xFF = 空 slot)
    u8 spell_id;   // 法術 ID (參 spell_effect_table)
};
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x564B3` |
| FD2.EXE | `0x7B6C7` |
| Δ | +0xC200 |

## entry samples

Entry 0: `05 11 09 01 0F 1A 15 02 1A 1B FF FF`

```
pair 0: LV  5 → spell 0x11 (魔刃術)
pair 1: LV  9 → spell 0x01 (烈炎術)
pair 2: LV 15 → spell 0x1A (毒擊術)
pair 3: LV 21 → spell 0x02 (炎龍術)
pair 4: LV 26 → spell 0x1B (麻痹術)
pair 5: empty
```

物理職角色成長路線，學到輔助 / 攻擊法術。

Entry 1: `07 14 0B 0E 10 12 18 16 1E 17 FF FF`

```
pair 0: LV  7 → spell 0x14 (解毒術)
pair 1: LV 11 → spell 0x0E (回復術)
pair 2: LV 16 → spell 0x12 (魔鎧術)
pair 3: LV 24 → spell 0x16 (封咒術)
pair 4: LV 30 → spell 0x17 (傳送術)
pair 5: empty
```

僧侶 / 法師輔助路線。

## 用途

`character_growth_entry.spell_learning_idx` 指向此表。當角色升到 `lv` 等級時，
遊戲自動把 `spell_id` 加進 `runtime_char.pSpells_known_bitmap`。

## 全 20 entries 對照

詳 `assets/characters.md` 法術習得段（依 character_growth.spell_learning_idx 串接）。

## table 後續未知資料段

`0x627A3..0x627D8` 25 bytes 是對齊 padding 與後續 cutscene_event_script_table 的
起點，與 spell_learning 無關。
