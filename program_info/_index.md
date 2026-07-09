# program_info/

對 FD2 遊戲程式系統的解析。檔名以 `src/` 模組名為準，每檔開頭的「驗證對象」節列出對應的
src 檔、主要 Ghidra 對象（函數名@位址）與相關資源檔，方便逐條對照核實。

## src 模組 ↔ 文件對照

| 文件 | 對應 src 模組 | 內容 |
|---|---|---|
| `overview.md` | `life/main.c` | 整體架構、FD2.LE 記憶體佈局、entry chain（`_cstart_` → `main`）、生命週期主迴圈、runtime_char 80-byte 佈局、子系統地圖 |
| `battle.md` | `battle/` | 戰鬥數值 pipeline、敵 AI 三層評分、回合 phase 時序、RNG 演算法與暖機、升級成長 |
| `spell.md` | `spell/` | 法術三層：28-slot dispatch、cinematic（召喚／必殺技）、effect applier；移動 XOR 施法 |
| `pathfind.md` | `util/pathfnd.c` | 移動範圍 flood fill 與尋路、per-job 移動成本表 |
| `field.md` | `field/` | 30 章 init/end/post handler、chapter jump table、FDFIELD turn-event／tile-step hook 派遣、勝負判定 |
| `dialog.md` | `dialog/dialog.c` | `fd2_display_dialog_scene` VM、portrait 快取、1bpp 字模、9-slice 對話框、speaker blit |
| `gfx.md` | `gfx/` | mode13h + RLE blit、tile_attribute、palette FX、`fd2_composite_battle_frame` 七步 finalizer |
| `anim.md` | `anim/` | FIGANI pose-stream 播放、法術視覺三段、panel/dialog slide、死亡/召喚動畫 |
| `audio.md` | `audio/audio.c` | BGM dispatcher、SFX trigger、章節 BGM 表；AIL vendor library 見 `rebuild_info/ail/` |
| `input.md` | `input/input.c` | BIOS 鍵盤直讀、scancode 表、wait-for-input 迴圈家族、BIOS tick pacing |
| `ui_menu.md` | `ui_menu/{menu,cursor,menufld,menucfg}.c` | 戰場側 UI：per-frame scancode dispatch、3 層 cursor、玩家行動選單、field-command modal |
| `town_menu.md` | `ui_menu/{chintro,shop,promote,status}.c` | 城鎮／章節交接選單樹：intro 畫面、商店、轉職、status 畫面 |
| `save.md` | `save/save.c` | FD2.SAV 存讀寫 helper、4-slot 選擇器、checksum／加解密、FD2.TMP portrait roundtrip |
| `rsrc.md` | `rsrc/rsrc.c` | `fd2_load_dat_resource` 統一 loader 與各章資源載入 |
| `table.md` | `table/`（14 檔） | 資料表模組級索引：14 個 table .c ↔ data_fd2_* 表 ↔ Ghidra 位址 ↔ assets/tables；10 個 `fd2_get_*_entry` accessor |
| `util.md` | `util/{misc,dpmi,noop}.c` | misc helper、party-roster helper、遊戲端 DPMI wrapper、delay wrapper |

模組拆分例外：`util` 模組拆成 `pathfind.md`（尋路）＋ `util.md`（其餘）；`ui_menu` 模組依檔案叢集
拆成戰場側 `ui_menu.md` 與城鎮側 `town_menu.md`；`crt` 模組歸 `rebuild_info/crt/`；`life` 模組併入
`overview.md`。

## chapters/ 子資料夾

每章 init/end handler 的函數呼叫流程、char_id 初始化序列、cutscene events、post_action handler、
FDFIELD event script，詳 `chapters/_index.md`。

## 相關文件

CRT layer、pool 分類、call graph、Watcom ABI、等價鐵則詳 `rebuild_info/equivalence/` 與
`rebuild_info/crt/`；資源檔格式詳 `resource_info/`；角色／道具／敵人／劇情數值詳 `assets/`。
