# 炎龍騎士團 2 逆向工程專案

這款遊戲不用多作介紹，是不少玩家(包含我自己)心目中的經典。

一直以來都對這款遊戲背後的運作機制感到好奇，想要了解其背後的原理，

最近 AI 大行其道，剛好想練習如何使用 AI 進行遊戲程式的逆向工程，

炎2看來就是最適合的選擇了。

感謝所有前輩對這款遊戲的研究心得，現在在 AI 的幫助下終於完成了這個目標。

這個專案從炎2遊戲執行檔中的機械指令碼還原出原始的 C 語言程式碼，

並且用當年的開發環境重新編譯出 fd2.exe，在 DOS 環境下直接取代原本的執行檔進行遊玩，100% 復刻原始執行檔的程式行為。

專案的目標是藉由原汁原味地復刻原始遊戲執行檔來分析並且理解遊戲的所有內容和每個面向，

感覺就像是把一台經典老爺車拆開來研究完每個零件以後再無損地組裝回去，享受過程的樂趣。

因此以遊戲體驗來說，這個專案的結果沒有新的東西，因為一樣都是在 DOS 環境 (DOSBox, 86Box 模擬器或是 DOS 實機) 裡面玩原本的遊戲內容。

---

## 如何編譯

[`src/`](src) 底下就是從原始執行檔還原出來的完整 C 原始碼，用當年的開發環境就能重新編譯出跟原版行為一模一樣的 `FD2.EXE`。

### 需要的工具

| 工具 | 說明 |
| --- | --- |
| **Watcom C/C++ 9.5a** | 當年開發炎2用的編譯器，可以在 Internet Archive 上找到。版本必須完全相同才能忠實重建(細節見 [`rebuild_info/crt/fid_match.md`](rebuild_info/crt/fid_match.md))。安裝後把環境變數 `%WATCOM%` 指向安裝目錄，或直接放在 `%USERPROFILE%\Documents\WATCOM_9.5a`。 |
| **DOSBox-X** | Watcom 9.5a 是 DOS 時代的工具，透過 DOSBox-X 來跑。記得加入系統 `PATH`。 |
| **Python 3** | 用來執行建置腳本。 |

### 指令

在 repo 根目錄執行建置腳本 [`tools/fd2_build/build_fd2.py`](tools/fd2_build/build_fd2.py)：

```bash
python tools/fd2_build/build_fd2.py
```

腳本會在 DOSBox-X 裡用 Watcom 編譯 `src/` 下的所有原始碼並連結，產出：

```
workspace/fd2_build/exe/out/FD2.EXE
```

之後把這個 `FD2.EXE` 覆蓋掉遊戲目錄裡的原檔，就能在 DOSBox-X / 86Box / DOS 實機上照常遊玩(編譯本身不需要遊戲資料檔)。

95 版的 fd2.exe 有包入 dos4gw.exe，98版則沒有，分開成兩個檔案放置。

這個專案是以 98 版為對象還原，如果要放到 95 版的遊戲資料夾內執行，也必須複製 dos4gw.exe  過去。

本 repo 不包含原始遊戲檔案。

---

## 各人物屬性數值比較表

把每個角色升級時的屬性成長數值全部解出來，整理成一頁可以互動查看、對照的比較表：

[![角色成長數值表預覽](docs/character-stat-comparison/growth_table_preview.png)](https://fdpsfred.github.io/fd2-anatomy/character-stat-comparison/fd2_growth_tables.html)

👉 **[點我開啟互動版：角色成長數值表](https://fdpsfred.github.io/fd2-anatomy/character-stat-comparison/fd2_growth_tables.html)**

(這頁由 [`tools/growth_table/`](tools/growth_table) 從遊戲資料自動產生。)

---

## 有趣的發現

### 找到沒有被用到的對話資料

遊戲第 1~6 章的戰鬥對白裡，各藏著一頁**永遠不會出現在遊戲中**的孤兒台詞——把程式碼所有會叫出對話的路徑都追過一遍，確定沒有任何地方會顯示這幾頁。

內容還是主角索爾的無厘頭獨白，兩句輪流出現：「奈野啊捏？」和「這‥‥這是什麼碗糕！」。

> 詳見 [`resource_info/fdtxt.md`](resource_info/fdtxt.md)

### 95 年初版和 98 年合輯版的差異

炎2發行過 1995 初版和 1998 合輯版兩個版本。把兩版的資源檔逐一比對後發現，整套遊戲資料**其實只有三個檔案真的不一樣**：

- **5 個法術改了名字**：`咒殺 → 咒殺術`，另外四個從功能描述式改成比較帥氣的稱呼——`攻擊術 → 魔刃術`、`防禦術 → 魔鎧術`、`速度術 → 風行術`、`施毒術 → 毒擊術`。
- **中文字模圖集重新編號**：字其實沒變，只是換了編號，卻連帶讓文字檔多出約兩成的位元組差異——一度看起來像大改，實際上一個字都沒動。
- **主角索爾的英雄職戰鬥動畫換版**：初版腳下有個橢圓台座，合輯版把它拿掉了。

![索爾英雄職站姿 95 vs 98 版對照](resource_info/img/verdiff_figani_sol_hero_frame_a.png)

*上排為 95 初版(腳下有紅褐色橢圓台座)，下排為 98 合輯版(台座已移除)。*

> 詳見 [`resource_info/version_diff.md`](resource_info/version_diff.md)

### 釐清敵人會升級的bug原因

有些玩家或許遇過一個罕見的怪現象：某個**敵人**在交戰後突然獲得經驗值、升級，所有屬性還暴增到誇張的地步。追進程式碼後弄清楚了原因，是三個小疏漏剛好湊在一起：

- **經驗值發錯對象**：遊戲在攻擊結束後，是把經驗發給「被打的一方」。這在「敵人打你、你反擊殺敵所以你得經驗」時剛好正確；但換成**我方 AI 友軍**(某些聯合作戰章節才有的自動作戰友軍)去打敵人時，被打的是敵人，經驗就發到敵人身上了。
- **殘留的經驗沒清乾淨**：一個角色殺敵賺到的經驗，是發給「出手殺敵的這個角色本人」。而發經驗的程序在對象「已滿級」或「已陣亡」時會提早結束、卻忘了把暫存的經驗歸零。於是當一個**滿級**角色殺敵，這份經驗因為它自己已滿級而沒被領走、也沒被清掉，就殘留下來，剛好被友軍下一次攻擊灌進敵人。
- **敵人根本沒有升級數值**：敵人本來就沒被設計成會升級，遊戲裡沒有敵人的成長資料。程式硬要幫敵人算升級加成時，讀到的是一段不相干的垃圾資料，算出來的數值大得離譜——這就是屬性暴增的來源。

三個環節同時湊齊才會觸發，所以非常罕見。

> 詳見 [`program_info/known_bugs.md`](program_info/known_bugs.md)

---

## 專案結構與知識庫

這個專案同時是一套完整的逆向工程知識庫，所有分析資料依用途分成以下幾個 folder：

### Folder 用途

| Folder | 內容 |
| --- | --- |
| [`src/`](src) | 逆向工程重建的完整 C 原始碼，64 個 `.c` 依 anim / battle / field / gfx / spell / ui_menu / table / save 等子系統目錄組織，用 Watcom 9.5a 可編譯成在 DOS 下正確執行的 `FD2.EXE`。解析遊戲資訊時以 src/ 為主要依據，Ghidra 反組譯為輔 |
| [`program_info/`](program_info) | 對遊戲程式系統的解析。包含整體架構、12 個 system (battle / animation / save_load / ...)、章節生命週期與事件派遣機制 (field) |
| [`resource_info/`](resource_info) | 對每一個遊戲資源檔案格式的解析。FD2.LE 結構、FD2.SAV 存檔、11 個 LLLLLL DAT (FDTXT / FDFIELD / FDSHAP / FDOTHER / DATO / FDMUS / ...)、FDICON.B24、中文字編碼 |
| [`rebuild_info/`](rebuild_info) | 重建 FD2.LE / FD2.EXE 所需的 toolchain / lib / 連結環境資料，含等價鐵則、AIL 抽取、wlink 設定與實機 build test |
| [`libs/`](libs) | 重建連結所需的第三方 vendor lib（AIL v3 音訊函式庫 [`ailv3.lib`](libs/ailv3/ailv3.lib) 與標頭） |
| [`tests/`](tests) | src/ 的決定論 playthrough 整合測試（注入鍵盤事件驅動遊戲、擷取 framebuffer + state 比對 golden）；詳見 [`tests/_index.md`](tests/_index.md) |
| [`chapters/`](chapters) | 30 章唯一文件，每章一檔。劇情概要、加入角色、敵人/寶物/商店、特殊機制、init/end/post/event handler 流程、FDFIELD hook、FDTXT 對白全文；跨章機制 (天空之鑰、招募矩陣、結局分歧) 與 30 章 handler 總表在 [`_index.md`](chapters/_index.md) |
| [`assets/`](assets) | 從程式和資源檔解析出的遊戲數值內容。32 角色、215 道具、36 法術、68 敵人、27 職業、數值表、結局文字、字模對應表 |
| [`tools/`](tools) | 重複利用的 Python script。各資源檔的 parser/decoder、glyph lookup table 建表工具、CRT FidDb pipeline |

### 額外檔案

- [`open_issues.md`](open_issues.md) — 整理所有當前未解問題與未做分析，分 5 類列出
- [`docs/`](docs) — GitHub Pages 發佈目錄（非知識庫），目前放角色成長數值比較頁，由 [`tools/growth_table/`](tools/growth_table) 產生
- `workspace/` — 真 scratch 區域，POC 與一次性 sanity test 才放這。**KB / tool script / _index.md 都不能引用 workspace/ path**
- `legacy/` — 凍結的舊資料 (workflow 過程紀錄、舊 catalog、舊 ground_truth)；
  新文件不引用此目錄，裡面的所有內容都已過時，工作時絕對不能閱讀和參考

### 從哪裡開始讀

- 想了解遊戲整體架構：[`program_info/overview.md`](program_info/overview.md)
- 想讀重建的遊戲原始碼 / 編譯 FD2.EXE：[`src/`](src)（依子系統分目錄） + [`rebuild_info/build_test/_index.md`](rebuild_info/build_test/_index.md)
- 想實作存檔修改：[`resource_info/save_format.md`](resource_info/save_format.md) + [`assets/items.md`](assets/items.md)
- 想看遊戲劇情：[`chapters/_index.md`](chapters/_index.md) 然後依章閱讀
- 想寫資源檔解碼器：[`resource_info/overview.md`](resource_info/overview.md) + 對應檔案的 `.md`
- 想理解戰鬥 AI：[`program_info/battle.md`](program_info/battle.md)
- 想知道 FD2 用哪個編譯器和 CRT lib：[`rebuild_info/crt/fid_match.md`](rebuild_info/crt/fid_match.md)
- 想理解等價鐵則 / pool 分類 / fall-through pattern：[`rebuild_info/equivalence/_index.md`](rebuild_info/equivalence/_index.md)
- 想抽 AIL `.obj` 重建：[`rebuild_info/ail/_index.md`](rebuild_info/ail/_index.md) + [`tools/ail_extract/_index.md`](tools/ail_extract/_index.md)
- 想知道 FD2.LE 怎麼連結出來 / wlink 設定：[`rebuild_info/link/wlink_settings.md`](rebuild_info/link/wlink_settings.md) + [`rebuild_info/link/le_layout.md`](rebuild_info/link/le_layout.md)
- 想知道實機 playtest 解過哪些 rebuild bug / 怎麼建置測試 src-only FD2.EXE：[`rebuild_info/build_test/_index.md`](rebuild_info/build_test/_index.md)
- 想看每個 folder 的檔案清單：各 folder 內的 `_index.md`
