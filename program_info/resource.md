# resource

唯一的「resource loader」：`load_dat_resource @ 0x000111BA`。任何 FD2 程式
要從資源檔抓資料，都會透過這個函式。資源檔本身的格式詳見
`resource_info/overview.md`。

## load_dat_resource signature

```c
void *load_dat_resource(
    ctx,  _,  _,                 // 3 個 CRT context args (忽略)
    char *filename,              // 資源檔名 (例 "FDFIELD.DAT")
    uint *old_buf,               // 若非 NULL 會先 crt_free(old_buf)
    int   index                  // 資源索引
);
```

## 演算法

```
if (old_buf) crt_free(old_buf);
fopen(filename, "rb")
fseek(fp, index*4 + 6, SEEK_SET)             // 跳過 6-byte LLLLLL signature
fread(header, 8, 1, fp)                      // 讀 (start, end) u32 pair
size = end - start
last_resource_size = size                    // 寫入 0x53BFF 給 caller
buf = crt_malloc(size)
fseek(fp, start, SEEK_SET)
fread(buf, size, 1, fp)
fclose(fp)
return buf
```

`last_resource_size @ 0x00053BFF` 留給 caller 不需自己追蹤 size。

## 走 load_dat_resource 的資源檔

9 個 (LLLLLL 格式)：FDTXT、FDOTHER、FDFIELD、FDSHAP、DATO、FDMUS、FIGANI、BG、TAI。

走獨立 `crt_fopen_read` 但同 LLLLLL 格式 (2 個)：TITLE、ANI。

唯一非 LLLLLL：FDICON.B24，由 `load_chapter_battle_data` 直接 `crt_fopen_read` 讀。

## 資源檔名表

`.object2 @ 0x00051A42` 存著 6 個主 DAT filename 的 null-terminated 字串
+ 5 個 u32 設定常數。

## 為何不把所有 callers 都歸到 resource system

劃分原則：function 的歸屬看「主業」，不看「用到什麼 resource」。例如
`load_save_and_init_engine` 主業是 save/load，雖然會呼叫
`load_dat_resource("FDOTHER.DAT", ...)` 載入 portrait，仍歸 save_load system。

實際 32 個 callers 分布：3 個 save 系統、2 個 CRT layer、其餘 27 個散在
battle / ui_menu / animation / field_map。

## chapter 資源載入器：`load_chapter_battle_data @ 0x1088D`

每章呼一次，連續載入該章的 3 個 FDFIELD entry（tile_map / tile_event / portrait
table）+ 對應 FDSHAP snapshot/attribute pair + FDTXT chapter_text + FDICON 等。
公式詳見 `resource_info/fdfield.md` 與各 DAT 檔的 entry index 對照表。
