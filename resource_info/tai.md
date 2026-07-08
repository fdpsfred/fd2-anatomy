# TAI.DAT — cinematic 前景 sprite 覆蓋層 (BG.DAT 配對)

TAI.DAT 是一個 RLE sprite 檔案庫。它唯一的用途是在戰鬥／法術／必殺技／召喚等
cinematic 畫面裡，當作畫在後景之上的「前景 sprite 覆蓋層」——也就是角色底座圖、
登場名牌 (name-plate)、施法者基座 pose 這類圖層。每次都固定畫在座標 `(0xA4, 0x9D)`，
與畫在 `(0, 0x32)` 的 BG.DAT 後景 backdrop 配對合成到同一張 mode-13h scratch buffer。

檔名 `TAI` 只是一個縮寫（字面近似 "Terrain AI"），但與遊戲 AI 邏輯完全無關：
內容是純 sprite 圖檔，戰鬥 AI 的地形參數不在這裡；per-terrain 的 palette 也不在這裡
（palette 另由 FDOTHER.DAT 載入）。

- file size 94,917 bytes，56 entries（idx 0..55），與 BG.DAT 同 index 網域。

## 檔案格式

LLLLLL archive（統一格式見 `overview.md`）。每個 entry 是一段 RLE 4-op sprite
stream：`+0 u16 width`、`+2 u16 height`、`+4` 起為 command byte 串。opcode 表、
len 公式與 palette_op 模式是所有 RLE sprite 共用的編碼，見 `codecs.md`，此處不重述。

56 個 entry 中有一部分是 7-byte placeholder，內容為常數 `0A 00 03 00 C9 C9 C9`
（與 BG.DAT 共用同一個 placeholder 常數）；其餘 entry 是可繪製的 sprite。

## 載入與繪製

TAI.DAT 全 binary 恰好有 6 個載入點，每一點都透過 `fd2_load_dat_resource`
（filename string `"TAI.DAT"` @ 0x52393）載入後，交給 `fd2_rle_blit_sprite`
（或其 palette-remap 姊妹版 `fd2_rle_blit_with_palette_remap`，見 `codecs.md`）畫到
`(0xA4, 0x9D)`。6 個載入點的 `old_buf` 引數皆為 0，TAI 與其配對的 BG 各自
獨立 `malloc`／`free`，兩者不共用 buffer slot。

| 載入函數 | index 來源 |
|---|---|
| `fd2_execute_special_attack_skill` @ 0x276ec | terrain-indexed：施法者腳下 tile attribute byte（out-buf +6） |
| `fd2_execute_summon_spell_cast` @ 0x27fc9 | terrain-indexed：施法者腳下 tile attribute byte |
| `fd2_play_spell_cast_sequence` @ 0x2A6BD | terrain-indexed：施法者／目標 tile attribute byte |
| `fd2_play_full_combat_cinematic` @ 0x28a6c | terrain-indexed：per-chapter cinematic override byte，override==0 時退回腳下 tile attribute |
| `fd2_play_figani_char_intro_animation` @ 0x28784 | 固定 idx 3（角色登場名牌 sprite） |
| `fd2_play_final_chapter_30_ending` @ 0x2c405 | 固定 idx 3（第 30 章結局 backdrop sprite） |

4 個 terrain-indexed 載入點用施法者／目標腳下的 tile attribute byte（或 per-chapter
cinematic override）當 index，讓底座圖隨戰場地形變化；2 個固定 idx 3 載入點分別是
角色登場橫幅名牌與結局畫面的固定前景圖。

## 與 BG.DAT 的對應

TAI.DAT 與 BG.DAT 都是 56 entries、同 index 網域、共用同一個 placeholder 常數
`0A 00 03 00 C9 C9 C9`。cinematic 合成時兩者成對載入：BG.DAT 後景畫在 `(0, 0x32)`，
TAI.DAT 前景畫在 `(0xA4, 0x9D)`，疊在同一張 clear buffer 上。BG.DAT 的格式與用途見
`bg.md`。

## 工具

- 解碼與 round-trip render 驗證見 `tools/decoders/_index.md`。
