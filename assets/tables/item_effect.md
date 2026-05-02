# item_effect_table

`.object3 @ 0x602AC`，215 entries × 23 bytes = 4945 bytes。signature
`0B 01 0A 00 5F 00`。

## struct layout

```
offset  size  field         意義
+0      1     unknown_00    prefix；entry 0 是 0x0B (signature marker)，其他為 0x00
+1      1     TY            物品類型 (01=劍 02=刀 03=槍 04=斧 05=弓 06=杖 07=爪 08=機械手臂 ... 20h=道具)
+2      2     AP            攻擊力增值 (u16 LE)
+4      2     HT            命中率增值
+6      2     DP            防禦力增值
+8      2     EV            速度 / 迴避增值
+10     1     S1            附加屬性 (02=中毒 03=雙擊 04=暴擊)
+11     1     S2            機率 %
+12     1     R1            最小攻擊距離
+13     1     R2            最大攻擊距離
+14     1     K1            使用效果 (見 `assets/items.md`)
+15     1     K2            數量 / 法術 ID (low)
+16     1     K3            數量 / 法術 ID (high)
+17     1     K4            施放距離 flags (bit4 = 直線，低 4 位 = 距離)
+18     1     K5            作用對象 (00 = 敵, 01 = 己)
+19     1     K6            影響範圍 (最大 3)
+20     2     MM            價格 (u16 LE)
+22     1     trailing_22   未知尾端 byte (多為 0x05)
```

## 跨版本偏移

| 版本 | 位址 |
|---|---|
| FD2.LE | `0x540AC` |
| FD2.EXE | `0x792C1` |
| Δ | +0xC200 |

## entry sample

| entry | bytes | 解析 |
|---|---|---|
| 00 (短劍) | `0B 01 0A 00 5F 00 ... 32 00 05` | TY=1(劍) AP=10 HT=95 MM=50 |
| 01 (闊劍) | `00 01 14 00 5F 00 ... FA 00 05` | TY=1(劍) AP=20 HT=95 MM=250 |

## 全 215 entries

詳 `assets/items.md`。

## prefix 細節

第 0 byte 是真實 prefix，不是 layout placeholder。entry 0 prefix = 0x0B
(與 signature 第一 byte 一致)，其他 entry prefix 都是 0x00。
