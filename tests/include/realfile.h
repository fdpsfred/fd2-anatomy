#ifndef REALFILE_H
#define REALFILE_H
/* Read-only helpers for the STAGED real game files (FDFIELD.DAT /
 * FDOTHER.DAT / FDICON.B24 / FD2.SAV ...) that build_test.py copies into
 * the test cwd. Resource-loader tests parse the SAME real bytes the loader
 * under test reads and cross-check the loader's output against this
 * independent parse — no fabricated fixtures, no hardcoded magic values.
 * Include AFTER the common preamble (needs stdio/stdlib + the project types).
 *
 * DAT archive layout (mirrors src/rsrc/rsrc.c fd2_load_dat_resource): a
 * 6-byte prefix, then a u32 absolute-offset table from file offset 6 (entry
 * i at 6+i*4); resource `index` spans [off[i], off[i+1]). */
static long realdat_read_resource(const char *name, int index,
                                  uint8 **out_payload)
{
    FILE   *fp;
    uint32  pair[2];
    long    size;
    uint8  *buf;

    *out_payload = 0;
    fp = fopen(name, "rb");
    if (fp == NULL) {
        return -1;
    }
    fseek(fp, (long)index * 4 + 6, SEEK_SET);
    fread(pair, 4, 2, fp);
    size = (long)pair[1] - (long)pair[0];
    buf = (uint8 *)malloc((size_t)(size > 0 ? size : 1));
    fseek(fp, (long)pair[0], SEEK_SET);
    fread(buf, 1, (size_t)(size > 0 ? size : 0), fp);
    fclose(fp);
    *out_payload = buf;
    return size;
}

#endif /* REALFILE_H */
