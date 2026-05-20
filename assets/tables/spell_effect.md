# data_fd2_battle_spell_effect_table

`.object3 @ 0x619FD`，36 entries × 7 bytes = 252 bytes。signature `32 00 5A 05`。

## struct layout

```
offset  size  field  意義
+0      2     DA     最大傷害 / 恢復力 (u16 LE)
+2      1     HT     命中率 %
+3      1     DS     施放距離 flags (bit4 = 直線, 低 4 位 = 距離)
+4      1     RN     影響範圍 (最大 3)
+5      1     MP     消耗法力
+6      1     WH     作用對象 (00 = 敵, 01 = 己)
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x557FD` |
| FD2.EXE | `0x7AA11` |
| Δ | +0xC200 |

## entry samples

| spell_id | 名稱 | bytes | 解析 |
|---|---|---|---|
| 00 | 火炎術 | `32 00 5A 05 00 02 00` | DA=50  HT=90 DS=5 RN=0 MP=2  WH=0 |
| 01 | 烈炎術 | `78 00 5A 05 00 06 00` | DA=120 HT=90 DS=5 RN=0 MP=6  WH=0 |
| 02 | 炎龍術 | `FA 00 5A 05 01 14 00` | DA=250 HT=90 DS=5 RN=1 MP=20 WH=0 |
| 03 | 天火術 | `F4 01 55 05 01 2A 00` | DA=500 HT=85 DS=5 RN=1 MP=42 WH=0 |

## 全 36 entries

詳 `assets/spells.md` (含每個 spell 的傷害 / 範圍 / MP / 命中率)。

## 邊界

`data_fd2_battle_spell_effect_table` 結束 at `0x619FD + 252 = 0x61AF9`，剛好接 `data_fd2_battle_enemy_data_table`
起點。表之間零 padding。
