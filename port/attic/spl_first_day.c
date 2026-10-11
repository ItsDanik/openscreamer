#include "spl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port.h"

#define SPL_HEADER 8
#define SPL_NODE   56

typedef struct {
    int32_t x, y, z;
    int32_t next;
} raw_node;

static int read_nodes(const char *path, raw_node **out, int *count)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "spl: cannot open %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc((size_t)size);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "spl: short read on %s\n", path);
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);

    int n = (int)((uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                  ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24));

    int avail = (int)((size - SPL_HEADER) / SPL_NODE);
    if (n > avail) n = avail;
    if (n > SPL_MAX_NODES) n = SPL_MAX_NODES;

    raw_node *nodes = calloc((size_t)n, sizeof(*nodes));
    for (int i = 0; i < n; i++) {
        const uint8_t *p = buf + SPL_HEADER + (size_t)i * SPL_NODE;
        /* little-endian int32 fields at +4, +8, +12, +16 */
        memcpy(&nodes[i].x,    p + 4,  4);
        memcpy(&nodes[i].y,    p + 8,  4);
        memcpy(&nodes[i].z,    p + 12, 4);
        memcpy(&nodes[i].next, p + 16, 4);
    }
    free(buf);
    *out = nodes;
    *count = n;
    return 0;
}

int spl_load(spline *out, const char *path)
{
    raw_node *nodes;
    int n;
    if (read_nodes(path, &nodes, &n) < 0) return -1;

    out->count = n;
    out->indexed = 0;
    for (int i = 0; i < n; i++) {
        out->pt[i].x = nodes[i].x;
        out->pt[i].y = nodes[i].y;
        out->pt[i].z = nodes[i].z;
    }
    free(nodes);
    return 0;
}

int spl_load_chained(spline *out, const char *path)
{
    raw_node *nodes;
    int n;
    if (read_nodes(path, &nodes, &n) < 0) return -1;

    char *seen = calloc((size_t)n, 1);
    int k = 0, c = 0;
    while (k >= 0 && k < n && !seen[k] && c < n) {
        seen[k] = 1;
        out->pt[c].x = nodes[k].x;
        out->pt[c].y = nodes[k].y;
        out->pt[c].z = nodes[k].z;
        c++;
        k = nodes[k].next;
    }
    free(seen);
    free(nodes);
    out->count = c;
    out->indexed = 1;
    return 0;
}

int spl_load_edges(spline *left, spline *right, const char *path)
{
    spline all;
    if (spl_load(&all, path) < 0) return -1;

    left->count = right->count = 0;
    left->indexed = right->indexed = 1;
    for (int i = 0; i < all.count; i++) {
        if (i & 1) right->pt[right->count++] = all.pt[i];
        else       left->pt[left->count++]   = all.pt[i];
    }
    return 0;
}
