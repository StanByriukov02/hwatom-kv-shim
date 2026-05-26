/* MOON R1 — 2-adic band index motor (ultrametric slot placement within 2 MiB leaf). */
#include "shim_internal.h"

#include <stdlib.h>

static unsigned g_2adic_band_seq;

int shim_2adic_enabled_v1(void)
{
    const char *e = getenv("HWATOM_SHIM_2ADIC");

    if (!e || !e[0]) {
        return 0;
    }
    return e[0] != '0';
}

void shim_2adic_reset(void)
{
    g_2adic_band_seq = 0;
}

unsigned shim_2adic_band_count(void)
{
    return g_2adic_band_seq;
}

unsigned shim_2adic_slots_per_leaf(size_t need_va, size_t leaf_bytes)
{
    if (need_va == 0 || leaf_bytes == 0) {
        return 1;
    }
    return (unsigned)(leaf_bytes / need_va);
}

size_t shim_2adic_offset_in_leaf(unsigned band_idx, size_t need_va, size_t leaf_bytes)
{
    unsigned slots = shim_2adic_slots_per_leaf(need_va, leaf_bytes);
    unsigned slot;

    if (slots == 0) {
        slots = 1;
    }
    if ((slots & (slots - 1u)) == 0u) {
        slot = band_idx & (slots - 1u);
    } else {
        slot = band_idx % slots;
    }
    return (size_t)slot * need_va;
}

unsigned shim_2adic_mega_leaf_for_band(unsigned band_idx, unsigned slots_per_leaf)
{
    if (slots_per_leaf == 0) {
        return 0;
    }
    return band_idx / slots_per_leaf;
}

unsigned shim_2adic_bump_band(void)
{
    return g_2adic_band_seq++;
}
