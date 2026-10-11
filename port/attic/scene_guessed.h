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
 *         +0x0C int32  E   + = a normal object, 0 or negative = the on-road class
 *         +0x10 int32  Y
 *         +0x14 int32  0
 *         +0x18 int32  T
 *         +0x1C int32  0
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
 * The object names separately come from the plain-text MAPnDAT.DTR section
 * OSTACLES (misspelled in the shipped data -- match it as-is).  Those are the
 * small AZ*.3DX roadside props, a different set from the UNR objects V indexes.
 */
#ifndef SCREAMER_SCENE_H
#define SCREAMER_SCENE_H

#include "mesh.h"

/* 809 / 1330 / 2359 records on tracks 1 / 3 / 5 -- 1024 was silently truncating. */
#define SCENE_MAX_OBJ 4096
#define SCENE_MAX_MESH 32

typedef struct {
    const mesh *m;
    float x, y, z;
    float yaw;
    float scale;
    int   v;      /* the object index the record stores (0..191) */
    /*
     * record +0x0C -- THE OBJECT'S HEIGHT, not a rotation and not an offset.
     *
     * Read out of the engine (FUN_00083164): the record pointer is walked with a
     * 64-byte stride and +0x08, +0x0C and +0x10 are each SUBTRACTED FROM THE
     * CAMERA POSITION and squared-summed into one distance -- a plain 3D
     * distance test against the view, which only works if all three are
     * coordinates.  +0x08 and +0x10 are also the two grid-index axes, so they
     * are the ground plane; +0x0C is therefore the height.
     *
     * The data agrees: over the lap the road-surface tiles' +0x0C is a
     * STAIRCASE in 512-unit steps from -4096 to +4096 (a 16-level height grid),
     * while the CAM spline's Z is a smooth curve.  They are different things,
     * and the earlier reading -- "add +0x0C to the road height" -- was wrong.
     */
    int   e;      /* record +0x0C, world height, quantised to 512 */
    int   t;      /* record +0x18 */
    int   layer;  /* 0 = on-road class, 1 = roadside scenery */
    int   flat;   /* mesh has no vertical extent */
    /*
     * A TERRAIN TILE: flat AND at the road's own height.  Those are the pieces
     * the port already synthesises from the splines, so they are skipped.
     *
     * flat alone is NOT enough, and assuming it was is why the bridges were
     * invisible.  Track 1's bridges are a flat deck -- V135/V136, 6204 x 4136 --
     * sitting **1792 units above the road**, with the V133/V134 walls at 2048
     * forming their parapets.  Skipping every flat mesh threw the decks away and
     * left the parapets floating in mid-air, which is exactly what Danik
     * reported: "some decorations are now floating in the air ... but not
     * something that resembles a bridge".
     */
    int   tile;
    /*
     * 1 = draw it LAID DOWN.  A thin ribbon (no thickness in local Z, under
     * 0.7 m tall) placed on the road is paint -- a lane marking -- and as
     * authored it stands upright in the lane and is invisible edge-on.  See the
     * roll in the scenery draw transform in main.c.
     */
    int   roll;
    /*
     * 1 = the mesh has no thickness (local Z extent <= 64): a billboard, a
     * railing panel, a decal.  Those take OBJECT-LOCAL texture coordinates --
     * Danik found it by hand: "the railings/roof above the crowd stands renders
     * correctly when I press u (object-local)".  A one-shot image wants its own
     * origin, not a slice of a world-wide pattern; surfaces that must tile
     * across many pieces (the road, the walls) keep world coordinates.
     */
    int   thin;
    /* 1 = the mesh is flat in local Z: a billboard facing across the road.
       Those take their texture crossed over (see the UV code in main.c). */
    int   facez;
    /* 1 = the mesh carries the road-surface material: a road tile, whose paint
       and kerbs are drawn while its asphalt is left to the spline road. */
    int   roadpiece;
} scene_object;

typedef struct {
    scene_object obj[SCENE_MAX_OBJ];
    int count;            /* everything loaded */
    int nlayer[2];        /* how many of each layer */
    int vmin, vmax;       /* the range of V actually seen */
} scene;

/*
 * `datdir` holds MAPnDAT.DTR; `gfxdir` holds MAPn.MMP and the .3DX files.
 * Returns the number of records placed.
 */
int scene_load(scene *sc, const char *datdir, const char *gfxdir, int track);

#endif /* SCREAMER_SCENE_H */
