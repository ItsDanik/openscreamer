/*
 * unr.h - MAPn.UNR: the track's road furniture meshes.
 *
 * THE FILE
 *
 * Read straight out of FUN_0004c744, the loader:
 *
 *     read 4 bytes -> blob_size
 *     read 4 bytes -> object_count
 *     allocate  count*4 + 0x1e4   under the tag "OBJ_LST"
 *     allocate  blob_size         under the tag "UNR_FILE"
 *     read count*4 bytes into the OBJ_LST array
 *     append 0xFFFFFFFF                       <- terminator
 *     for each entry: entry += blob_base      <- relocate
 *     read blob_size bytes into the blob
 *
 * So the layout is:
 *
 *     +0x00  int32   blob_size
 *     +0x04  int32   object_count          (192 for track 1)
 *     +0x08  count x int32   offsets, RELATIVE TO THE BLOB
 *     blob                   starts at 8 + 4*count
 *
 * and the arithmetic confirms it exactly: 776 + 660976 == 661752 == the file
 * size of MAP1.UNR, and the offsets are strictly increasing.  An earlier read
 * of this header was off by one dword, which is why the objects looked like
 * nonsense for so long.
 *
 * THE OBJECTS
 *
 * Each object is a .3DX-shaped file in miniature, at the same block offset
 * 0x330 that .3DX always uses:
 *
 *     +0x10  0xFFFFFFFF
 *     +0x14  block count            (1 for every object in track 1)
 *     +0x18  block offsets          ([0x330])
 *     block  +0x10  a = vertex count
 *            +0x14  b = face count
 *            +0x1C  vertex array offset, relative to the object
 *            +0x24  ... face table
 *
 * faces are 36-byte records, exactly as in .3DX.  All 192 objects in MAP1.UNR
 * parse, and 86 of them are flat planes -- which is what the pieces are:
 *
 *     obj 0    110 v, 52 f   X +-3079, Y = 0, Z +-2053   road surface tile
 *     obj 2     30 v, 16 f   X +-2068, Y +-517, Z = 0    wall / barrier
 *     obj 15     8 v,  2 f   X +-1034, Y +-1034, Z = 0   small sign panel
 *
 * A flat plane in local Y is a horizontal road tile; flat in local Z is a
 * vertical panel.  That is why some MAPn.MMP records sit on the centreline:
 * they are the road surface itself, not scenery (see scene.h).
 */
#ifndef SCREAMER_UNR_H
#define SCREAMER_UNR_H

#include <stdint.h>

/* The object count varies per track: 192, 356, 353, 236, 360, 307 for 1..6. */
#define UNR_MAX_OBJ 2048     /* Screamer's tracks have up to 261 objects, Screamer 2's 594 and more */

typedef struct {
    uint8_t *file;      /* the whole file, kept alive for unr_object() */
    long     len;
    long     blob;      /* offset of the blob within `file` */
    int      count;
    int32_t  off[UNR_MAX_OBJ + 1];   /* blob-relative, off[count] = blob_size */
} unr;

/* Returns 0 on success. */
int unr_load(unr *u, const char *path);

/*
 * The bytes of object k as a standalone .3DX-shaped buffer, valid until the
 * next unr_load().  Returns NULL if k is out of range.
 */
const uint8_t *unr_object(const unr *u, int k, long *out_len);

#endif /* SCREAMER_UNR_H */
