/* A-2.13 P1 packing + A-2.14 mega-VA (fewer cuMemAddressReserve per N leaves). */
#include "shim_internal.h"

#include <stdio.h>
#include <string.h>

#define SHIM_PACK_MEGA_LEAVES_DEFAULT 32u
#define SHIM_PACK_MEGA_LEAVES_MAX 8192u

typedef struct shim_pack_arena_st {
    int active;
    CUdeviceptr base_va;
    size_t va_len;
    CUmemGenericAllocationHandle handle;
    size_t next_off;
    size_t logical_bytes;
} shim_pack_arena_t;

typedef struct shim_mega_st {
    int active;
    CUdeviceptr base_va;
    size_t va_len;
    unsigned mega_leaves;
    unsigned leaf_used;
} shim_mega_t;

static shim_pack_arena_t g_pack_arena;
static shim_mega_t g_mega;
static size_t g_pack_committed_total;
static size_t g_pack_committed_peak;
static int g_pack_handle_refs;

static void shim_pack_note_committed(size_t n)
{
    g_pack_committed_total += n;
    if (g_pack_committed_total > g_pack_committed_peak) {
        g_pack_committed_peak = g_pack_committed_total;
    }
}

static size_t shim_pack_mega_reserve_override(void)
{
    static const char *keys[] = {
        "HWATOM_MOON_MEGA_RESERVE_BYTES",
        "HWATOM_PACK_MEGA_RESERVE_BYTES",
        NULL,
    };
    size_t i;

    for (i = 0; keys[i] != NULL; i++) {
        const char *e = getenv(keys[i]);
        char *end = NULL;
        unsigned long long v;

        if (!e || !e[0]) {
            continue;
        }
        v = strtoull(e, &end, 10);
        if (end != e && v > 0) {
            return shim_round_leaf_size((size_t)v);
        }
    }
    return 0;
}

static int shim_pack_moon_geometry_active(void)
{
    return shim_pack_mega_reserve_override() > 0;
}

static unsigned shim_pack_mega_leaf_count(void)
{
    const char *e = getenv("HWATOM_PACK_MEGA_LEAVES");
    char *end = NULL;
    unsigned long long v;

    if (shim_pack_moon_geometry_active()) {
        return 0;
    }
    if (!e || !e[0]) {
        return SHIM_PACK_MEGA_LEAVES_DEFAULT;
    }
    v = strtoull(e, &end, 10);
    if (end == e || v == 0 || v > SHIM_PACK_MEGA_LEAVES_MAX) {
        return SHIM_PACK_MEGA_LEAVES_DEFAULT;
    }
    return (unsigned)v;
}

int shim_pack_mega_enabled_v1(void)
{
    const char *e = getenv("HWATOM_PACK_MEGA");
    if (!e || !e[0]) {
        return 1;
    }
    return e[0] != '0';
}

/* Eval-tier cap: max logical slots per 2 MiB leaf (0 = unlimited on lab build). */
unsigned shim_pack_k_cap_max_v1(void)
{
#ifdef HWATOM_EVAL_SHIM_BUILD
    const unsigned default_cap = 2u;
#else
    const unsigned default_cap = 0u;
#endif
    const char *e = getenv("HWATOM_PACK_K_CAP");
    char *end = NULL;
    unsigned long long v;

    if (!e || !e[0]) {
        return default_cap;
    }
    if (e[0] == '0') {
#ifdef HWATOM_EVAL_SHIM_BUILD
        return default_cap;
#else
        return 0;
#endif
    }
    v = strtoull(e, &end, 10);
    if (end == e || v == 0) {
        return default_cap;
    }
    if (v > 32) {
        return default_cap;
    }
#ifdef HWATOM_EVAL_SHIM_BUILD
    if ((unsigned)v < default_cap) {
        return default_cap;
    }
#endif
    return (unsigned)v;
}

int shim_pack_eval_build_v1(void)
{
#ifdef HWATOM_EVAL_SHIM_BUILD
    return 1;
#else
    return 0;
#endif
}

static int shim_pack_k_cap_blocks_reserve_locked(size_t need_va)
{
    unsigned cap = shim_pack_k_cap_max_v1();

    if (cap == 0 || !g_pack_arena.active || need_va == 0) {
        return 0;
    }
    if (cap == 1) {
        return 1;
    }
    if (g_pack_arena.next_off == 0) {
        return 0;
    }
    return (g_pack_arena.next_off / need_va) >= cap;
}

size_t shim_placement_round_v1(size_t req)
{
    size_t min_leaf = shim_leaf_bytes_v1();
    size_t best = shim_round_leaf_size(req);
    unsigned lv;

    for (lv = 0; lv <= 2; lv++) {
        size_t r = shim_leaf_ladder_round(req, lv);
        if (r >= req && r <= min_leaf && r < best) {
            best = r;
        }
    }
    return best;
}

void shim_pack_reset(void)
{
    memset(&g_pack_arena, 0, sizeof(g_pack_arena));
    memset(&g_mega, 0, sizeof(g_mega));
    g_pack_committed_total = 0;
    g_pack_committed_peak = 0;
    g_pack_handle_refs = 0;
}

int shim_pack_arena_active(void)
{
    return g_pack_arena.active;
}

static void shim_pack_micro_reset_locked(CUdeviceptr base_va, size_t va_len)
{
    memset(&g_pack_arena, 0, sizeof(g_pack_arena));
    g_pack_arena.active = 1;
    g_pack_arena.base_va = base_va;
    g_pack_arena.va_len = va_len;
    g_pack_arena.next_off = 0;
    g_pack_arena.logical_bytes = 0;
}

static void shim_pack_flush_locked(void)
{
    memset(&g_pack_arena, 0, sizeof(g_pack_arena));
    memset(&g_mega, 0, sizeof(g_mega));
    shim_2adic_reset();
}

void shim_pack_flush(void)
{
    pthread_mutex_lock(&g_map_epoch_mu);
    shim_pack_flush_locked();
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_pack_mega_open(CUdeviceptr base_va, size_t va_len)
{
    size_t leaf = shim_leaf_bytes_v1();
    unsigned cap;
    unsigned n = shim_pack_mega_leaf_count();

    pthread_mutex_lock(&g_map_epoch_mu);
    memset(&g_mega, 0, sizeof(g_mega));
    g_mega.active = 1;
    g_mega.base_va = base_va;
    g_mega.va_len = va_len;
    cap = (leaf > 0 && va_len >= leaf) ? (unsigned)(va_len / leaf) : 1u;
    if (cap < 1u) {
        cap = 1u;
    }
    g_mega.mega_leaves = (n == 0) ? cap : ((n < cap) ? n : cap);
    g_mega.leaf_used = 1;
    shim_pack_micro_reset_locked(base_va, leaf);
    pthread_mutex_unlock(&g_map_epoch_mu);
}

/* Next cuMemAddressReserve size when a new mega window is required. */
size_t shim_pack_new_reserve_size(void)
{
    size_t leaf = shim_leaf_bytes_v1();
    size_t moon = shim_pack_mega_reserve_override();
    unsigned n;

    if (moon > 0) {
        return moon;
    }
    if (!shim_pack_mega_enabled_v1()) {
        return leaf;
    }
    n = shim_pack_mega_leaf_count();
    if (n == 0) {
        return leaf;
    }
    return (size_t)n * leaf;
}

int shim_pack_need_new_mega_reserve(void)
{
    int need = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    if (!shim_pack_mega_enabled_v1()) {
        need = !g_pack_arena.active;
    } else if (!g_mega.active) {
        need = 1;
    } else if (g_mega.leaf_used >= g_mega.mega_leaves) {
        need = 1;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return need;
}

/* A-2.14: next 2 MiB leaf inside active mega — no new cuMemAddressReserve. */
int shim_pack_take_mega_leaf(CUdeviceptr *va_out, size_t *va_map_size)
{
    size_t leaf = shim_leaf_bytes_v1();
    unsigned idx;

    if (!va_out || !va_map_size || !shim_pack_mega_enabled_v1()) {
        return 0;
    }

    pthread_mutex_lock(&g_map_epoch_mu);
    if (!g_mega.active || g_mega.leaf_used >= g_mega.mega_leaves) {
        pthread_mutex_unlock(&g_map_epoch_mu);
        return 0;
    }
    idx = g_mega.leaf_used;
    if ((size_t)(idx + 1u) * leaf > g_mega.va_len) {
        pthread_mutex_unlock(&g_map_epoch_mu);
        return 0;
    }
    *va_out = g_mega.base_va + (CUdeviceptr)((size_t)idx * leaf);
    *va_map_size = leaf;
    g_mega.leaf_used++;
    shim_pack_micro_reset_locked(*va_out, leaf);
    pthread_mutex_unlock(&g_map_epoch_mu);
    return 1;
}

int shim_pack_try_reserve(CUdeviceptr *va_out, size_t req_logical, size_t *va_map_size)
{
    size_t leaf = shim_leaf_bytes_v1();
    size_t need_va = shim_placement_round_v1(req_logical);

    if (!va_out || !va_map_size || req_logical == 0 || need_va == 0 || need_va > leaf) {
        return 0;
    }

    pthread_mutex_lock(&g_map_epoch_mu);

    if (shim_pack_k_cap_blocks_reserve_locked(need_va)) {
        pthread_mutex_unlock(&g_map_epoch_mu);
        return 0;
    }

    if (g_pack_arena.active && g_pack_arena.next_off + need_va <= g_pack_arena.va_len) {
        size_t off = g_pack_arena.next_off;
        unsigned band_idx = 0;

        if (shim_2adic_enabled_v1()) {
            band_idx = shim_2adic_bump_band();
            if (g_pack_arena.next_off == 0) {
                off = shim_2adic_offset_in_leaf(band_idx, need_va, g_pack_arena.va_len);
                if (off + need_va > g_pack_arena.va_len) {
                    off = 0;
                }
            }
        }
        *va_out = g_pack_arena.base_va + (CUdeviceptr)off;
        *va_map_size = need_va;
        if (shim_2adic_enabled_v1() && g_pack_arena.next_off == 0) {
            g_pack_arena.next_off = off + need_va;
        } else {
            g_pack_arena.next_off += need_va;
        }
        g_pack_arena.logical_bytes += req_logical;
        pthread_mutex_unlock(&g_map_epoch_mu);
        return 1;
    }

    pthread_mutex_unlock(&g_map_epoch_mu);
    return 0;
}

int shim_pack_open_arena(CUdeviceptr va, size_t va_len, CUmemGenericAllocationHandle handle)
{
    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_pack_arena.active && g_pack_arena.base_va == va && !g_pack_arena.handle) {
        g_pack_arena.handle = handle;
        shim_pack_note_committed(va_len);
        g_pack_handle_refs = 1;
        pthread_mutex_unlock(&g_map_epoch_mu);
        return 0;
    }
    shim_pack_flush_locked();
    g_pack_arena.active = 1;
    g_pack_arena.base_va = va;
    g_pack_arena.va_len = va_len;
    g_pack_arena.handle = handle;
    g_pack_arena.next_off = 0;
    g_pack_arena.logical_bytes = 0;
    shim_pack_note_committed(va_len);
    g_pack_handle_refs = 1;
    pthread_mutex_unlock(&g_map_epoch_mu);
    return 0;
}

void shim_pack_retain_handle(void)
{
    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_pack_arena.active && g_pack_handle_refs > 0) {
        g_pack_handle_refs++;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

int shim_pack_release_ref(CUmemGenericAllocationHandle handle)
{
    int defer = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_pack_arena.active && g_pack_arena.handle == handle && g_pack_handle_refs > 0) {
        g_pack_handle_refs--;
        defer = g_pack_handle_refs > 0;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return defer;
}

int shim_pack_is_packed_subva_locked(CUdeviceptr va)
{
    if (!g_pack_arena.active || va == g_pack_arena.base_va) {
        return 0;
    }
    if (g_mega.active && va >= g_mega.base_va &&
        va < g_mega.base_va + (CUdeviceptr)g_mega.va_len) {
        return va >= g_pack_arena.base_va &&
               va < g_pack_arena.base_va + (CUdeviceptr)g_pack_arena.va_len;
    }
    return g_pack_arena.active && va >= g_pack_arena.base_va &&
           va < g_pack_arena.base_va + (CUdeviceptr)g_pack_arena.va_len;
}

int shim_pack_is_packed_subva(CUdeviceptr va)
{
    int sub = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    sub = shim_pack_is_packed_subva_locked(va);
    pthread_mutex_unlock(&g_map_epoch_mu);
    return sub;
}

size_t shim_pack_map_offset_for_va(CUdeviceptr va)
{
    size_t off = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_pack_arena.active && va >= g_pack_arena.base_va &&
        va < g_pack_arena.base_va + (CUdeviceptr)g_pack_arena.va_len) {
        off = (size_t)(va - g_pack_arena.base_va);
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return off;
}

CUmemGenericAllocationHandle shim_pack_arena_handle_locked(void)
{
    if (g_pack_arena.active) {
        return g_pack_arena.handle;
    }
    return NULL;
}

CUmemGenericAllocationHandle shim_pack_arena_handle(void)
{
    CUmemGenericAllocationHandle h = NULL;
    pthread_mutex_lock(&g_map_epoch_mu);
    h = shim_pack_arena_handle_locked();
    pthread_mutex_unlock(&g_map_epoch_mu);
    return h;
}

void shim_pack_bump_usage(size_t logical_req)
{
    size_t need = shim_placement_round_v1(logical_req);

    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_pack_arena.active && need > 0 &&
        g_pack_arena.next_off + need <= g_pack_arena.va_len) {
        g_pack_arena.next_off += need;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

size_t shim_pack_committed_bytes(void)
{
    size_t n = 0;
    pthread_mutex_lock(&g_map_epoch_mu);
    n = g_pack_committed_total;
    pthread_mutex_unlock(&g_map_epoch_mu);
    return n;
}

size_t shim_pack_committed_peak_bytes(void)
{
    size_t n = 0;
    pthread_mutex_lock(&g_map_epoch_mu);
    n = g_pack_committed_peak;
    pthread_mutex_unlock(&g_map_epoch_mu);
    return n;
}

unsigned shim_pack_mega_leaf_used(void)
{
    unsigned n = 0;
    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_mega.active) {
        n = g_mega.leaf_used;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return n;
}

unsigned shim_pack_mega_reserve_count(void)
{
    unsigned reserves = 0;
    unsigned leaves;
    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_mega.active && g_mega.mega_leaves > 0) {
        leaves = g_mega.leaf_used;
        if (leaves > 0) {
            reserves = 1 + (leaves - 1) / g_mega.mega_leaves;
        }
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return reserves;
}
