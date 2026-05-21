# 全遊戲共用對話片段 (FDTXT.DAT entry 0)

全遊戲共用對話片段庫 (661 pages)。`SUB_DIALOG_A` / `SUB_DIALOG_B` opcode 從
chapter dialogs 遞迴到此；`fd2_play_final_chapter_30_ending` 用 `char.identity+1`
(角色名) 與 `char.bJob_id+0x96` (職業名) 動態 page 索引拿系統文字。

## Entry metadata

- idx: 0
- purpose: `all_game_text` (`fd2_main` 啟動載入)
- range: [0x92, 0x1e66) size 7636 bytes
- page_count: 661

## Notation

- `[OPCODE]` / `[OPCODE=0xNNNN]` — dialog VM control code (詳見 `resource_info/fdtxt.md`)
- `{ascii char}` — 直接渲染的 ASCII glyph (font atlas indices 0x20–0x7E)
- 中文字 — glyph_id 已從 `assets/text/glyph_table.md` 替換為實際字符
- `〈NNNN〉` — 該 glyph_id 無 lookup entry 時的 fallback (極少出現)

## Pages

### Page 0  (offset 0x52a, span 74 bytes)

```text
0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ
[END]
```

### Page 1  (offset 0x574, span 6 bytes)

```text
索爾
[END]
```

### Page 2  (offset 0x57a, span 6 bytes)

```text
哈諾
[END]
```

### Page 3  (offset 0x580, span 6 bytes)

```text
鐵諾
[END]
```

### Page 4  (offset 0x586, span 8 bytes)

```text
哈瓦特
[END]
```

### Page 5  (offset 0x58e, span 8 bytes)

```text
亞雷斯
[END]
```

### Page 6  (offset 0x596, span 6 bytes)

```text
洛娜
[END]
```

### Page 7  (offset 0x59c, span 6 bytes)

```text
萊汀
[END]
```

### Page 8  (offset 0x5a2, span 10 bytes)

```text
蘭斯洛特
[END]
```

### Page 9  (offset 0x5ac, span 8 bytes)

```text
希莉亞
[END]
```

### Page 10  (offset 0x5b4, span 6 bytes)

```text
悠妮
[END]
```

### Page 11  (offset 0x5ba, span 6 bytes)

```text
瑪琳
[END]
```

### Page 12  (offset 0x5c0, span 8 bytes)

```text
索菲亞
[END]
```

### Page 13  (offset 0x5c8, span 6 bytes)

```text
凱麗
[END]
```

### Page 14  (offset 0x5ce, span 8 bytes)

```text
貝克威
[END]
```

### Page 15  (offset 0x5d6, span 4 bytes)

```text
珊
[END]
```

### Page 16  (offset 0x5da, span 10 bytes)

```text
塞可邦勒
[END]
```

### Page 17  (offset 0x5e4, span 8 bytes)

```text
凱拉斯
[END]
```

### Page 18  (offset 0x5ec, span 12 bytes)

```text
米亞斯多德
[END]
```

### Page 19  (offset 0x5f8, span 6 bytes)

```text
蜜蒂
[END]
```

### Page 20  (offset 0x5fe, span 8 bytes)

```text
羅德曼
[END]
```

### Page 21  (offset 0x606, span 6 bytes)

```text
莎拉
[END]
```

### Page 22  (offset 0x60c, span 6 bytes)

```text
約拿
[END]
```

### Page 23  (offset 0x612, span 8 bytes)

```text
卡里斯
[END]
```

### Page 24  (offset 0x61a, span 6 bytes)

```text
羅蘭
[END]
```

### Page 25  (offset 0x620, span 8 bytes)

```text
希爾法
[END]
```

### Page 26  (offset 0x628, span 6 bytes)

```text
謝多
[END]
```

### Page 27  (offset 0x62e, span 10 bytes)

```text
聖寇拉斯
[END]
```

### Page 28  (offset 0x638, span 12 bytes)

```text
巴拿羅西亞
[END]
```

### Page 29  (offset 0x644, span 8 bytes)

```text
達克塞
[END]
```

### Page 30  (offset 0x64c, span 10 bytes)

```text
亞齊梅吉
[END]
```

### Page 31  (offset 0x656, span 6 bytes)

```text
蓋亞
[END]
```

### Page 32  (offset 0x65c, span 6 bytes)

```text
渥德
[END]
```

### Page 33  (offset 0x662, span 2 bytes)

```text
[END]
```

### Page 34  (offset 0x664, span 2 bytes)

```text
[END]
```

### Page 35  (offset 0x666, span 2 bytes)

```text
[END]
```

### Page 36  (offset 0x668, span 2 bytes)

```text
[END]
```

### Page 37  (offset 0x66a, span 2 bytes)

```text
[END]
```

### Page 38  (offset 0x66c, span 2 bytes)

```text
[END]
```

### Page 39  (offset 0x66e, span 2 bytes)

```text
[END]
```

### Page 40  (offset 0x670, span 2 bytes)

```text
[END]
```

### Page 41  (offset 0x672, span 2 bytes)

```text
[END]
```

### Page 42  (offset 0x674, span 2 bytes)

```text
[END]
```

### Page 43  (offset 0x676, span 2 bytes)

```text
[END]
```

### Page 44  (offset 0x678, span 2 bytes)

```text
[END]
```

### Page 45  (offset 0x67a, span 2 bytes)

```text
[END]
```

### Page 46  (offset 0x67c, span 2 bytes)

```text
[END]
```

### Page 47  (offset 0x67e, span 2 bytes)

```text
[END]
```

### Page 48  (offset 0x680, span 2 bytes)

```text
[END]
```

### Page 49  (offset 0x682, span 2 bytes)

```text
[END]
```

### Page 50  (offset 0x684, span 2 bytes)

```text
[END]
```

### Page 51  (offset 0x686, span 2 bytes)

```text
[END]
```

### Page 52  (offset 0x688, span 2 bytes)

```text
[END]
```

### Page 53  (offset 0x68a, span 2 bytes)

```text
[END]
```

### Page 54  (offset 0x68c, span 2 bytes)

```text
[END]
```

### Page 55  (offset 0x68e, span 2 bytes)

```text
[END]
```

### Page 56  (offset 0x690, span 2 bytes)

```text
[END]
```

### Page 57  (offset 0x692, span 2 bytes)

```text
[END]
```

### Page 58  (offset 0x694, span 2 bytes)

```text
[END]
```

### Page 59  (offset 0x696, span 2 bytes)

```text
[END]
```

### Page 60  (offset 0x698, span 2 bytes)

```text
[END]
```

### Page 61  (offset 0x69a, span 2 bytes)

```text
[END]
```

### Page 62  (offset 0x69c, span 2 bytes)

```text
[END]
```

### Page 63  (offset 0x69e, span 2 bytes)

```text
[END]
```

### Page 64  (offset 0x6a0, span 2 bytes)

```text
[END]
```

### Page 65  (offset 0x6a2, span 2 bytes)

```text
[END]
```

### Page 66  (offset 0x6a4, span 2 bytes)

```text
[END]
```

### Page 67  (offset 0x6a6, span 2 bytes)

```text
[END]
```

### Page 68  (offset 0x6a8, span 2 bytes)

```text
[END]
```

### Page 69  (offset 0x6aa, span 6 bytes)

```text
士兵
[END]
```

### Page 70  (offset 0x6b0, span 6 bytes)

```text
騎兵
[END]
```

### Page 71  (offset 0x6b6, span 6 bytes)

```text
傭兵
[END]
```

### Page 72  (offset 0x6bc, span 6 bytes)

```text
豹人
[END]
```

### Page 73  (offset 0x6c2, span 6 bytes)

```text
精靈
[END]
```

### Page 74  (offset 0x6c8, span 6 bytes)

```text
龍人
[END]
```

### Page 75  (offset 0x6ce, span 6 bytes)

```text
惡魔
[END]
```

### Page 76  (offset 0x6d4, span 2 bytes)

```text
[END]
```

### Page 77  (offset 0x6d6, span 6 bytes)

```text
士兵
[END]
```

### Page 78  (offset 0x6dc, span 10 bytes)

```text
精英戰士
[END]
```

### Page 79  (offset 0x6e6, span 10 bytes)

```text
鎧甲武士
[END]
```

### Page 80  (offset 0x6f0, span 6 bytes)

```text
傭兵
[END]
```

### Page 81  (offset 0x6f6, span 10 bytes)

```text
黑暗戰士
[END]
```

### Page 82  (offset 0x700, span 8 bytes)

```text
狂戰士
[END]
```

### Page 83  (offset 0x708, span 6 bytes)

```text
騎兵
[END]
```

### Page 84  (offset 0x70e, span 10 bytes)

```text
突擊騎兵
[END]
```

### Page 85  (offset 0x718, span 10 bytes)

```text
地獄騎士
[END]
```

### Page 86  (offset 0x722, span 10 bytes)

```text
黑暗騎士
[END]
```

### Page 87  (offset 0x72c, span 8 bytes)

```text
龍騎士
[END]
```

### Page 88  (offset 0x734, span 8 bytes)

```text
弓箭手
[END]
```

### Page 89  (offset 0x73c, span 10 bytes)

```text
黑暗射手
[END]
```

### Page 90  (offset 0x746, span 8 bytes)

```text
狙擊手
[END]
```

### Page 91  (offset 0x74e, span 8 bytes)

```text
魔法師
[END]
```

### Page 92  (offset 0x756, span 10 bytes)

```text
黑暗法師
[END]
```

### Page 93  (offset 0x760, span 6 bytes)

```text
巫師
[END]
```

### Page 94  (offset 0x766, span 6 bytes)

```text
僧侶
[END]
```

### Page 95  (offset 0x76c, span 10 bytes)

```text
黑暗僧侶
[END]
```

### Page 96  (offset 0x776, span 8 bytes)

```text
大祭師
[END]
```

### Page 97  (offset 0x77e, span 6 bytes)

```text
盜賊
[END]
```

### Page 98  (offset 0x784, span 10 bytes)

```text
盜賊頭目
[END]
```

### Page 99  (offset 0x78e, span 10 bytes)

```text
影之忍者
[END]
```

### Page 100  (offset 0x798, span 10 bytes)

```text
黑暗殺手
[END]
```

### Page 101  (offset 0x7a2, span 8 bytes)

```text
武術家
[END]
```

### Page 102  (offset 0x7aa, span 10 bytes)

```text
黑暗鬥士
[END]
```

### Page 103  (offset 0x7b4, span 6 bytes)

```text
獸人
[END]
```

### Page 104  (offset 0x7ba, span 10 bytes)

```text
獸人隊長
[END]
```

### Page 105  (offset 0x7c4, span 6 bytes)

```text
火龍
[END]
```

### Page 106  (offset 0x7ca, span 6 bytes)

```text
雷龍
[END]
```

### Page 107  (offset 0x7d0, span 10 bytes)

```text
龍人戰士
[END]
```

### Page 108  (offset 0x7da, span 10 bytes)

```text
龍人法師
[END]
```

### Page 109  (offset 0x7e4, span 6 bytes)

```text
魔鬼
[END]
```

### Page 110  (offset 0x7ea, span 6 bytes)

```text
惡魔
[END]
```

### Page 111  (offset 0x7f0, span 8 bytes)

```text
大惡魔
[END]
```

### Page 112  (offset 0x7f8, span 8 bytes)

```text
機甲兵
[END]
```

### Page 113  (offset 0x800, span 12 bytes)

```text
機甲突擊兵
[END]
```

### Page 114  (offset 0x80c, span 10 bytes)

```text
光束砲座
[END]
```

### Page 115  (offset 0x816, span 10 bytes)

```text
機甲射手
[END]
```

### Page 116  (offset 0x820, span 10 bytes)

```text
機甲守衛
[END]
```

### Page 117  (offset 0x82a, span 10 bytes)

```text
機甲隊長
[END]
```

### Page 118  (offset 0x834, span 8 bytes)

```text
卡特那
[END]
```

### Page 119  (offset 0x83c, span 6 bytes)

```text
萊汀
[END]
```

### Page 120  (offset 0x842, span 6 bytes)

```text
薩卡
[END]
```

### Page 121  (offset 0x848, span 6 bytes)

```text
瑪爾
[END]
```

### Page 122  (offset 0x84e, span 10 bytes)

```text
沼澤怪物
[END]
```

### Page 123  (offset 0x858, span 8 bytes)

```text
地魔神
[END]
```

### Page 124  (offset 0x860, span 8 bytes)

```text
水魔神
[END]
```

### Page 125  (offset 0x868, span 8 bytes)

```text
風魔神
[END]
```

### Page 126  (offset 0x870, span 8 bytes)

```text
火魔神
[END]
```

### Page 127  (offset 0x878, span 8 bytes)

```text
空魔神
[END]
```

### Page 128  (offset 0x880, span 8 bytes)

```text
暗黑龍
[END]
```

### Page 129  (offset 0x888, span 2 bytes)

```text
[END]
```

### Page 130  (offset 0x88a, span 2 bytes)

```text
[END]
```

### Page 131  (offset 0x88c, span 2 bytes)

```text
[END]
```

### Page 132  (offset 0x88e, span 2 bytes)

```text
[END]
```

### Page 133  (offset 0x890, span 2 bytes)

```text
[END]
```

### Page 134  (offset 0x892, span 8 bytes)

```text
女村民
[END]
```

### Page 135  (offset 0x89a, span 8 bytes)

```text
男村民
[END]
```

### Page 136  (offset 0x8a2, span 12 bytes)

```text
卡納恩三世
[END]
```

### Page 137  (offset 0x8ae, span 2 bytes)

```text
[END]
```

### Page 138  (offset 0x8b0, span 2 bytes)

```text
[END]
```

### Page 139  (offset 0x8b2, span 2 bytes)

```text
[END]
```

### Page 140  (offset 0x8b4, span 2 bytes)

```text
[END]
```

### Page 141  (offset 0x8b6, span 6 bytes)

```text
人類
[END]
```

### Page 142  (offset 0x8bc, span 6 bytes)

```text
精靈
[END]
```

### Page 143  (offset 0x8c2, span 6 bytes)

```text
豹人
[END]
```

### Page 144  (offset 0x8c8, span 6 bytes)

```text
龍人
[END]
```

### Page 145  (offset 0x8ce, span 6 bytes)

```text
魔族
[END]
```

### Page 146  (offset 0x8d4, span 6 bytes)

```text
機械
[END]
```

### Page 147  (offset 0x8da, span 6 bytes)

```text
元素
[END]
```

### Page 148  (offset 0x8e0, span 6 bytes)

```text
獸人
[END]
```

### Page 149  (offset 0x8e6, span 6 bytes)

```text
其他
[END]
```

### Page 150  (offset 0x8ec, span 4 bytes)

```text
龍
[END]
```

### Page 151  (offset 0x8f0, span 6 bytes)

```text
劍士
[END]
```

### Page 152  (offset 0x8f6, span 6 bytes)

```text
戰士
[END]
```

### Page 153  (offset 0x8fc, span 6 bytes)

```text
騎士
[END]
```

### Page 154  (offset 0x902, span 6 bytes)

```text
弓兵
[END]
```

### Page 155  (offset 0x908, span 6 bytes)

```text
法師
[END]
```

### Page 156  (offset 0x90e, span 6 bytes)

```text
僧侶
[END]
```

### Page 157  (offset 0x914, span 6 bytes)

```text
盜賊
[END]
```

### Page 158  (offset 0x91a, span 6 bytes)

```text
武者
[END]
```

### Page 159  (offset 0x920, span 6 bytes)

```text
劍聖
[END]
```

### Page 160  (offset 0x926, span 8 bytes)

```text
聖戰士
[END]
```

### Page 161  (offset 0x92e, span 8 bytes)

```text
聖騎士
[END]
```

### Page 162  (offset 0x936, span 8 bytes)

```text
狙擊手
[END]
```

### Page 163  (offset 0x93e, span 8 bytes)

```text
大法師
[END]
```

### Page 164  (offset 0x946, span 6 bytes)

```text
祭師
[END]
```

### Page 165  (offset 0x94c, span 8 bytes)

```text
龍劍士
[END]
```

### Page 166  (offset 0x954, span 6 bytes)

```text
鬥士
[END]
```

### Page 167  (offset 0x95a, span 6 bytes)

```text
英雄
[END]
```

### Page 168  (offset 0x960, span 8 bytes)

```text
魔戰士
[END]
```

### Page 169  (offset 0x968, span 8 bytes)

```text
龍騎士
[END]
```

### Page 170  (offset 0x970, span 8 bytes)

```text
神射手
[END]
```

### Page 171  (offset 0x978, span 8 bytes)

```text
召喚師
[END]
```

### Page 172  (offset 0x980, span 6 bytes)

```text
聖者
[END]
```

### Page 173  (offset 0x986, span 6 bytes)

```text
忍者
[END]
```

### Page 174  (offset 0x98c, span 6 bytes)

```text
武聖
[END]
```

### Page 175  (offset 0x992, span 6 bytes)

```text
機兵
[END]
```

### Page 176  (offset 0x998, span 8 bytes)

```text
？？？
[END]
```

### Page 177  (offset 0x9a0, span 6 bytes)

```text
　　
[END]
```

### Page 178  (offset 0x9a6, span 8 bytes)

```text
？？？
[END]
```

### Page 179  (offset 0x9ae, span 2 bytes)

```text
[END]
```

### Page 180  (offset 0x9b0, span 2 bytes)

```text
[END]
```

### Page 181  (offset 0x9b2, span 6 bytes)

```text
短劍
[END]
```

### Page 182  (offset 0x9b8, span 6 bytes)

```text
闊劍
[END]
```

### Page 183  (offset 0x9be, span 6 bytes)

```text
長劍
[END]
```

### Page 184  (offset 0x9c4, span 6 bytes)

```text
巨劍
[END]
```

### Page 185  (offset 0x9ca, span 8 bytes)

```text
黑暗劍
[END]
```

### Page 186  (offset 0x9d2, span 8 bytes)

```text
戰士劍
[END]
```

### Page 187  (offset 0x9da, span 8 bytes)

```text
巨魔劍
[END]
```

### Page 188  (offset 0x9e2, span 8 bytes)

```text
斬鐵劍
[END]
```

### Page 189  (offset 0x9ea, span 8 bytes)

```text
流水劍
[END]
```

### Page 190  (offset 0x9f2, span 10 bytes)

```text
大地之劍
[END]
```

### Page 191  (offset 0x9fc, span 8 bytes)

```text
龍神劍
[END]
```

### Page 192  (offset 0xa04, span 8 bytes)

```text
炎龍劍
[END]
```

### Page 193  (offset 0xa0c, span 6 bytes)

```text
小刀
[END]
```

### Page 194  (offset 0xa12, span 6 bytes)

```text
匕首
[END]
```

### Page 195  (offset 0xa18, span 8 bytes)

```text
淬毒刀
[END]
```

### Page 196  (offset 0xa20, span 10 bytes)

```text
盜賊匕首
[END]
```

### Page 197  (offset 0xa2a, span 8 bytes)

```text
暗殺刀
[END]
```

### Page 198  (offset 0xa32, span 8 bytes)

```text
護手刀
[END]
```

### Page 199  (offset 0xa3a, span 8 bytes)

```text
忍者刀
[END]
```

### Page 200  (offset 0xa42, span 8 bytes)

```text
地獄刀
[END]
```

### Page 201  (offset 0xa4a, span 6 bytes)

```text
刺矛
[END]
```

### Page 202  (offset 0xa50, span 6 bytes)

```text
騎槍
[END]
```

### Page 203  (offset 0xa56, span 6 bytes)

```text
長戟
[END]
```

### Page 204  (offset 0xa5c, span 8 bytes)

```text
先鋒矛
[END]
```

### Page 205  (offset 0xa64, span 8 bytes)

```text
斬馬刀
[END]
```

### Page 206  (offset 0xa6c, span 10 bytes)

```text
惡魔之矛
[END]
```

### Page 207  (offset 0xa76, span 10 bytes)

```text
黃金之戟
[END]
```

### Page 208  (offset 0xa80, span 8 bytes)

```text
龍之槍
[END]
```

### Page 209  (offset 0xa88, span 8 bytes)

```text
破魔槍
[END]
```

### Page 210  (offset 0xa90, span 8 bytes)

```text
戰神戟
[END]
```

### Page 211  (offset 0xa98, span 8 bytes)

```text
聖之槍
[END]
```

### Page 212  (offset 0xaa0, span 8 bytes)

```text
巨神戟
[END]
```

### Page 213  (offset 0xaa8, span 6 bytes)

```text
手斧
[END]
```

### Page 214  (offset 0xaae, span 8 bytes)

```text
迴旋斧
[END]
```

### Page 215  (offset 0xab6, span 6 bytes)

```text
戰斧
[END]
```

### Page 216  (offset 0xabc, span 6 bytes)

```text
戰鎚
[END]
```

### Page 217  (offset 0xac2, span 6 bytes)

```text
巨斧
[END]
```

### Page 218  (offset 0xac8, span 8 bytes)

```text
血之斧
[END]
```

### Page 219  (offset 0xad0, span 8 bytes)

```text
閃電鎚
[END]
```

### Page 220  (offset 0xad8, span 10 bytes)

```text
狂暴之斧
[END]
```

### Page 221  (offset 0xae2, span 8 bytes)

```text
光之斧
[END]
```

### Page 222  (offset 0xaea, span 10 bytes)

```text
力量之斧
[END]
```

### Page 223  (offset 0xaf4, span 10 bytes)

```text
大地之鎚
[END]
```

### Page 224  (offset 0xafe, span 8 bytes)

```text
魔神斧
[END]
```

### Page 225  (offset 0xb06, span 6 bytes)

```text
短弓
[END]
```

### Page 226  (offset 0xb0c, span 6 bytes)

```text
長弓
[END]
```

### Page 227  (offset 0xb12, span 8 bytes)

```text
黑暗弓
[END]
```

### Page 228  (offset 0xb1a, span 6 bytes)

```text
巨弓
[END]
```

### Page 229  (offset 0xb20, span 8 bytes)

```text
精靈弓
[END]
```

### Page 230  (offset 0xb28, span 8 bytes)

```text
狙擊弓
[END]
```

### Page 231  (offset 0xb30, span 8 bytes)

```text
封魔弓
[END]
```

### Page 232  (offset 0xb38, span 8 bytes)

```text
風神弓
[END]
```

### Page 233  (offset 0xb40, span 6 bytes)

```text
長棍
[END]
```

### Page 234  (offset 0xb46, span 8 bytes)

```text
釘頭鎚
[END]
```

### Page 235  (offset 0xb4e, span 6 bytes)

```text
巨鎚
[END]
```

### Page 236  (offset 0xb54, span 6 bytes)

```text
槤枷
[END]
```

### Page 237  (offset 0xb5a, span 8 bytes)

```text
魔法杖
[END]
```

### Page 238  (offset 0xb62, span 8 bytes)

```text
封咒杖
[END]
```

### Page 239  (offset 0xb6a, span 8 bytes)

```text
力之杖
[END]
```

### Page 240  (offset 0xb72, span 8 bytes)

```text
黑暗杖
[END]
```

### Page 241  (offset 0xb7a, span 8 bytes)

```text
龍之杖
[END]
```

### Page 242  (offset 0xb82, span 8 bytes)

```text
光之杖
[END]
```

### Page 243  (offset 0xb8a, span 8 bytes)

```text
鐵指套
[END]
```

### Page 244  (offset 0xb92, span 6 bytes)

```text
鐵爪
[END]
```

### Page 245  (offset 0xb98, span 10 bytes)

```text
皇帝指環
[END]
```

### Page 246  (offset 0xba2, span 6 bytes)

```text
毒爪
[END]
```

### Page 247  (offset 0xba8, span 8 bytes)

```text
殭屍爪
[END]
```

### Page 248  (offset 0xbb0, span 8 bytes)

```text
碎岩爪
[END]
```

### Page 249  (offset 0xbb8, span 10 bytes)

```text
金鋼指環
[END]
```

### Page 250  (offset 0xbc2, span 8 bytes)

```text
裂刃爪
[END]
```

### Page 251  (offset 0xbca, span 10 bytes)

```text
巨神指環
[END]
```

### Page 252  (offset 0xbd4, span 8 bytes)

```text
魔龍爪
[END]
```

### Page 253  (offset 0xbdc, span 10 bytes)

```text
威力手臂
[END]
```

### Page 254  (offset 0xbe6, span 10 bytes)

```text
金鋼手臂
[END]
```

### Page 255  (offset 0xbf0, span 10 bytes)

```text
雷神手臂
[END]
```

### Page 256  (offset 0xbfa, span 10 bytes)

```text
衝擊手臂
[END]
```

### Page 257  (offset 0xc04, span 8 bytes)

```text
光束劍
[END]
```

### Page 258  (offset 0xc0c, span 8 bytes)

```text
狙擊槍
[END]
```

### Page 259  (offset 0xc14, span 8 bytes)

```text
光束槍
[END]
```

### Page 260  (offset 0xc1c, span 8 bytes)

```text
光束炮
[END]
```

### Page 261  (offset 0xc24, span 6 bytes)

```text
拳頭
[END]
```

### Page 262  (offset 0xc2a, span 6 bytes)

```text
利爪
[END]
```

### Page 263  (offset 0xc30, span 6 bytes)

```text
觸手
[END]
```

### Page 264  (offset 0xc36, span 10 bytes)

```text
巨岩手臂
[END]
```

### Page 265  (offset 0xc40, span 8 bytes)

```text
激水炮
[END]
```

### Page 266  (offset 0xc48, span 8 bytes)

```text
風牙斬
[END]
```

### Page 267  (offset 0xc50, span 8 bytes)

```text
魔燄波
[END]
```

### Page 268  (offset 0xc58, span 8 bytes)

```text
空雷震
[END]
```

### Page 269  (offset 0xc60, span 10 bytes)

```text
聖者之戒
[END]
```

### Page 270  (offset 0xc6a, span 10 bytes)

```text
勇者徽章
[END]
```

### Page 271  (offset 0xc74, span 10 bytes)

```text
精靈契印
[END]
```

### Page 272  (offset 0xc7e, span 10 bytes)

```text
領悟之書
[END]
```

### Page 273  (offset 0xc88, span 10 bytes)

```text
心眼之書
[END]
```

### Page 274  (offset 0xc92, span 10 bytes)

```text
白金徽章
[END]
```

### Page 275  (offset 0xc9c, span 10 bytes)

```text
生命之實
[END]
```

### Page 276  (offset 0xca6, span 10 bytes)

```text
魔力水晶
[END]
```

### Page 277  (offset 0xcb0, span 10 bytes)

```text
風精之羽
[END]
```

### Page 278  (offset 0xcba, span 8 bytes)

```text
十字弓
[END]
```

### Page 279  (offset 0xcc2, span 8 bytes)

```text
水晶弓
[END]
```

### Page 280  (offset 0xcca, span 8 bytes)

```text
雷神鞭
[END]
```

### Page 281  (offset 0xcd2, span 10 bytes)

```text
天空之鑰
[END]
```

### Page 282  (offset 0xcdc, span 10 bytes)

```text
傳送法杖
[END]
```

### Page 283  (offset 0xce6, span 10 bytes)

```text
修理套件
[END]
```

### Page 284  (offset 0xcf0, span 6 bytes)

```text
火燄
[END]
```

### Page 285  (offset 0xcf6, span 8 bytes)

```text
火神弓
[END]
```

### Page 286  (offset 0xcfe, span 10 bytes)

```text
鬥神指環
[END]
```

### Page 287  (offset 0xd08, span 6 bytes)

```text
冰燄
[END]
```

### Page 288  (offset 0xd0e, span 8 bytes)

```text
黑暗波
[END]
```

### Page 289  (offset 0xd16, span 2 bytes)

```text
[END]
```

### Page 290  (offset 0xd18, span 2 bytes)

```text
[END]
```

### Page 291  (offset 0xd1a, span 2 bytes)

```text
[END]
```

### Page 292  (offset 0xd1c, span 2 bytes)

```text
[END]
```

### Page 293  (offset 0xd1e, span 2 bytes)

```text
[END]
```

### Page 294  (offset 0xd20, span 2 bytes)

```text
[END]
```

### Page 295  (offset 0xd22, span 2 bytes)

```text
[END]
```

### Page 296  (offset 0xd24, span 2 bytes)

```text
[END]
```

### Page 297  (offset 0xd26, span 2 bytes)

```text
[END]
```

### Page 298  (offset 0xd28, span 2 bytes)

```text
[END]
```

### Page 299  (offset 0xd2a, span 2 bytes)

```text
[END]
```

### Page 300  (offset 0xd2c, span 2 bytes)

```text
[END]
```

### Page 301  (offset 0xd2e, span 2 bytes)

```text
[END]
```

### Page 302  (offset 0xd30, span 2 bytes)

```text
[END]
```

### Page 303  (offset 0xd32, span 2 bytes)

```text
[END]
```

### Page 304  (offset 0xd34, span 8 bytes)

```text
巨岩鎧
[END]
```

### Page 305  (offset 0xd3c, span 8 bytes)

```text
魔神甲
[END]
```

### Page 306  (offset 0xd44, span 4 bytes)

```text
11
[END]
```

### Page 307  (offset 0xd48, span 4 bytes)

```text
22
[END]
```

### Page 308  (offset 0xd4c, span 8 bytes)

```text
魔岩鎧
[END]
```

### Page 309  (offset 0xd54, span 6 bytes)

```text
布衣
[END]
```

### Page 310  (offset 0xd5a, span 8 bytes)

```text
旅行裝
[END]
```

### Page 311  (offset 0xd62, span 10 bytes)

```text
精靈披風
[END]
```

### Page 312  (offset 0xd6c, span 10 bytes)

```text
元素護體
[END]
```

### Page 313  (offset 0xd76, span 6 bytes)

```text
皮甲
[END]
```

### Page 314  (offset 0xd7c, span 8 bytes)

```text
硬皮甲
[END]
```

### Page 315  (offset 0xd84, span 8 bytes)

```text
夜行裝
[END]
```

### Page 316  (offset 0xd8c, span 10 bytes)

```text
魔法皮甲
[END]
```

### Page 317  (offset 0xd96, span 10 bytes)

```text
盜賊之衣
[END]
```

### Page 318  (offset 0xda0, span 8 bytes)

```text
忍者裝
[END]
```

### Page 319  (offset 0xda8, span 8 bytes)

```text
潛行服
[END]
```

### Page 320  (offset 0xdb0, span 8 bytes)

```text
神祕裝
[END]
```

### Page 321  (offset 0xdb8, span 8 bytes)

```text
暗殺服
[END]
```

### Page 322  (offset 0xdc0, span 10 bytes)

```text
黑暗之衣
[END]
```

### Page 323  (offset 0xdca, span 10 bytes)

```text
惡魔皮甲
[END]
```

### Page 324  (offset 0xdd4, span 8 bytes)

```text
雷神服
[END]
```

### Page 325  (offset 0xddc, span 8 bytes)

```text
環狀甲
[END]
```

### Page 326  (offset 0xde4, span 8 bytes)

```text
鎖子甲
[END]
```

### Page 327  (offset 0xdec, span 6 bytes)

```text
鱗甲
[END]
```

### Page 328  (offset 0xdf2, span 10 bytes)

```text
連環鋼甲
[END]
```

### Page 329  (offset 0xdfc, span 10 bytes)

```text
合金鎖甲
[END]
```

### Page 330  (offset 0xe06, span 10 bytes)

```text
魔法鱗甲
[END]
```

### Page 331  (offset 0xe10, span 8 bytes)

```text
血環甲
[END]
```

### Page 332  (offset 0xe18, span 10 bytes)

```text
金鋼鎖甲
[END]
```

### Page 333  (offset 0xe22, span 10 bytes)

```text
黑暗鱗甲
[END]
```

### Page 334  (offset 0xe2c, span 10 bytes)

```text
力量鎖甲
[END]
```

### Page 335  (offset 0xe36, span 10 bytes)

```text
惡魔鱗甲
[END]
```

### Page 336  (offset 0xe40, span 8 bytes)

```text
龍鱗甲
[END]
```

### Page 337  (offset 0xe48, span 6 bytes)

```text
鎧甲
[END]
```

### Page 338  (offset 0xe4e, span 8 bytes)

```text
銀鎧甲
[END]
```

### Page 339  (offset 0xe56, span 10 bytes)

```text
魔法鎧甲
[END]
```

### Page 340  (offset 0xe60, span 8 bytes)

```text
重鎧甲
[END]
```

### Page 341  (offset 0xe68, span 10 bytes)

```text
地獄鎧甲
[END]
```

### Page 342  (offset 0xe72, span 8 bytes)

```text
聖鎧甲
[END]
```

### Page 343  (offset 0xe7a, span 10 bytes)

```text
大地鎧甲
[END]
```

### Page 344  (offset 0xe84, span 10 bytes)

```text
龍神鎧甲
[END]
```

### Page 345  (offset 0xe8e, span 6 bytes)

```text
長袍
[END]
```

### Page 346  (offset 0xe94, span 8 bytes)

```text
法師袍
[END]
```

### Page 347  (offset 0xe9c, span 8 bytes)

```text
僧侶袍
[END]
```

### Page 348  (offset 0xea4, span 8 bytes)

```text
祭司袍
[END]
```

### Page 349  (offset 0xeac, span 8 bytes)

```text
聖者袍
[END]
```

### Page 350  (offset 0xeb4, span 8 bytes)

```text
霧之袍
[END]
```

### Page 351  (offset 0xebc, span 10 bytes)

```text
黑暗之袍
[END]
```

### Page 352  (offset 0xec6, span 8 bytes)

```text
天之袍
[END]
```

### Page 353  (offset 0xece, span 8 bytes)

```text
武道服
[END]
```

### Page 354  (offset 0xed6, span 8 bytes)

```text
武鬥裝
[END]
```

### Page 355  (offset 0xede, span 8 bytes)

```text
鬥士服
[END]
```

### Page 356  (offset 0xee6, span 10 bytes)

```text
武者鎧甲
[END]
```

### Page 357  (offset 0xef0, span 10 bytes)

```text
妖魔鬥服
[END]
```

### Page 358  (offset 0xefa, span 10 bytes)

```text
武神護甲
[END]
```

### Page 359  (offset 0xf04, span 10 bytes)

```text
戰鬥裝甲
[END]
```

### Page 360  (offset 0xf0e, span 10 bytes)

```text
突擊裝甲
[END]
```

### Page 361  (offset 0xf18, span 8 bytes)

```text
重裝甲
[END]
```

### Page 362  (offset 0xf20, span 10 bytes)

```text
特殊裝甲
[END]
```

### Page 363  (offset 0xf2a, span 6 bytes)

```text
硬皮
[END]
```

### Page 364  (offset 0xf30, span 10 bytes)

```text
青色硬皮
[END]
```

### Page 365  (offset 0xf3a, span 6 bytes)

```text
龍鱗
[END]
```

### Page 366  (offset 0xf40, span 6 bytes)

```text
勒皮
[END]
```

### Page 367  (offset 0xf46, span 10 bytes)

```text
虛無護甲
[END]
```

### Page 368  (offset 0xf50, span 6 bytes)

```text
王袍
[END]
```

### Page 369  (offset 0xf56, span 10 bytes)

```text
鬥士護甲
[END]
```

### Page 370  (offset 0xf60, span 8 bytes)

```text
地之袍
[END]
```

### Page 371  (offset 0xf68, span 8 bytes)

```text
水之袍
[END]
```

### Page 372  (offset 0xf70, span 10 bytes)

```text
大地裝甲
[END]
```

### Page 373  (offset 0xf7a, span 6 bytes)

```text
藥草
[END]
```

### Page 374  (offset 0xf80, span 8 bytes)

```text
回復劑
[END]
```

### Page 375  (offset 0xf88, span 8 bytes)

```text
再生藥
[END]
```

### Page 376  (offset 0xf90, span 10 bytes)

```text
神聖之水
[END]
```

### Page 377  (offset 0xf9a, span 8 bytes)

```text
解毒劑
[END]
```

### Page 378  (offset 0xfa2, span 8 bytes)

```text
退麻劑
[END]
```

### Page 379  (offset 0xfaa, span 10 bytes)

```text
力量藥水
[END]
```

### Page 380  (offset 0xfb4, span 10 bytes)

```text
耐力藥水
[END]
```

### Page 381  (offset 0xfbe, span 10 bytes)

```text
速度藥水
[END]
```

### Page 382  (offset 0xfc8, span 8 bytes)

```text
綠寶石
[END]
```

### Page 383  (offset 0xfd0, span 8 bytes)

```text
紅寶石
[END]
```

### Page 384  (offset 0xfd8, span 8 bytes)

```text
藍寶石
[END]
```

### Page 385  (offset 0xfe0, span 6 bytes)

```text
鑽石
[END]
```

### Page 386  (offset 0xfe6, span 8 bytes)

```text
飛龍卵
[END]
```

### Page 387  (offset 0xfee, span 8 bytes)

```text
魔法水
[END]
```

### Page 388  (offset 0xff6, span 8 bytes)

```text
水晶粒
[END]
```

### Page 389  (offset 0xffe, span 10 bytes)

```text
控制中樞
[END]
```

### Page 390  (offset 0x1008, span 10 bytes)

```text
黃金徽章
[END]
```

### Page 391  (offset 0x1012, span 8 bytes)

```text
星之眼
[END]
```

### Page 392  (offset 0x101a, span 8 bytes)

```text
光之眼
[END]
```

### Page 393  (offset 0x1022, span 8 bytes)

```text
暗之眼
[END]
```

### Page 394  (offset 0x102a, span 8 bytes)

```text
冰之眼
[END]
```

### Page 395  (offset 0x1032, span 8 bytes)

```text
火之眼
[END]
```

### Page 396  (offset 0x103a, span 2 bytes)

```text
[END]
```

### Page 397  (offset 0x103c, span 2 bytes)

```text
[END]
```

### Page 398  (offset 0x103e, span 2 bytes)

```text
[END]
```

### Page 399  (offset 0x1040, span 2 bytes)

```text
[END]
```

### Page 400  (offset 0x1042, span 2 bytes)

```text
[END]
```

### Page 401  (offset 0x1044, span 2 bytes)

```text
[END]
```

### Page 402  (offset 0x1046, span 2 bytes)

```text
[END]
```

### Page 403  (offset 0x1048, span 2 bytes)

```text
[END]
```

### Page 404  (offset 0x104a, span 2 bytes)

```text
[END]
```

### Page 405  (offset 0x104c, span 2 bytes)

```text
[END]
```

### Page 406  (offset 0x104e, span 2 bytes)

```text
[END]
```

### Page 407  (offset 0x1050, span 2 bytes)

```text
[END]
```

### Page 408  (offset 0x1052, span 2 bytes)

```text
[END]
```

### Page 409  (offset 0x1054, span 2 bytes)

```text
[END]
```

### Page 410  (offset 0x1056, span 16 bytes)

```text
要記錄戰況嗎？
[END]
```

### Page 411  (offset 0x1066, span 18 bytes)

```text
戰況已被記錄了！
[END]
```

### Page 412  (offset 0x1078, span 22 bytes)

```text
是嗎？那麼就不要了·
[END]
```

### Page 413  (offset 0x108e, span 16 bytes)

```text
要讀取戰況嗎？
[END]
```

### Page 414  (offset 0x109e, span 14 bytes)

```text
那麼請稍等！
[END]
```

### Page 415  (offset 0x10ac, span 16 bytes)

```text
要離開戰場嗎？
[END]
```

### Page 416  (offset 0x10bc, span 16 bytes)

```text
那麼請休息吧！
[END]
```

### Page 417  (offset 0x10cc, span 16 bytes)

```text
決定要行軍嗎？
[END]
```

### Page 418  (offset 0x10dc, span 18 bytes)

```text
那麼全軍發進吧！
[END]
```

### Page 419  (offset 0x10ee, span 24 bytes)

```text
要結束本回合的行動嗎？
[END]
```

### Page 420  (offset 0x1106, span 32 bytes)

```text
好的，
[PAGE_BREAK]
就結束本回合的行動吧！
[END]
```

### Page 421  (offset 0x1126, span 22 bytes)

```text
發現寶箱，要打開嗎？
[END]
```

### Page 422  (offset 0x113c, span 20 bytes)

```text
打開寶箱，發現
[SUB_DIALOG_A]
！
[END]
```

### Page 423  (offset 0x1150, span 22 bytes)

```text
道具滿了，要交換嗎？
[END]
```

### Page 424  (offset 0x1166, span 24 bytes)

```text
那麼就把
[SUB_DIALOG_A]
[PAGE_BREAK]
放回去吧！
[END]
```

### Page 425  (offset 0x117e, span 26 bytes)

```text
拿起
[SUB_DIALOG_A]
，
[PAGE_BREAK]
把
[SUB_DIALOG_B]
放了回去！
[END]
```

### Page 426  (offset 0x1198, span 24 bytes)

```text
打開寶箱，
[PAGE_BREAK]
發現[NUMBER]元！
[END]
```

### Page 427  (offset 0x11b0, span 24 bytes)

```text
打開寶箱，可惜是空的！
[END]
```

### Page 428  (offset 0x11c8, span 22 bytes)

```text
發現寶藏，要挖掘嗎？
[END]
```

### Page 429  (offset 0x11de, span 20 bytes)

```text
挖掘寶藏，得到
[SUB_DIALOG_A]
！
[END]
```

### Page 430  (offset 0x11f2, span 24 bytes)

```text
挖掘寶藏，
[PAGE_BREAK]
發現[NUMBER]元！
[END]
```

### Page 431  (offset 0x120a, span 24 bytes)

```text
挖掘寶藏，可惜是空的！
[END]
```

### Page 432  (offset 0x1222, span 24 bytes)

```text
從敵人身上，
[PAGE_BREAK]
得到
[SUB_DIALOG_A]
！
[END]
```

### Page 433  (offset 0x123a, span 22 bytes)

```text
道具滿了，要丟棄嗎？
[END]
```

### Page 434  (offset 0x1250, span 18 bytes)

```text
那麼就放棄
[SUB_DIALOG_A]
吧！
[END]
```

### Page 435  (offset 0x1262, span 26 bytes)

```text
從敵人身上，
[PAGE_BREAK]
得到[NUMBER]元！
[END]
```

### Page 436  (offset 0x127c, span 32 bytes)

```text
戰況記錄被破壞了，
[PAGE_BREAK]
放棄讀取！
[END]
```

### Page 437  (offset 0x129c, span 42 bytes)

```text
現在的隊伍之中，
[PAGE_BREAK]
似乎沒有人可以裝備喔！
[END]
```

### Page 438  (offset 0x12c6, span 16 bytes)

```text
您的錢不夠喔！
[END]
```

### Page 439  (offset 0x12d6, span 18 bytes)

```text
[SUB_DIALOG_A]
要[NUMBER]元，好嗎？
[END]
```

### Page 440  (offset 0x12e8, span 24 bytes)

```text
歡迎光臨，需要什麼嗎？
[END]
```

### Page 441  (offset 0x1300, span 8 bytes)

```text
火炎術
[END]
```

### Page 442  (offset 0x1308, span 8 bytes)

```text
烈炎術
[END]
```

### Page 443  (offset 0x1310, span 8 bytes)

```text
炎龍術
[END]
```

### Page 444  (offset 0x1318, span 8 bytes)

```text
天火術
[END]
```

### Page 445  (offset 0x1320, span 8 bytes)

```text
電擊術
[END]
```

### Page 446  (offset 0x1328, span 8 bytes)

```text
落雷術
[END]
```

### Page 447  (offset 0x1330, span 8 bytes)

```text
轟雷術
[END]
```

### Page 448  (offset 0x1338, span 8 bytes)

```text
神雷術
[END]
```

### Page 449  (offset 0x1340, span 8 bytes)

```text
聖光彈
[END]
```

### Page 450  (offset 0x1348, span 8 bytes)

```text
咒殺術
[END]
```

### Page 451  (offset 0x1350, span 8 bytes)

```text
碎岩術
[END]
```

### Page 452  (offset 0x1358, span 8 bytes)

```text
地震術
[END]
```

### Page 453  (offset 0x1360, span 8 bytes)

```text
裂地術
[END]
```

### Page 454  (offset 0x1368, span 8 bytes)

```text
治療術
[END]
```

### Page 455  (offset 0x1370, span 8 bytes)

```text
回復術
[END]
```

### Page 456  (offset 0x1378, span 8 bytes)

```text
再生術
[END]
```

### Page 457  (offset 0x1380, span 8 bytes)

```text
神恩術
[END]
```

### Page 458  (offset 0x1388, span 8 bytes)

```text
魔刃術
[END]
```

### Page 459  (offset 0x1390, span 8 bytes)

```text
魔鎧術
[END]
```

### Page 460  (offset 0x1398, span 8 bytes)

```text
風行術
[END]
```

### Page 461  (offset 0x13a0, span 8 bytes)

```text
解毒術
[END]
```

### Page 462  (offset 0x13a8, span 8 bytes)

```text
袪麻術
[END]
```

### Page 463  (offset 0x13b0, span 8 bytes)

```text
封咒術
[END]
```

### Page 464  (offset 0x13b8, span 8 bytes)

```text
傳送術
[END]
```

### Page 465  (offset 0x13c0, span 8 bytes)

```text
破龍擊
[END]
```

### Page 466  (offset 0x13c8, span 8 bytes)

```text
行動術
[END]
```

### Page 467  (offset 0x13d0, span 8 bytes)

```text
毒擊術
[END]
```

### Page 468  (offset 0x13d8, span 8 bytes)

```text
麻庳術
[END]
```

### Page 469  (offset 0x13e0, span 8 bytes)

```text
淒煌斬
[END]
```

### Page 470  (offset 0x13e8, span 8 bytes)

```text
熾炎刀
[END]
```

### Page 471  (offset 0x13f0, span 8 bytes)

```text
音速刃
[END]
```

### Page 472  (offset 0x13f8, span 2 bytes)

```text
[END]
```

### Page 473  (offset 0x13fa, span 8 bytes)

```text
熾天使
[END]
```

### Page 474  (offset 0x1402, span 8 bytes)

```text
風妖精
[END]
```

### Page 475  (offset 0x140a, span 8 bytes)

```text
破壞神
[END]
```

### Page 476  (offset 0x1412, span 8 bytes)

```text
暗邪鬼
[END]
```

### Page 477  (offset 0x141a, span 2 bytes)

```text
[END]
```

### Page 478  (offset 0x141c, span 16 bytes)

```text
記錄讀取完畢！
[END]
```

### Page 479  (offset 0x142c, span 24 bytes)

```text
本章不能由商店內讀取！
[END]
```

### Page 480  (offset 0x1444, span 12 bytes)

```text
道具滿了！
[END]
```

### Page 481  (offset 0x1450, span 26 bytes)

```text
增加攻擊力的效果消失了！
[END]
```

### Page 482  (offset 0x146a, span 26 bytes)

```text
增加防禦力的效果消失了！
[END]
```

### Page 483  (offset 0x1484, span 24 bytes)

```text
增加速度的效果消失了！
[END]
```

### Page 484  (offset 0x149c, span 20 bytes)

```text
身上的毒性消退了！
[END]
```

### Page 485  (offset 0x14b0, span 20 bytes)

```text
身上的痲痺消退了！
[END]
```

### Page 486  (offset 0x14c4, span 20 bytes)

```text
封咒的效果消失了！
[END]
```

### Page 487  (offset 0x14d8, span 28 bytes)

```text
毒性發作，
[PAGE_BREAK]
HP減少[NUMBER]點！
[END]
```

### Page 488  (offset 0x14f4, span 16 bytes)

```text
得到經驗[NUMBER]點！
[END]
```

### Page 489  (offset 0x1504, span 18 bytes)

```text
等級上升了！！
[PARAGRAPH]
[END]
```

### Page 490  (offset 0x1516, span 18 bytes)

```text
力量上升[NUMBER]點！
[PARAGRAPH]
[END]
```

### Page 491  (offset 0x1528, span 18 bytes)

```text
耐力上升[NUMBER]點！
[PARAGRAPH]
[END]
```

### Page 492  (offset 0x153a, span 18 bytes)

```text
速度上升[NUMBER]點！
[PARAGRAPH]
[END]
```

### Page 493  (offset 0x154c, span 20 bytes)

```text
MHP上升[NUMBER]點！
[PARAGRAPH]
[END]
```

### Page 494  (offset 0x1560, span 20 bytes)

```text
MMP上升[NUMBER]點！
[PARAGRAPH]
[END]
```

### Page 495  (offset 0x1574, span 8 bytes)

```text
酒　店
[END]
```

### Page 496  (offset 0x157c, span 8 bytes)

```text
武器店
[END]
```

### Page 497  (offset 0x1584, span 8 bytes)

```text
出　口
[END]
```

### Page 498  (offset 0x158c, span 8 bytes)

```text
道具店
[END]
```

### Page 499  (offset 0x1594, span 8 bytes)

```text
教　會
[END]
```

### Page 500  (offset 0x159c, span 8 bytes)

```text
？？？
[END]
```

### Page 501  (offset 0x15a4, span 16 bytes)

```text
呃，買東西嗎？
[END]
```

### Page 502  (offset 0x15b4, span 28 bytes)

```text
這個
[SUB_DIALOG_A]
，
[PAGE_BREAK]
[NUMBER]元，要不要啊？
[END]
```

### Page 503  (offset 0x15d0, span 14 bytes)

```text
還要什麼嗎？
[END]
```

### Page 504  (offset 0x15de, span 10 bytes)

```text
錢不夠！
[END]
```

### Page 505  (offset 0x15e8, span 18 bytes)

```text
沒有人可以裝備！
[END]
```

### Page 506  (offset 0x15fa, span 14 bytes)

```text
[SUB_DIALOG_A]
帶不動了！
[END]
```

### Page 507  (offset 0x1608, span 16 bytes)

```text
要裝備上去嗎？
[END]
```

### Page 508  (offset 0x1618, span 28 bytes)

```text
這個
[SUB_DIALOG_A]
，
[PAGE_BREAK]
[NUMBER]元，賣不賣啊？
[END]
```

### Page 509  (offset 0x1634, span 16 bytes)

```text
[SUB_DIALOG_A]
沒東西可賣！
[END]
```

### Page 510  (offset 0x1644, span 12 bytes)

```text
要給誰呢？
[END]
```

### Page 511  (offset 0x1650, span 14 bytes)

```text
[SUB_DIALOG_A]
沒東西了！
[END]
```

### Page 512  (offset 0x165e, span 14 bytes)

```text
誰的東西呢？
[END]
```

### Page 513  (offset 0x166c, span 16 bytes)

```text
要進入戰場嗎？
[END]
```

### Page 514  (offset 0x167c, span 20 bytes)

```text
〈<無儲存記錄〉>
[END]
```

### Page 515  (offset 0x1690, span 12 bytes)

```text
第　二　章
[END]
```

### Page 516  (offset 0x169c, span 12 bytes)

```text
第　三　章
[END]
```

### Page 517  (offset 0x16a8, span 12 bytes)

```text
第　四　章
[END]
```

### Page 518  (offset 0x16b4, span 12 bytes)

```text
第　五　章
[END]
```

### Page 519  (offset 0x16c0, span 12 bytes)

```text
第　六　章
[END]
```

### Page 520  (offset 0x16cc, span 12 bytes)

```text
第　七　章
[END]
```

### Page 521  (offset 0x16d8, span 12 bytes)

```text
第　八　章
[END]
```

### Page 522  (offset 0x16e4, span 12 bytes)

```text
第　九　章
[END]
```

### Page 523  (offset 0x16f0, span 12 bytes)

```text
第　十　章
[END]
```

### Page 524  (offset 0x16fc, span 10 bytes)

```text
第十一章
[END]
```

### Page 525  (offset 0x1706, span 10 bytes)

```text
第十二章
[END]
```

### Page 526  (offset 0x1710, span 10 bytes)

```text
第十三章
[END]
```

### Page 527  (offset 0x171a, span 10 bytes)

```text
第十四章
[END]
```

### Page 528  (offset 0x1724, span 10 bytes)

```text
第十五章
[END]
```

### Page 529  (offset 0x172e, span 10 bytes)

```text
第十六章
[END]
```

### Page 530  (offset 0x1738, span 10 bytes)

```text
第十七章
[END]
```

### Page 531  (offset 0x1742, span 10 bytes)

```text
第十八章
[END]
```

### Page 532  (offset 0x174c, span 10 bytes)

```text
第十九章
[END]
```

### Page 533  (offset 0x1756, span 10 bytes)

```text
第二十章
[END]
```

### Page 534  (offset 0x1760, span 12 bytes)

```text
第二十一章
[END]
```

### Page 535  (offset 0x176c, span 12 bytes)

```text
第二十二章
[END]
```

### Page 536  (offset 0x1778, span 12 bytes)

```text
第二十三章
[END]
```

### Page 537  (offset 0x1784, span 12 bytes)

```text
第二十四章
[END]
```

### Page 538  (offset 0x1790, span 12 bytes)

```text
第二十五章
[END]
```

### Page 539  (offset 0x179c, span 12 bytes)

```text
第二十六章
[END]
```

### Page 540  (offset 0x17a8, span 12 bytes)

```text
第二十七章
[END]
```

### Page 541  (offset 0x17b4, span 12 bytes)

```text
第二十八章
[END]
```

### Page 542  (offset 0x17c0, span 12 bytes)

```text
第二十九章
[END]
```

### Page 543  (offset 0x17cc, span 10 bytes)

```text
第三十章
[END]
```

### Page 544  (offset 0x17d6, span 2 bytes)

```text
[END]
```

### Page 545  (offset 0x17d8, span 2 bytes)

```text
[END]
```

### Page 546  (offset 0x17da, span 2 bytes)

```text
[END]
```

### Page 547  (offset 0x17dc, span 2 bytes)

```text
[END]
```

### Page 548  (offset 0x17de, span 2 bytes)

```text
[END]
```

### Page 549  (offset 0x17e0, span 6 bytes)

```text
[NUMBER]）
[END]
```

### Page 550  (offset 0x17e6, span 6 bytes)

```text
孤島
[END]
```

### Page 551  (offset 0x17ec, span 8 bytes)

```text
羅德鎮
[END]
```

### Page 552  (offset 0x17f4, span 14 bytes)

```text
往塞拉村途中
[END]
```

### Page 553  (offset 0x1802, span 10 bytes)

```text
塞拉村前
[END]
```

### Page 554  (offset 0x180c, span 8 bytes)

```text
塞拉村
[END]
```

### Page 555  (offset 0x1814, span 10 bytes)

```text
普里茲港
[END]
```

### Page 556  (offset 0x181e, span 14 bytes)

```text
往王城的途中
[END]
```

### Page 557  (offset 0x182c, span 14 bytes)

```text
王城前的戰鬥
[END]
```

### Page 558  (offset 0x183a, span 12 bytes)

```text
騎士的抉擇
[END]
```

### Page 559  (offset 0x1846, span 14 bytes)

```text
洞窟中的激戰
[END]
```

### Page 560  (offset 0x1854, span 10 bytes)

```text
幻之森林
[END]
```

### Page 561  (offset 0x185e, span 8 bytes)

```text
北山道
[END]
```

### Page 562  (offset 0x1866, span 14 bytes)

```text
哈斯米爾之戰
[END]
```

### Page 563  (offset 0x1874, span 12 bytes)

```text
平原的會戰
[END]
```

### Page 564  (offset 0x1880, span 14 bytes)

```text
拉卡湖的激戰
[END]
```

### Page 565  (offset 0x188e, span 10 bytes)

```text
冰原之戰
[END]
```

### Page 566  (offset 0x1898, span 12 bytes)

```text
血與冰之刃
[END]
```

### Page 567  (offset 0x18a4, span 12 bytes)

```text
遙遠的彼岸
[END]
```

### Page 568  (offset 0x18b0, span 14 bytes)

```text
黑暗中的狙擊
[END]
```

### Page 569  (offset 0x18be, span 14 bytes)

```text
死亡般的沈寂
[END]
```

### Page 570  (offset 0x18cc, span 10 bytes)

```text
亞述森林
[END]
```

### Page 571  (offset 0x18d6, span 12 bytes)

```text
遠古的呼喚
[END]
```

### Page 572  (offset 0x18e2, span 12 bytes)

```text
向天空之旅
[END]
```

### Page 573  (offset 0x18ee, span 14 bytes)

```text
在天空的彼方
[END]
```

### Page 574  (offset 0x18fc, span 12 bytes)

```text
火焰的審判
[END]
```

### Page 575  (offset 0x1908, span 12 bytes)

```text
未知的迴廊
[END]
```

### Page 576  (offset 0x1914, span 14 bytes)

```text
命運的交會點
[END]
```

### Page 577  (offset 0x1922, span 8 bytes)

```text
探索者
[END]
```

### Page 578  (offset 0x192a, span 16 bytes)

```text
無邊的黑暗之中
[END]
```

### Page 579  (offset 0x193a, span 12 bytes)

```text
傳說的終章
[END]
```

### Page 580  (offset 0x1946, span 2 bytes)

```text
[END]
```

### Page 581  (offset 0x1948, span 2 bytes)

```text
[END]
```

### Page 582  (offset 0x194a, span 2 bytes)

```text
[END]
```

### Page 583  (offset 0x194c, span 2 bytes)

```text
[END]
```

### Page 584  (offset 0x194e, span 2 bytes)

```text
[END]
```

### Page 585  (offset 0x1950, span 14 bytes)

```text
有什麼事嗎？
[END]
```

### Page 586  (offset 0x195e, span 12 bytes)

```text
還有事嗎？
[END]
```

### Page 587  (offset 0x196a, span 12 bytes)

```text
學會了
[SUB_DIALOG_A]
！
[END]
```

### Page 588  (offset 0x1976, span 24 bytes)

```text
隊伍中沒有須要復活的！
[END]
```

### Page 589  (offset 0x198e, span 14 bytes)

```text
誰要復活呢？
[END]
```

### Page 590  (offset 0x199c, span 24 bytes)

```text
[SUB_DIALOG_A]
復活
[PAGE_BREAK]
要[NUMBER]元，好嗎？
[END]
```

### Page 591  (offset 0x19b4, span 24 bytes)

```text
隊伍中沒有可以轉職的！
[END]
```

### Page 592  (offset 0x19cc, span 14 bytes)

```text
誰要轉職呢？
[END]
```

### Page 593  (offset 0x19da, span 8 bytes)

```text
轉職成
[END]
```

### Page 594  (offset 0x19e2, span 14 bytes)

```text
[SUB_DIALOG_A]
要轉職嗎？
[END]
```

### Page 595  (offset 0x19f0, span 14 bytes)

```text
職業轉成
[SUB_DIALOG_A]
！
[END]
```

### Page 596  (offset 0x19fe, span 18 bytes)

```text
移動力增加[NUMBER]點！
[END]
```

### Page 597  (offset 0x1a10, span 8 bytes)

```text
敵全滅
[END]
```

### Page 598  (offset 0x1a18, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 599  (offset 0x1a22, span 8 bytes)

```text
敵全滅
[END]
```

### Page 600  (offset 0x1a2a, span 20 bytes)

```text
索爾死亡　市民全滅
[END]
```

### Page 601  (offset 0x1a3e, span 8 bytes)

```text
敵全滅
[END]
```

### Page 602  (offset 0x1a46, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 603  (offset 0x1a50, span 8 bytes)

```text
敵全滅
[END]
```

### Page 604  (offset 0x1a58, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 605  (offset 0x1a62, span 8 bytes)

```text
敵全滅
[END]
```

### Page 606  (offset 0x1a6a, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 607  (offset 0x1a74, span 8 bytes)

```text
敵全滅
[END]
```

### Page 608  (offset 0x1a7c, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 609  (offset 0x1a86, span 8 bytes)

```text
敵全滅
[END]
```

### Page 610  (offset 0x1a8e, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 611  (offset 0x1a98, span 8 bytes)

```text
敵全滅
[END]
```

### Page 612  (offset 0x1aa0, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 613  (offset 0x1aaa, span 8 bytes)

```text
敵全滅
[END]
```

### Page 614  (offset 0x1ab2, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 615  (offset 0x1abc, span 8 bytes)

```text
敵全滅
[END]
```

### Page 616  (offset 0x1ac4, span 38 bytes)

```text
索爾死亡　索菲亞死亡
[PAGE_BREAK]
卡納恩三世死亡
[END]
```

### Page 617  (offset 0x1aea, span 8 bytes)

```text
敵全滅
[END]
```

### Page 618  (offset 0x1af2, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 619  (offset 0x1afc, span 8 bytes)

```text
敵全滅
[END]
```

### Page 620  (offset 0x1b04, span 26 bytes)

```text
索爾死亡
[PAGE_BREAK]
米亞斯多德死亡
[END]
```

### Page 621  (offset 0x1b1e, span 8 bytes)

```text
敵全滅
[END]
```

### Page 622  (offset 0x1b26, span 22 bytes)

```text
索爾死亡　精靈族全滅
[END]
```

### Page 623  (offset 0x1b3c, span 8 bytes)

```text
敵全滅
[END]
```

### Page 624  (offset 0x1b44, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 625  (offset 0x1b4e, span 8 bytes)

```text
敵全滅
[END]
```

### Page 626  (offset 0x1b56, span 24 bytes)

```text
索爾死亡
[PAGE_BREAK]
塞可邦勒死亡
[END]
```

### Page 627  (offset 0x1b6e, span 8 bytes)

```text
敵全滅
[END]
```

### Page 628  (offset 0x1b76, span 20 bytes)

```text
索爾死亡
[PAGE_BREAK]
密蒂死亡
[END]
```

### Page 629  (offset 0x1b8a, span 8 bytes)

```text
敵全滅
[END]
```

### Page 630  (offset 0x1b92, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 631  (offset 0x1b9c, span 14 bytes)

```text
黑暗騎士死亡
[END]
```

### Page 632  (offset 0x1baa, span 34 bytes)

```text
索爾死亡　約拿死亡
[PAGE_BREAK]
蘭斯洛特死亡
[END]
```

### Page 633  (offset 0x1bcc, span 8 bytes)

```text
敵全滅
[END]
```

### Page 634  (offset 0x1bd4, span 26 bytes)

```text
索爾死亡
[PAGE_BREAK]
巴拿羅西亞死亡
[END]
```

### Page 635  (offset 0x1bee, span 26 bytes)

```text
沼澤怪物之外的
[PAGE_BREAK]
敵人全滅
[END]
```

### Page 636  (offset 0x1c08, span 30 bytes)

```text
索爾死亡
[PAGE_BREAK]
謝多死亡或精靈全滅
[END]
```

### Page 637  (offset 0x1c26, span 8 bytes)

```text
敵全滅
[END]
```

### Page 638  (offset 0x1c2e, span 32 bytes)

```text
索爾死亡
[PAGE_BREAK]
羅蘭死亡　希爾法死亡
[END]
```

### Page 639  (offset 0x1c4e, span 8 bytes)

```text
敵全滅
[END]
```

### Page 640  (offset 0x1c56, span 22 bytes)

```text
索爾死亡
[PAGE_BREAK]
希爾法死亡
[END]
```

### Page 641  (offset 0x1c6c, span 14 bytes)

```text
擊毀機甲隊長
[END]
```

### Page 642  (offset 0x1c7a, span 42 bytes)

```text
索爾死亡　希爾法死亡
[PAGE_BREAK]
卡里斯或羅德曼死亡
[END]
```

### Page 643  (offset 0x1ca4, span 8 bytes)

```text
敵全滅
[END]
```

### Page 644  (offset 0x1cac, span 10 bytes)

```text
索爾死亡
[END]
```

### Page 645  (offset 0x1cb6, span 8 bytes)

```text
敵全滅
[END]
```

### Page 646  (offset 0x1cbe, span 24 bytes)

```text
索爾死亡
[PAGE_BREAK]
聖寇拉斯死亡
[END]
```

### Page 647  (offset 0x1cd6, span 8 bytes)

```text
敵全滅
[END]
```

### Page 648  (offset 0x1cde, span 30 bytes)

```text
索爾死亡
[PAGE_BREAK]
悠妮或亞齊梅吉死亡
[END]
```

### Page 649  (offset 0x1cfc, span 14 bytes)

```text
擊毀機甲隊長
[END]
```

### Page 650  (offset 0x1d0a, span 20 bytes)

```text
索爾死亡
[PAGE_BREAK]
悠妮死亡
[END]
```

### Page 651  (offset 0x1d1e, span 14 bytes)

```text
擊毀機甲隊長
[END]
```

### Page 652  (offset 0x1d2c, span 20 bytes)

```text
索爾死亡
[PAGE_BREAK]
悠妮死亡
[END]
```

### Page 653  (offset 0x1d40, span 14 bytes)

```text
解除防衛系統
[END]
```

### Page 654  (offset 0x1d4e, span 20 bytes)

```text
索爾死亡
[PAGE_BREAK]
悠妮死亡
[END]
```

### Page 655  (offset 0x1d62, span 12 bytes)

```text
空魔神死亡
[END]
```

### Page 656  (offset 0x1d6e, span 20 bytes)

```text
索爾死亡
[PAGE_BREAK]
悠妮死亡
[END]
```

### Page 657  (offset 0x1d82, span 18 bytes)

```text
本章
[SUB_DIALOG_A]
必須出場！
[END]
```

### Page 658  (offset 0x1d94, span 20 bytes)

```text
確定要進入戰場嗎？
[END]
```

### Page 659  (offset 0x1da8, span 28 bytes)

```text
這個
[SUB_DIALOG_A]
，
[PAGE_BREAK]
值[NUMBER]元，要賣嗎？
[END]
```

### Page 660  (offset 0x1dc4, span 16 bytes)

```text
記錄儲存完畢！
[END]
```

