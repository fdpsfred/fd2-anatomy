# 法術習得等級資料（每組 12 bytes）

來源：`Strategy_Guide/code_hack2.htm` §6

```
6.法術習得等級資料：7B6C7h或564B3h開始，每個12 byte
  開頭為05 11 09 01

LV MG LV MG LV MG LV MG LV MG LV MG

LV = 法術習得等級（FFh表沒有）
MG = 法術編號（FFh表沒有）

順序無所謂，但不可比出場人物等級低。

    位址  人物            LV MG LV MG LV MG LV MG LV MG LV MG
----------------------------------------------------------------
00  7B6C7 悠妮            05 11 09 01 0F 1A 15 02 1A 1B FF FF
01  7B6D3 瑪琳            07 14 0B 0E 10 12 18 16 1E 17 FF FF
02  7B6DF 索菲亞          12 13 18 15 1E 19 FF FF FF FF FF FF
03  7B6EB 珊              14 0A 18 06 FF FF FF FF FF FF FF FF
04  7B6F7 索爾    劍聖    04 18 FF FF FF FF FF FF FF FF FF FF
05  7B703 蜜蒂            02 1E FF FF FF FF FF FF FF FF FF FF
06  7B70F 鐵諾    劍聖    04 1D FF FF FF FF FF FF FF FF FF FF
07  7B71B 悠妮    大法師  03 1B 07 03 0D 13 16 0B FF FF FF FF
08  7B727 珊      大法師  04 02 08 07 0C 0B FF FF FF FF FF FF (亞奇梅吉使用同一編號)
09  7B733 瑪琳    祭師    03 0F 09 15 12 13 17 10 FF FF FF FF
0A  7B73F 索爾    英雄    04 0F 08 03 0E 07 14 08 18 0C 1E 17
0B  7B74B 瑪琳    聖者    03 0F 09 08 10 13 15 15 1A 10 FF FF
0C  7B757 索菲亞  聖者    03 0F 08 08 0F 1B 14 15 19 10 FF FF
0D  7B763 珊      聖者    04 0F 0A 08 0F 16 14 14 18 07 FF FF
0E  7B76F 悠妮    聖者    03 0F 09 08 10 15 15 12 19 03 FF FF
0F  7B77B 哈諾    魔戰士  04 12 0B 0F 12 14 FF FF FF FF FF FF
10  7B787 哈瓦特  魔戰士  03 13 09 0F 10 14 FF FF FF FF FF FF
11  7B793 達可賽          0F 0B 14 12 17 0B FF FF FF FF FF FF
12  7B79F 索菲亞  祭師    04 0F 0A 16 12 10 18 12 FF FF FF FF
13  7B7AB 悠妮    召喚師  02 22 06 23 0B 17 0F 03 13 21 17 20
```
