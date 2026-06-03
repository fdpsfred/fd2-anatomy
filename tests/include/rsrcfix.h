#ifndef RSRCFIX_H
#define RSRCFIX_H
/* shared test fixtures for the resource domain; include AFTER the
   common preamble (needs stdio/stdlib/string + the project types). */

/* Write a valid FDICON.B24 that the real fd2_load_portrait_to_cache (and the
 * real fd2_init_runtime_char_for_battle that calls it) can parse. Layout:
 *   [0..5]      6-byte magic (skipped by the loader's fseek to offset 6)
 *   [6..0x1A45] 140 x 12 x 4-byte (= 0x1A40) flat sprite-header table of
 *               monotonically increasing absolute offsets
 *   [0x1A46..]  data region the frame offsets point into
 * Header entry e holds (6 + 0x1A40 + e*4), so any portrait's
 * data_size = offsets[12]-offsets[0] = 12*4 = 48 bytes — small, distinct,
 * and fully present in the file. */
#define FDICON_HDR_ENTRIES (140 * 12)               /* 1680 4-byte offsets    */
#define FDICON_DATA_BASE   (6 + 0x1a40)             /* first data byte offset */
static void write_fake_fdicon(void)
{
    FILE  *fp;
    int    e;
    uint8  byte;
    int32  off;

    fp = fopen("FDICON.B24", "wb");
    byte = 0;
    for (e = 0; e < 6; e++) {                       /* 6-byte magic           */
        fwrite(&byte, 1, 1, fp);
    }
    for (e = 0; e <= FDICON_HDR_ENTRIES; e++) {     /* +1 trailing end-mark    */
        off = (int32)(FDICON_DATA_BASE + e * 4);
        fwrite(&off, 4, 1, fp);
    }
    /* data region: enough bytes for every offset the loader may fread */
    for (e = 0; e < FDICON_HDR_ENTRIES * 4 + 64; e++) {
        byte = (uint8)e;
        fwrite(&byte, 1, 1, fp);
    }
    fclose(fp);
}

/* Write a packed DAT archive the real fd2_load_dat_resource can parse. The
 * loader reads 8 bytes at file offset index*4+6 ([start,end] = two adjacent
 * offset-table entries), sets last_loaded_resource_size = end-start, then
 * freads `size` bytes from `start`. Layout produced here:
 *   [0..5]                 6-byte header prefix (skipped by index*4+6 base)
 *   [6 .. 6+(n+1)*4-1]     (n+1) u32 absolute offsets
 *   [data_base ..]         payloads laid sequentially
 * so resource `k` has size sizes[k] and bytes payloads[k][0..sizes[k]-1].
 * `payloads[k]` may be NULL to emit a zero-filled payload of sizes[k] bytes. */
static void write_fake_dat(const char *name, int n,
                           const int *sizes, const uint8 **payloads)
{
    FILE   *fp;
    int32   data_base;
    int32   off;
    int     k;
    int     j;
    uint8   zero;
    uint8   prefix[6];

    fp = fopen(name, "wb");
    data_base = (int32)(6 + (n + 1) * 4);

    memset(prefix, 0, sizeof(prefix));
    fwrite(prefix, 1, 6, fp);

    /* offset table: offsets[0]=data_base, offsets[k+1]=offsets[k]+sizes[k] */
    off = data_base;
    for (k = 0; k <= n; k++) {
        fwrite(&off, 4, 1, fp);
        if (k < n) {
            off = (int32)(off + sizes[k]);
        }
    }

    /* payloads, sequential */
    zero = 0;
    for (k = 0; k < n; k++) {
        if (payloads != 0 && payloads[k] != 0) {
            fwrite(payloads[k], 1, (size_t)sizes[k], fp);
        } else {
            for (j = 0; j < sizes[k]; j++) {
                fwrite(&zero, 1, 1, fp);
            }
        }
    }
    fclose(fp);
}

#endif /* RSRCFIX_H */
