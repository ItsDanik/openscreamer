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
#define MESH_MAX_VERT 2560   /* the menu's track previews need 2337 (AMAP5) */
#define MESH_MAX_PARTS 16
#define MESH_MAX_FACE 1792   /* ... and 1579 faces (AMAP6) */

typedef struct { float x, y, z; } vec3;

typedef struct {
    /* the arrays are allocated by mesh_load_at() to the size the object needs (the
       MESH_MAX_* numbers are only the most one object may have) */
    vec3   *vert;
    uint8_t *ncorner;                 /* 3 or 4 */
    int32_t (*idx)[4];
    /*
     * The face's own normal, in LOCAL space -- exact, not derived from the
     * geometry.  Three int16 at +0x12, +0x16 and +0x1a, scaled so that 16384 is
     * 1.0 (the engine's 14-bit fixed point; a face pointing straight down reads
     * -16383).  The words between them are 0 or -1 and are not part of it.
     */
    vec3   *normal;
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
     * mesh_load_at()'s `r` is the true base + 2 (how its field offsets were
     * first written down) -- so the material is at r+2.
     */
    int32_t *material;
    /* face record +0x02: the palette colour of a face whose material is
       routine 128 -- no texture, one flat colour (0xd8846) */
    int16_t *colour;
    /*
     * TEXTURE COORDINATES -- one (u, v) pair per VERTEX, raw values 0..255.
     *
     * The block header's +0x24 is the offset (from the object base, like the
     * vertex array at +0x1C) of an array PARALLEL to the vertex array: 6 bytes
     * per vertex, int16 u, int16 v, int16 0.  The renderer indexes it with the
     * very same byte offset a face uses for its corner vertex
     * (FUN at 0xd5350, the textured-polygon set-up):
     *
     *     mov  edx, [0x741c4]            ; = object base + block[+0x24]
     *     mov  bx,  [face + 8]           ; corner's vertex byte offset
     *     movsx eax, word [edx + ebx]    ; raw u
     *     imul eax, [material + 8]       ; x the material's U extent
     *     sar  eax, 5
     *     movsx eax, word [edx + ebx+2]  ; raw v
     *     imul eax, [material + 0xc]     ; x the material's V extent
     *
     * So the raw value is a 0..255 fraction of the material's own rectangle;
     * see the material notes in main.c.  An earlier belief that "the face
     * record carries no texture coordinates" was true and beside the point:
     * they are per vertex, not per face.
     */
    int16_t (*uv)[2];
    /* distance between the front and rear axles, from the positions of the
       wheel blocks (0 for anything that is not a car).  The game derives the
       same number when it sets a car up (0x136c8). */
    float   wheelbase;
    /* how far ahead of / behind the model's origin the two axles are; the
       nose is local -Z, so the front axle is the one at negative z */
    float   axle_front, axle_rear;
    /* how far out from the centre line the wheels are */
    float   track_half;
    /* the block header's +0x18: flags (bit 0x2000 selects a lighting pass) */
    int     flags;
    /* the fourth letter of the first block's name (block +0x6b).  The game
       reads it as the piece's SURFACE: 'e' -- erba, grass -- is rough ground
       (0x11ccc keeps it at 0x8020 and 0x11a30 tests it against 0x65) */
    char    surface;
    /* the first block is named akk..tap.. or akksper: one of the flat lawn tiles the
       tracks' makers laid under the road (see the depth test in main.c) */
    int     carpet;
    /*
     * WHAT THE GAME'S "WHAT AM I STANDING ON" TEST USES (0x11ccc, 0x1bfc4,
     * 0x1c09c), all from the object's FIRST block only:
     *   bound_r, bound_cx, bound_cz   header +0x00 (int32) and the int16s at
     *       +0x04 and +0x08: a circle the point must be in before the faces
     *       are looked at;
     *   coll[]   the faces in the order of the list at header +0x28 (uint16
     *       offsets from the block) -- NOT the order the records are stored
     *       in.  The test stops at the first face the point is over, so the
     *       order is part of the answer.  -1 = an entry that is no face (the
     *       list is the first row of the draw-order table and a few pieces'
     *       rows are cut short by its 0xFFFF end mark).
     */
    float   bound_r, bound_cx, bound_cz;
    int     ncoll;
    int16_t *coll;
    int     nvert;
    int     nface;
    /*
     * MOVING PARTS.  When the object's header +0x10 is not -1 it is a number
     * of FRAMES, and every block's position record (block header +0x0C) is
     * followed, from its +0x1C, by one 0x30-byte entry per frame: 12 int16 --
     * a 3x3 matrix, 16383 = 1.0, and a translation -- and then the same again
     * for the inverse.  The engine copies the twelve to where a block's matrix
     * and position go (0x8ad14; Screamer 2 0x7ec30), so they are applied to
     * the vertex as it is stored -- a row vector times the matrix, the result
     * in the order x, up, z:
     *
     *     x' = (m0*x + m3*y + m6*z) / 16383 + t0
     *     y' = (m1*x + m4*y + m7*z) / 16383 + t1
     *     z' = (m2*x + m5*y + m8*z) / 16383 + t2
     *
     * These objects are modelled with Z up and every frame carries the turn to
     * Y up: a part that does not move has (1,0,0 / 0,0,1 / 0,1,0).
     *
     * The frame shown is  frames * clock >> 16  with the animation clock that
     * also runs the textures (0x8abf8, 0x8ad14): the whole movement takes 3.33
     * seconds and repeats.  Seven objects on tracks 3-6 have it (100 to 400
     * frames each).
     */
    /* every block of the object: which vertices and faces are its, where it
       sits in the object (the position record), and its flags (+0x18) */
    int     nblock;
    /* per-VERTEX normals: block header +0x20 is the offset of an int32 count
       and that many (float x, y, z) unit vectors.  Only blocks that are drawn
       with reflections have them (a car's body, nose and tail). */
    vec3   *vnormal;
    struct mesh_block { int vbase, vcount, fbase, fcount, flags, has_vnormal; float ox, oy, oz; }
            block[MESH_MAX_PARTS];
    uint8_t *face_block;
    int     nframes;
    int     nparts;
    struct mesh_part {
        int vbase, vcount, fbase, fcount;
        int16_t *frame;           /* nframes x 12 */
        vec3    *rest_vert;       /* the block's own vertices, before placing */
        vec3    *rest_normal;
    } part[MESH_MAX_PARTS];
} mesh;

/* move every loaded mesh that has moving parts to the clock's frame (0..65535) */
void mesh_animate_all(int clock16);
/* how far from its origin a mesh with moving parts ever reaches */
float mesh_reach(const mesh *m);

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
