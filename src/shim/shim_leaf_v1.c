/* A-2.6 — 2 MB leaf alignment helper (C-113 / sec31 Q1; iron may override). */
#include "shim_internal.h"

#include <stdlib.h>

#define HWATOM_LEAF_DEFAULT (1u << 21)

size_t shim_leaf_bytes_v1(void)
{
    const char *e = getenv("HWATOM_LEAF_BYTES");
    char *end = NULL;
    unsigned long long v;

    if (!e || !e[0]) {
        return (size_t)HWATOM_LEAF_DEFAULT;
    }
    v = strtoull(e, &end, 10);
    if (end == e || v == 0) {
        return (size_t)HWATOM_LEAF_DEFAULT;
    }
    return (size_t)v;
}

size_t shim_round_leaf_size(size_t size)
{
    size_t leaf = shim_leaf_bytes_v1();
    if (leaf == 0) {
        return HWATOM_LEAF_DEFAULT;
    }
    if (size == 0) {
        return leaf;
    }
    if (size % leaf == 0) {
        return size;
    }
    return ((size / leaf) + 1) * leaf;
}

int shim_is_leaf_aligned(size_t size)
{
    size_t leaf = shim_leaf_bytes_v1();
    return leaf != 0 && size % leaf == 0;
}

size_t shim_leaf_ladder_round(size_t size, unsigned level)
{
    size_t leaf = shim_leaf_bytes_v1();

    if (level > 2) {
        level = 2;
    }
    if (leaf == 0) {
        leaf = HWATOM_LEAF_DEFAULT;
    }
    while (level > 0 && leaf > (1u << 19)) {
        leaf >>= 1;
        level--;
    }
    if (size == 0) {
        return leaf;
    }
    if (size % leaf == 0) {
        return size;
    }
    return ((size / leaf) + 1) * leaf;
}
