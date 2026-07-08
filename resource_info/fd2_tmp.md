# FD2.TMP — portrait sprite cache swap 檔

FD2.TMP 是 in-memory portrait sprite cache（`data_fd2_portrait_sprite_cache @ 0x53A61`）的
raw byte image，固定 0x32A00 bytes（207,360 bytes）。它是 runtime swap / scratch 檔，不是
靜態遊戲資源：開機不讀取，每次章節 init 都會被重新產生，遊戲關閉後留在磁碟上的內容無意義。

## 產生（寫）

章節 portrait 載入尾端由 `fd2_load_chapter_portraits_and_dump_tmp @ 0x10b4e`
（src/rsrc/rsrc.c:439）寫出：

```c
fp = fopen("FD2.TMP", "wb");
fwrite((void *)data_fd2_portrait_sprite_cache, 1, 0x32a00, fp);
fclose(fp);
```

每次載完該章要用的 portrait set 就 truncate 並整份重寫（"wb" 開檔即清空），所以檔案永遠是
當前章節工作快取的完整快照。

## 讀回

`fd2_restore_portrait_cache_from_tmp @ 0x29117`（src/rsrc/rsrc.c:623）把整份 image 讀回：

```c
fp = fopen("FD2.TMP", "rb");
data_fd2_portrait_sprite_cache = (uint32)malloc(0x32a00);
fread((void *)data_fd2_portrait_sprite_cache, 1, 0x32a00, fp);
fclose(fp);
```

呼叫者為 FIGANI 戰鬥 cinematic 的 `fd2_execute_special_attack_skill`、
`fd2_play_full_combat_cinematic`、`fd2_play_spell_cast_sequence`。這些 cinematic 會釋放並替換
in-game 的 portrait / tile 快取，播完後靠這個 read-back 從 FD2.TMP 重新載回章節初始化時算好的
工作 portrait set，不必再解析一次 FDICON.B24。

## 內容佈局

檔案內容就是 portrait cache buffer 的逐 byte 複製，佈局與快取相同（src/rsrc/rsrc.c:191-199、
714-732）：

```
[0 .. 0x77F]   frame-offset lookup table：40 portrait × 12 sprite × 4-byte 絕對 offset
[0x780 ..]     每個已快取 portrait 依序 append 的 packed sprite payload
```

sprite payload 用 RLE 4-op sprite 編碼，opcode 見 `codecs.md`。frame-offset table 的每個
entry 是相對快取基底的絕對 offset，讀取端載入快取指標後 index `*(int32 *)(base + frame*4)`
再加回 base 即得 sprite bytes 起點。

## 相關

- 快取本身的生成與各 sprite 讀取見 `fdicon.md`。
- FD2.TMP 在章節切換 / cinematic 流程中的程式面時序見 `program_info/save.md`。
