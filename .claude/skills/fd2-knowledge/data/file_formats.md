# 資料檔結構（FDFIELD.DAT / FDSHAP.DAT）

來源：`Strategy_Guide/code_hack2.htm`

```
修改：FD2.EXE（不同版本的資訊位置可能不同，可利用開頭數值尋找）

修改：FDFIELD.DAT地圖資料

1.檔頭=連續6個4Ch

修改：FDSHAP.DAT

地形控制資料：2422Eh開始，每個地形4 byte，按地形編號排列
  寶箱資訊：1 byte
    20h = 1表寶箱
    40h = 1表隱藏物品
  移動資訊：1 byte
    0 = 正常狀態（AP+05,DP+00）
    1 = 不可移動（AP+00,DP+00）
    2 = 騎士移動力減低（AP-05,DP+10）
    3 = 所有人物移動力減低（AP-05,DP+10）
    4 = 所有人物移動力減低（AP-05,DP-05）
    5 = 不可移動（AP+00,DP+00）
  戰鬥背景編號：2 byte
```
