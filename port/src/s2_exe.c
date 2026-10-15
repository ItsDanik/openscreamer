/*
 * SCREAMER 2: the player's STARTL.EXE into the flat layout its addresses mean.
 *
 * The file is DOS/4GW's stub and then an LE image: objects (1 code at 0x10000, 2 and 3
 * data from 0x110000), each a run of 4096-byte pages, and relocation records a page -- the
 * places where the program holds one of its own addresses.  Only the kind "32-bit offset"
 * (source type 7) is a flat address; the others (selectors) are left as they are.  The same
 * as tools/unle.py --fixups, whose output this was checked against byte for byte.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "s2_exe.h"

uint8_t s2_flat[S2_FLAT];
/* the image as read: what s2_exe_fresh() puts back.  (Not allocated: the port puts its
   variables back as they were between its parts, and would lose the pointer.) */
#define S2_IMAGE 0x139c90u           /* the one program the port's addresses are of: this long ... */
#define S2_IMAGE_SUM 0x0ee6b6c1u     /* ... with this sum (STARTL.EXE of 1,055,478 bytes) */
static uint8_t s2_image[S2_IMAGE];
static int s2_image_ok;

static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t u16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

/* a checksum of the image (FNV-1a): the addresses in s2_hooks.c are this one program's */
static uint32_t fnv(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

int s2_exe_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "screamer 2: cannot open %s (the game's program is needed for a race)\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = len > 0x100 && len < (8 << 20) ? malloc((size_t)len + 16) : NULL;
    if (!d || fread(d, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(d); fprintf(stderr, "screamer 2: cannot read %s\n", path); return 0; }
    fclose(f);
    memset(d + len, 0, 16);
    const uint32_t le = u32(d + 0x3c);
    if (d[0] != 'M' || d[1] != 'Z' || le + 0xb0 > (uint32_t)len || d[le] != 'L' || d[le + 1] != 'E') {
        free(d); fprintf(stderr, "screamer 2: %s is not the game's program (no LE image)\n", path); return 0;
    }
#define H(o) u32(d + le + (o))
    const uint32_t npages = H(0x14), psize = H(0x28), objtab = le + H(0x40), objcnt = H(0x44),
                   fpt = le + H(0x68), frt = le + H(0x6c), datapages = H(0x80);
    uint32_t base[8] = { 0 }, top = 0;
    static uint32_t page_va[1024];
    if (objcnt > 7 || npages > 1024 || psize != 4096) { free(d); fprintf(stderr, "screamer 2: %s: not the layout expected\n", path); return 0; }
    uint8_t *img = calloc(1, S2_FLAT);
    if (!img) { free(d); return 0; }
    for (uint32_t i = 0; i < objcnt; i++) {
        const uint8_t *o = d + objtab + i * 24;
        const uint32_t vsize = u32(o), b = u32(o + 4), first = u32(o + 0xc), n = u32(o + 0x10);
        base[i + 1] = b;
        if (b + vsize > top) top = b + vsize;
        for (uint32_t p = 0; p < n; p++) {
            const uint32_t page = first - 1 + p, fo = datapages + page * psize, va = b + p * psize;
            if (page >= npages || va + psize > S2_FLAT) { free(d); free(img); fprintf(stderr, "screamer 2: %s: not the layout expected\n", path); return 0; }
            page_va[page] = va;
            if (fo < (uint32_t)len) memcpy(img + va, d + fo, fo + psize <= (uint32_t)len ? psize : (uint32_t)len - fo);   /* (the last page is short) */
        }
    }
    for (uint32_t page = 0; page < npages; page++) {
        uint32_t o = frt + u32(d + fpt + 4 * page);
        const uint32_t end = frt + u32(d + fpt + 4 * page + 4);
        while (o < end && end <= (uint32_t)len) {
            const uint32_t src = d[o], flags = d[o + 1];
            uint32_t n = 1, obj, toff = 0, list;
            int32_t one = 0;
            o += 2;
            if (flags & 3) { free(d); free(img); fprintf(stderr, "screamer 2: %s: a relocation of a kind not expected\n", path); return 0; }
            if (src & 0x20) n = d[o++]; else { one = (int16_t)u16(d + o); o += 2; }
            if (flags & 0x40) { obj = u16(d + o); o += 2; } else obj = d[o++];
            if ((src & 0x0f) != 2) {
                if (flags & 0x10) { toff = u32(d + o); o += 4; } else { toff = u16(d + o); o += 2; }
            }
            list = o;
            if (src & 0x20) o += 2 * n;
            if ((src & 0x0f) != 7 || obj > 7) continue;
            for (uint32_t k = 0; k < n; k++) {
                const int32_t so = (src & 0x20) ? (int16_t)u16(d + list + 2 * k) : one;
                const uint32_t va = page_va[page] + (uint32_t)so;
                if (so < 0 || va + 4 > top) continue;                    /* (so < 0: done from the page before) */
                const uint32_t v = base[obj] + toff;
                img[va] = (uint8_t)v; img[va + 1] = (uint8_t)(v >> 8); img[va + 2] = (uint8_t)(v >> 16); img[va + 3] = (uint8_t)(v >> 24);
            }
        }
    }
#undef H
    free(d);
    if (getenv("OS_S2EXEDUMP")) { FILE *o = fopen(getenv("OS_S2EXEDUMP"), "wb"); if (o) { fwrite(img, 1, top, o); fclose(o); } }   /* a test: against unle.py's */
    s2_image_ok = 0;
    if (top != S2_IMAGE || fnv(img, top) != S2_IMAGE_SUM) {
        fprintf(stderr, "screamer 2: %s is not the version of the game's program this port knows -- its routines are found by address (image of %u bytes, sum 0x%08x)\n", path, top, fnv(img, top));
        free(img);
        return 0;
    }
    memcpy(s2_image, img, S2_IMAGE);
    free(img);
    s2_image_ok = 1;
    s2_exe_fresh();
    return 1;
}

void s2_exe_fresh(void)
{
    memset(s2_flat, 0, sizeof s2_flat);
    if (s2_image_ok) memcpy(s2_flat, s2_image, S2_IMAGE);
}
