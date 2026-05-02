# TITLE.DAT — dead resource

LLLLLL archive (詳 `overview.md`)，**FD2 主遊戲 (FD2.EXE / FD2.LE) 從未載入此檔**。
file size 23,377 bytes，7 entries (idx 0..6)。

## Binary no-ref proof

標準 binary 字串掃描驗證：

```
FD2.LE: standalone "TITLE.DAT" string occurrences = 0
FD2.EXE: standalone "TITLE.DAT" string occurrences = 0
```

無任何 fopen / `load_dat_resource` caller 引用 "TITLE.DAT" 字串。對 FD2 主
遊戲而言是 dead resource，永遠不會被載入。

## 推測用途 (非主遊戲)

雖然 TITLE.DAT 在 FLAME2 目錄存在，但對主遊戲是 dead resource。可能為：

1. **早期開發殘留** — 原本計畫的標題畫面資源，後改用其他方式 (FDOTHER 內嵌 title sprite)
2. **launcher / installer 殘留** — 廠商 packaging 工具可能用此檔顯示自己的 splash
3. **共享其他檔案集** — 多遊戲共用 launcher 系列的標題資源
4. **發行商的 cut content** — 本來要做但發行時砍掉的功能

從 LLLLLL signature 看，這是漢堂自家 LLLLLL DAT format — 跟其他 LLLLLL DAT
同源。可能是漢堂內部 build pipeline 統一輸出的 DAT 但 FD2 沒接這個 build target。

## 完整分類

| 分類 | 計數 | 比例 |
|---|---|---|
| confirmed_dead_with_binary_no_ref_proof | 7 | 100% |
