# rename_explain.md — tests/ 改名同步指南（給接手的 agent）

## 你的任務

`tests/` 約 40+ 個 `.c`/`.h` 仍引用 src_refine 改名前的**舊 symbol 名**，對現行
`src/include/globals.h`/`protos.h`/`types.h` 已不存在 → 編譯不過。把這些舊名換成新名，
讓 `tests/` 能對現行 `src/include/` 編譯通過。這是 src_refine closeout 的 **item-1 餘項**
（emit configs + KB docs 已同步、唯獨 tests/ 刻意延後）。

例：n=173 一個舊 global `data_fd2_chapter_portrait_load_buffer` 就有約 71 處跨 16 檔。

## 背景：為什麼 tests/ 沒被同步

src_refine Stage-2 + closeout 把約 78 個 symbol 改名，並已同步到 `src/` + Ghidra +
emit configs + KB docs。但 `tests/` 是改名前生成的，當時未一起改 → 留下一批已死的舊名。
**所有 tests/ 內的相關引用都是 pre-rename 的舊名**（這個前提下面會用到）。

## 對照檔：`tools/src_refine/data/rename_old2new.json`

三個區塊：

| 區塊 | 內容 | tests 用途 |
|---|---|---|
| `symbols` (78) | **全名** old→new（function + global） | **主要用這個**——tests `.c`/`.h` 引用的是 `globals.h`/`protos.h` 的真實全名 |
| `prefixless_functions` (39) | 去掉 `fd2_` 前綴的函數名 | 備用——若 tests 註解裡有無前綴的函數名才需要 |
| `kb_prose_shorthands` (5) | KB 散文的任意簡寫 | tests 一般用不到 |

新名以 **live Ghidra 為準**校準過，已含全部 closeout 修正（n=132/133 動詞、n=168/169 pose
交換、n=181 `_overlay→_sfx_bank`）。這份是 closeout 快照；若之後又有新改名，以 live Ghidra
為準重新 derive。

## 建議做法：單趟原子替換 + word-boundary

直接吃 `symbols` 區塊。**務必用單趟（single-pass）原子替換**——一個 regex alternation 配
替換函數查表，每個 match 依「原始匹配字串」替換一次、替換結果不回掃。**嚴禁逐名 sequential
兩趟替換**（見下方 pose 警告，A→B 再 B→A 會抵銷回原樣）。

word-boundary 用 `(?<![A-Za-z0-9_])(...)(?![A-Za-z0-9_])`，避免：
- prefix collision：`data_fd2_current_chapter_text` 不可誤傷 `..._text_ptr`
- `fd2_load_chapter_portrait` 不可誤傷 `fd2_load_chapter_portraits_and_dump_tmp`

可直接套用的 recipe（本次 emit/KB 同步用的就是同一招）：

```python
import json, re, os
m = json.load(open("tools/src_refine/data/rename_old2new.json", encoding="utf-8"))["symbols"]
olds = sorted(m, key=len, reverse=True)                       # 長名優先，避免前綴遮蔽
pat = re.compile(r"(?<![A-Za-z0-9_])(" + "|".join(re.escape(o) for o in olds) + r")(?![A-Za-z0-9_])")
for root, _, files in os.walk("tests"):
    for fn in files:
        if not fn.endswith((".c", ".h")):
            continue
        fp = os.path.join(root, fn)
        txt = open(fp, encoding="utf-8", newline="").read()   # newline="" 保留行尾
        new, n = pat.subn(lambda x: m[x.group(1)], txt)        # 單趟原子替換
        if n:
            open(fp, "w", encoding="utf-8", newline="").write(new)
            print(fp, n)
```

跑完它會印出每個改動檔 + 替換數；再跑一次同 `pat` 掃描應為 0 殘留（pose 對兩條會誤報，見下）。

## 關鍵警告（務必遵守）

1. **絕不替換 n=178 macro alias**：`data_fd2_input_last_key_pressed` 與
   `data_fd2_input_key_input_mode` 是 union `data_fd2_input_int16_regs` 的 `.h.al`/`.h.ah`
   **巨集別名、仍是有效 src 識別字**。它們**已從 `symbols` 排除**，上面的 recipe 不會碰到；
   但你若改用別的掃描方式，務必跳過——盲換會把巨集的 AL-byte 讀取錯誤摺進整個 union。

2. **pose 配對交換（`symbols` 內唯一危險條目）**：這兩條互為反向——
   - `data_fd2_chapter_intro_portrait_pose_x_column_table` ↔ `..._pose_y_row_table`

   這是 n=168/169 的真實「兩表名字對調」。**單趟原子替換是安全且正確的**（tests 的引用都是
   pre-swap 舊名，單趟會給對的結果）；但**逐名 sequential 兩趟會抵銷回原樣**。套用後若 tests
   有引用到 pose 表，務必編譯 + 對照該表實際軸向用法再確認一次。重掃時這兩個名字會「誤報殘留」
   （因為交換後兩個名字都還在、只是位置對調），那是假警報、不是漏改。

## 驗證（必做）

- 套用後 `tests/` 必須對現行 `src/include/` 編譯通過（走 test build gate）。
- 新名的**權威來源 = live Ghidra（依位址查）+ `src/include/globals.h`/`protos.h`/`types.h`**。
  有疑慮時以這些為準，不要只信本快照。
- **不要碰** `tools/src_refine/data/shards/` 與 `src_info*.json`（凍結的歷史檔）。

## 範圍界線

- 本指南**只處理改名同步**。若某個 test 改完名仍編不過，可能是**另一類問題**，例如：
  test 寫入了現在已是 `const` 的表（ISS-19/20 的 `*_menu_state_template` 家族）——那要照
  memory `feedback_const_data_never_demote_for_tests`（`#if 0` SKIP + Phase 3 重寫，
  **絕不為了讓 test 編過而把 const 拿掉**），不是改名能解決的。
- 已完成、**不要重做**：`src/` + Ghidra 全部改名；emit configs（routing.json /
  emit_issues.json / routing.md / data_routing.json）+ KB docs（~25 個 `.md`）。你只需做 `tests/`。
