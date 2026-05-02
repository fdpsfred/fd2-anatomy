# shop_table

`.object3 @ 0x62390`，28 entries × 28 bytes = 784 bytes。signature `80 81 84 A5 FF`。

## struct layout

```
offset  size  field     意義
+0      12    weapons   武器店 item IDs (0xFF = 空 slot)
+12     8     items     道具店 item IDs (0xFF = 空 slot)
+20     8     mystery   神秘商店 item IDs (0xFF = 空 slot)
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x56190` |
| FD2.EXE | `0x7B3A4` |
| Δ | +0xC200 |

## entry sample

Entry 0 (推測對應第 1 章商店)：

```
weapons: 80 81 84 A5 FF FF FF FF FF FF FF FF   (items 0x80, 0x81, 0x84, 0xA5)
items:   C0 FF FF FF FF FF FF FF                (item 0xC0 only)
mystery: FF FF FF FF 01 16 35 C0                (items 01, 16, 35, C0 in slots 4-7)
```

神秘商店前 4 slots 為空，物品在 slots 4-7 — 這個 pattern 在後續 entries 也
出現，推測是 UI layout 的固定 slot 設計。

## 後續未知資料段

shop_table 結束後到 `spell_learning_table` 起點之間 19 bytes：

```
0x6269C: 69 8F 9A A2 AB B1 FF        (7 bytes — 看起來像遞增序列 + FF terminator)
0x626A3: C3 CF FF FF FF FF FF FF     (8 bytes — 2 個非 FF + 6 個 FF，類似 item slots)
0x626AB: C3 CF FF FF FF FF FF FF     (8 bytes — 同樣 pattern 又一次)
```

後兩個 8-byte 區塊看似 「items[8]」 結構 (2 個物品 ID + 6 empty)。可能是 shop_table
的「特殊商店」補充資料，或另一張未定型小表。
