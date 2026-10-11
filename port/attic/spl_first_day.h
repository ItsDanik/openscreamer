/*
 * spl.h - Screamer .SPL spline loader
 *
 * Format (recovered from the binary; see CLAUDE.md):
 *
 *   header  8 bytes:  int32 count, int32 0
 *   node   56 bytes:  int32 marker (1)
 *                     int32 X
 *                     int32 Y
 *                     int32 Z            elevation
 *                     int32 next         index of the next node -- a LINKED LIST
 *                     int32 X, Y         repeated twice more (unused here)
 *                     int32 0 ...
 *
 * Nodes are NOT stored in path order: you must follow `next`, which closes
 * back on node 0.
 *
 * Not every spline is a path. MAPnSLA.SPL holds the track's LEFT and RIGHT
 * EDGES interleaved -- node 2k and 2k+1 are partners, and the gap between
 * them is the road width. Take them in node order and you get a zigzag.
 */
#ifndef SCREAMER_SPL_H
#define SCREAMER_SPL_H

#include <stdint.h>

#define SPL_MAX_NODES 4096

typedef struct {
    int32_t x, y, z;   /* x,y ground plane; z elevation */
} spl_point;

typedef struct {
    spl_point pt[SPL_MAX_NODES];
    int       count;   /* nodes in path order */
    int       indexed; /* 1 if the file's `next` field was followed */
} spline;

/* Load a .SPL in file order. Returns 0 on success. */
int spl_load(spline *out, const char *path);

/*
 * Load a .SPL and reorder it by following the `next` chain from node 0.
 * Use for CAM/OPP-style paths. Returns 0 on success.
 */
int spl_load_chained(spline *out, const char *path);

/*
 * Load an edge-pair spline (MAPnSLA.SPL) and split it into the two
 * interleaved halves. Returns 0 on success.
 */
int spl_load_edges(spline *left, spline *right, const char *path);

#endif /* SCREAMER_SPL_H */
