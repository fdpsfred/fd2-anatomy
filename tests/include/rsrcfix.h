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

#endif /* RSRCFIX_H */
