# tools/rsrc_unresolved/

關閉 `open_issues.md` 舊 [資源檔格式未解] 諸項時所用的 ground-truth 分析工具，
對真實遊戲檔（`fd2_game_files/`）逐項驗證，結論已整合進 `resource_info/`。可作為
這些資源格式結論的 regression 工具重跑。

## 檔案

- `analyze.py` — 一次跑完 5 項驗證，輸出 JSON 到 `workspace/rsrc_unresolved/`：
  - FDOTHER 29 個 nested sub-archive、176 sub-entry 的內容分類（RLE sprite vs
    8-bit PCM 樣本 vs VGA palette），及 12 個 confirmed-dead outer idx 的內容 dump。
  - ANI.DAT 9 entry 的 0xAD 檔頭（AFM 工具 banner + 320×200 描述子）與 per-frame
    8-byte header 各欄位（`+4..+7` 恆 0 驗證）、frame 逐一 walk 對齊 entry size。
  - FDSHAP tile-attribute 表 33 個 entry 的 byte `+1`（0..5）/ `+2`（0..0x37）/ `+3`
    （恆 0）值域統計。
  - TAI.DAT 56 entry 的 placeholder（16）與 RLE sprite（40）round-trip 分類。

  LLLLLL 容器解析與 RLE 4-op 解碼重用 `tools/decoders/`（`rle_decode_sized`）；
  所有位址／計數皆由 binary 現場解出，不從 KB 硬編。

- `verify_dead.py` — 對 `src/` 窮舉每一個 `fd2_load_dat_resource(FDOTHER, …)` 載入點
  與每一張餵 FDOTHER index 的表（3 張全域 dispatch 表 + 施法 cinematic 的 3 張
  function-local `const` 表），對 12 個候選 dead idx 判定 live/dead。結論：只有
  0x60/0x61/0x62 真 dead；其餘 9 個由 `fd2_play_spell_cast_sequence` 的
  `intro_sfx_bank[]` / `player_/enemy_team_sprite_id[]` 以 `table[spell_id]` 載入。
  取代舊 binary immediate-search 的 dead 判定（regression 工具）。

## 資料儲放

- output：`workspace/rsrc_unresolved/*.json`（scratch，KB 不引用）。
