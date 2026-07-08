# TITLE.DAT — dead resource

LLLLLL archive (詳 `overview.md`)，**FD2 主遊戲 (FD2.EXE / FD2.LE) 從未載入此檔**。
file size 23,377 bytes，7 entries (idx 0..6)。

## Binary no-ref proof

標準 binary 字串掃描驗證：

```
FD2.LE: standalone "TITLE.DAT" string occurrences = 0
FD2.EXE: standalone "TITLE.DAT" string occurrences = 0
```

無任何 fopen / `fd2_load_dat_resource` caller 引用 "TITLE.DAT" 字串。對 FD2 主
遊戲而言是 dead resource，永遠不會被載入。

## 檔案性質

TITLE.DAT 與其他 10 個資源檔同為漢堂自家 LLLLLL DAT format (同一 build pipeline
輸出)，存在於 FLAME2 目錄，但 FD2 主遊戲沒有任何載入路徑接它。它在 FD2 的地位就是
一個未被引用的 LLLLLL archive；為何隨遊戲一起發行，屬於 FD2 binary 之外的事實
(pending，無法由程式碼定案)。

## 完整分類

| 分類 | 計數 | 比例 |
|---|---|---|
| confirmed_dead_with_binary_no_ref_proof | 7 | 100% |
