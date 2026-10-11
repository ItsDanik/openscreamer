#include "unr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int unr_load(unr *u, const char *path)
{
    memset(u, 0, sizeof *u);

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 8) { fclose(f); return -1; }

    uint8_t *buf = malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);

    int32_t blob_size = (int32_t)rd32(buf);
    int32_t count     = (int32_t)rd32(buf + 4);
    long    blob      = 8 + 4L * count;

    if (count < 1 || count > UNR_MAX_OBJ || blob > len) {
        fprintf(stderr, "unr: %s: bad header (count %d, blob %ld, len %ld)\n",
                path, count, blob, len);
        free(buf); return -1;
    }
    /* The exact identity that pins the header: blob + blob_size == file size. */
    if (blob + blob_size > len) {
        fprintf(stderr, "unr: %s: blob %d + size %d > file %ld\n",
                path, (int)blob, blob_size, len);
        free(buf); return -1;
    }

    u->file  = buf;
    u->len   = len;
    u->blob  = blob;
    u->count = count;
    for (int k = 0; k < count; k++)
        u->off[k] = (int32_t)rd32(buf + 8 + 4L * k);
    u->off[count] = blob_size;      /* so object k ends where k+1 begins */

    return 0;
}

const uint8_t *unr_object(const unr *u, int k, long *out_len)
{
    if (!u->file || k < 0 || k >= u->count) return NULL;
    int32_t a = u->off[k], b = u->off[k + 1];
    if (a < 0 || b < a || u->blob + b > u->len) return NULL;
    if (out_len) *out_len = b - a;
    return u->file + u->blob + a;
}
