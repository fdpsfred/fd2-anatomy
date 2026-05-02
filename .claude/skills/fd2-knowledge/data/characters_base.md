# 人物出場屬性表（每位 24 bytes）

來源：`Strategy_Guide/code_hack2.htm` §4

```
4.人物出場屬性資訊：7ADB5h或55BA1h開始，每個人物24 byte
  開頭為01 01 01 2A

-- -- -- -- -- RA CL LV HP HP MP MP MV MG MG MG
MG IT IT IT IT IT IT AP AP DP DP DX DX -- -- --

RA = 種族編號
CL = 職業編號
LV = 出場時等級
HP = 人物基礎生命，實際出場HP=人物基礎生命+(LV-1)*HP最小成長值
MP = 人物基礎法力，實際出場MP=人物基礎法力+(LV-1)*MP最小成長值
MV = 移動力
MG = 已會的法術
IT = 初始裝備（FF表沒有）
AP = 人物基礎攻擊力，實際出場AP=人物基礎攻擊力+LV*AP最小成長值
DP = 人物基礎防禦力，實際出場DP=人物基礎防禦力+LV*DP最小成長值
DX = 人物基礎速度值，實際出場DX=人物基礎速度值+LV*DX最小成長值
```
