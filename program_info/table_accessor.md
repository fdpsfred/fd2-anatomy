# table_accessor

5 個 tiny function，每個 return 一張 `.object3` 資料表的 base pointer 加上
index offset。Borland C++ 編譯器把「take address of global array indexed by
variable」的表達式發成共用 helper function，而不是直接 inline，可能是 code-size
最佳化或 debug build artifact。

## 5 個 getter

| 位址 | 名稱 | 回傳 | Entry size | Entry count |
|---|---|---|---|---|
| `0x0004E4A2` | `get_spell_learning_entry` | `&spell_learning_table[idx]` | 12 B | 20 |
| `0x0004E4D1` | `get_char_growth_entry` | `&character_growth_table[idx]` | 11 B | 68 |
| `0x0004E4E8` | `get_char_base_entry` | `&character_base_table[idx]` | 24 B | 32 |
| `0x0004E4FF` | `get_enemy_data_entry` | `&enemy_data_table[idx]` | 10 B | 68 |
| `0x0004E516` | `get_spell_effect_entry` | `&spell_effect_table[idx]` | 7 B | 36 |

5 個 helper 連續排列在 `.object1` 的 `0x4E4A2-0x4E51A` (116 bytes)，緊接在其他
battle-相關 function 之後、靠近 `.object1` 尾端 (0x4EBD8)。

## 範例 decompile

```c
undefined1 * __cdecl get_char_growth_entry(int param_1) {
    return &character_growth_table[param_1].AP_min;
}
```

命名後其他 function (例如 `calc_magic_damage`) 的 decompile 會顯示成
`psVar3 = (short *)get_spell_effect_entry(param_5); sVar1 = *psVar3;` (= spell.DA)，
比 raw `FUN_0004e516` 容易讀。

## 為何只有 5 個

`item_effect_table` / `shop_table` / `job_magic_resist_table` / `job_crit_table`
沒有對應的 helper。這 4 張表的 access pattern 是直接從 global pointer 做 offset，
編譯器選擇直接 inline 沒產生共享 helper。
