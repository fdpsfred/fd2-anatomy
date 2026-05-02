# character_base_table

`.object3 @ 0x61DA1`，32 entries × 24 bytes = 768 bytes。
signature `01 01 01 2A`。

## struct layout

```
offset  size  field   意義
+0      1     RA      種族 ID
+1      1     CL      職業 ID
+2      1     LV      出場時等級
+3      2     HP      基礎 HP (u16 LE)；效 HP = HP + (LV-1) × HP_min
+5      2     MP      基礎 MP；效 MP = MP + (LV-1) × MP_min
+7      1     MV      移動力
+8      4     MG[4]   初始已會法術 (4 byte slots; 0 = 無 / 非 0 = spell_id)
+12     6     IT[6]   初始裝備 item IDs (0xFF = 空 slot)
+18     2     AP      基礎攻擊力；效 AP = AP + LV × AP_min
+20     2     DP      基礎防禦力
+22     2     DX      基礎敏捷 / 迴避
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x55BA1` |
| FD2.EXE | `0x7ADB5` |
| Δ | +0xC200 |

## entry sample

Entry 0 (索爾)：`01 01 01 2A 00 00 00 04 00 00 00 00 00 84 C0 FF FF FF 00 00 00 00 00 00`

```
RA=01 CL=01 LV=01
HP=42 MP=0 MV=4
MG=00 00 00 00        (出場無已會法術)
IT=00 84 C0 FF FF FF  (短劍 / item 0x84 / item 0xC0 + 3 空 slot)
AP=0 DP=0 DX=0        (基礎全 0，實際靠 LV × growth + 裝備)
```

## 全 32 entries

詳 `assets/characters.md`。
