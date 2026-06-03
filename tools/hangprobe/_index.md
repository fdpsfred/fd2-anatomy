# tools/hangprobe — DOSBox-X 測試結束行為實證

`probe.py` 在隔離 mount 下，用鏡像 `tests/build.bat` 尾段（compile → link `system dos4g` → run 重導向 stdout → echo done → exit）的最小環境，逐案啟動 DOSBox-X 並每秒取樣 `proc` 是否存活、`DONE.TXT`、`test.out`、`HB.TXT`。用來決定 `tools/emit/build_test.py` 該如何零等待偵測測試結束。

用法：`python tools/hangprobe/probe.py [--case norm|crash|hang|beat|beat2|all] [--secs N]`（輸出寫 `workspace/hangprobe/`，scratch）。

## 結論（已落實到 build_test.py）

- **正常結束**：`DONE.TXT` 出現、DOSBox process ~2s 退出。
- **硬 crash（NULL call → DOS/4GW GP fault）**：DOS/4G error dump 印到（重導向的）`test.out`，控制權**交回 batch** → `echo done` 照跑 → `DONE.TXT` 出現、process ~2s 退出。**crash 不是卡住的原因**，反而很快。
- **真 hang（無窮迴圈）**：process 永不退出、`DONE.TXT` 不出現 —— 唯一會撞 timeout 的情況。
- **`fflush` 到重導向 stdout 不會即時傳到 host 檔**：DOSBox local-drive 把寫入快取到 file **close** 才 commit，所以 `test.out` 在程式結束前 host 端恆為 0 bytes（監看 test.out 成長行不通）。
- **每拍 fopen/fprintf/fclose 一個專用 heartbeat 檔則 host 即時可見**：close 強制 commit，故 build_test 用 `HB.TXT` 偵測 hang 並指出卡住的 test。
