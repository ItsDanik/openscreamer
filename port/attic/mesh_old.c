#include "mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0]
    | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int16_t  rd16(const uint8_t *p) { return (int16_t)((uint16_t)p[0]
    | ((uint16_t)p[1] << 8)); }

/*
 * Locate the face-record table inside an object block.
 *
 * The table's position is given by the per-octant draw-order table at the end
 * of the block (its entries are the records' byte offsets, stride 36), but it
 * is simpler and just as reliable to scan for it: the records are rigidly
 * 36 bytes apart, so the first offset where b consecutive records each have a
 * sane corner count and in-range indices is the table.
 */
static long find_faces(const uint8_t *buf, long len, long off, int a, int b)
{
    long best = -1;
    int  best_hits = 0;
    for (long start = off; start < off + 0x400 && start < len; start++) {
        int hits = 0;
        for (int k = 0; k < b; k++) {
            long r = start + 36L * k;
            if (r + 8 > len) break;
            int n = rd16(buf + r + 4);
            if (n < 3 || n > 8) break;
            int ok = 1;
            for (int j = 0; j <= n; j++) {
                long i16 = rd16(buf + r + 6 + 2 * j);
                if (i16 < 0 || i16 >= a * 6 || (i16 % 6)) { ok = 0; break; }
            }
            if (!ok) break;
            hits++;
        }
        if (hits > best_hits) { best_hits = hits; best = start; }
        if (best_hits == b) break;
    }
    return best_hits == b ? best : -1;
}

int mesh_load(mesh *m, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);
    int r = mesh_load_at(m, buf, len);
    free(buf);
    return r;
}

int mesh_load_at(mesh *m, const uint8_t *buf, long len)
{
    m->nvert = 0;
    m->nface = 0;

    if (len < 0x1C) return -1;
    int nobj = (int)rd32(buf + 0x14);
    if (nobj < 1 || 0x18 + 4 * nobj > len) return -1;

    for (int oi = 0; oi < nobj; oi++) {
        long off = (long)rd32(buf + 0x18 + 4 * oi);
        if (off < 0 || off + 0x20 > len) continue;
        int a  = (int)rd32(buf + off + 0x10);
        int b  = (int)rd32(buf + off + 0x14);
        long vo = (long)rd32(buf + off + 0x1C);
        if (a < 3 || b < 1) continue;
        if (vo + 6L * a > len) continue;
        if (m->nvert + a > MESH_MAX_VERT || m->nface + b > MESH_MAX_FACE) {
            fprintf(stderr, "mesh: block %d needs %d/%d verts/faces, array full\n",
                    oi, a, b);
            continue;
        }

        long fac = find_faces(buf, len, off + 0x24, a, b);
        if (fac < 0) continue;
        if (m->nvert == 0) m->texscale = (int)rd32(buf + off + 0x18);

        int base = m->nvert;
        for (int i = 0; i < a; i++) {
            const uint8_t *p = buf + vo + 6L * i;
            m->vert[base + i].x = (float)rd16(p);
            m->vert[base + i].y = (float)rd16(p + 2);
            m->vert[base + i].z = (float)rd16(p + 4);
        }
        m->nvert += a;

        for (int k = 0; k < b; k++) {
            long r = fac + 36L * k;
            int n = rd16(buf + r + 4);
            if (n < 3) n = 3;
            if (n > 4) n = 4;
            int fi = m->nface;
            m->ncorner[fi] = (uint8_t)n;
            for (int j = 0; j < 4; j++) {
                long i16 = (j < n) ? rd16(buf + r + 6 + 2 * j) : -1;
                m->idx[fi][j] = (i16 < 0) ? -1 : (int32_t)(base + i16 / 6);
            }
            m->normal[fi].x = (float)rd16(buf + r + 0x12) / 16384.0f;
            m->normal[fi].y = (float)rd16(buf + r + 0x16) / 16384.0f;
            m->normal[fi].z = (float)rd16(buf + r + 0x1a) / 16384.0f;
            /* r is the true face base + 2 (see mesh.h), so the material field
               at face+0x04 is at r+2. */
            m->material[fi]  = rd16(buf + r + 2);
            m->nface++;
        }
    }
    return (m->nface > 0) ? 0 : -1;
}
