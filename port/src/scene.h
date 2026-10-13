/*
 * scene.h - track scenery: which objects, and where.
 *
 * ---------------------------------------------------------------- MAPn.MMP
 *
 * A two-level 256x256 grid of offsets into the file.  FUN_0004c904 walks it at
 * load time and adds the file base to every entry that is not -1, which is how
 * the format was identified.
 *
 *     +0x0000  256 x int32   level-1 offsets, -1 = empty row
 *     row:     256 x int32   level-2 offsets, -1 = empty cell
 *
 * What a grid cell points at is a 40-byte header with FIVE (base, count) pairs:
 *
 *     base[i]  = i32(cell + 0x00 + 4i)      i = 0..4
 *     count[i] = i32(cell + 0x14 + 4i)
 *
 * That pairing is read straight out of the game's own code -- FUN_000120c0 uses
 * (+0x08, +0x1C) and (+0x0C, +0x20), FUN_00082e88 uses (+0x10, +0x24) -- and it
 * is why exactly the first five words are relocated by FUN_0004c904 and the rest
 * are not.
 *
 * The five arrays are laid out back to back after the header, and that chain
 * holds in 182/182 cells of track 1:
 *
 *     cell + 0x28              n0+n1 records, 64 bytes each
 *       + 64*(n0+n1)           layer 2 array, 12-byte elements
 *       + 12*n2                layer 3 array, 28-byte elements
 *       + 28*n3                layer 4 array
 *
 * Layers 0 and 1 are the SAME array split at n0.  FUN_000120c0 reads layer 2
 * and layer 3 to find the nearest object (X at element+4, Y at element+8);
 * FUN_00082e88 reads layer 4 as a bounding box for culling.  Layer 0 and layer 1
 * are what this module loads.
 *
 * ------------------------------------------------------------- the records
 *
 *     64 bytes, and 646/646 of track 1's layer-1 records carry -1 at +0x20,
 *     which is what pins the stride:
 *
 *         +0x00 int32  V   object index.  Spans 0..191, and MAPn.UNR declares
 *                          exactly 192 objects -- so V indexes the UNR, not the
 *                          .3DX props in the GFX container.
 *         +0x04 int32  1
 *         +0x08 int32  X
 *         +0x0C int32  HEIGHT (the vertical coordinate)
 *         +0x10 int32  Y   the second ground axis
 *         +0x14 int32  0   } the three rotation angles; only the middle one,
 *         +0x18 int32  T   } about the vertical, is ever non-zero.
 *         +0x1C int32  0   } T IS THE HEADING, 2048 units to the full turn.
 *         +0x20 int32  -1
 *
 * LAYER 0 IS THE ON-ROAD CLASS AND LAYER 1 IS THE ROADSIDE SCENERY.  Measured
 * against the resampled centreline of track 1 (tools/mmp_dump.py):
 *
 *     layer 0: 163 records, 53% inside the road, median 0.91 half-widths,
 *              never further out than 3.71
 *     layer 1: 646 records,  8% inside the road, median 3.80 half-widths,
 *              out to 22.65 for the distant stuff
 *
 * This matters because the first record of a cell is a layer-0 record, and an
 * earlier version of this file read only that one -- which put 182 on-road
 * markers around the track and looked exactly like "boxes in the middle of the
 * road".  Do not go back to reading a single record per cell.
 *
 * ------------------------------------------------------------- the heading
 *
 * T (+0x18) is the piece's rotation about the vertical, 2048 units per turn,
 * and a piece is placed as
 *
 *     world = (X + lx*cos - lz*sin,  Y + lx*sin + lz*cos,  HEIGHT + ly)
 *
 * Proof: drawn from above with that rule, the flat tiles of track 1 abut edge
 * to edge into one continuous road all the way round the lap, bends included
 * (the bend pieces step T by 57 = 10 degrees).  With the sign flipped the
 * bends shatter; with 1024 or 4096 per turn nothing lines up.  And for all 40
 * V0 road tiles the nearest neighbouring V0 lies exactly along the rotated
 * local Z axis, 40 of 40.
 *
 * So NOTHING about a piece's placement is derived from the track splines any
 * more.  The track is the tiles: road surface, ground, walls, bridges, all of
 * it, placed and turned by the record alone.
 */
#ifndef SCREAMER_SCENE_H
#define SCREAMER_SCENE_H

#include "mesh.h"

/* 809 / 1330 / 2359 records on tracks 1 / 3 / 5 */
#define SCENE_MAX_OBJ 4096

/* a car's wheels: how far they have turned, and the front ones' steering */
typedef struct {
    float spin, steer;
    float body_pitch, body_roll, body_drop;   /* the body on its suspension */
    float env_scroll;       /* how far the reflections have slid, in texels */
    int   env;              /* which reflection picture: 0, 0x8000 or 0xff00 */
    /* SCREAMER 2: each wheel by itself -- how far it has turned and how far its
       suspension is pushed in (wheel 0 front +X, 1 rear +X, 2 front -X, 3 rear -X) */
    int   per_wheel;
    float wheel_spin[4], wheel_in[4];
    /* ... and what each block of the model is, by its number (the game knows them by name,
       not by where they are): 0 nothing, 1 the body, 0x10 + wheel = the part of a wheel that
       turns with it, 0x20 + wheel = the part that only steers */
    unsigned char role[16];
} car_pose;

typedef struct {
    const mesh *m;
    const car_pose *pose;   /* NULL for everything that is not a car */
    float zbias;            /* 0, or how much nearer to count it in the depth test (shadows) */
    int   sprite;           /* record +0x04 bit 0x8000 (0x118a4 sets it on the smoke puffs): the
                               engine builds this object's turn from its angles LESS the
                               camera's (0x8abc7 -> 0x8b26c -> 0x4bca0), so it keeps facing
                               the screen however the camera turns, tips or leans */
    float x, y, z;    /* world: x, y on the ground, z up (record +0x08/+0x10/+0x0C) */
    float yaw;        /* radians, from T */
    float pitch, roll; /* tilt, applied before yaw; 0 for track pieces (cars use it) */
    float radius;     /* furthest vertex from the origin, for culling */
    int   v;          /* MAPn.UNR object index */
    int   t;          /* record +0x18, raw */
    int   layer;      /* 0 or 1: which of the cell's two record runs */
    int   kind;       /* record +0x3C: Screamer 2's ROAD KIND (the id of a row of the track's
                         ROAD KINDS table: grip, smoke); always 0 in Screamer */
    int   phase;      /* record +0x20: which frame of its textures, 0..65535 over
                         all of them; -1 (every track piece) = the animation clock */
} scene_object;

/*
 * THE WALLS AND THE ROAD'S DIRECTION are in MAPn.MMP too: the cell's third and
 * fourth arrays (base at cell +0x08 / +0x0C, counts at +0x1C / +0x20).
 *
 *   array 2, 12 bytes:  float angle, int x, int y
 *       a point on the road's centre line and the heading of the road there
 *       (same convention as the car's heading: forward = (sin a, -cos a)).
 *       FUN_000120c0(0) returns the nearest one; the game uses it for the
 *       start position and to tell which way the road goes.
 *
 *   array 3, 28 bytes:  float angle, int x, int y, int x1, y1, x2, y2
 *       ONE WALL SEGMENT: its two ends, its middle, and its angle -- the
 *       wall's normal is (cos a, -sin a).  Chained end to end they are two
 *       closed loops, one each side of the road.  FUN_000120c0(1) returns the
 *       nearest one to a corner of the car that has left the track.
 */
/* (`cell` = row * 256 + column of the grid cell whose list it is in: Screamer 2 looks
   for the nearest only in the nine cells round a point, 0x2a371) */
typedef struct { float angle, x, y, x1, y1, x2, y2; int cell; } scene_wall;
typedef struct { float angle, x, y; int cell; } scene_dir;
#define SCENE_MAX_WALL 8192

typedef struct {
    scene_object obj[SCENE_MAX_OBJ];
    int count;
    int nlayer[2];
    int vmin, vmax;
    scene_wall wall[SCENE_MAX_WALL];
    int nwall;
    scene_dir dir[SCENE_MAX_WALL];
    int ndir;
} scene;

/* `gfxdir` holds MAPn.MMP and MAPn.UNR.  Returns the number of records. */
int scene_load(scene *sc, const char *gfxdir, int track);

#endif /* SCREAMER_SCENE_H */
