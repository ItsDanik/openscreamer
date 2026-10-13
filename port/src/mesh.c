#include "mesh.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0]
    | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int16_t  rd16(const uint8_t *p) { return (int16_t)((uint16_t)p[0]
    | ((uint16_t)p[1] << 8)); }

/*
 * THE FACES OF A BLOCK, reached the way the game reaches them.
 *
 * The block header's +0x28 .. +0x44 are EIGHT offsets (from the block), one
 * per octant the object can be seen from; each leads to a row of uint16 face
 * offsets (from the block again, each the base of a 36-byte face record) ended
 * by 0xFFFF.  A row is the order to draw the faces in from that side, and it
 * leaves out the faces that cannot be seen from there -- so no single row need
 * hold them all, but the eight between them do.  The faces are every offset
 * that appears in any row, here in rising order.
 *
 * (Until 2026-10-11 the loader SCANNED the block for a run of plausible
 * 36-byte records instead -- first-day code.  Checked over every model in the
 * game, 5174 blocks and 83900 faces, the two give exactly the same records.)
 */
static int block_faces(const uint8_t *buf, long len, long off, int b, long *out)
{
    int n = 0;
    for (int row = 0; row < 8; row++) {
        long lo = (long)(int32_t)rd32(buf + off + 0x28 + 4 * row);
        if (lo <= 0) continue;
        for (long p = off + lo; p + 2 <= len; p += 2) {
            long e = (long)(uint16_t)rd16(buf + p);
            if (e == 0xFFFF) break;
            if (off + e + 36 > len) break;              /* not a face record */
            int k = 0;
            while (k < n && out[k] < e) k++;
            if (k < n && out[k] == e) continue;         /* already have it */
            if (n >= b) return -1;                      /* more faces than the header says */
            for (int j = n; j > k; j--) out[j] = out[j - 1];
            out[k] = e;
            n++;
        }
    }
    return n;
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

static mesh *g_moving[64];
static int   g_nmoving;

static void mesh_pose(mesh *m, int f)
{
    for (int pi = 0; pi < m->nparts; pi++) {
        struct mesh_part *p = &m->part[pi];
        const int16_t *q = p->frame + 12 * f;
        const float k = 1.0f / 16383.0f;
        for (int i = 0; i < p->vcount; i++) {
            vec3 v = p->rest_vert[i];
            m->vert[p->vbase + i].x = (q[0] * v.x + q[3] * v.y + q[6] * v.z) * k + q[9];
            m->vert[p->vbase + i].y = (q[1] * v.x + q[4] * v.y + q[7] * v.z) * k + q[10];
            m->vert[p->vbase + i].z = (q[2] * v.x + q[5] * v.y + q[8] * v.z) * k + q[11];
        }
        /* The turn from the model's Z-up to Y-up mirrors the part, and the engine knows it:
           drawing a block that has frames it first changes the sense of its back-face test
           (0x8acf0 against 0x8ad02 patch a jge / jl into the face routines at three places;
           Screamer 2 0x7ec0c / 0x7ec1e).  Here that is the normal turned round, so that for
           the renderer it points away from the side that shows, as every other face's does. */
        for (int i = 0; i < p->fcount; i++) {
            vec3 v = p->rest_normal[i];
            m->normal[p->fbase + i].x = -(q[0] * v.x + q[3] * v.y + q[6] * v.z) * k;
            m->normal[p->fbase + i].y = -(q[1] * v.x + q[4] * v.y + q[7] * v.z) * k;
            m->normal[p->fbase + i].z = -(q[2] * v.x + q[5] * v.y + q[8] * v.z) * k;
        }
    }
}

float mesh_reach(const mesh *m)
{
    float best = 0.0f;
    for (int pi = 0; pi < m->nparts; pi++) {
        const struct mesh_part *p = &m->part[pi];
        float rv = 0.0f, rt = 0.0f;
        for (int i = 0; i < p->vcount; i++) {
            vec3 v = p->rest_vert[i];
            float d = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
            if (d > rv) rv = d;
        }
        for (int f = 0; f < m->nframes; f++) {
            const int16_t *q = p->frame + 12 * f;
            float d = sqrtf((float)q[9] * q[9] + (float)q[10] * q[10] + (float)q[11] * q[11]);
            if (d > rt) rt = d;
        }
        if (rv + rt > best) best = rv + rt;
    }
    return best;
}

void mesh_animate_all(int clock16)
{
    for (int i = 0; i < g_nmoving; i++) {
        mesh *m = g_moving[i];
        int f = (int)(((long)m->nframes * (clock16 & 0xffff)) >> 16);
        if (f < 0 || f >= m->nframes) f = 0;
        mesh_pose(m, f);
    }
}

/* mesh_load_at() fills these, then gives the mesh arrays of its own, no bigger than needed */
static vec3    s_vert[MESH_MAX_VERT], s_normal[MESH_MAX_FACE], s_vnormal[MESH_MAX_VERT];
static uint8_t s_ncorner[MESH_MAX_FACE], s_face_block[MESH_MAX_FACE];
static int32_t s_idx[MESH_MAX_FACE][4], s_material[MESH_MAX_FACE];
static int16_t s_colour[MESH_MAX_FACE], s_uv[MESH_MAX_VERT][2], s_coll[MESH_MAX_FACE];

static void *keep(const void *from, size_t n)
{
    void *p = malloc(n ? n : 1);
    if (p) memcpy(p, from, n);
    return p;
}

static void mesh_drop(mesh *m)
{
    if (m->vert && m->vert != s_vert) {
        free(m->vert); free(m->normal); free(m->vnormal); free(m->ncorner); free(m->face_block);
        free(m->idx); free(m->material); free(m->colour); free(m->uv); free(m->coll);
    }
    m->vert = NULL; m->normal = NULL; m->vnormal = NULL; m->ncorner = NULL; m->face_block = NULL;
    m->idx = NULL; m->material = NULL; m->colour = NULL; m->uv = NULL; m->coll = NULL;
}

static int mesh_fill(mesh *m, const uint8_t *buf, long len);

int mesh_load_at(mesh *m, const uint8_t *buf, long len)
{
    mesh_drop(m);                       /* loaded before (the menu's models): give the old arrays back */
    m->vert = s_vert; m->normal = s_normal; m->vnormal = s_vnormal; m->ncorner = s_ncorner;
    m->face_block = s_face_block; m->idx = s_idx; m->material = s_material; m->colour = s_colour;
    m->uv = s_uv; m->coll = s_coll;
    int r = mesh_fill(m, buf, len);
    if (r != 0 || m->nface < 1) {
        m->vert = NULL; mesh_drop(m);
        m->nvert = m->nface = m->ncoll = 0;
        return -1;
    }
    size_t v = (size_t)m->nvert, f = (size_t)m->nface;
    m->vert = keep(s_vert, v * sizeof *s_vert);         m->vnormal = keep(s_vnormal, v * sizeof *s_vnormal);
    m->uv = keep(s_uv, v * sizeof *s_uv);               m->normal = keep(s_normal, f * sizeof *s_normal);
    m->ncorner = keep(s_ncorner, f);                    m->face_block = keep(s_face_block, f);
    m->idx = keep(s_idx, f * sizeof *s_idx);            m->material = keep(s_material, f * sizeof *s_material);
    m->colour = keep(s_colour, f * sizeof *s_colour);   m->coll = keep(s_coll, (size_t)m->ncoll * sizeof *s_coll);
    return 0;
}

static int mesh_fill(mesh *m, const uint8_t *buf, long len)
{
    m->nvert = 0;
    m->nface = 0;
    m->nframes = 0;
    m->nparts = 0;
    m->nblock = 0;
    m->ncoll = 0;
    m->carpet = 0;
    m->bound_r = m->bound_cx = m->bound_cz = 0.0f;
    if (len >= 0x14) {
        int nf = (int)rd32(buf + 0x10);
        if (nf > 1 && nf <= 4096) m->nframes = nf;
    }
    m->wheelbase = 0.0f;
    m->axle_front = m->axle_rear = 468.0f;
    m->track_half = 0.0f;
    float axle_lo = 0.0f, axle_hi = 0.0f;
    int axles = 0;

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

        if (off + 0x48 > len) continue;
        static long face_at[MESH_MAX_FACE];
        b = block_faces(buf, len, off, b, face_at);
        if (b < 1) continue;
        if (m->nvert == 0) m->flags = (int)rd32(buf + off + 0x18);
        if (m->nvert == 0) m->surface = (off + 0x6c <= len) ? (char)buf[off + 0x6b] : 0;
        if (m->nvert == 0 && off + 0x78 <= len) {
            char nm[17] = { 0 };
            for (int i = 0; i < 16 && buf[off + 0x68 + i]; i++) nm[i] = (char)(buf[off + 0x68 + i] | 0x20);
            /* tap(peto) on tracks 1-4, sper on Sunbeach Hill: the grass laid under the
               road.  NOT the other akk.. ground objects: acq / mare are water, rai
               the tram rails laid ON the road, and the rest are not flat */
            m->carpet = !strncmp(nm, "akk", 3) && (strstr(nm, "tap") != NULL || strstr(nm, "sper") != NULL);
        }
        long uo = (long)(int32_t)rd32(buf + off + 0x24);
        int has_uv = (uo > 0 && uo + 6L * a <= len);

        /*
         * The block's own POSITION within the object.  Header +0x0C is the
         * offset of a 36-byte record whose int32s at +0x10/+0x14/+0x18 are an
         * x, y, z offset for the whole block.  It is zero for track pieces and
         * it is what puts a car together: AUTO00's tyre and hub blocks are
         * modelled at the origin and carry (+-226, 109, 490) for the front
         * axle and (+-226, 94, -445) for the rear.
         */
        float tx = 0.0f, ty = 0.0f, tz = 0.0f;
        long so = (long)(int32_t)rd32(buf + off + 0x0C);
        if (so > 0 && so + 0x1C <= len) {
            tx = (float)(int32_t)rd32(buf + so + 0x10);
            ty = (float)(int32_t)rd32(buf + so + 0x14);
            tz = (float)(int32_t)rd32(buf + so + 0x18);
        }

        if (tx != 0.0f) {                       /* a wheel: it sits off-centre */
            if (!axles || tz < axle_lo) axle_lo = tz;
            if (!axles || tz > axle_hi) axle_hi = tz;
            axles = 1;
            m->wheelbase = axle_hi - axle_lo;
            m->axle_front = -axle_lo;
            m->axle_rear  =  axle_hi;
            if (fabsf(tx) > m->track_half) m->track_half = fabsf(tx);
        }

        int base = m->nvert;
        int bi = m->nblock < MESH_MAX_PARTS ? m->nblock++ : MESH_MAX_PARTS - 1;
        m->block[bi].vbase = base; m->block[bi].vcount = a;
        m->block[bi].fbase = m->nface; m->block[bi].fcount = b;
        m->block[bi].flags = (int)rd32(buf + off + 0x18);
        m->block[bi].ox = tx; m->block[bi].oy = ty; m->block[bi].oz = tz;
        long no = (long)(int32_t)rd32(buf + off + 0x20);
        m->block[bi].has_vnormal = (no > 0 && no + 4 + 12L * a <= len && (int)rd32(buf + no) == a);
        for (int i = 0; i < a; i++) {
            vec3 vn = { 0.0f, 0.0f, 0.0f };
            if (m->block[bi].has_vnormal) memcpy(&vn, buf + no + 4 + 12L * i, 12);
            m->vnormal[base + i] = vn;
        }
        for (int i = 0; i < a; i++) {
            const uint8_t *p = buf + vo + 6L * i;
            m->vert[base + i].x = (float)rd16(p)     + tx;
            m->vert[base + i].y = (float)rd16(p + 2) + ty;
            m->vert[base + i].z = (float)rd16(p + 4) + tz;
            m->uv[base + i][0] = has_uv ? rd16(buf + uo + 6L * i)     : 0;
            m->uv[base + i][1] = has_uv ? rd16(buf + uo + 6L * i + 2) : 0;
        }
        m->nvert += a;

        /* a moving part: keep the block as modelled, and its frames */
        if (m->nframes && so > 0 && so + 0x1C + 0x30L * m->nframes <= len &&
            m->nparts < MESH_MAX_PARTS) {
            struct mesh_part *p = &m->part[m->nparts++];
            p->vbase = base; p->vcount = a; p->fbase = m->nface; p->fcount = b;
            p->frame = malloc(sizeof(int16_t) * 12 * (size_t)m->nframes);
            p->rest_vert = malloc(sizeof(vec3) * (size_t)a);
            p->rest_normal = malloc(sizeof(vec3) * (size_t)b);
            for (int f = 0; f < m->nframes; f++)
                for (int j = 0; j < 12; j++)
                    p->frame[f * 12 + j] = (int16_t)rd16(buf + so + 0x1C + 0x30L * f + 2 * j);
            for (int i = 0; i < a; i++) {
                const uint8_t *q = buf + vo + 6L * i;
                p->rest_vert[i].x = (float)rd16(q);
                p->rest_vert[i].y = (float)rd16(q + 2);
                p->rest_vert[i].z = (float)rd16(q + 4);
            }
        }

        if (oi == 0) {                  /* the first block: what the game's ground test reads */
            long lo = (long)(int32_t)rd32(buf + off + 0x28);
            m->bound_r  = (float)(int32_t)rd32(buf + off);
            m->bound_cx = (float)rd16(buf + off + 4);
            m->bound_cz = (float)rd16(buf + off + 8);
            for (int k = 0; k < b && lo > 0 && off + lo + 2L * k + 2 <= len; k++) {
                long e = (long)(uint16_t)rd16(buf + off + lo + 2L * k);
                int at = -1;
                for (int j = 0; j < b; j++) if (face_at[j] == e) at = j;
                m->coll[m->ncoll++] = (int16_t)(at >= 0 ? m->nface + at : -1);
            }
        }
        for (int k = 0; k < b; k++) {
            long r = off + face_at[k] + 2;          /* the face record's base + 2, as the fields below are written */
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
            m->colour[fi]    = rd16(buf + r);
            m->face_block[fi] = (uint8_t)bi;
            m->nface++;
        }
    }
    if (m->nparts) {
        for (int pi = 0; pi < m->nparts; pi++)
            for (int i = 0; i < m->part[pi].fcount; i++)
                m->part[pi].rest_normal[i] = m->normal[m->part[pi].fbase + i];
        if (g_nmoving < 64) g_moving[g_nmoving++] = m;
        mesh_pose(m, 0);
    } else
        m->nframes = 0;
    return (m->nface > 0) ? 0 : -1;
}
