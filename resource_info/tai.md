# TAI.DAT — terrain overlay / AI 配對資料 (BG 配對)

每個 active TAI entry 都跟著 BG entry 同步載入 (推測 `TAI = "Terrain AI"` 縮寫)。
file size 94,917 bytes，56 entries (idx 0..55)。

## 檔案格式

LLLLLL archive (詳 `overview.md`)，與 BG.DAT 配對載入。

## 用途

從 callsite 模式：
- `play_spell_cast_sequence`: TAI load 緊接在 BG load 之後
- `execute_summon_spell_cast`: 載入 BG + TAI 配對
- `play_full_combat_cinematic`: 同模式
- `execute_special_attack_skill`: 同模式

可能用途：
1. **Terrain overlay data** (覆蓋層 sprite 提供地形動畫元素)
2. **AI behavior parameters** (per-terrain AI 修正參數)
3. **Tile palette data** (per-terrain palette 資訊)

`play_spell_cast_sequence` 的 dynamic call 將 `pBg_or_alt` 作為 `old_buf` 傳入
TAI load — 表示 TAI 與 BG 共用 buffer slot (一個 swap 一個就 free 另一個)。

## Entry format

```
+0x00  u16 LE  ?width   (例 0x009B = 155)
+0x02  u16 LE  ?height  (例 0x002A = 42)
+0x04  bytes   data (opcode/payload — byte-level decode 待後續驗證)
```

例 TAI[5]: 2813 bytes，header `9B 00 2A 00`，then `F7 80 8C 04 8B 01 8A 03 8B 01 8A 0E ...`

Placeholder pattern：16 個 entries 是 7-byte `0A 00 03 00 C9 C9 C9` (與 BG.DAT
placeholder 相同，共用 constant)。Placeholder idx：0, 1, 2, 3, 11, 20, ... (共
16 個 — 比 BG 多 6 個)。

## Static idx (2 個)

| idx | callsite | 用途 |
|---|---|---|
| 0x00 | `execute_summon_spell_cast` / `play_figani_char_intro_animation` | 通用 / placeholder pattern |
| 0x03 | `play_final_chapter_30_ending` | 結局特殊 terrain |

注意：TAI[0] 是 7-byte placeholder (與 default load 一致)。執行 spell cast 時若無
特殊 TAI 載入，可能 fall back to placeholder default。

## Dynamic-domain (4 個 dynamic callsite)

| Caller | 推測 idx |
|---|---|
| `execute_special_attack_skill` | terrain_id |
| `play_spell_cast_sequence` | 同 BG 配對 (= load(BG) 的 buffer pointer 傳入) |
| `play_full_combat_cinematic` | terrain dispatch |

## 完整分類

| 分類 | 計數 | 比例 |
|---|---|---|
| documented_static | 2 | 3.6% |
| documented_dynamic_domain (terrain-derived, BG-paired) | 38 | 67.9% |
| placeholder (7 bytes) | 16 | 28.5% |
| **TOTAL** | **56** | **100%** |

## 與 BG.DAT 的對應

兩 DAT 都 56 entries (相同 idx 數)；placeholder 分布略不同 (BG 10 個、TAI 16 個)，
但 placeholder constant 相同 (`0A 00 03 00 C9 C9 C9`)。配對載入發生在 spell
cinematic / combat cinematic / summon 等場景。

## 工具

- 解碼：`tools/decoders/bg_tai_decoder.py` (與 BG.DAT 共用)
