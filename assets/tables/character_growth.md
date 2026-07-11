# data_fd2_battle_character_growth_table

`.object3 @ 0x620A1`，68 entries × 11 bytes = 748 bytes，範圍 `[0x620A1, 0x6238D)`。
entry 0 前 4 byte `06 08 04 06` 可當定位 anchor。位址空間換算見 `assets/tables/_index.md`。

Ghidra type `char_growth_entry[68]`；C struct `character_growth` 定義在
`src/include/types.h`。

## struct layout（11 B）

每個屬性佔 2 byte：`_min` = 每級最小成長，`_max` = **exclusive 上界（= 最大成長 + 1）**。

| offset | size | types.h 欄名 | 意義 |
|---|---|---|---|
| +0  | 1 | ap_min | 每級 AP 最小成長 |
| +1  | 1 | ap_max | AP exclusive 上界（最大成長 = ap_max − 1）|
| +2  | 1 | dp_min | |
| +3  | 1 | dp_max | DP exclusive 上界 |
| +4  | 1 | dx_min | |
| +5  | 1 | dx_max | DX exclusive 上界 |
| +6  | 1 | hp_min | |
| +7  | 1 | hp_max | HP exclusive 上界 |
| +8  | 1 | mp_min | |
| +9  | 1 | mp_max | MP exclusive 上界 |
| +10 | 1 | spell_learning_idx | `spell_learning_table` 的 index；0xFF = 無 |

## 成長 roll 公式（exclusive 上界語意）

唯一消費 `_max` 的程式是升級 / 轉職共用的
`fd2_roll_stat_gain_and_show_message @ 0x1E529`：

```
range = _max - _min
若 range == 0：不呼叫 RNG，gain 恆等於 _min
否則：         gain = _min + (fd2_advance_rng_state() % range)   -> gain 均勻落在 [_min, _max-1]
```

因此 raw byte `06 08` 代表每級成長 **6~7**（不是 6~8）。`_max == _min` 的 pair（如 MP
`00 00`、哈瓦特 DX `01 01`）成長恆為 `_min`。出場屬性計算（`fd2_init_runtime_char_*`）
只讀 `_min` 欄，不讀 `_max`。

## 組語證據（最大成長 = `_max − 1`）

`fd2_roll_stat_gain_and_show_message @ 0x1E529`（`src/battle/btl_turn.c`）核心指令：

```
0001e53f  MOVZX EBP, byte[EAX]     ; EBP = _min
0001e542  MOVZX ESI, byte[EAX+1]   ; ESI = _max（exclusive 上界）
0001e546  SUB   ESI, EBP           ; range = _max − _min（無 +1）
0001e548  JZ    0x1e558            ; range==0 → 跳過 roll，gain = _min
0001e54a  CALL  0x4E893            ; EAX = fd2_advance_rng_state()
0001e551  SAR   EDX, 0x1F          ; sign-extend EAX → EDX:EAX
0001e554  IDIV  ESI                ; EDX = rng % range
0001e558  ADD   EBP, ESI           ; gain = _min + (rng % range)
```

對應 C：

```c
range      = growth_pair[1] - growth_pair[0];               /* _max − _min，無 +1 */
rand_extra = (range != 0) ? fd2_advance_rng_state() % range : 0;
gain       = growth_pair[0] + rand_extra;                   /* 落在 [_min, _max − 1] */
```

`range` 沒有 `+1`，`rng % range ∈ [0, range−1]`，故原版每級最大成長 = `_min + range − 1 = _max − 1`（非 `_max`）。

## 「升級最大值修改版」執行檔的 off-by-one

存在一支被改過 binary 的 `fd2.exe`，改法為**每次升級強制給每個屬性最大成長**。該修改把每級最大取成
`_max` 本身（exclusive 上界那個 byte）而非原版真正上限 `_max − 1`，等效於直接用整個 `range` 當 roll 結果
（`gain = _min + range = _max`）而非 `_min + (range − 1)`——即**每級比原版真正最大多 1**。

- **影響**：此 exe 的實機成長值每級 +1 高於原版，N 級累積多 N。例：索爾 劍士 LV40 → 轉職英雄 → LV40
  全程最大，原版 `AP 839 / DP 559 / DX 200`，此修改版 `AP 918 / DP 638 / DX 279`。
- `tools/growth_table` 的「全程最大」刻意保持**原版真正上限 `_max − 1`**。
- **佐證強度**：原版 `_max − 1` 為**已驗證**（上節組語 + C @ 0x1E529）；修改版的 off-by-one 為**推論**——
  由實機屬性值精準吻合 `_max`（全程均勻 +1／級）反推，尚未反組譯該修改版 binary。

## entry sample

Entry 0（索爾基礎）：`06 08 04 06 02 03 08 0C 00 00 FF`

```
AP min/上界 = 6 / 8   -> 實際每級成長 6~7
DP min/上界 = 4 / 6   -> 4~5
DX min/上界 = 2 / 3   -> 恆 2
HP min/上界 = 8 / 12  -> 8~11
MP min/上界 = 0 / 0   -> 恆 0（range==0，不 roll）
spell_learning_idx = 0xFF（無）
```

## entry 數量與邊界

68 entries 涵蓋 32 角色的基礎 growth 與轉職後 growth，含少數 reserved slot。最後一筆
（entry 67 = `08 0C 08 0A 03 03 0A 0F 00 00 FF`）結束於 `0x6238D`，其後緊接
`data_fd2_chapter_intro_metadata_table @ 0x6238D`，兩表零 padding（`0x6238D` 起的
`00 00 54 ...` 是 intro_metadata entry[0] 的 bCategory / hotkey，不屬本表）。

## 對應 spell_learning

`spell_learning_idx` 指向 `data_fd2_battle_spell_learning_table @ 0x626B3` 的對應 entry，
描述此職業在哪些等級學什麼法術。詳 `spell_learning.md`。

## 全 68 entries

數值與逐角色升級範圍見 `assets/characters.md`。
