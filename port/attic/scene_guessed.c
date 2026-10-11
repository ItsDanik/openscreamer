#include "scene.h"

#include "unr.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------- OSTACLES from the DTR */

/*
 * Object names come from the OSTACLES section of MAPnDAT.DTR.  Tokens look
 * like `sazcono`: a leading 's' then the name.  (The heading is misspelled in
 * the shipped data -- match it as-is.)
 */
static int read_obstacle_names(char names[][32], int max, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    int in_section = 0, n = 0;

    while (fgets(line, sizeof line, f) && n < max) {
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) continue;
        if (!in_section) {
            if (strncmp(p, "OSTACLES", 8) == 0) in_section = 1;
            continue;
        }
        if (strncmp(p, "END_LIST", 8) == 0) break;
        if (*p == '*') continue;
        char *tok = strtok(p, " \t\r\n");
        while (tok && n < max) {
            if (tok[0] == 's' && isalpha((unsigned char)tok[1])) {
                size_t l = strlen(tok + 1);
                if (l && l < 31) {
                    for (size_t i = 0; i <= l; i++)
                        names[n][i] = (char)tolower((unsigned char)tok[1 + i]);
                    n++;
                }
            }
            tok = strtok(NULL, " \t\r\n");
        }
    }
    fclose(f);
    return n;
}

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

            int32_t n0 = I32(cell + 0x14);
            int32_t n1 = I32(cell + 0x18);
            if (n0 < 0 || n1 < 0 || n0 + n1 > 64) continue;   /* nonsense */

            for (int k = 0; k < n0 + n1 && n < SCENE_MAX_OBJ; k++) {
                int32_t o = cell + 0x28 + MMP_REC * k;
                scene_object *so = &sc->obj[n];
                so->v     = I32(o + MMP_V);
                so->x     = (float)I32(o + MMP_X);
                so->e     = I32(o + MMP_E);
                so->y     = (float)I32(o + MMP_Y);
                so->t     = I32(o + MMP_T);
                so->layer = (k < n0) ? 0 : 1;
                so->m     = NULL;
                so->z     = 0.0f;
                so->yaw   = 0.0f;
                so->scale = 1.0f;
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
 * A mesh with no vertical extent is a surface -- 86 of MAP1.UNR's 192 objects
 * are like this, and the big ones are the road-surface tiles (6.2 m wide x
 * 4.1 m of length, laid end to end along the centreline).  The port already
 * synthesises the road and the ground from the .SPL splines, so drawing those
 * again would just z-fight grey slabs across the view.  Skip them by geometry,
 * which is a property of the mesh, not a guess about the record.
 */
static int mesh_is_horizontal(const mesh *m)
{
    for (int i = 1; i < m->nvert; i++)
        if (m->vert[i].y != m->vert[0].y) return 0;
    return 1;
}

/*
 * Load the 192 object meshes of MAPn.UNR.  Each is a .3DX-shaped file in
 * miniature, so the same parser handles them.  Kept static: 192 meshes at
 * ~30 KB is ~6 MB, which has no business on the stack.
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

int scene_load(scene *sc, const char *datdir, const char *gfxdir, int track)
{
    char path[512];
    static char names[SCENE_MAX_MESH][32];
    static mesh prop_mesh[SCENE_MAX_MESH];
    int nmesh = 0;

    memset(sc, 0, sizeof *sc);

    snprintf(path, sizeof path, "%s/MAP%dDAT.DTR", datdir, track);
    int nnames = read_obstacle_names(names, SCENE_MAX_MESH, path);
    if (nnames == 0)
        fprintf(stderr, "scene: no OSTACLES in %s\n", path);

    for (int i = 0; i < nnames; i++) {
        snprintf(path, sizeof path, "%s/%s.3DX", gfxdir, names[i]);
        char *slash = strrchr(path, '/');
        for (char *q = slash ? slash + 1 : path; *q && *q != '.'; q++)
            *q = (char)toupper((unsigned char)*q);
        if (mesh_load(&prop_mesh[nmesh], path) == 0)
            nmesh++;
        else
            fprintf(stderr, "scene: could not load %s\n", path);
    }

    mesh *unr_mesh = NULL;
    int nunr_total = 0;
    int nunr = load_unr(&unr_mesh, gfxdir, track, &nunr_total);

    if (nunr == 0 && nmesh == 0) {
        fprintf(stderr, "scene: no meshes at all, nothing to place\n");
        return 0;
    }

    snprintf(path, sizeof path, "%s/MAP%d.MMP", gfxdir, track);
    sc->count = read_mmp(sc, path);

    /*
     * V is the object's index into MAPn.UNR -- V spans 0..191 and the UNR
     * declares exactly 192 objects.  Use the UNR mesh.  Only if the UNR failed
     * to load do we fall back to cycling the OSTACLES props, which at least
     * draws something in the right place; the log says which happened.
     */
    int from_unr = 0, from_prop = 0, surface = 0;
    for (int i = 0; i < sc->count; i++) {
        int v = sc->obj[i].v;
        const mesh *cand = NULL;
        if (nunr && v >= 0 && v < nunr_total && unr_mesh[v].nface > 0) {
            cand = &unr_mesh[v];
            from_unr++;
        } else if (nmesh) {
            cand = &prop_mesh[(unsigned)(v < 0 ? 0 : v) % (unsigned)nmesh];
            from_prop++;
        }
        /*
         * Keep the mesh even when it is a tile.  The caller needs the tiles:
         * they are what the road's own height comes from (see the height pass in
         * main.c), and it is the caller's job to keep them out of the draw list.
         * They used to be cleared here, which threw the road's elevation away.
         */
        sc->obj[i].flat = (cand && mesh_is_horizontal(cand)) ? 1 : 0;
        if (sc->obj[i].flat) surface++;
        sc->obj[i].m = cand;
    }

    fprintf(stderr, "scene: UNR %d/%d objects, %d props; %d records "
                    "(layer0 %d, layer1 %d), V %d..%d\n"
                    "       meshes: %d from UNR, %d props, %d flat surfaces "
                    "skipped\n",
            nunr, nunr_total, nmesh, sc->count,
            sc->nlayer[0], sc->nlayer[1], sc->vmin, sc->vmax,
            from_unr, from_prop, surface);
    return sc->count;
}
