#include "scene.h"

#include "unr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"

/* -------------------------------------------------------------- MMP placement */

#define MMP_REC   64     /* record stride, pinned by -1 at +0x20 in 646/646 */
#define MMP_X     0x08
#define MMP_E     0x0C
#define MMP_Y     0x10
#define MMP_V     0x00
#define MMP_T     0x18

/*
 * Walk the two-level grid and pull both record runs out of every cell.  See
 * scene.h for the layout; this function is deliberately narrow -- it does not
 * try to interpret V, E or T.
 */
static int read_mmp(scene *sc, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)len + 8);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b); fclose(f); return 0;
    }
    fclose(f);

#define I32(o) ((o) >= 0 && (o) + 4 <= len ? \
        (int32_t)((uint32_t)b[o] | ((uint32_t)b[o+1] << 8) | \
                  ((uint32_t)b[o+2] << 16) | ((uint32_t)b[o+3] << 24)) : -1)
    int n = 0, ncells = 0;
    sc->vmin = 1 << 30;
    sc->vmax = -1;

    for (int y = 0; y < 256; y++) {
        int32_t row = I32(4 * y);
        if (row <= 0) continue;
        for (int x = 0; x < 256; x++) {
            int32_t cell = I32(row + 4 * x);
            if (cell <= 0) continue;
            ncells++;

            /* arrays 2 and 3: road direction points and wall segments */
            {
                int32_t b2 = I32(cell + 0x08), c2 = I32(cell + 0x1C);
                int32_t b3 = I32(cell + 0x0C), c3 = I32(cell + 0x20);
                for (int k = 0; k < c2 && c2 < 256 && sc->ndir < SCENE_MAX_WALL; k++) {
                    int32_t o = b2 + 12 * k;
                    if (o < 0 || o + 12 > len) break;
                    scene_dir *d = &sc->dir[sc->ndir++];
                    memcpy(&d->angle, b + o, 4);
                    d->x = (float)I32(o + 4);
                    d->y = (float)I32(o + 8);
                }
                for (int k = 0; k < c3 && c3 < 256 && sc->nwall < SCENE_MAX_WALL; k++) {
                    int32_t o = b3 + 28 * k;
                    if (o < 0 || o + 28 > len) break;
                    scene_wall *w = &sc->wall[sc->nwall++];
                    memcpy(&w->angle, b + o, 4);
                    w->x  = (float)I32(o + 4);   w->y  = (float)I32(o + 8);
                    w->x1 = (float)I32(o + 12);  w->y1 = (float)I32(o + 16);
                    w->x2 = (float)I32(o + 20);  w->y2 = (float)I32(o + 24);
                }
            }

            int32_t n0 = I32(cell + 0x14);
            int32_t n1 = I32(cell + 0x18);
            if (n0 < 0 || n1 < 0 || n0 + n1 > 64) continue;   /* nonsense */

            for (int k = 0; k < n0 + n1 && n < SCENE_MAX_OBJ; k++) {
                int32_t o = cell + 0x28 + MMP_REC * k;
                scene_object *so = &sc->obj[n];
                so->v     = I32(o + MMP_V);
                so->x     = (float)I32(o + MMP_X);
                so->y     = (float)I32(o + MMP_Y);
                so->t     = I32(o + MMP_T);
                so->layer = (k < n0) ? 0 : 1;
                so->phase = I32(o + 0x20);
                so->pose  = NULL;
                so->zbias = 0.0f;
                so->m     = NULL;
                so->z     = (float)I32(o + MMP_E);
                /* T is the heading, 2048 to the turn -- see scene.h */
                so->yaw   = (float)so->t * (6.28318530718f / 2048.0f);
                /*
                 * A DELIBERATE DEPARTURE FROM THE GAME.  The pieces were laid
                 * out at whole multiples of 5 degrees, but the heading stored
                 * with each is up to 2.5 units short of that (90 degrees is
                 * stored as 510, not 512; 180 as 1022; 270 as 1535).  Used as
                 * stored -- which is what the game's code does, straight into
                 * its sine table -- neighbouring road pieces miss each other
                 * by about 25 units sideways: the lane lines step at every
                 * join and slivers open between tiles (Danik, track 1).
                 * Measured over every pair of neighbouring road pieces the
                 * mean gap is 18 units as stored and 10 snapped, and at the
                 * join Danik pointed at the sideways miss goes from 25 to 0.1.
                 * OS_NOSNAP=1 gives the headings as stored.
                 */
                if (!getenv("OS_NOSNAP")) {
                    const float step = 2048.0f / 72.0f;
                    float snapped = floorf((float)so->t / step + 0.5f) * step;
                    if (fabsf(snapped - (float)so->t) <= 2.5f)
                        so->yaw = snapped * (6.28318530718f / 2048.0f);
                }
                if (so->v < sc->vmin) sc->vmin = so->v;
                if (so->v > sc->vmax) sc->vmax = so->v;
                sc->nlayer[so->layer]++;
                n++;
            }
        }
    }
#undef I32
    free(b);
    if (ncells == 0) sc->vmin = sc->vmax = 0;
    return n;
}

/* ------------------------------------------------------------------------ api */

/*
 * Load the object meshes of MAPn.UNR.  Each is a .3DX-shaped file in
 * miniature, so the same parser handles them.  Kept static: 192 meshes are a
 * few MB, which has no business on the stack.
 */
static int load_unr(mesh **out, const char *gfxdir, int track, int *count)
{
    static mesh    unr_mesh[UNR_MAX_OBJ];
    static unr     u;
    char path[512];

    *out = unr_mesh;
    *count = 0;

    snprintf(path, sizeof path, "%s/MAP%d.UNR", gfxdir, track);
    if (unr_load(&u, path) != 0) {
        fprintf(stderr, "scene: no UNR at %s\n", path);
        return 0;
    }

    int ok = 0;
    for (int k = 0; k < u.count; k++) {
        long len = 0;
        const uint8_t *obj = unr_object(&u, k, &len);
        if (!obj) continue;
        if (mesh_load_at(&unr_mesh[k], obj, len) == 0)
            ok++;
        else
            fprintf(stderr, "scene: UNR object %d did not parse\n", k);
    }
    *count = u.count;
    return ok;
}

int scene_load(scene *sc, const char *gfxdir, int track)
{
    char path[512];
    memset(sc, 0, sizeof *sc);

    mesh *unr_mesh = NULL;
    int nunr_total = 0;
    int nunr = load_unr(&unr_mesh, gfxdir, track, &nunr_total);
    if (nunr == 0) return 0;

    snprintf(path, sizeof path, "%s/MAP%d.MMP", gfxdir, track);
    sc->count = read_mmp(sc, path);

    int placed = 0;
    for (int i = 0; i < sc->count; i++) {
        scene_object *so = &sc->obj[i];
        int v = so->v;
        if (v < 0 || v >= nunr_total || unr_mesh[v].nface == 0) continue;
        so->m = &unr_mesh[v];
        float r2 = 0.0f;
        for (int k = 0; k < so->m->nvert; k++) {
            const vec3 *p = &so->m->vert[k];
            float d = p->x * p->x + p->y * p->y + p->z * p->z;
            if (d > r2) r2 = d;
        }
        so->radius = sqrtf(r2);
        if (so->m->nparts) {
            float reach = mesh_reach(so->m);
            if (reach > so->radius) so->radius = reach;
            fprintf(stderr, "scene: moving object V%d (%d frames, %d parts) at %.0f, %.0f, height %.0f\n",
                    v, so->m->nframes, so->m->nparts, so->x, so->y, so->z);
        }
        placed++;
    }

    fprintf(stderr, "scene: UNR %d/%d objects parsed; %d records "
                    "(layer0 %d, layer1 %d), %d with a mesh; %d wall segments, "
                    "%d road-direction points\n",
            nunr, nunr_total, sc->count, sc->nlayer[0], sc->nlayer[1], placed,
            sc->nwall, sc->ndir);
    return sc->count;
}
