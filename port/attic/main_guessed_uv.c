/*
 * main.c - OpenScreamer, track preview.
 *
 * Loads a track's road edges (MAPnSLA.SPL), resamples them into a smooth
 * densely-tessellated road, drapes a ground skirt either side, and paints the
 * MAPn.BCK backdrop behind it all with distance fog.
 *
 * Software rasteriser on a 320x200 framebuffer -- the original's low-res mode,
 * and what a MiSTer hybrid core wants (ARM makes a framebuffer, FPGA scans it
 * out).  The window is 4:3 and the projection carries an aspect correction, so
 * geometry is undistorted on a modern display; see ASPECT below.
 *
 * TWO THINGS THAT EASILY BREAK THIS -- read before changing either:
 *
 * 1. The splines are CONTROL POINTS, not a mesh.  Track 1 has only 14 edge
 *    pairs over ~127,000 units, so literally drawn a section is a hairline at
 *    the horizon.  road_build() resamples.  Do not remove the tessellation.
 *
 * 2. The road ribbon must be extruded PERPENDICULAR TO THE LOCAL TANGENT with
 *    a scalar width.  Interpolating the half-width *vector* between control
 *    points lets it rotate and pass through zero, which twists the ribbon into
 *    a DNA helix.  Only the width is interpolated; the direction is derived
 *    from the centreline tangent every sample.
 *
 * Not yet: textures, .3DX scenery, terrain, physics.
 */

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spl.h"
#include "scene.h"

#define FB_W 320
#define FB_H 200

/*
 * 320x200 is 1.6:1 but was always displayed at 4:3, so its pixels are 1.2x
 * wider than tall.  We render at 4:3 (window 960x720) and multiply the
 * horizontal focal by ASPECT, so a circle in world space looks round on screen.
 * The backdrop sampling uses the same corrected focal, which makes it land at
 * 1:1 against the framebuffer -- without that it is magnified and looks chunky.
 */
#define ASPECT 1.2f

#define SUBDIV    24          /* sub-quads per spline section */
#define MAX_QUADS 8192
#define SKIRT     120.0f       /* ground width, in multiples of the half-width */

typedef struct {
    float sx[4], sy[4];
    float depth;
    int   seq;             /* draw order inside one depth window: 0 road, 1 its detail */
    uint32_t colour;
    int visible;
    /* texture, when the surface has a material texture */
    const uint8_t *tex;
    int      tw, th;
    float    u[4], v[4];
    float    iw[4];        /* 1/depth per corner, for perspective correction */
    uint32_t tint;         /* the material's lit colour, multiplied in */
    int      keyed;        /* palette index 0 is TRANSPARENT (an overlay) */
    float    tex_weight;   /* 0 = flat colour only, 1 = full texture */
} quad;

typedef struct {
    float centre[MAX_QUADS][3];
    float left[MAX_QUADS][3];     /* road edges, derived in road_build */
    float right[MAX_QUADS][3];
    int   count;
    float width;
    float arc[MAX_QUADS];      /* cumulative arc from sample 0, for road texturing */
} road;

typedef struct { int w, h; uint8_t *pix; } sky;   /* RGB, w*h*3 */

static uint32_t *fb;
static const scene *g_scene;
static uint32_t g_pal[256];      /* MAPn.COL, scaled to 8 bits */

/*
 * Local-space units per texture repeat.  The face record carries no texture
 * coordinates, so UVs are derived from the face's own extent divided by this.
 * It is a CHOICE, not decoded data -- tune it by eye.  1024 units is about 3 m,
 * so a 64x64 texture lands at roughly 5 cm per texel.
 */
#define TEXSCALE 8192.0f

/*
 * The texture scale is NOT decoded data -- the face record carries no texture
 * coordinates.  So it is tunable at runtime, which is far more useful than
 * another guess: OS_TEXSCALE sets world units per texture repeat.
 *     OS_TEXSCALE=1024 port/build/openscreamer build/dec/MAP1 1
 */
static float g_texscale = TEXSCALE;          /* world units per U repeat */
static float g_texscalev = TEXSCALE * 16.0f / 256.0f;   /* per V, see TEXW/TEXH below */
static float g_texoffu = 0.0f, g_texoffv = 0.0f;        /* UV phase, in repeats */
static int   g_uvworld = 1;   /* 1 = UVs from world coords, 0 = object-local */
static int   g_texrot  = 0;   /* 0 / 90 / 180 / 270, orientation of the strip */
static int   g_useobjscale = 0;  /* use the block's +0x18 as a per-object scale */
static int   g_roadmat = 0;      /* MLB material index used for the road surface */
/*
 * ROAD WIDTH MULTIPLIER.
 *
 * The .SPL edge pairs give a half-width of 1309 -- a 2617-unit road -- and that
 * is what this port has always used.  It is wrong.  The game's OWN road tiles
 * measure 6158 units across (local X is the across-road axis: their local Z
 * pieces, 4106 long, are spaced 4199 apart along the road, so Z is the length).
 * 6158 / 2617 = 2.35.
 *
 * Danik spotted it as "the track is supposed to be wider" and "check if we only
 * render one lane" -- we were.  It also makes his four bright/dark cycles come
 * out at 4.6 m each, i.e. lanes, instead of 2 m.
 */
static float g_roadwidth = 2.353f;

/*
 * Ground outside the road.  Material 19 is the greenest thing in track 1's
 * material set (by green-to-red+blue ratio, and it looks like foliage in the
 * atlas); the default flat skirt was a bare grey.  Projected from above.
 */
static int   g_groundmat = 19;
/* 1 = drop scenery whose footprint reaches across the road (the default).
   OS_ONROAD=0 turns it off -- see the comment at the filter itself. */
static int   g_onroad = 1;
/* 1 = road elevation from the game's road tiles' height field (the default),
   0 = from MAPnCAM.SPL's Z (what this port used before). */
static int   g_roadz = 1;
/*
 * 0 = the thin ribbons stand up, as authored (the default).
 *
 * These were laid flat for two sessions on the theory that they were lane
 * paint.  The TEXTURES say otherwise and are unambiguous once looked at: the
 * materials those pieces carry are a guardrail (material 20: a rail with posts
 * on a transparent background) and a picket railing (material 25).  A railing
 * lying on the ground is not paint, and the "dashed centre line" that was
 * visible in renders was that railing seen from above -- which is also why
 * Danik, comparing against the original, said there was no lane line.
 *
 * OS_ROLL=1 lays them down again for an A/B.
 */
static int   g_roll = 0;
/*
 * OS_MATSCALE=1 takes the texture scale from the material record's first field
 * (16 or 64) instead of the one global value.  OFF by default: the field is NOT
 * a dimension -- it takes values like 224 and 128 as well, and 4096 bytes does
 * not divide by them -- so the "size x 512" reading is a hypothesis, not a
 * result.  It is here because it is cheap to A/B, and because a quarter of
 * track 1's materials are not 16 and a single global scale cannot be right for
 * all of them.
 */
static int   g_matscale = 0;
/*
 * OS_TILEPAINT=1 draws the road tiles' own painted faces -- the markings the
 * original paints on the tarmac, which live in the TILES and not in the road
 * texture (see CLAUDE.md).  OFF by default: the geometry and the placement are
 * right (the strips land along the road and across it, 38 cm wide, exactly
 * where the tile puts them), but the textures those materials point at come out
 * as colour SPLOTCHES, which means they are tile SHEETS rather than the 256x16
 * strips every other texture in the file is.  That is the next thing to decode;
 * until then this shows the marking POSITIONS and not the markings.
 */
static int    g_tilepaint = 0;
/*
 * 0 = strict layer filter (the default).
 *
 * The road's own tiles are in LAYER 0 and carry the lane markings as faces of
 * materials 147/148 -- see CLAUDE.md.  Drawing them shows the markings' true
 * positions, but their textures come out as colour SPLOTCHES (147/148 point at
 * tile SHEETS, not at the 256x16 strips everything else is), so the road is
 * cleaner without them until that sheet is decoded.  OS_LAYERROAD=1 (with
 * OS_TILEPAINT=1) is how to look at the positions meanwhile.
 */
static int    g_layerroad = 0;
/* 1 = a Z-facing billboard takes its texture crossed over (90 deg CW).  See the
   comment in the UV code; OS_BILLROT=0 restores the straight mapping. */
static int    g_billboardrot = 1;
/*
 * OS_FACEFIT=1 (or the `b` key) maps the texture ONCE across a thin piece's face
 * instead of tiling it in world units.  A railing panel or a crowd stand is a
 * one-shot image; tiling it at a world scale shows a slice of it magnified, which
 * is what "it needs scaling as well" describes.  This is the candidate rule --
 * Danik gets to accept or reject it in one keystroke.
 */
static int    g_facefit = 0;
static float  g_tilelift = 40.0f;
/* 1 = the object's own record height is its world height (default);
   0 = the old reading, the height as an offset added to the road. */
static int   g_objz = 1;
/* OS_OBJDUMP=<radius>: print every placed object within that many units of the
   camera while rendering.  A debugging aid for "what is that slab?". */
static int   g_objdump = 0;
/*
 * Road texture scale, in the two directions, live-adjustable:
 *   - / =   WIDTH  -- how many copies of the strip fit across the road
 *   ; / '   LENGTH -- how often the strip repeats along the road
 * Danik counts four cycles across the width in the original.
 */
static float g_roaducyc = 4.0f;
static float g_roadvlen = 1.0f;

/*
 * The textures are 256 wide x 16 tall, a 16:1 aspect, so the two UV axes need
 * DIFFERENT world scales or every texel comes out 16x too tall -- which is the
 * vertical smearing on the long walls.  V is scaled so texels stay square.
 */
#define TEXSCALEV g_texscalev

/*
 * EVERY texture in MAPn.MLB is 4096 bytes = 256 x 16.  All of them, whatever
 * the `size` field says.
 *
 * Established by row correlation, not by eye: for a stack of textures, the mean
 * |row_i - row_{i+1}| was 15.5 at a stride of 256, against 49 at 16, 77 at 64
 * and 27 at 512.  The true stride is the one where consecutive rows look alike,
 * and it is 256 for BOTH size-16 and size-64 materials -- so `size` is not a
 * dimension at all.  It means something else, not decoded.
 *
 * Two earlier readings were wrong and both looked plausible: as size*size (a
 * 16x16 thumbnail cut out of a 256x16 strip -- smeared banding) and as
 * (4096/size) x size (right for size 16, wrong for size 64).
 */
#define TEXBYTES 4096
#define TEXW     256
#define TEXH     16

/* ---------------------------------------------------------------- colours */

#define COL_SKY_FALLBACK 0xFF161320u
#define COL_ROAD_A       0xFF4E5660u
#define COL_ROAD_B       0xFF454D57u
#define COL_EDGE         0xFF9AA3B0u
#define COL_GROUND       0xFF23282Eu
#define COL_FOG          0xFF2A2434u

static uint32_t mix(uint32_t a, uint32_t b, float t)
{
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    int r = (int)(ar + (br - ar) * t);
    int g = (int)(ag + (bg - ag) * t);
    int bl = (int)(ab + (bb - ab) * t);
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

/* ---------------------------------------------------------------- framebuffer */

/*
 * Textured triangle.  The barycentric weights are already computed for the
 * coverage test, so interpolating UV with them is free -- affine, not
 * perspective-correct, which is exactly what a 1995 software renderer did and
 * what the 320x200 framebuffer wants anyway.
 */
static void tri_tex(const quad *q, int i0, int i1, int i2)
{
    float x0 = q->sx[i0], y0 = q->sy[i0];
    float x1 = q->sx[i1], y1 = q->sy[i1];
    float x2 = q->sx[i2], y2 = q->sy[i2];
    float u0 = q->u[i0], v0 = q->v[i0];
    float u1 = q->u[i1], v1 = q->v[i1];
    float u2 = q->u[i2], v2 = q->v[i2];

    int minx = (int)floorf(fminf(x0, fminf(x1, x2)));
    int maxx = (int)ceilf (fmaxf(x0, fmaxf(x1, x2)));
    int miny = (int)floorf(fminf(y0, fminf(y1, y2)));
    int maxy = (int)ceilf (fmaxf(y0, fmaxf(y1, y2)));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= FB_W) maxx = FB_W - 1;
    if (maxy >= FB_H) maxy = FB_H - 1;

    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (fabsf(area) < 1e-6f) return;
    int tw = q->tw, th = q->th;
    uint32_t tint = q->tint;
    float iw0 = q->iw[i0], iw1 = q->iw[i1], iw2 = q->iw[i2];

    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            float qx = x + 0.5f, qy = y + 0.5f;
            float w0 = ((x1 - x0) * (qy - y0) - (qx - x0) * (y1 - y0)) / area;
            float w1 = ((qx - x0) * (y2 - y0) - (x2 - x0) * (qy - y0)) / area;
            if (w0 < 0 || w1 < 0 || w0 + w1 > 1.0f) continue;
            /*
             * CAREFUL WITH THE WEIGHTS.  w0 is E01(p)/area, which is the
             * barycentric weight of VERTEX 2, and w1 is that of VERTEX 1 --
             * the coverage test does not care, and the flat path cannot show
             * the difference because all three corners are one colour.  Writing
             * w2*u0 + w0*u1 + w1*u2 therefore swaps u1 and u2 across every
             * triangle, which tears the texture and is invisible until you
             * actually texture something.
             */
            float w2 = 1.0f - w0 - w1;      /* == the weight of vertex 0 */
            /*
             * PERSPECTIVE-CORRECT.  Interpolate UV/depth and 1/depth, then
             * divide -- not UV directly.  Affine interpolation is what smeared
             * the road into diagonal bands: the quads are 2617 units across and
             * seen at a grazing angle, the worst case for it.
             */
            float I0 = w2 * iw0, I1 = w1 * iw1, I2 = w0 * iw2;
            float I  = I0 + I1 + I2;
            if (I < 1e-9f) continue;
            float u = (I0 * u0 + I1 * u1 + I2 * u2) / I;
            float v = (I0 * v0 + I1 * v1 + I2 * v2) / I;
            int tu = (int)(u * tw) % tw; if (tu < 0) tu += tw;
            int tv = (int)(v * th) % th; if (tv < 0) tv += th;
            uint8_t idx = q->tex[tv * tw + tu];
            /*
             * COLOUR KEY.  Index 0 is the transparent colour: lane markings,
             * signs and decals are separate strips with a black background,
             * drawn over the road rather than blended into it.  Material 25 is
             * 59% index 0, material 20 34% -- without this they render as black
             * slabs instead of lines.
             */
            if (q->keyed && idx == 0) continue;
            uint32_t c = g_pal[idx];
            /* tint: multiply by the material's lit colour, /255 */
            uint32_t r = (((c >> 16) & 0xFF) * ((tint >> 16) & 0xFF)) >> 8;
            uint32_t g = (((c >>  8) & 0xFF) * ((tint >>  8) & 0xFF)) >> 8;
            uint32_t b = (( c        & 0xFF) * ( tint        & 0xFF)) >> 8;
            fb[y * FB_W + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
}

static void tri(float x0, float y0, float x1, float y1, float x2, float y2,
                uint32_t c)
{
    int minx = (int)floorf(fminf(x0, fminf(x1, x2)));
    int maxx = (int)ceilf (fmaxf(x0, fmaxf(x1, x2)));
    int miny = (int)floorf(fminf(y0, fminf(y1, y2)));
    int maxy = (int)ceilf (fmaxf(y0, fmaxf(y1, y2)));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= FB_W) maxx = FB_W - 1;
    if (maxy >= FB_H) maxy = FB_H - 1;

    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (fabsf(area) < 1e-6f) return;

    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            float qx = x + 0.5f, qy = y + 0.5f;
            float w0 = ((x1 - x0) * (qy - y0) - (qx - x0) * (y1 - y0)) / area;
            float w1 = ((qx - x0) * (y2 - y0) - (x2 - x0) * (qy - y0)) / area;
            if (w0 >= 0 && w1 >= 0 && w0 + w1 <= 1.0f) fb[y * FB_W + x] = c;
        }
    }
}

static void quad_fill(const quad *q, uint32_t c)
{
    if (q->tex) {
        tri_tex(q, 0, 1, 2);
        tri_tex(q, 0, 2, 3);
        return;
    }
    tri(q->sx[0], q->sy[0], q->sx[1], q->sy[1], q->sx[2], q->sy[2], c);
    tri(q->sx[0], q->sy[0], q->sx[2], q->sy[2], q->sx[3], q->sy[3], c);
}

/* ------------------------------------------------------------------ sky */

static int sky_load(sky *s, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char magic[4];
    uint16_t w = 0, h = 0;
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "SKY1", 4) ||
        fread(&w, 2, 1, f) != 1 || fread(&h, 2, 1, f) != 1) {
        fclose(f);
        return -1;
    }
    s->w = w;
    s->h = h;
    s->pix = malloc((size_t)w * h * 3);
    size_t got = fread(s->pix, 1, (size_t)w * h * 3, f);
    fclose(f);
    return got == (size_t)w * h * 3 ? 0 : -1;
}

/*
 * Cylindrical backdrop.  The panorama is mapped so that one source pixel
 * subtends the same angle as one screen pixel at the given focals -- that is
 * what makes it 1:1 instead of magnified.  Its bottom edge sits on the
 * horizon; anything below is ground, which the road and skirt then draw over.
 */
static void sky_paint(const float yaw, const float pitch, const sky *s,
                      float focal_x, float focal_y)
{
    if (!s->pix) {
        for (int i = 0; i < FB_W * FB_H; i++) fb[i] = COL_SKY_FALLBACK;
        return;
    }
    float ang_h = (float)s->w / focal_x;    /* radians the image spans */
    float ang_v = (float)s->h / focal_y;

    for (int y = 0; y < FB_H; y++) {
        float rayPitch = pitch + (FB_H * 0.5f - (float)y) / focal_y;
        int v = (int)((1.0f - rayPitch / ang_v) * (float)s->h);
        uint32_t *row = fb + (size_t)y * FB_W;
        if (v >= s->h) {                       /* below the horizon */
            for (int x = 0; x < FB_W; x++) row[x] = COL_GROUND;
            continue;
        }
        if (v < 0) v = 0;
        for (int x = 0; x < FB_W; x++) {
            /*
             * MINUS.  Screen x increasing means the world ray turns the other
             * way: project() gives rx = sin(yaw - theta), so theta decreases as
             * x increases.  This was a plus, which was consistent with the old
             * mirrored projection and became wrong the moment that was fixed --
             * the backdrop scrolled the wrong way when turning.
             */
            float rayYaw = yaw - ((float)x - FB_W * 0.5f) / focal_x;
            float u = rayYaw / ang_h;
            u -= floorf(u);
            int px = (int)(u * (float)s->w);
            if (px < 0) px = 0;
            if (px >= s->w) px = s->w - 1;
            const uint8_t *p = s->pix + ((size_t)v * s->w + px) * 3;
            row[x] = 0xFF000000u | ((uint32_t)p[0] << 16) |
                     ((uint32_t)p[1] << 8) | p[2];
        }
    }
}

/* ------------------------------------------------------------------ road */

static void catmull(const float p0[3], const float p1[3], const float p2[3],
                    const float p3[3], float t, float out[3])
{
    float t2 = t * t, t3 = t2 * t;
    for (int k = 0; k < 3; k++) {
        float m1 = -p0[k] + p2[k];
        float m2 = 2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k];
        float m3 = -p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k];
        out[k] = p1[k] + 0.5f * (m1 * t + m2 * t2 + m3 * t3);
    }
}

static void road_build(road *r, const spline *left, const spline *right)
{
    int n = left->count < right->count ? left->count : right->count;
    if (n > SPL_MAX_NODES) n = SPL_MAX_NODES;

    static float c[SPL_MAX_NODES][3];
    static float w[SPL_MAX_NODES];
    for (int i = 0; i < n; i++) {
        c[i][0] = (left->pt[i].x + right->pt[i].x) * 0.5f;
        c[i][1] = (left->pt[i].y + right->pt[i].y) * 0.5f;
        c[i][2] = (left->pt[i].z + right->pt[i].z) * 0.5f;
        float dx = (float)(right->pt[i].x - left->pt[i].x);
        float dy = (float)(right->pt[i].y - left->pt[i].y);
        w[i] = 0.5f * sqrtf(dx * dx + dy * dy);
    }

    /* pass 1: resample the centreline and the scalar width */
    static float dc[MAX_QUADS][3], dw[MAX_QUADS];
    r->count = 0;
    for (int i = 0; i < n; i++) {
        const float *c0 = c[(i - 1 + n) % n], *c1 = c[i];
        const float *c2 = c[(i + 1) % n],     *c3 = c[(i + 2) % n];
        float w0 = w[(i - 1 + n) % n], w1 = w[i];
        float w2 = w[(i + 1) % n],     w3 = w[(i + 2) % n];
        for (int s = 0; s < SUBDIV && r->count < MAX_QUADS; s++) {
            float t = (float)s / (float)SUBDIV;
            int k = r->count;
            catmull(c0, c1, c2, c3, t, dc[k]);
            float m1 = -w0 + w2, m2 = 2 * w0 - 5 * w1 + 4 * w2 - w3;
            float m3 = -w0 + 3 * w1 - 3 * w2 + w3;
            dw[k] = w1 + 0.5f * (m1 * t + m2 * t * t + m3 * t * t * t);
            r->count++;
        }
    }
    int m = r->count;

    /* pass 2: extrude perpendicular to the tangent -- the direction is always
       derived, never interpolated, so the ribbon cannot twist */
    float sum = 0;
    for (int i = 0; i < m; i++) {
        const float *pv = dc[(i - 1 + m) % m];
        const float *nx = dc[(i + 1) % m];
        float tx = nx[0] - pv[0], ty = nx[1] - pv[1];
        float len = sqrtf(tx * tx + ty * ty);
        if (len < 1e-6f) { tx = 1.0f; ty = 0.0f; len = 1.0f; }
        /* left-hand perpendicular in the XY plane */
        float sx = -ty / len, sy = tx / len;
        float half = dw[i] * g_roadwidth;
        if (half < 1.0f) half = 1.0f;
        for (int k = 0; k < 3; k++) r->centre[i][k] = dc[i][k];
        r->left[i][0]  = dc[i][0] - sx * half;
        r->left[i][1]  = dc[i][1] - sy * half;
        r->left[i][2]  = dc[i][2];
        r->right[i][0] = dc[i][0] + sx * half;
        r->right[i][1] = dc[i][1] + sy * half;
        r->right[i][2] = dc[i][2];
        sum += half * 2.0f;
    }
    r->width = sum / (float)m;

    /*
     * Cumulative arc length, so the road texture can be laid along the road by
     * DISTANCE rather than by sample index -- indices bunch up on tight corners
     * (the resampling is uniform in t, not in length), which would compress the
     * texture exactly where the road bends hardest.
     */
    r->arc[0] = 0.0f;
    for (int i = 1; i < m; i++) {
        float dx = r->centre[i][0] - r->centre[i - 1][0];
        float dy = r->centre[i][1] - r->centre[i - 1][1];
        r->arc[i] = r->arc[i - 1] + sqrtf(dx * dx + dy * dy);
    }
}

/* side -1 = left edge, +1 = right edge; values beyond extrapolate outwards */
static void road_point(const road *r, int i, float side, float out[3])
{
    float a = (1.0f - side) * 0.5f, b = (1.0f + side) * 0.5f;
    for (int k = 0; k < 3; k++)
        out[k] = r->left[i][k] * a + r->right[i][k] * b;
}

/* ------------------------------------------------------------------ palette */

/*
 * MAPn.COL is a 256-entry VGA palette laid out as eight ramps of 32.  See the
 * shading code in render_frame() for why that layout matters.
 */
/*
 * MAPn.MLB's material table: 16-byte records of
 *     [texture size, texture_record_offset, colourA, colourB]
 * indexed by byte offset / 16, which is exactly how a face refers to one.
 * colourA is the lit colour and colourB the shaded one, as palette indices --
 * 303 of the 342 records in track 1 have A != B, so the pair is real.
 */
typedef struct {
    int size, texrec, colA, colB;
    uint8_t *tex;      /* tw*th, 8-bit, indexed by MAPn.COL */
    int tw, th;
    /*
     * 1 when the texture is mostly palette index 0 -- a colour-keyed overlay.
     * Lane markings, signs and decals are separate strips with a black
     * background drawn OVER the road; material 25 is 59% index 0, material 20
     * 34%.  These must not be dropped by the on-road filter, or every lane line
     * disappears.
     */
    int keyed;
} material;
#define MAT_MAX 1024
static material g_mat[MAT_MAX];
static int      g_nmat;
/* true if any face of this mesh uses the given MLB material record */
static int mesh_uses_material(const mesh *m, int mat)
{
    for (int f = 0; f < m->nface; f++)
        if (m->material[f] / 16 == mat) return 1;
    return 0;
}

/* true if any face of this mesh uses a colour-keyed (overlay) material */
static int mesh_is_overlay(const mesh *m)
{
    for (int f = 0; f < m->nface; f++) {
        int mi = m->material[f] / 16;
        if (mi >= 0 && mi < MAT_MAX && g_mat[mi].keyed) return 1;
    }
    return 0;
}

static uint8_t *g_mlb;
static long     g_mlb_len;

/*
 * A material's texture is `size * size` RAW 8-bit indices at the offset its
 * texture record holds; `MAPn.COL` is the palette for all of them.  There is no
 * compression and no per-texture palette -- an earlier reading that treated these
 * bytes as PCX-style RLE produced convincing-looking but meaningless output,
 * because asking a raw buffer for exactly size*size values always "succeeds".
 *
 * What confirmed it is the spacing: consecutive texture data offsets are exactly
 * size*size + 768 apart, and rendering them raw through MAPn.COL gives brick
 * courses, road markings, blue-striped barriers and grass.
 */
static void mlb_load(const char *path)
{
    for (int i = 0; i < MAT_MAX; i++) free(g_mat[i].tex);
    memset(g_mat, 0, sizeof g_mat);
    g_nmat = 0;

    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "note: no materials at %s\n", path); return; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f);
        fprintf(stderr, "note: cannot read %s\n", path);
        return;
    }
    fclose(f);

    g_mlb = buf;
    g_mlb_len = len;
    int ntex = 0;

    for (int i = 0; i < MAT_MAX && (long)(i * 16 + 16) <= len; i++) {
        const unsigned char *p = buf + i * 16;
        int sz = (int)(uint32_t)p[0] | ((uint32_t)p[1] << 8)
               | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        int ro = (int)(uint32_t)p[4] | ((uint32_t)p[5] << 8)
               | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
        int ca = (int)(uint32_t)p[8] | ((uint32_t)p[9] << 8)
               | ((uint32_t)p[10] << 16) | ((uint32_t)p[11] << 24);
        int cb = (int)(uint32_t)p[12] | ((uint32_t)p[13] << 8)
               | ((uint32_t)p[14] << 16) | ((uint32_t)p[15] << 24);
        int pow2 = (sz == 8 || sz == 16 || sz == 32 || sz == 64 ||
                    sz == 128 || sz == 256);
        if (!pow2 || ro <= 0 || ro > len) continue;
        g_mat[i].size = sz; g_mat[i].texrec = ro;
        g_mat[i].colA = ca;  g_mat[i].colB = cb;
        g_nmat++;

        /* the texture record at +0x04 of the material record */
        if (ro + 12 <= len) {
            int doff = (int)(uint32_t)buf[ro + 8] | ((uint32_t)buf[ro + 9] << 8)
                     | ((uint32_t)buf[ro + 10] << 16)
                     | ((uint32_t)buf[ro + 11] << 24);
            if (doff > 0 && (long)doff + TEXBYTES <= len) {
                g_mat[i].tex = malloc(TEXBYTES);
                if (g_mat[i].tex) {
                    memcpy(g_mat[i].tex, buf + doff, TEXBYTES);
                    g_mat[i].tw = TEXW;
                    g_mat[i].th = TEXH;
                    int zero = 0;
                    for (int q = 0; q < TEXBYTES; q++)
                        if (g_mat[i].tex[q] == 0) zero++;
                    g_mat[i].keyed = (zero * 4 > TEXBYTES);   /* >25% transparent */
                    ntex++;
                }
            }
        }
    }
    fprintf(stderr, "pal:materials %d records, %d textures (%ld bytes)\n",
            g_nmat, ntex, len);
}

static void pal_load(const char *path)
{
    for (int i = 0; i < 256; i++)
        g_pal[i] = 0xFF000000u;                 /* black if we cannot read it */
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "note: no palette at %s\n", path); return; }
    /*
     * The values are 0..63 -- a 6-bit VGA DAC palette -- so scale by 4.  Without
     * it everything renders at a quarter brightness: the palette's "white" entry
     * reads (61,61,61), which is 244 once scaled.
     */
    unsigned char b[768];
    if (fread(b, 1, sizeof b, f) == sizeof b)
        for (int i = 0; i < 256; i++)
            g_pal[i] = 0xFF000000u
                     | ((uint32_t)(b[3*i]     * 4) << 16)
                     | ((uint32_t)(b[3*i + 1] * 4) << 8)
                     |  (uint32_t)(b[3*i + 2] * 4);
    fclose(f);
}

/* ------------------------------------------------------------------- camera */

typedef struct { float x, y, z, yaw, pitch; } camera;

/*
 * World (X east, Y north, Z up) -> view -> screen.
 * yaw is measured from +X, so forward = (cos yaw, sin yaw); hence atan2(dy,dx).
 * focal_x already carries ASPECT.  Returns 0 if behind the near plane.
 */
static int project(const camera *cam, const float w[3], float focal_x,
                   float focal_y, float *sx, float *sy, float *depth)
{
    float dx = w[0] - cam->x, dy = w[1] - cam->y, dz = w[2] - cam->z;

    float c = cosf(cam->yaw), s = sinf(cam->yaw);
    float fx = c * dx + s * dy;
    /*
     * RIGHT = forward x up.  With forward = (cos yaw, sin yaw) and up = +Z that
     * is (sin yaw, -cos yaw), so rx = s*dx - c*dy.  This line used to read
     * -s*dx + c*dy -- exactly the negative -- which mirrored the entire world
     * horizontally.  One sign, and it caused: the track appearing to run the
     * other way round, geometry that did not line up with the original, LEFT
     * and RIGHT and A and D feeling reversed (patched at the time as separate
     * "bugs"), and textures that could never be matched, because a mirror is
     * not any scale, offset or rotation.
     */
    float rx = s * dx - c * dy;

    float cp = cosf(cam->pitch), sp = sinf(cam->pitch);
    float ry  = cp * dz - sp * fx;
    float fwd = sp * dz + cp * fx;

    if (fwd < 1.0f) return 0;

    *sx = FB_W * 0.5f + rx * focal_x / fwd;
    *sy = FB_H * 0.5f - ry * focal_y / fwd;
    *depth = fwd;
    return 1;
}

/* ------------------------------------------------------------------- main */

/*
 * Road-surface detail -- the edge lines and the tiles' paint -- shares its plane
 * with the road, so the sort that decides what covers what has to break those
 * ties deliberately.  The painter's sort orders by DISTANCE; two quads at the
 * same distance come out in an arbitrary order, and which side of the road won
 * was changing with the bend -- Danik saw the markings on one side, then the
 * other, and never both.
 *
 * A quad emitted with a bias sorts that many units NEARER, so it is drawn after
 * the surface it is painted on.  800 units is small next to the 10,000-unit
 * distances involved, so it cannot lift a marking over something genuinely in
 * front of it.
 */
/*
 * PER-MATERIAL TEXTURE ROTATION.
 *
 * The rotation a texture needs is a property of the texture, not of the piece:
 * Danik, looking at the track, reports some surfaces upright and others needing
 * 90 degrees, at the same time and with the same mapping rules.  So rather than
 * guess the rule, `n` cycles the rotation of the material under the crosshair
 * while he looks at it -- press until it is upright, read the console, and the
 * table gets set as the default here.  Entries are 0/90/180/270.
 */
static int g_matrot[MAT_MAX];
/*
 * Per-material SCALE multiplier, for the same reason: Danik reports that the
 * crowd and the roof cannot be made right by rotation alone -- "it needs scaling
 * as well" -- and the scale that suits one material does not suit another.
 * `z` / `x` change it for the piece under the crosshair.
 */
static float g_matscale_mul[MAT_MAX];
static int   g_matscale_init = 0;

static float g_quadbias = 0.0f;
static int   g_quadseq = 0;      /* 1 = road-surface detail (edge lines, paint) */

/*
 * Painter's order: furthest first.  Within 400 units of each other, a quad with
 * a higher `seq` is drawn later -- that is how the road's own edge lines stay on
 * top of the road they belong to.  A plain depth comparison got that wrong
 * wherever the two are the same distance apart, and a fixed bias (tried first)
 * is not enough on long or sharp turns, where the quads' average depths differ
 * by far more than any sane bias.  Danik, twice: the marking "clips through the
 * road surface" and it was "never visible on both sides".
 */
static int cmp_depth(const void *a, const void *b)
{
    const quad *qa = a, *qb = b;
    float d = qa->depth - qb->depth;
    if (d > 400.0f) return -1;
    if (d < -400.0f) return 1;
    if (qa->seq != qb->seq) return (qa->seq > qb->seq) ? 1 : -1;
    return (d > 0.0f) ? -1 : (d < 0.0f) ? 1 : 0;
}

static void emit(float p[4][3], uint32_t col, const camera *cam,
                 float focal_x, float focal_y, float fog_far,
                 quad *quads, int *nq)
{
    if (*nq >= MAX_QUADS) return;
    quad q;
    /* Must zero: q.tex and q.tsize are read by quad_fill, and a filled path
       that left them uninitialised segfaulted the texture sampler. */
    memset(&q, 0, sizeof q);
    q.colour = col;
    q.visible = 1;
    float d = 0;
    for (int k = 0; k < 4; k++) {
        if (!project(cam, p[k], focal_x, focal_y, &q.sx[k], &q.sy[k],
                     &q.depth)) {
            q.visible = 0;
            break;
        }
        float ddx = p[k][0] - cam->x, ddy = p[k][1] - cam->y;
        d += sqrtf(ddx * ddx + ddy * ddy);
    }
    if (!q.visible) return;
    q.depth = d * 0.25f - g_quadbias;
    q.seq = g_quadseq;
    float f = 1.0f - expf(-q.depth / fog_far);
    q.colour = mix(col, COL_FOG, f * 0.85f);
    quads[(*nq)++] = q;
}

/* As emit(), but carrying a texture and its per-corner UVs. */
static void emit_tex(float p[4][3], uint32_t tint, const uint8_t *tex,
                     int tw, int th, int keyed, const float uv[4][2],
                     const camera *cam,
                     float focal_x, float focal_y, float fog_far,
                     quad *quads, int *nq)
{
    if (*nq >= MAX_QUADS) return;
    quad q;
    memset(&q, 0, sizeof q);
    q.visible = 1;
    float d = 0;
    for (int k = 0; k < 4; k++) {
        float dep = 0.0f;
        if (!project(cam, p[k], focal_x, focal_y, &q.sx[k], &q.sy[k], &dep)) {
            q.visible = 0;
            break;
        }
        /* 1/depth per corner: this is what makes the mapping perspective
           correct.  Without it a wide quad seen at a grazing angle -- which is
           every road quad -- smears its texture into diagonal bands. */
        q.iw[k] = 1.0f / (dep > 1.0f ? dep : 1.0f);
        float ddx = p[k][0] - cam->x, ddy = p[k][1] - cam->y;
        d += sqrtf(ddx * ddx + ddy * ddy);
    }
    if (!q.visible) return;
    q.depth = d * 0.25f - g_quadbias;
    q.colour = tint;
    q.tex = tex;
    q.tw = tw; q.th = th;
    q.tint = tint;
    q.keyed = keyed;
    for (int k = 0; k < 4; k++) {
        q.u[k] = uv[k][0]; q.v[k] = uv[k][1];
    }
    quads[(*nq)++] = q;
}

/*
 * The road's height at a world point: the interpolated profile at the nearest
 * sample.  Used to pin tile paint to the road it is painted on.
 */
static float road_z_at(const road *r, float x, float y)
{
    float best = 1e30f;
    int bi = 0;
    for (int k = 0; k < r->count; k++) {
        float dx = r->centre[k][0] - x, dy = r->centre[k][1] - y;
        float d = dx*dx + dy*dy;
        if (d < best) { best = d; bi = k; }
    }
    return r->centre[bi][2];
}


/*
 * WHAT AM I LOOKING AT?  Finds the piece nearest the centre of the screen and
 * prints the facts that decide how it is drawn: mesh number, layer, the flags,
 * each face's material, and the texture size that material record gives.
 * Guessing which mesh someone means by "the roof above the crowd stands" has
 * cost two rounds; this turns it into a number.
 */
static const scene_object *under_crosshair(const camera *cam, float focal_x,
                                           float focal_y)
{
    if (!g_scene) return NULL;
    /*
     * Of the pieces near the centre of the screen, the one IN FRONT is the one
     * being looked at.  Nearest-to-centre alone picked distant scenery behind
     * the piece of interest, which is how a report of "the crowd" came back as a
     * barrier 1000 units away.
     */
    const scene_object *best = NULL;
    float bestdep = 1e30f;
    for (int o = 0; o < g_scene->count; o++) {
        const scene_object *so = &g_scene->obj[o];
        /* the ground and the road are not what anyone points at */
        if (!so->m || so->tile || so->roadpiece) continue;
        float c[3] = { so->x, so->y, so->z };
        float sx, sy, dep;
        if (!project(cam, c, focal_x, focal_y, &sx, &sy, &dep)) continue;
        float ddx = sx - FB_W * 0.5f, ddy = sy - FB_H * 0.5f;
        if (ddx*ddx + ddy*ddy > 150.0f * 150.0f) continue;
        if (dep < bestdep) { bestdep = dep; best = so; }
    }
    if (!best) {
        /* nothing projected near the centre: fall back to the nearest by angle */
        float bestpx = 1e30f;
        for (int o = 0; o < g_scene->count; o++) {
            const scene_object *so = &g_scene->obj[o];
            if (!so->m) continue;
            float c[3] = { so->x, so->y, so->z };
            float sx, sy, dep;
            if (!project(cam, c, focal_x, focal_y, &sx, &sy, &dep)) continue;
            float ddx = sx - FB_W * 0.5f, ddy = sy - FB_H * 0.5f;
            float d2 = ddx * ddx + ddy * ddy;
            if (d2 < bestpx) { bestpx = d2; best = so; }
        }
    }
    return best;
}

static void describe_under_crosshair(const camera *cam, float focal_x, float focal_y)
{
    const scene_object *so = under_crosshair(cam, focal_x, focal_y);
    if (!so) { printf("nothing under the crosshair\n"); fflush(stdout); return; }
    const mesh *mm = so->m;
    printf("\n--- under the crosshair ---\n"
           "mesh V%d   layer %d   pos (%.0f, %.0f)   height %.0f   %d faces\n"
           "flags: thin %d  billboard(Z) %d  roll %d  tile %d  roadpiece %d  flat %d\n",
           so->v, so->layer, so->x, so->y, so->z, mm->nface,
           so->thin, so->facez, so->roll, so->tile, so->roadpiece, so->flat);
    int shown = 0;
    for (int f = 0; f < mm->nface && shown < 6; f++) {
        int mi = mm->material[f] / 16;
        printf("  face %d: material %d (record %d)  texture %dx%d  size field %d  keyed %d\n",
               f, mm->material[f], mi, g_mat[mi].tw, g_mat[mi].th,
               g_mat[mi].size, g_mat[mi].keyed);
        shown++;
    }
    fflush(stdout);
}

static void render_frame(const camera *cam, const road *r, const sky *s,
                         quad *quads, float focal_x, float focal_y)
{
    sky_paint(cam->yaw, cam->pitch, s, focal_x, focal_y);

    int nq = 0;
    float fog_far = r->width * 32.0f;

    for (int i = 0; i < r->count; i++) {
        int j = (i + 1) % r->count;
        float p[4][3];

        float guv[2][4][2];
        int gtex = (g_groundmat >= 0 && g_mat[g_groundmat].tex);
        for (int side = 0; side < 2; side++) {
            float a = side ? +1.0f : -SKIRT, b = side ? +SKIRT : -1.0f;
            road_point(r, i, a, p[0]); road_point(r, i, b, p[1]);
            road_point(r, j, b, p[2]); road_point(r, j, a, p[3]);
            if (gtex) {
                /*
                 * Top-down projection on world XY: the ground is a single
                 * outward plane, so a planar projection from above is both
                 * correct and continuous from one road section to the next
                 * (local UVs would restart at every sample).
                 */
                for (int k = 0; k < 4; k++) {
                    guv[side][k][0] = p[k][0] / (g_texscale * 4.0f);
                    guv[side][k][1] = p[k][1] / (g_texscale * 4.0f);
                }
                emit_tex(p, 0xFFF0F0F0u, g_mat[g_groundmat].tex,
                         g_mat[g_groundmat].tw, g_mat[g_groundmat].th,
                         g_mat[g_groundmat].keyed, guv[side],
                         cam, focal_x, focal_y, fog_far, quads, &nq);
            } else {
                emit(p, COL_GROUND, cam, focal_x, focal_y, fog_far, quads, &nq);
            }
        }

        road_point(r, i, -1.0f, p[0]); road_point(r, i, +1.0f, p[1]);
        road_point(r, j, +1.0f, p[2]); road_point(r, j, -1.0f, p[3]);
        /*
         * The road surface, textured from the game's own road material.
         *
         * The pieces the game uses for road are flat horizontal tiles, and the
         * dominant material among them is offset 0 -- a smooth grey surface.
         * U runs along the road by arc length (so the texture does not bunch up
         * on tight corners) and V spans exactly 0..1 across the width, which is
         * what the 256x16 strip wants: 256 along the road, 16 across.
         */
        if (g_roadmat >= 0 && g_mat[g_roadmat].tex) {
            /*
             * SWAPPED, per Danik: the texture's WIDTH must span across the road
             * and its 16-tall axis run along the length.  The other way round
             * gave zebra stripes across the road every ~10 m, because the
             * 256-long axis was laying its variation along the direction of
             * travel.  This way the cross-section -- dark at the edges, bright
             * up the middle -- lands across the road, which is what a road
             * texture is drawn for.
             */
            /*
             * SQUARE TEXELS.  U spans the full 256-texel width of the strip
             * across the road, so a texel is (width/256) units across.  V must
             * advance at the same rate or the texture is stretched along the
             * road -- at the old scale each of the 16 rows covered 512 units,
             * a 51:1 aspect, which is what drew long streaks down the road
             * instead of a surface.  One full repeat of 16 rows therefore
             * spans 16 * width/256 = width/16 units.
             */
            /*
             * FOUR cycles across the width, as Danik counts them in the
             * original: the strip tiles four times across the road, not once.
             * With 256 texels per repeat and a repeat of width/4 units, a texel
             * is width/1024 units across; the 16-row axis must then cover
             * 16 * width/1024 = width/64 units per repeat for square texels.
             */
            /*
             * The road uses the SAME per-material texel size as everything
             * else, so the paint and the surface it is painted on share one
             * coordinate system and cannot drift apart.  Material 0's record
             * says size 16 -> 8192 units per repeat across 256 texels = 32
             * units per texel (9.6 cm); the 16-tall axis then repeats every
             * 16 * 32 = 512 units along the road.
             *
             * This replaced a private scale (four cycles of the whole strip
             * across the width, and a 96-unit repeat along it) which made the
             * road 5x finer-grained than the scenery and shimmered.  The two
             * knobs survive as multipliers: OS_ROADUCYC scales the width,
             * OS_ROADVLEN the length.
             */
            float rusp = g_texscale;                 /* units per U repeat */
            {
                int mi = g_roadmat;
                if (mi >= 0 && mi < MAT_MAX && g_mat[mi].size > 0)
                    rusp = (float)g_mat[mi].size * 512.0f * g_texscale / TEXSCALE;
            }
            float rvsp = rusp * 16.0f / 256.0f * g_roadvlen;
            if (rvsp < 1.0f) rvsp = 1.0f;
            /*
             * THE ROAD IS THREE STRIPS, NOT ONE PLUS AN OVERLAY.
             *
             * The edge lines used to be separate quads lying exactly on top of
             * the road surface, which put two coplanar surfaces in the same
             * depth sort -- and the sort has to choose, so on long turns the
             * line kept dipping through the tarmac (Danik, twice).  A lift and
             * then a depth-sequence tiebreak were both attempts to win an
             * argument that should not be happening: the fix is that the strips
             * do not overlap at all.  The road surface spans -0.86..+0.86 of the
             * half-width and the two edge strips take -0.98..-0.86 and
             * +0.86..+0.98.  Three non-overlapping quads tile the road, and
             * nothing is coplanar with anything.
             */
            float uacross = g_roaducyc * r->width / rusp;
            float uv[4][2];
            float usurf = 0.0f, usurf2 = uacross;
            uv[0][0] = usurf;            uv[0][1] = r->arc[i] / rvsp;
            uv[1][0] = usurf2;           uv[1][1] = r->arc[i] / rvsp;
            uv[2][0] = usurf2;           uv[2][1] = r->arc[j] / rvsp;
            uv[3][0] = usurf;            uv[3][1] = r->arc[j] / rvsp;
            road_point(r, i, -0.86f, p[0]); road_point(r, i, +0.86f, p[1]);
            road_point(r, j, +0.86f, p[2]); road_point(r, j, -0.86f, p[3]);
            emit_tex(p, 0xFFFFFFFFu, g_mat[g_roadmat].tex,
                     g_mat[g_roadmat].tw, g_mat[g_roadmat].th,
                     g_mat[g_roadmat].keyed, uv,
                     cam, focal_x, focal_y, fog_far, quads, &nq);
        } else {
            emit(p, ((i / 6) & 1) ? COL_ROAD_A : COL_ROAD_B,
                 cam, focal_x, focal_y, fog_far, quads, &nq);
        }

        for (int side = 0; side < 2; side++) {
            float a = side ? 0.86f : -0.98f;
            float b = side ? 0.98f : -0.86f;
            road_point(r, i, a, p[0]); road_point(r, i, b, p[1]);
            road_point(r, j, b, p[2]); road_point(r, j, a, p[3]);
            emit(p, COL_EDGE, cam, focal_x, focal_y, fog_far, quads, &nq);
        }
    }

    /* scenery: transform each .3DX face into world space and emit it */
    if (g_scene) {
        const float lx = -0.35f, ly = 0.45f, lz = -0.82f;
        const int dbg = g_objdump;
        for (int o = 0; o < g_scene->count; o++) {
            const scene_object *so = &g_scene->obj[o];
            const mesh *mm = so->m;
            /* Terrain tiles are the road and the ground, which this port builds
               from the splines instead; drawing them too would double-draw the
               surface.  They are still in the scene because the road's own
               HEIGHT comes from them -- see the height pass in main().  Note
               `tile`, not `flat`: a flat piece that is NOT at road level is a
               bridge deck and must be drawn. */
            if (!mm) continue;
            /*
             * A TERRAIN TILE is not thrown away -- its SURFACE is, because the
             * port builds that from the splines.  The rest of the tile is kept
             * when it is a marking.
             *
             * That distinction is worth the trouble: the road tiles carry
             * painted markings as their own faces.  In V0 the 32 asphalt faces
             * (material 0) are joined by faces of material 147 that are FLAT AND
             * COPLANAR with them -- Y = 0, X -3079..-2951, i.e. a 128-unit
             * (38 cm) painted strip along the outer edge of the road.  In V23
             * the same material sits at Y 64..128, a raised kerb.  So 147/148
             * are the road's own paint and kerbs, and skipping the whole tile
             * threw the markings away with the surface.
             */
            if (so->tile && !g_tilepaint) continue;
            /* a pure ground tile is the ground, which the port builds already;
               a ROAD piece keeps going, because its paint and kerbs are on it */
            if (so->tile && !so->roadpiece) continue;
            /* any mesh that carries the road surface material is a road piece,
               whatever its height -- the paint rule below needs that */
            const int roadmesh = mesh_uses_material(mm, g_roadmat);
            const int is_road = so->tile || so->roadpiece || roadmesh;
            if (dbg > 0) {
                float ddx = so->x - cam->x, ddy = so->y - cam->y;
                float d = sqrtf(ddx*ddx + ddy*ddy);
                if (d < (float)dbg)
                    printf("obj V%3d  dist %8.0f  pos %.0f,%.0f  z %.0f  E %6d"
                           "  yaw %6.1f  faces %d\n",
                           so->v, d, so->x, so->y, so->z, so->e,
                           so->yaw * 180.0f / (float)M_PI, mm->nface);
            }
            /*
             * so->yaw aligns the piece's local Z with the road tangent, set at
             * load time (see main()).  It used to billboard toward the camera,
             * which is right for a flat sprite but wrong for track geometry:
             * billboarding a 4 m wall that should run along the road turns it
             * across the road instead.
             */
            float ryaw = so->yaw;
            float rot_c = cosf(ryaw), rot_s = sinf(ryaw);
            for (int fi = 0; fi < mm->nface; fi++) {
                int nc = mm->ncorner[fi];
                if (nc < 3) continue;
                /*
                 * On a terrain tile, draw only what is NOT the surface: the
                 * road material and the ground material are already drawn by
                 * the spline road, everything else on the tile is paint or a
                 * kerb and belongs on top of it.
                 */
                /*
                 * The road surface and the ground are drawn by the spline
                 * builder for the whole lap, so any face carrying those
                 * materials is skipped wherever it appears -- not only on the
                 * flat tiles.  The tiles that carry a kerb (V23 and the
                 * -family) are not "flat", and skipping only flat ones left
                 * their asphalt faces z-fighting with the road.
                 */
                {
                    int mi = mm->material[fi] / 16;
                    /*
                     * The asphalt of a road piece AT ROAD LEVEL is the surface
                     * the port builds from the splines.  An ELEVATED road piece
                     * is an overpass deck and keeps its asphalt -- it is a real
                     * structure, not the surface under the car.
                     */
                    if ((mi == g_roadmat || mi == g_groundmat) && so->roadpiece)
                        continue;
                    /*
                     * Flat paint on a road tile is skipped unless OS_TILEPAINT
                     * is on -- its textures are the undecoded tile sheets, and
                     * splotches across the tarmac are worse than a clean road.
                     * The RAISED faces (the kerbs) stay either way: they are
                     * geometry the road needs, not paint.  Flat is measured off
                     * the face's own vertices, so this does not depend on any
                     * material number.
                     */
                    if (roadmesh && !g_tilepaint) {
                        float fy0 = 1e30f, fy1 = -1e30f;
                        for (int k = 0; k < nc; k++) {
                            int vi = mm->idx[fi][k];
                            if (vi < 0) continue;
                            float y = mm->vert[vi].y;
                            if (y < fy0) fy0 = y;
                            if (y > fy1) fy1 = y;
                        }
                        if (fy1 - fy0 <= 8.0f) continue;
                    }
                }
                float p[4][3];
                for (int k = 0; k < 4; k++) {
                    int kk = (k < nc) ? k : nc - 1;
                    int vi = mm->idx[fi][kk];
                    if (vi < 0) { p[k][0] = p[0][0]; p[k][1] = p[0][1];
                                  p[k][2] = p[0][2]; continue; }
                    /*
                     * .3DX LOCAL AXES ARE NOT WORLD AXES.
                     * In local space Y is UP and Z is depth: the car mesh is
                     * 1476 long x 753 wide x 450 tall, and the roadside sprites
                     * are flat in Z (AZGIRL is 502 x 1340 x 0) -- i.e. vertical
                     * billboards.  Mapping local Z to world height lays every
                     * sprite flat on the ground, which is exactly what it did.
                     *
                     * Sprites also face +/-Z locally, so rotate them about the
                     * vertical to face the camera -- without that a flat quad
                     * viewed edge-on is invisible.
                     */
                    float lx = mm->vert[vi].x * so->scale;
                    float ly = mm->vert[vi].y * so->scale;   /* height */
                    float lz = mm->vert[vi].z * so->scale;   /* depth  */
                    if (so->roll) {
                        /*
                         * ROAD PAINT LIES DOWN.
                         *
                         * The lane-marking meshes are thin ribbons authored
                         * standing up -- V44 is 4136 long x 258 TALL x 0 thick --
                         * and drawn as they come they are a 0.8 m wall along the
                         * lane, seen edge-on from a chase camera: invisible.
                         * Laid down they are what they obviously are, a 0.8 m
                         * wide painted stripe, and their keyed texture supplies
                         * the gaps between dashes.
                         *
                         * 90 degrees of roll: local Y (the ribbon's height in
                         * the mesh) becomes the across-road direction, and local
                         * Z (its thickness axis) becomes up.
                         */
                        p[k][0] = so->x + lx * rot_c - ly * rot_s;
                        p[k][1] = so->y + lx * rot_s + ly * rot_c;
                        p[k][2] = so->z + lz;
                        continue;
                    }
                    p[k][0] = so->x + lx * rot_c - lz * rot_s;
                    p[k][1] = so->y + lx * rot_s + lz * rot_c;
                    /*
                     * so->z is the record's +0x0C field, which is the object's
                     * own world height (see scene.h) -- it is NOT an offset to
                     * be added to the road.  Local Y is added on top of it, so
                     * a 2 m prop stands on the ground, not straddling it.
                     */
                    p[k][2] = so->z + ly + (so->tile ? g_tilelift : 0.0f);
                }
                /*
                 * TILE PAINT FOLLOWS THE ROAD, FACE BY FACE.
                 *
                 * A road tile is a rigid quad at ONE height, while the port's
                 * road is a smoothed ribbon; where the road slopes, the tile's
                 * paint pokes through on one side and sinks under on the other,
                 * and which side that is changes with the bend -- which is
                 * exactly what Danik reported ("never visible on both sides...
                 * when the road bends it switches").  The data is symmetric:
                 * V0 has 4 marking faces fully left and 4 fully right of the
                 * same material, so both should always show.
                 *
                 * Snapping each face to the road's height at the face's own
                 * centre fixes it for the whole lap, on both sides.
                 */
                if (is_road) {
                    float cx = 0.25f * (p[0][0] + p[1][0] + p[2][0] + p[3][0]);
                    float cy = 0.25f * (p[0][1] + p[1][1] + p[2][1] + p[3][1]);
                    float rz = road_z_at(r, cx, cy) + g_tilelift;
                    for (int k = 0; k < 4; k++) p[k][2] = rz;
                    g_quadseq = 1;
                }
                /*
                 * Light with the face's STORED normal, not one derived from the
                 * winding: it is exact data read from the record (int16 at
                 * +0x12/+0x16/+0x1a, 16384 = 1.0) and it cannot be thrown off by
                 * a collapsed or duplicated vertex.  Falls back to the winding
                 * cross-product if the stored normal is empty.
                 *
                 * The normal is in .3DX LOCAL space, so it takes the same axis
                 * swap as the vertices: local X and Z are horizontal, local Y is
                 * up.
                 */
                float nlx = mm->normal[fi].x, nly = mm->normal[fi].y,
                      nlz = mm->normal[fi].z;
                float nx, ny, nz;
                if (nlx == 0.0f && nly == 0.0f && nlz == 0.0f) {
                    float ux = p[1][0]-p[0][0], uy = p[1][1]-p[0][1],
                          uz = p[1][2]-p[0][2];
                    float vx = p[2][0]-p[0][0], vy = p[2][1]-p[0][1],
                          vz = p[2][2]-p[0][2];
                    nx = uy*vz-uz*vy; ny = uz*vx-ux*vz; nz = ux*vy-uy*vx;
                } else if (so->roll) {
                    /* same roll as the vertices: the mesh's thickness axis is
                       the one that ends up pointing up */
                    nx = nlx * rot_c - nly * rot_s;
                    ny = nlx * rot_s + nly * rot_c;
                    nz = nlz;
                } else {
                    nx = nlx * rot_c - nlz * rot_s;
                    ny = nlx * rot_s + nlz * rot_c;
                    nz = nly;
                }
                float nl = sqrtf(nx*nx+ny*ny+nz*nz);
                if (nl < 1e-6f) continue;
                float l = (nx*lx + ny*ly + nz*lz) / nl;
                if (l < 0) l = -l;
                /* Ambient-heavy: the ramp bottoms out at black, and a wall
                   facing across the light would otherwise shade to nothing. */
                l = 0.50f + 0.50f * l;
                /*
                 * The face's material: an offset into MAPn.MLB's 16-byte table
                 * (see mesh.h).  It gives the texture and the pair of palette
                 * indices used for shading.
                 */
                uint32_t lit = 0, dark = 0;
                const uint8_t *tex = NULL;
                int tw = 0, th = 0;
                int mi = mm->material[fi] / 16;
                if (mi >= 0 && mi < MAT_MAX && g_mat[mi].size) {
                    int ca = g_mat[mi].colA & 0xFF, cb = g_mat[mi].colB & 0xFF;
                    lit  = g_pal[ca];
                    dark = g_pal[cb];
                    tex  = g_mat[mi].tex;
                    tw   = g_mat[mi].tw;
                    th   = g_mat[mi].th;
                } else {
                    lit = dark = 0xFF404040u;      /* no material: neutral grey */
                }
                /* Interpolate the material's own shaded colour towards its own
                   lit colour.  That is what the pair is for. */
                float r0 = (float)((dark >> 16) & 0xFF);
                float g0 = (float)((dark >>  8) & 0xFF);
                float b0 = (float)( dark        & 0xFF);
                float r1 = (float)((lit  >> 16) & 0xFF);
                float g1 = (float)((lit  >>  8) & 0xFF);
                float b1 = (float)( lit         & 0xFF);
                uint32_t col = 0xFF000000u
                    | ((uint32_t)(r0 + (r1 - r0) * l) << 16)
                    | ((uint32_t)(g0 + (g1 - g0) * l) << 8)
                    |  (uint32_t)(b0 + (b1 - b0) * l);

                if (tex) {
                    /*
                     * UVs.  The face record carries none, so derive them from
                     * the surface itself: the face normal's DOMINANT local axis
                     * is the surface normal, and the other two axes are the
                     * texture plane.  That keeps every face of the same
                     * orientation tiled the same way -- picking the two widest
                     * axes per face instead let neighbouring faces of one wall
                     * choose different pairs, and the texture jumped between
                     * them, which is what "scrambled" was.
                     *
                     * TEXSCALE units per repeat is still a choice, not decoded
                     * data.
                     */
                    /*
                     * UVs come from WORLD coordinates, not the object's local
                     * ones.  Two reasons, and the second is the important one:
                     *
                     *  - the face record carries no texture coordinates at all,
                     *    so they have to be derived from the geometry;
                     *  - the track is assembled from many pieces that abut each
                     *    other.  Local-space UVs restart the texture at every
                     *    piece's own origin, so a road marking or a brick course
                     *    jumps at every join.  A world-space projection is
                     *    continuous across the whole track, which is what makes
                     *    a tiled texture read as one surface.
                     *
                     * The plane is the two world axes perpendicular to the
                     * surface, so a floor gets X/Y and a wall gets the wall's
                     * own two axes.
                     */
                    int dax = 0;
                    float an[3] = { nx, ny, nz };
                    for (int ax = 1; ax < 3; ax++)
                        if (fabsf(an[ax]) > fabsf(an[dax])) dax = ax;
                    int ta = -1, tb = -1;
                    for (int ax = 0; ax < 3; ax++) {
                        if (ax == dax) continue;
                        if (ta < 0) ta = ax; else tb = ax;
                    }
                    /* U (the 256-wide axis) goes on the longer of the two. */
                    float ea = 0.0f, eb = 0.0f;
                    float loc[4][3];
                    for (int k = 0; k < 4; k++) {
                        int kk = (k < nc) ? k : nc - 1;
                        int vi = mm->idx[fi][kk];
                        loc[k][0] = loc[k][1] = loc[k][2] = 0.0f;
                        if (vi >= 0) {
                            loc[k][0] = mm->vert[vi].x;
                            loc[k][1] = mm->vert[vi].y;
                            loc[k][2] = mm->vert[vi].z;
                        }
                        float da = fabsf(loc[k][ta] - loc[0][ta]);
                        float db = fabsf(loc[k][tb] - loc[0][tb]);
                        if (da > ea) ea = da;
                        if (db > eb) eb = db;
                    }
                    int t1 = ta, t2 = tb;
                    if (eb > ea) { t1 = tb; t2 = ta; }
                    /*
                     * Two possible origins, switchable with `u` so they can be
                     * compared without a rebuild.
                     *
                     *   world  -- continuous across abutting pieces, but every
                     *             object shares one phase;
                     *   local  -- each object frames itself, but the pattern
                     *             restarts at every join.
                     *
                     * Danik found world clearly better on the crowd stands, so
                     * that is the default.
                     */
                    /*
                     * Per-object texture scale from the block header's +0x18,
                     * normalised against the value 19 of 20 objects carry so
                     * the global knob still trims it.
                     */
                    float oscale = 1.0f;
                    if (g_useobjscale && mm->texscale > 0)
                        oscale = (float)mm->texscale / 512.0f;
                    /*
                     * THE TEXTURE SCALE IS IN THE MATERIAL RECORD -- it is not a
                     * number to be guessed.
                     *
                     * The material record's first field is 16 or 64, and
                     * `16 * 512 = 8192` is EXACTLY the world-units-per-repeat
                     * that Danik arrived at by eye for the whole track.  That
                     * agreement is what makes this the reading rather than
                     * another knob: the field is the size of one texture repeat
                     * in 512-unit steps, so a size-16 material tiles every 8192
                     * units (256 texels of 32 units each = 9.6 cm per texel) and
                     * a size-64 material every 32768.
                     *
                     * A single global scale can therefore never fit everything:
                     * 8 of track 1's material set is size 64 and the rest 16, so
                     * one global value is 8x wrong for a quarter of the track's
                     * surfaces -- which is exactly the "no matter how I scale it
                     * it does not fit" that this is here to end.
                     *
                     * The 16-tall axis gets the same texel size, so the texels
                     * stay square: 16 texels x (2 * size) units.
                     */
                    float uscale = g_texscale, vscale = TEXSCALEV;
                    {
                        int mi2 = mm->material[fi] / 16;
                        if (mi2 >= 0 && mi2 < MAT_MAX && g_matscale_mul[mi2] > 0.0f) {
                            uscale *= g_matscale_mul[mi2];
                            vscale  = uscale * 16.0f / 256.0f;
                        }
                    }
                    if (g_matscale) {
                        int mi = mm->material[fi] / 16;
                        if (mi >= 0 && mi < MAT_MAX && g_mat[mi].size > 0) {
                            float base = (float)g_mat[mi].size * 512.0f;
                            uscale = base * g_texscale / TEXSCALE;
                            vscale = uscale * 16.0f / 256.0f;
                        }
                    }
                    float uv[4][2];
                    /*
                     * One texture repeat across the face, for the pieces that are
                     * a single image: billboards and panels.  The face's own
                     * extent along the two texture axes sets the scale, so the
                     * image is neither cropped nor repeated -- which is the
                     * "scaling" Danik asked for.
                     */
                    float fext1 = 0.0f, fext2 = 0.0f;
                    if (g_facefit && so->thin) {
                        float lo1 = 1e30f, hi1 = -1e30f, lo2 = 1e30f, hi2 = -1e30f;
                        for (int k = 0; k < 4; k++) {
                            float v1 = loc[k][t1], v2 = loc[k][t2];
                            if (v1 < lo1) lo1 = v1;
                            if (v1 > hi1) hi1 = v1;
                            if (v2 < lo2) lo2 = v2;
                            if (v2 > hi2) hi2 = v2;
                        }
                        fext1 = (hi1 > lo1) ? (hi1 - lo1) : 1.0f;
                        fext2 = (hi2 > lo2) ? (hi2 - lo2) : 1.0f;
                        lo1 = lo1; lo2 = lo2;
                    }
                    int wuv = g_uvworld && !so->thin;
                    for (int k = 0; k < 4; k++) {
                        float a1, a2;
                        if (fext1 > 0.0f) {
                            a1 = loc[k][t1] / fext1 * (uscale * oscale);
                            a2 = loc[k][t2] / fext2 * (vscale * oscale);
                        } else {
                            a1 = (wuv ? p[k][t1] : loc[k][t1]);
                            a2 = (wuv ? p[k][t2] : loc[k][t2]);
                        }
                        float A = a1 / (uscale * oscale);
                        float B = a2 / (vscale * oscale);
                        /*
                         * Which way the 256-wide strip lies on the surface.
                         * `o` cycles it; a 90-degree error cannot be fixed by
                         * any scale or offset, only by rotating the mapping.
                         */
                        int rot = g_matrot[mm->material[fi] / 16];
                        if (g_texrot) rot = (rot + g_texrot) % 360;
                        float u, v;
                        switch (rot) {
                        case 90:  u =  B; v = -A; break;
                        case 180: u = -A; v = -B; break;
                        case 270: u = -B; v =  A; break;
                        default:  u =  A; v =  B; break;
                        }
                        /*
                         * A BILLBOARD'S TEXTURE IS CROSSED OVER.
                         *
                         * Danik: "The crowd texture itself needs 90d CW
                         * rotation."  The crowd stands are flat in Z (they face
                         * the road); the railing panels, which look right as
                         * they are, are flat in X.  So the distinction is not
                         * "thin" but WHICH way the piece faces: a Z-facing
                         * billboard takes its texture with the 256-wide axis
                         * running down and the 16-tall axis across.
                         */
                        if (g_billboardrot && so->facez) {
                            float uu = v, vv = -u;
                            u = uu; v = vv;
                        }
                        uv[k][0] = u + g_texoffu;
                        uv[k][1] = v + g_texoffv;
                    }
                    /*
                     * Modulate the texture by the LIGHT LEVEL only, as a grey
                     * scale.  The texture already carries the surface's colour,
                     * and the material's colourA/colourB pair is a pair of
                     * palette indices for *flat* shading -- multiplying the two
                     * together double-darkens, which is what the first attempt
                     * did (the pair is often near-black, like (11,0,0)).
                     */
                    float tf = 0.45f + 0.55f * l;
                    uint32_t v = (uint32_t)(255.0f * tf);
                    if (v > 255) v = 255;
                    uint32_t tint = 0xFF000000u | (v << 16) | (v << 8) | v;
                    emit_tex(p, tint, tex, tw, th, g_mat[mi].keyed, uv,
                             cam, focal_x, focal_y, fog_far, quads, &nq);
                } else {
                    emit(p, col, cam, focal_x, focal_y, fog_far, quads, &nq);
                }
                g_quadseq = 0;
            }
        }
    }

    qsort(quads, (size_t)nq, sizeof(quad), cmp_depth);
    for (int i = 0; i < nq; i++) quad_fill(&quads[i], quads[i].colour);
}

static void write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", FB_W, FB_H);
    for (int i = 0; i < FB_W * FB_H; i++) {
        uint32_t c = fb[i];
        unsigned char rgb[3] = { (unsigned char)(c >> 16),
                                 (unsigned char)(c >> 8),
                                 (unsigned char)c };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("wrote %s\n", path);
}

int main(int argc, char **argv)
{
    const char *data  = (argc > 1) ? argv[1] : "build/dec/MAP1";
    int track         = (argc > 2) ? atoi(argv[2]) : 1;
    const char *shot  = (argc > 3 && strcmp(argv[3], "--shot") == 0)
                        ? argv[4] : NULL;
    float t0          = (argc > 5) ? (float)atof(argv[5]) : 0.0f;
    char gfxdir[512];
    snprintf(gfxdir, sizeof gfxdir, "build/dec/MAP%dL", track);

    char path[512];
    spline left, right;
    snprintf(path, sizeof path, "%s/MAP%dSLA.SPL", data, track);
    if (spl_load_edges(&left, &right, path) < 0) return 1;
    if (left.count < 4) { fprintf(stderr, "no road edges\n"); return 1; }

    road *r = calloc(1, sizeof(road));
    road_build(r, &left, &right);

    /*
     * ELEVATION.  The road ribbon is built from MAPnSLA.SPL, whose Z is a
     * constant 2 on every node -- it carries no vertical information at all, so
     * the road came out dead flat while the original has climbs, dips and
     * crests.  MAPnCAM.SPL (14 nodes, exactly one per SLA edge pair) DOES vary:
     * -3504 to +4501 on track 1.  That is the profile.
     *
     * It is applied by NEAREST XY rather than by index, so it does not matter
     * whether CAM's chain runs the same direction as the SLA pairs, or starts
     * at a different point on the loop.
     */
    {
        static spline cam;
        static float cx[8192], cy[8192], cz[8192];
        int nc = 0;
        snprintf(path, sizeof path, "%s/MAP%dCAM.SPL", data, track);
        if (spl_load_chained(&cam, path) == 0 && cam.count >= 4) {
            int n = cam.count;
            for (int i = 0; i < n && nc < 8192; i++) {
                const spl_point *a = &cam.pt[i];
                const spl_point *b = &cam.pt[(i + 1) % n];
                for (int q = 0; q < 32 && nc < 8192; q++) {
                    float t = (float)q / 32.0f;
                    cx[nc] = a->x + (b->x - a->x) * t;
                    cy[nc] = a->y + (b->y - a->y) * t;
                    cz[nc] = a->z + (b->z - a->z) * t;
                    nc++;
                }
            }
            for (int i = 0; i < r->count; i++) {
                float best = 1e30f;
                int bi = 0;
                for (int j = 0; j < nc; j++) {
                    float ddx = cx[j] - r->centre[i][0];
                    float ddy = cy[j] - r->centre[i][1];
                    float d = ddx * ddx + ddy * ddy;
                    if (d < best) { best = d; bi = j; }
                }
                float z = cz[bi];
                r->centre[i][2] += z;
                r->left[i][2]   += z;
                r->right[i][2]  += z;
            }
            int zlo = cam.pt[0].z, zhi = cam.pt[0].z;
            for (int i = 1; i < n; i++) {
                if (cam.pt[i].z < zlo) zlo = cam.pt[i].z;
                if (cam.pt[i].z > zhi) zhi = cam.pt[i].z;
            }
            printf("elevation from MAP%dCAM.SPL: %d nodes, Z %d..%d\n",
                   track, n, zlo, zhi);
        } else {
            fprintf(stderr, "note: no elevation spline at %s\n", path);
        }
    }

    sky sk = { 0, 0, NULL };
    snprintf(path, sizeof path, "build/sky/MAP%d.sky", track);
    if (sky_load(&sk, path) != 0)
        fprintf(stderr, "note: no backdrop at %s\n", path);

    const char *tsenv = getenv("OS_TEXSCALE");
    if (tsenv) {
        g_texscale = (float)atof(tsenv);
        const char *tv = getenv("OS_TEXSCALEV");
        g_texscalev = tv ? (float)atof(tv)
                         : g_texscale * 16.0f / 256.0f;
    }
    if (getenv("OS_TEXOFFU")) g_texoffu = (float)atof(getenv("OS_TEXOFFU"));
    if (getenv("OS_TEXOFFV")) g_texoffv = (float)atof(getenv("OS_TEXOFFV"));
    if (getenv("OS_TEXROT"))  g_texrot  = atoi(getenv("OS_TEXROT"));
    if (getenv("OS_UVLOCAL")) g_uvworld = 0;
    if (getenv("OS_OBJSCALE")) g_useobjscale = 1;
    if (getenv("OS_ROADMAT"))  g_roadmat = atoi(getenv("OS_ROADMAT"));
    if (getenv("OS_ROADW"))    g_roadwidth = (float)atof(getenv("OS_ROADW"));
    if (getenv("OS_GROUNDMAT")) g_groundmat = atoi(getenv("OS_GROUNDMAT"));
    if (getenv("OS_ONROAD"))   g_onroad = atoi(getenv("OS_ONROAD"));
    if (getenv("OS_ROADZ"))    g_roadz  = atoi(getenv("OS_ROADZ"));
    if (getenv("OS_ROLL"))     g_roll   = atoi(getenv("OS_ROLL"));
    if (getenv("OS_MATSCALE")) g_matscale = atoi(getenv("OS_MATSCALE"));
    if (getenv("OS_TILEPAINT")) g_tilepaint = atoi(getenv("OS_TILEPAINT"));
    if (getenv("OS_LAYERROAD")) g_layerroad = atoi(getenv("OS_LAYERROAD"));
    if (getenv("OS_BILLROT"))   g_billboardrot = atoi(getenv("OS_BILLROT"));
    if (getenv("OS_FACEFIT"))   g_facefit = atoi(getenv("OS_FACEFIT"));
    if (getenv("OS_TILELIFT")) g_tilelift = (float)atof(getenv("OS_TILELIFT"));
    if (getenv("OS_OBJZ"))     g_objz   = atoi(getenv("OS_OBJZ"));
    if (getenv("OS_OBJDUMP"))  g_objdump = atoi(getenv("OS_OBJDUMP"));
    if (getenv("OS_ROADUCYC")) g_roaducyc = (float)atof(getenv("OS_ROADUCYC"));
    if (getenv("OS_ROADVLEN")) g_roadvlen = (float)atof(getenv("OS_ROADVLEN"));
    printf("texture scale: U %.0f  V %.0f units per repeat\n",
           g_texscale, g_texscalev);

    snprintf(path, sizeof path, "%s/MAP%d.COL", gfxdir, track);
    pal_load(path);
    snprintf(path, sizeof path, "%s/MAP%d.MLB", gfxdir, track);
    mlb_load(path);

    /* only meaningful once the materials are in */
    if (g_roadmat >= 0 && g_mat[g_roadmat].tex)
        printf("road material: index %d (%dx%d)\n",
               g_roadmat, g_mat[g_roadmat].tw, g_mat[g_roadmat].th);
    else
        fprintf(stderr, "note: no road material %d -- road stays flat\n", g_roadmat);

    printf("track %d: %d edge pairs -> %d road quads, width %.0f units"
           " (x%.2f of the raw spline width), backdrop %dx%d\n",
           track, left.count, r->count, r->width, g_roadwidth, sk.w, sk.h);

    static scene sc;
    int nobj = scene_load(&sc, data, gfxdir, track);

    /*
     * THE ROAD'S OWN ELEVATION COMES FROM THE GAME'S TILES, not from the CAM
     * spline.
     *
     * MAPnCAM.SPL looks like the camera path, and its Z is a smooth curve -- but
     * it is NOT the road.  The road is built out of flat tiles (V0, V1, V4, V5,
     * V23, ...) whose record field +0x0C is a height quantised to 512 units, and
     * a tile that is visibly ON the road can sit 2000 units below the CAM curve
     * (measured: MAP1, V23 at lap 52.5%, on-road, 2235 units apart).  Since the
     * scenery is placed at that same field, building the road from anything else
     * leaves every object floating or sunk by however far the two curves differ.
     *
     * So the road's elevation is built from the ROAD TILES -- a tile being any
     * mesh that carries the road material, flat or not (choosing by "flat mesh"
     * misses the whole staircase, because most of them carry a 256-unit kerb).
     *
     * IT MUST BE INTERPOLATED, NOT SNAPPED.  The tile field is quantised to 512
     * units, so assigning each sample its nearest tile's height gives the road a
     * visible staircase -- Danik, correctly: "the visible steps are wrong, it
     * should be smooth".  So each tile contributes one (arc, height) key at the
     * point of the lap where it sits, and the road is the piecewise-linear curve
     * through those keys.  The authored breakdown into tiles is then a property
     * of the data, not a shape in the road.
     *
     * OS_ROADZ=0 puts the CAM curve back for an A/B.
     */
    if (g_roadz == 1) {
        static float karc[512], kh[512];
        int nk = 0, ntiles = 0;
        for (int i = 0; i < sc.count && nk < 512; i++) {
            if (!sc.obj[i].m || !mesh_uses_material(sc.obj[i].m, g_roadmat))
                continue;
            ntiles++;
            /* where on the lap this tile sits: the nearest road sample */
            float best = 1e30f;
            int bi = 0;
            for (int k = 0; k < r->count; k++) {
                float dx = r->centre[k][0] - sc.obj[i].x;
                float dy = r->centre[k][1] - sc.obj[i].y;
                float d = dx*dx + dy*dy;
                if (d < best) { best = d; bi = k; }
            }
            karc[nk] = r->arc[bi];
            kh[nk]   = (float)sc.obj[i].e;
            nk++;
        }
        /* sort by arc length (insertion sort: 102 keys on track 1) */
        for (int a = 1; a < nk; a++) {
            float ka = karc[a], hh = kh[a];
            int b = a - 1;
            while (b >= 0 && karc[b] > ka) {
                karc[b+1] = karc[b]; kh[b+1] = kh[b]; b--;
            }
            karc[b+1] = ka; kh[b+1] = hh;
        }
        /* merge keys that land on top of each other (the two half-road tiles of
           one piece sit at the same place); averaging them keeps the curve from
           stepping where the data is only doubled */
        int m = 0;
        for (int a = 0; a < nk; a++) {
            if (m > 0 && karc[a] - karc[m-1] < 900.0f) {
                kh[m-1] = 0.5f * (kh[m-1] + kh[a]);
                karc[m-1] = 0.5f * (karc[m-1] + karc[a]);
            } else {
                karc[m] = karc[a]; kh[m] = kh[a]; m++;
            }
        }
        nk = m;

        /* the lap closes: the last sample connects back to the first */
        float total = r->arc[r->count - 1];
        {
            float dx = r->centre[0][0] - r->centre[r->count-1][0];
            float dy = r->centre[0][1] - r->centre[r->count-1][1];
            total += sqrtf(dx*dx + dy*dy);
        }

        if (nk >= 2) {
            for (int k = 0; k < r->count; k++) {
                float a = r->arc[k];
                int j = 0;
                while (j < nk && karc[j] < a) j++;
                float a0, h0, a1, h1;
                if (j == 0) {
                    a0 = karc[nk-1] - total; h0 = kh[nk-1];
                    a1 = karc[0];            h1 = kh[0];
                } else if (j >= nk) {
                    a0 = karc[nk-1];          h0 = kh[nk-1];
                    a1 = karc[0] + total;     h1 = kh[0];
                } else {
                    a0 = karc[j-1]; h0 = kh[j-1];
                    a1 = karc[j];   h1 = kh[j];
                }
                float t = (a1 > a0) ? (a - a0) / (a1 - a0) : 0.0f;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                float hz = h0 + (h1 - h0) * t;
                r->centre[k][2] = hz;
                r->left[k][2]   = hz;
                r->right[k][2]  = hz;
            }
            /* soften the corners where the keys change slope, or the crests read
               as creases -- two passes of a 3-tap average */
            for (int pass = 0; pass < 2; pass++) {
                float prev = r->centre[0][2];
                for (int k = 1; k + 1 < r->count; k++) {
                    float cur = r->centre[k][2];
                    float sm = 0.25f * prev + 0.5f * cur + 0.25f * r->centre[k+1][2];
                    prev = cur;
                    r->centre[k][2] = sm;
                    r->left[k][2] = sm;
                    r->right[k][2] = sm;
                }
            }
        }
        printf("road height: interpolated through %d road tiles (%d keys, "
               "record +0x0C), not the CAM spline\n", ntiles, nk);
        if (getenv("OS_ROADDUMP")) {
            int step = atoi(getenv("OS_ROADDUMP"));
            if (step < 1) step = 1;
            for (int k = 0; k < r->count; k += step)
                printf("road sample %4d  (%.0f,%.0f)  z %.0f\n",
                       k, r->centre[k][0], r->centre[k][1], r->centre[k][2]);
        }
    }

    /*
     * MAPn.MMP carries two classes and they must not be mixed up.  Layer 0 is
     * the on-road class -- measured 53% inside the road, median 0.91 half
     * widths.  Layer 1 is the roadside scenery: 8% inside, median 3.80.  The
     * 7th argument selects: 0, 1, or -1 for both.  Default 1, because layer 0
     * on its own is what looked like "boxes in the middle of the road".
     */
    int want_layer = (argc > 7) ? atoi(argv[7]) : 1;
    if (want_layer == 2) nobj = 0;      /* 2 = draw no scenery at all (A/B test) */
    g_scene = nobj ? &sc : NULL;
    if (nobj) {
        /*
         * Keep the selected layer, drop the objects that land on the road
         * surface itself, and drop each survivor onto the road.
         *
         * The on-road records are NOT a parse error -- the layout is proven
         * (scene.h) and every record sits in the grid cell its own coordinates
         * select, 809 of 809.  They are a series propagated along the lap (V is
         * small, E negative, and T steps by a constant 57 between neighbours),
         * i.e. road pieces and start-line furniture, which belong on the
         * centreline by construction.  The original draws no scenery there, so
         * neither do we; `want_layer` 3 keeps them for inspection.
         *
         * The half-width is taken PER ROAD SAMPLE, not as a global mean: it
         * varies 2202..2919 units around track 1 and the mean would misjudge
         * the narrow sections.
         */
        int kept = 0, onroad = 0;
        for (int i = 0; i < sc.count; i++) {
            /*
             * THE ROAD IS NOT SCENERY, so the layer filter must not touch it.
             *
             * Every mesh that carries the marking materials (147/148) is in
             * LAYER 0 -- V0 x40, V23 x14, V71 x18 and the rest of the road
             * family -- and layer 0 is exactly what the default view excludes,
             * because it also holds the start-line furniture that once looked
             * like "boxes in the middle of the road".  So the markings the
             * original paints on the tarmac were filtered out before anything
             * could draw them.  A piece of road surface is let through either
             * way; scenery still obeys the layer filter.
             */
            int isroad0 = (sc.obj[i].m &&
                           mesh_uses_material(sc.obj[i].m, g_roadmat)) ? 1 : 0;
            if ((want_layer == 0 || want_layer == 1) &&
                sc.obj[i].layer != want_layer && !(g_layerroad && isroad0))
                continue;
            float best = 1e30f;
            int bi = 0;
            for (int k = 0; k < r->count; k++) {
                float dx = r->centre[k][0] - sc.obj[i].x;
                float dy = r->centre[k][1] - sc.obj[i].y;
                float d = dx*dx + dy*dy;
                if (d < best) { best = d; bi = k; }
            }
            float wx = r->left[bi][0] - r->right[bi][0];
            float wy = r->left[bi][1] - r->right[bi][1];
            float half = 0.5f * sqrtf(wx*wx + wy*wy);
            /*
             * Test the piece's FOOTPRINT, not just its origin.  These meshes are
             * up to 8272 units across -- a piece whose centre sits clear of the
             * road can still reach right across it, which is what put slabs in
             * the middle of the view.  Local X and Z are the horizontal axes, so
             * the reach is their max radius.
             */
            float rad = 0.0f;
            float ex = 0.0f, ey = 0.0f, ez = 0.0f;   /* local X, Y, Z extents */
            float ymin = 0.0f, ymax = 0.0f;          /* local vertical span */
            if (sc.obj[i].m) {
                float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
                float z0 = 1e30f, z1 = -1e30f;
                for (int k = 0; k < sc.obj[i].m->nvert; k++) {
                    float lx = sc.obj[i].m->vert[k].x;
                    float ly = sc.obj[i].m->vert[k].y;
                    float lz = sc.obj[i].m->vert[k].z;
                    float d2 = lx*lx + lz*lz;
                    if (d2 > rad) rad = d2;
                    if (lx < x0) x0 = lx;
                    if (lx > x1) x1 = lx;
                    if (ly < y0) y0 = ly;
                    if (ly > y1) y1 = ly;
                    if (lz < z0) z0 = lz;
                    if (lz > z1) z1 = lz;
                }
                rad = sqrtf(rad);
                ex = x1 - x0;
                ey = y1 - y0;
                ez = z1 - z0;
                ymin = y0;
                ymax = y1;
            }
            float reach = half + rad;
            /*
             * PAINT: a thin ribbon that is on the road.  Decided here because
             * both the draw transform and the filter need to know.
             */
            float roadz = r->centre[bi][2];
            int paint = g_roll && (ez <= 64.0f && ey > 0.0f && ey <= 700.0f &&
                                   best < half * half);
            /* Is this a piece of the road itself?  Needed HERE, because the
               road's own pieces must not be filtered as obstructions -- a tile
               crossing the road IS the road. */
            /*
             * A piece of road AT ROAD LEVEL is the surface the port builds from
             * the splines, so its asphalt is left out and only its paint and
             * kerbs are kept.  A piece of road WELL ABOVE (or below) the
             * profile is a different structure -- track 1's overpasses, which
             * are road pieces on their own deck -- and that one is a real
             * object: draw all of it, asphalt included.
             *
             * Getting this wrong is what showed Danik "markings at the side of
             * the road, never on both sides, switching with the bend": it was
             * the PAINT of an overpass whose deck was being skipped, hanging in
             * the air.
             */
            int elevated = fabsf((float)sc.obj[i].e - roadz) > 1200.0f;
            int roadpiece = (!elevated && sc.obj[i].m &&
                             mesh_uses_material(sc.obj[i].m, g_roadmat)) ? 1 : 0;
            /*
             * OBSTRUCTION: does the piece stand in the band the player drives
             * through?  The piece's own vertical span, from the record height,
             * against the road's height at that point.
             *
             * This is what separates a bridge from a wall, and getting it wrong
             * is why the bridges and the tunnel were missing.  A bridge deck
             * carries a big record height -- the structures above the track run
             * 2269 to 12486, well past the +/-4096 the terrain tiles live in --
             * so it spans the road far ABOVE the driver and must be kept.  The
             * pieces that boxed the camera in sit AT road level, and those are
             * the ones to drop.
             */
            float pbase = (float)sc.obj[i].e + ymin;
            float ptop  = (float)sc.obj[i].e + ymax;
            /*
             * The band the driver occupies: from just above the surface to
             * 1200 units (3.6 m) up.  The camera rides at 600.  A ceiling above
             * that -- a bridge deck at 1792 -- is passed UNDER and must not
             * count as an obstruction, or every bridge is deleted again.
             */
            int obstructs = (ptop > roadz + 150.0f) && (pbase < roadz + 1200.0f);
            /*
             * Drop a piece that BLOCKS THE ROAD: its footprint reaches across
             * the road AND it stands in the band the player drives through.
             *
             * The footprint half matters because these meshes reach up to 9308
             * units from their centre -- a piece whose centre is clear of the
             * road can still lie right across it.  The BAND half is what took
             * until now to get right, and it is why the bridges and the tunnel
             * were missing: a bridge spans the road exactly like a wall does,
             * and dropping everything that crossed the road deleted every one of
             * them.  The difference is height, and the record height now means
             * something (see scene.h), so it can be told apart.
             *
             * PAINT is exempt -- a lane marking's whole job is to lie on the
             * road.  Note the exemption is the measured one (thin ribbon, on the
             * road) and NOT "has a keyed material": keyed pieces also include
             * the crowd stand (V153, 4 m tall) and the big plastic structures,
             * and exempting those is what left props standing in the middle of
             * the track.
             *
             * OS_ONROAD=0 disables the test for an A/B; --layer 3 keeps
             * everything.
             */
            if (g_onroad && want_layer != 3 && sc.obj[i].m && !paint &&
                !sc.obj[i].flat && !roadpiece &&
                obstructs && best < reach * reach) { onroad++; continue; }
            /*
             * Orientation.  The record carries no usable heading -- +0x0C is
             * the height, +0x14 and +0x1C are always zero, and +0x18 is not a
             * heading in any of the conventions tried (2048/4096/8192/65536
             * per turn, phase swept, all come out near chance) -- so the angle
             * has to be derived from the track, and the track angle is right.
             *
             * WHICH LOCAL AXIS IS ALIGNED TO IT IS NOT THE SAME FOR EVERY
             * MESH, and that is what put pieces across the road.  The barrier
             * family (V3, V2: 4136 x 1034 x 0) is long in LOCAL X and flat in
             * Z, so "align local Z with the road" stood it ACROSS the road --
             * which is exactly what swallowed the camera on the downhill.  The
             * road tiles and the other walls (V133, 0 x 516 x 4136) are long in
             * local Z.  So: align whichever horizontal axis is the longer one.
             *
             * This is measured off the mesh, not guessed, and it is the mesh's
             * own property.
             */
            int bm = (bi + 3) % r->count, bp = (bi + r->count - 3) % r->count;
            float tang = atan2f(r->centre[bm][1] - r->centre[bp][1],
                                r->centre[bm][0] - r->centre[bp][0]);
            sc.obj[kept] = sc.obj[i];
            /*
             * ROAD PAINT IS LAID FLAT.
             *
             * A ribbon that (a) is at most 0.7 m tall, (b) has no thickness in
             * local Z, and (c) is placed ON the road, is paint: a lane marking
             * or a painted kerb stripe.  Drawn as authored it is a wall standing
             * in the lane, which is invisible edge-on from a chase camera --
             * which is why Danik could not see any lane markings.  Rolled down
             * it is a 0.8 m wide stripe with the keyed texture's dashes in it.
             *
             * All three tests are measured, not guessed: the height and the
             * thickness come off the mesh, and "on the road" is the distance to
             * the centreline against the half-width, which is the same test the
             * on-road filter uses.
             */
            sc.obj[kept].roll = paint;
            /*
             * Flat AND at the road's height = a terrain tile, which the port
             * builds from the splines already.  Flat and NOT at the road's
             * height = a bridge deck / platform / ceiling, and gets drawn.
             */
            /*
             * The +/-1200 is measured, not picked.  The ground tiles are
             * quantised differently from the road tiles and legitimately sit up
             * to 931 units off the road's height -- at 700 one of those was
             * classified as a DECK and drawn, which put a grass ceiling over the
             * camera (Danik: "a grass ceiling above our heads").  The bridge
             * decks are at 1792, so anything between 931 and 1792 divides the
             * two cleanly.
             */
            sc.obj[kept].tile = (sc.obj[i].flat && pbase < roadz + 1200.0f &&
                                 ptop > roadz - 1200.0f) ? 1 : 0;
            /*
             * "Thin" means flat in EITHER horizontal direction, not only in Z.
             * The railing panels that crown the crowd stands are 0 x 516 x 4136
             * -- flat in X -- so a Z-only test missed exactly the pieces Danik
             * still had to press `u` for: "I still have to press u to make the
             * crowd-stand roof look ok".
             */
            sc.obj[kept].thin = (ex <= 64.0f || ez <= 64.0f) ? 1 : 0;
            sc.obj[kept].facez = (ez <= 64.0f && ey > 0.0f) ? 1 : 0;
            sc.obj[kept].roadpiece = roadpiece;
            if (getenv("OS_TILEDBG") && (sc.obj[i].v == 135 || sc.obj[i].v == 136))
                printf("tile-dbg V%d  e %d  pbase %.0f  ptop %.0f  roadz %.0f "
                       " flat %d tile %d  yaw %.1f\n", sc.obj[i].v, sc.obj[i].e,
                       pbase, ptop, roadz, sc.obj[i].flat, sc.obj[kept].tile,
                       sc.obj[kept].yaw * 180.0f / (float)M_PI);
            /*
             * Paint lies ON the road, so a rolled piece takes the ROAD's height
             * at that point, not its own record height.  The record height is
             * one of the 512-unit levels and a marking can be laid where the
             * road is somewhere between two of them (V44's records all say 306,
             * which is the flat part of the lap); keeping it would float the
             * stripe up to a metre off the tarmac.  The 8-unit lift keeps it out
             * of the road quads it is painted on.
             */
            if (sc.obj[kept].roll)
                sc.obj[kept].z = r->centre[bi][2] + 40.0f;
            /*
             * The object's height.  g_objz 1 (default) = the record's own
             * +0x0C, used straight, because that is the same field the road
             * tiles carry -- so the road and everything on it agree.  0 = the
             * old reading, the height treated as an offset to the road.
             */
            sc.obj[kept].z = g_objz ? (float)sc.obj[i].e
                                    : r->centre[bi][2] + (float)sc.obj[i].e;
            /*
             * A rotation by `yaw` sends local (lx,lz) to
             *     (lx cos - lz sin, lx sin + lz cos)
             * so local Z lands on the direction yaw+90 and local X on yaw.
             */
            sc.obj[kept].yaw = tang - (ex > ez ? 0.0f : (float)M_PI / 2.0f);
            kept++;
        }
        sc.count = kept;
        int tf = 0;
        for (int i = 0; i < sc.count; i++) tf += sc.obj[i].m ? sc.obj[i].m->nface : 0;
        printf("scene: %d of %d records placed (layer %d, %d on-road skipped),"
               " %d faces\n", sc.count, nobj, want_layer, onroad, tf);
    }

    float focal_y = (FB_H * 0.5f) / tanf(0.5f * 50.0f * (float)M_PI / 180.0f);
    float focal_x = focal_y * ASPECT;
    /* camera height as a multiple of the road width.  1.15 is a drone shot;
       eye level for a car is about 0.12 (a 1.4 m eye in a 11.8 m wide road,
       given ~1 unit ~ 4.5 mm).  Override with the 6th argument. */
    /*
     * Chase height, as a multiple of the road width.  Just above eye level:
     * a 1.4 m eye in an 7.9 m road is 0.175, so 0.22 sits a little higher,
     * which is where the original's chase camera rides.  The camera Z is added
     * to the centreline's own Z, which now carries MAPnCAM.SPL's elevation, so
     * it follows the road's climbs and dips rather than just its plan.
     */
    /*
     * ABSOLUTE height in world units, not a multiple of the road width.
     *
     * It used to be `r->width * factor`, which was a trap: when the road was
     * widened from 2617 to 6158 units the camera rose by the same 2.35x, so the
     * road looked exactly as narrow as before on screen and the width change
     * appeared to have done nothing.  A 1.8 m eye is 600 units whether the road
     * under it is 8 m or 18 m wide.
     */
    float cam_height = (argc > 6) ? (float)atof(argv[6]) : 600.0f;
    printf("camera: %.0f units above the road (about %.1f m)\n",
           cam_height, cam_height * 0.003f);
    float base_pitch = -0.16f;

    fb = malloc(sizeof(uint32_t) * FB_W * FB_H);
    quad *quads = malloc(sizeof(quad) * MAX_QUADS);

    if (shot) {
        int i0 = ((int)t0 % r->count + r->count) % r->count;
        int i1 = (i0 + 1) % r->count;
        float f = t0 - floorf(t0);
        camera cam;
        cam.x = r->centre[i0][0] + (r->centre[i1][0] - r->centre[i0][0]) * f;
        cam.y = r->centre[i0][1] + (r->centre[i1][1] - r->centre[i0][1]) * f;
        cam.z = r->centre[i0][2] + (r->centre[i1][2] - r->centre[i0][2]) * f
                + cam_height;
        cam.yaw = atan2f(r->centre[i1][1] - r->centre[i0][1],
                         r->centre[i1][0] - r->centre[i0][0]);
        cam.pitch = base_pitch;
        /*
         * OS_OVERHEAD=<h> renders the same frame from h units above the road
         * looking straight down, so the flies-eye check works headlessly too
         * (and therefore reproducibly): a structure that is in the wrong place
         * is obvious from above and invisible from the driving seat.
         */
        if (getenv("OS_OVERHEAD")) {
            cam.z = r->centre[i0][2] + (float)atof(getenv("OS_OVERHEAD"));
            cam.pitch = -1.45f;
        }
        if (getenv("OS_INSPECT")) describe_under_crosshair(&cam, focal_x, focal_y);
        render_frame(&cam, r, &sk, quads, focal_x, focal_y);
        write_ppm(shot);
        free(quads); free(fb); free(r); free(sk.pix);
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    /* 4:3, as 320x200 was always meant to be shown */
    SDL_Window *win = SDL_CreateWindow("OpenScreamer - track preview",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       FB_W * 3, FB_W * 3 * 3 / 4, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         FB_W, FB_H);

    printf("\ncontrols\n"
           "  arrows        drive / turn (auto-drive until you touch them)\n"
           "  W A S D       fly horizontally,  R / F  up and down (SHIFT = 4x)\n"
           "  mouse drag    look around\n"
           "  v             LOOK FROM ABOVE: hover over the road looking down\n"
           "                R / F then change the hover height\n"
           "  h             back on the road, chasing it\n"
           "  m             toggle auto-drive / free flight\n"
           "  i             describe the piece under the crosshair (console)\n"
           "  n             turn that piece's texture 90 degrees (console says which)\n"
           "  z / x         texture scale down / up for that piece's material\n"
           "  b             fit the texture once across billboards and panels\n"
           "  esc           quit\n\n");
    fflush(stdout);
    camera cam = { 0, 0, 0, 0, base_pitch };
    float t = t0;
    int running = 1, manual = 0, mouse_look = 0;
    int overhead = 0;               /* v: hover above the road, look down */
    float overhead_height = 12000.0f;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            /*
             * Mouse look: hold a button and move the mouse to turn.  The
             * cursor is captured (relative mode) while the button is held so
             * turning does not stop at the window edge; it is released on
             * button-up so the window still behaves normally.  A platform that
             * refuses relative mode just falls back to plain motion deltas.
             */
            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                mouse_look = 1;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                manual = 1;
            }
            if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
                mouse_look = 0;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            if (ev.type == SDL_MOUSEMOTION && mouse_look) {
                const float sens = 0.004f;
                cam.yaw   -= (float)ev.motion.xrel * sens;
                cam.pitch -= (float)ev.motion.yrel * sens;
                if (cam.pitch >  1.50f) cam.pitch =  1.50f;
                if (cam.pitch < -1.50f) cam.pitch = -1.50f;
                base_pitch = cam.pitch;
                manual = 1;
            }
            if (ev.type == SDL_KEYDOWN) {
                switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: running = 0; break;
                case SDLK_m:  manual = !manual; break;
                /*
                 * LOOK FROM ABOVE.  v = orbit high over the road and look
                 * down at it, pressing it again drops back to the chase view.
                 * It is a debugging view: "there is a grass ceiling at t=48"
                 * is answerable in one keystroke instead of a conversation.
                 * r/f then climb and descend freely (held, with SHIFT = 4x),
                 * so the whole structure can be flown around.
                 */
                /*
                 * `i` -- WHAT AM I LOOKING AT?
                 *
                 * Prints the piece nearest the centre of the screen with the
                 * facts that decide how it is drawn: its mesh number, which
                 * layer it is in, the flags (thin / billboard / roll / tile),
                 * the material of each of its faces, and the texture size the
                 * material record gives.  Guessing which mesh someone means by
                 * "the roof above the crowd stands" has cost two rounds; this
                 * turns it into a number.  Output goes to the console.
                 */
                case SDLK_i:
                    describe_under_crosshair(&cam, focal_x, focal_y);
                    break;
                /*
                 * `n` -- turn the texture of the piece under the crosshair by 90
                 * degrees.  Held down it cycles.  The console prints the material
                 * and the angle; that pair is what gets written into the default
                 * table.  This exists because the rotation belongs to the texture
                 * and cannot be derived from the geometry.
                 */
                case SDLK_z:
                case SDLK_x: {
                    const scene_object *so = under_crosshair(&cam, focal_x, focal_y);
                    if (!so || !so->m) { printf("nothing under the crosshair\n"); }
                    else {
                        int mi = -1;
                        for (int f = 0; f < so->m->nface; f++) {
                            int m = so->m->material[f] / 16;
                            if (m >= 0 && m < MAT_MAX && g_mat[m].tex) { mi = m; break; }
                        }
                        if (mi < 0) printf("mesh V%d has no textured face\n", so->v);
                        else {
                            float v = g_matscale_mul[mi] > 0 ? g_matscale_mul[mi] : 1.0f;
                            v *= (ev.key.keysym.sym == SDLK_x) ? 1.25f : 0.8f;
                            if (v < 0.01f) v = 0.01f;
                            if (v > 100.0f) v = 100.0f;
                            g_matscale_mul[mi] = v;
                            printf("mesh V%d: material %d texture scale x%.3f"
                                   "  (world units per repeat %.0f)\n",
                                   so->v, mi, v, g_texscale * v);
                        }
                    }
                    fflush(stdout);
                    break;
                }
                case SDLK_b:
                    /* fit the texture ONCE across each face of the thin pieces */
                    g_facefit = !g_facefit;
                    printf("fit texture to face (billboards/panels): %s\n",
                           g_facefit ? "on" : "off");
                    fflush(stdout);
                    break;
                case SDLK_n: {
                    const scene_object *so = under_crosshair(&cam, focal_x, focal_y);
                    if (!so || !so->m) { printf("nothing under the crosshair\n"); }
                    else {
                        int mi = -1;
                        for (int f = 0; f < so->m->nface; f++) {
                            int m = so->m->material[f] / 16;
                            if (m >= 0 && m < MAT_MAX && g_mat[m].tex) { mi = m; break; }
                        }
                        if (mi < 0) printf("mesh V%d has no textured face\n", so->v);
                        else {
                            g_matrot[mi] = (g_matrot[mi] + 90) % 360;
                            printf("mesh V%d: material %d rotated to %d degrees\n",
                                   so->v, mi, g_matrot[mi]);
                        }
                    }
                    fflush(stdout);
                    break;
                }
                case SDLK_v:
                    overhead = !overhead;
                    manual = 1;
                    printf("overhead view: %s\n", overhead ? "on" : "off");
                    fflush(stdout);
                    break;
                case SDLK_h:
                    /* snap back to the road, chasing it again */
                    overhead = 0; manual = 0;
                    printf("back on the road (auto drive)\n"); fflush(stdout);
                    break;
                case SDLK_LEFT:  cam.yaw += 0.06f; manual = 1; break;
                case SDLK_RIGHT: cam.yaw -= 0.06f; manual = 1; break;
                case SDLK_UP:
                    cam.pitch += 0.02f; base_pitch = cam.pitch; manual = 1; break;
                case SDLK_DOWN:
                    cam.pitch -= 0.02f; base_pitch = cam.pitch; manual = 1; break;
                /*
                 * Live texture-scale adjustment.  The scale is NOT decoded
                 * data -- the face record carries no texture coordinates -- so
                 * finding the right value by eye is the only way, and doing it
                 * one rebuild at a time is hopeless.  Held keys repeat.
                 */
                case SDLK_PAGEUP:
                    g_texscale *= 1.10f;
                    if (g_texscale > 200000.0f) g_texscale = 200000.0f;
                    printf("U texscale %.1f   V texscale %.1f\n",
                           g_texscale, g_texscalev); fflush(stdout);
                    break;
                case SDLK_PAGEDOWN:
                    g_texscale /= 1.10f;
                    if (g_texscale < 4.0f) g_texscale = 4.0f;
                    printf("U texscale %.1f   V texscale %.1f\n",
                           g_texscale, g_texscalev); fflush(stdout);
                    break;
                /* road texture: width (- =) and length (; ') */
                case SDLK_MINUS:
                    g_roaducyc *= 0.90f;
                    if (g_roaducyc < 0.10f) g_roaducyc = 0.10f;
                    printf("road width cycles %.2f   length %.2f\n",
                           g_roaducyc, g_roadvlen); fflush(stdout);
                    break;
                case SDLK_EQUALS:
                    g_roaducyc *= 1.10f;
                    if (g_roaducyc > 64.0f) g_roaducyc = 64.0f;
                    printf("road width cycles %.2f   length %.2f\n",
                           g_roaducyc, g_roadvlen); fflush(stdout);
                    break;
                case SDLK_SEMICOLON:
                    g_roadvlen *= 0.90f;
                    if (g_roadvlen < 0.02f) g_roadvlen = 0.02f;
                    printf("road width cycles %.2f   length %.2f\n",
                           g_roaducyc, g_roadvlen); fflush(stdout);
                    break;
                case SDLK_QUOTE:
                    g_roadvlen *= 1.10f;
                    if (g_roadvlen > 64.0f) g_roadvlen = 64.0f;
                    printf("road width cycles %.2f   length %.2f\n",
                           g_roaducyc, g_roadvlen); fflush(stdout);
                    break;
                case SDLK_k:
                    g_useobjscale = !g_useobjscale;
                    printf("per-object texture scale (+0x18): %s\n",
                           g_useobjscale ? "on" : "off");
                    fflush(stdout);
                    break;
                case SDLK_o:
                    g_texrot += 90;
                    if (g_texrot >= 360) g_texrot = 0;
                    printf("texture orientation: %d degrees\n", g_texrot);
                    fflush(stdout);
                    break;
                case SDLK_u:
                    g_uvworld = !g_uvworld;
                    printf("UV origin: %s\n",
                           g_uvworld ? "world" : "object-local");
                    fflush(stdout);
                    break;
                case SDLK_LEFTBRACKET:
                    g_texoffu -= 0.02f; break;
                case SDLK_RIGHTBRACKET:
                    g_texoffu += 0.02f; break;
                case SDLK_COMMA:
                    g_texoffv -= 0.02f; break;
                case SDLK_PERIOD:
                    g_texoffv += 0.02f; break;
                case SDLK_HOME:
                    g_texscalev *= 1.10f;
                    if (g_texscalev > 200000.0f) g_texscalev = 200000.0f;
                    printf("U texscale %.1f   V texscale %.1f\n",
                           g_texscale, g_texscalev); fflush(stdout);
                    break;
                case SDLK_END:
                    g_texscalev /= 1.10f;
                    if (g_texscalev < 4.0f) g_texscalev = 4.0f;
                    printf("U texscale %.1f   V texscale %.1f\n",
                           g_texscale, g_texscalev); fflush(stdout);
                    break;
                default: break;
                }
            }
        }
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_SPACE]) manual = 0;

        /*
         * The spline drives the camera ONLY while it is not in manual mode.
         * These assignments used to sit outside the `if (!manual)`, so the
         * camera was yanked back to the track every frame and a WASD nudge
         * survived exactly one frame -- "moves slightly then snaps back".
         * Manual mode is now free flight; SPACE hands control back.
         */
        if (!manual) {
            t += 0.5f;
            while (t >= r->count) t -= r->count;
            /*
             * Say where we are, so a report of "the tunnel is missing" can be
             * turned into a road sample index and jumped straight to with
             * --shot.  Printed on stdout (so it can be pasted) and in the
             * window title (so it can be read off while driving).
             */
            {
                static int last = -99;
                int idx = (int)t;
                if (idx / 8 != last / 8) {
                    last = idx;
                    char title[128];
                    snprintf(title, sizeof title,
                             "OpenScreamer  t=%d of %d   lap %.1f%%",
                             idx, r->count, 100.0f * t / (float)r->count);
                    SDL_SetWindowTitle(win, title);
                    printf("%s\n", title);
                    fflush(stdout);
                }
            }

            int i0 = ((int)t % r->count + r->count) % r->count;
            int i1 = (i0 + 1) % r->count;
            float f = t - floorf(t);
            cam.x = r->centre[i0][0] + (r->centre[i1][0] - r->centre[i0][0]) * f;
            cam.y = r->centre[i0][1] + (r->centre[i1][1] - r->centre[i0][1]) * f;
            cam.z = r->centre[i0][2] + (r->centre[i1][2] - r->centre[i0][2]) * f
                    + cam_height;

            float target = atan2f(r->centre[i1][1] - r->centre[i0][1],
                                  r->centre[i1][0] - r->centre[i0][0]);
            float d = target - cam.yaw;
            while (d >  (float)M_PI) d -= 2 * (float)M_PI;
            while (d < -(float)M_PI) d += 2 * (float)M_PI;
            cam.yaw += d * 0.2f;

            cam.pitch = base_pitch;
        }

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S]) {
            manual = 1;
            float s = (keys[SDL_SCANCODE_W] ? 1 : -1) * r->width * 0.2f;
            cam.x += cosf(cam.yaw) * s;
            cam.y += sinf(cam.yaw) * s;
        }
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D]) {
            manual = 1;
            float s = (keys[SDL_SCANCODE_A] ? -1 : 1) * r->width * 0.2f;
            cam.x += cosf(cam.yaw - (float)M_PI / 2) * s;
            cam.y += sinf(cam.yaw - (float)M_PI / 2) * s;
        }
        /* vertical flight: R up, F down, SHIFT = 4x.  Flying is how a structure
           that is in the wrong place gets SEEN to be in the wrong place. */
        if (keys[SDL_SCANCODE_R] || keys[SDL_SCANCODE_F]) {
            manual = 1;
            float sp = r->width * 0.2f;
            if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) sp *= 4.0f;
            if (overhead) {
                /* hover: R/F move the camera, not the target */
                overhead_height += (keys[SDL_SCANCODE_R] ? sp : -sp);
                if (overhead_height < 200.0f) overhead_height = 200.0f;
                if (overhead_height > 400000.0f) overhead_height = 400000.0f;
                printf("overhead height %.0f\n", overhead_height); fflush(stdout);
            } else {
                cam.z += (keys[SDL_SCANCODE_R] ? sp : -sp);
            }
        }
        if (overhead) {
            /*
             * Frame the current road position from above, looking straight
             * down, keeping the yaw so "up" on screen is the direction of
             * travel.  Re-framed every frame, so the camera can be parked and
             * still hover over the car.
             */
            int i0 = ((int)t % r->count + r->count) % r->count;
            cam.x = r->centre[i0][0];
            cam.y = r->centre[i0][1];
            cam.z = r->centre[i0][2] + overhead_height;
            cam.pitch = -1.45f;
            base_pitch = cam.pitch;
        }

        render_frame(&cam, r, &sk, quads, focal_x, focal_y);
        SDL_UpdateTexture(tex, NULL, fb, FB_W * sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    free(quads); free(fb); free(r); free(sk.pix);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    printf("\nlast ROAD texture settings:  width cycles %.2f   length %.2f\n"
           "  re-run with:  OS_ROADUCYC=%.2f OS_ROADVLEN=%.2f\n",
           g_roaducyc, g_roadvlen, g_roaducyc, g_roadvlen);
    printf("\nlast texture settings:\n"
           "   U scale %.1f   V scale %.1f\n"
           "   U offset %.2f   V offset %.2f\n"
           "   orientation %d deg   UV origin %s\n"
           "  re-run with:  OS_TEXSCALE=%.1f OS_TEXSCALEV=%.1f "
           "OS_TEXOFFU=%.2f OS_TEXOFFV=%.2f OS_TEXROT=%d %s"
           "port/build/openscreamer build/dec/MAP1 1\n",
           g_texscale, g_texscalev, g_texoffu, g_texoffv,
           g_texrot, g_uvworld ? "world" : "object-local",
           g_texscale, g_texscalev, g_texoffu, g_texoffv, g_texrot,
           g_uvworld ? "" : "OS_UVLOCAL=1 ");
    fflush(stdout);
    SDL_Quit();
    return 0;
}
