/*
 * mesh.h - Screamer .3DX object loader
 *
 * Format (recovered from the binary; see CLAUDE.md):
 *
 *   file header   +0x14 = N objects, +0x18 = N uint32 block offsets
 *   object block  +0x10 = a  vertex count
 *                 +0x14 = b  face count
 *                 +0x1C = FILE OFFSET OF THE VERTEX ARRAY
 *   vertices      a x int16 xyz triplets (6 bytes each)
 *   faces         b records of 36 bytes:
 *                   +0x00 int16  constant within a file
 *                   +0x02 int16  constant within a file
 *                   +0x04 int16  n   corner count (4 = quad, 3 = triangle)
 *                   +0x06 (n+1) x int16  CLOSED LOOP of corner vertex BYTE
 *                                        OFFSETS; /6 for the index, and the
 *                                        last entry repeats the first
 *                   +0x10 ...    normal / material
 *
 * The mesh is indexed / shared-vertex.  a == 4b holds only for objects whose
 * vertex array happens to be stored in face order -- do not rely on it.
 */
#ifndef SCREAMER_MESH_H
#define SCREAMER_MESH_H

#include <stdint.h>

/*
 * Sized from what the data actually holds -- every object in MAP1.UNR and every
 * .3DX prop in MAP1L was measured: max 189 vertices / 144 faces per UNR object,
 * 128 / 89 for the props, and a few blocks can stack in one file.  The previous
 * 20000/8000 made each mesh ~400 KB, which is 76 MB for the 192 UNR objects and
 * would never fit a MiSTer.  1024/1024 is ~30 KB per mesh.
 */
#define MESH_MAX_VERT 1024
#define MESH_MAX_FACE 1024

typedef struct { float x, y, z; } vec3;

typedef struct {
    vec3    vert[MESH_MAX_VERT];
    uint8_t ncorner[MESH_MAX_FACE];   /* 3 or 4 */
    int32_t idx[MESH_MAX_FACE][4];
    /*
     * The face's own normal, in LOCAL space -- exact, not derived from the
     * geometry.  Three int16 at +0x12, +0x16 and +0x1a, scaled so that 16384 is
     * 1.0 (the engine's 14-bit fixed point; a face pointing straight down reads
     * -16383).  The words between them are 0 or -1 and are not part of it.
     */
    vec3    normal[MESH_MAX_FACE];
    /*
     * The MATERIAL field.  This is the int16 at face+0x04, and it is an OFFSET
     * INTO MAPn.MLB's 16-byte material record table.  Read out of the renderer
     * (FUN_00083468):
     *
     *     MOVSX EAX, word ptr [EBP + 0x4]      ; int16 at face+4
     *     ADD EAX, dword ptr [0x00021540]      ; += the MATERIALS blob base
     *     MOV EAX, dword ptr [EAX]
     *     CALL dword ptr [EAX + 0x722e8]       ; dispatch on what it finds
     *
     * Verified against the data: 3000 of 3009 faces in track 1 land on a valid
     * material record (size a power of two, texture record in range).  Every
     * value is a multiple of 16, which is exactly the table stride.
     *
     * Note the offset: the face base used here is the TRUE one, at which the
     * corner count sits at +0x06 (FUN_0001bfc4 tests *(short *)(face + 6) == 4).
     * mesh_load_at()'s `r` is the true base + 2, because find_faces() scans for
     * a run of records and lands two bytes in -- so the material is at r+2.
     */
    int32_t material[MESH_MAX_FACE];
    /*
     * The block header's +0x18 field.  512 on 19 of track 1's first 20 objects
     * and 2048 on one, and objects 36/44 differ ONLY in this field while having
     * identical geometry -- so it varies independently of the mesh and is a
     * candidate per-object TEXTURE SCALE.  That would explain why one global
     * scale never fitted anything.
     */
    int     texscale;
    int     nvert;
    int     nface;
} mesh;

/* Load every object in a .3DX into one indexed mesh. Returns 0 on success. */
int mesh_load(mesh *m, const char *path);

/*
 * The same parse, on a buffer already in memory.
 *
 * A MAPn.UNR object IS a .3DX-shaped file in miniature: +0x14 = block count,
 * +0x18 = block offsets, +0x10 = 0xFFFFFFFF, and block 0 at 0x330.  So the
 * loader works on a UNR object unchanged, once unr_object() hands it over.
 */
int mesh_load_at(mesh *m, const uint8_t *buf, long len);

#endif /* SCREAMER_MESH_H */
