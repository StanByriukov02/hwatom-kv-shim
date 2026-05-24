/* A-2.4 v1.1 — multi-VA slot table (concurrent resident maps). */
#include "shim_internal.h"

#include <string.h>

#define SHIM_VA_SLOT_CAP 65536u

typedef struct shim_va_slot_st {
    uint32_t gen;
    CUdeviceptr va;
    size_t size;
    size_t alignment;
    unsigned long long res_flags;
    int has_handle;
    CUmemGenericAllocationHandle handle;
    size_t rounded_size;
    size_t req_bytes;
    size_t map_offset;
    unsigned long long alloc_flags;
    int mapped;
    int packed_arena;
} shim_va_slot_t;

static shim_va_slot_t g_va_slots[SHIM_VA_SLOT_CAP];
static uint32_t g_va_slot_gen = 1;
static int g_pending_reserve_idx = -1;
static int g_active_slot_count = 0;

/* Legacy single-slot view for unit tests (most recently committed VA). */
shim_reservation_slot_t g_reservation_slot;

static int shim_slot_index_locked(int idx)
{
    return idx >= 0 && (unsigned)idx < SHIM_VA_SLOT_CAP && g_va_slots[idx].gen != 0;
}

static int shim_slot_alloc_locked(void)
{
    unsigned i;

    for (i = 0; i < SHIM_VA_SLOT_CAP; i++) {
        if (g_va_slots[i].gen == 0) {
            memset(&g_va_slots[i], 0, sizeof(g_va_slots[i]));
            g_va_slots[i].gen = g_va_slot_gen++;
            if (g_va_slot_gen == 0) {
                g_va_slot_gen = 1;
            }
            return (int)i;
        }
    }
    return -1;
}

static void shim_slot_free_locked(int idx)
{
    if (!shim_slot_index_locked(idx)) {
        return;
    }
    memset(&g_va_slots[idx], 0, sizeof(g_va_slots[idx]));
    if (g_active_slot_count > 0) {
        g_active_slot_count--;
    }
}

static int shim_slot_find_by_va_locked(CUdeviceptr va)
{
    unsigned i;

    for (i = 0; i < SHIM_VA_SLOT_CAP; i++) {
        if (g_va_slots[i].gen != 0 && g_va_slots[i].va == va) {
            return (int)i;
        }
    }
    return -1;
}

static int shim_slot_find_by_handle_locked(CUmemGenericAllocationHandle handle)
{
    unsigned i;

    for (i = 0; i < SHIM_VA_SLOT_CAP; i++) {
        if (g_va_slots[i].gen != 0 && g_va_slots[i].has_handle &&
            g_va_slots[i].handle == handle) {
            return (int)i;
        }
    }
    return -1;
}

static int shim_slot_find_pending_create_locked(void)
{
    int best = -1;
    unsigned i;

    for (i = 0; i < SHIM_VA_SLOT_CAP; i++) {
        if (g_va_slots[i].gen != 0 && g_va_slots[i].va != 0 && !g_va_slots[i].has_handle) {
            best = (int)i;
        }
    }
    return best;
}

static void shim_legacy_sync_from_slot_locked(int idx)
{
    if (!shim_slot_index_locked(idx)) {
        memset(&g_reservation_slot, 0, sizeof(g_reservation_slot));
        return;
    }
    const shim_va_slot_t *s = &g_va_slots[idx];

    g_reservation_slot.active = 1;
    g_reservation_slot.va = s->va;
    g_reservation_slot.size = s->size;
    g_reservation_slot.alignment = s->alignment;
    g_reservation_slot.flags = s->res_flags;

    if (s->has_handle) {
        g_alloc_slot.active = 1;
        g_alloc_slot.handle = s->handle;
        g_alloc_slot.rounded_size = s->rounded_size;
        g_alloc_slot.flags = s->alloc_flags;
        g_alloc_slot.mapped = s->mapped;
    } else {
        memset(&g_alloc_slot, 0, sizeof(g_alloc_slot));
    }
}

const shim_reservation_slot_t *shim_reservation_snapshot(void)
{
    return &g_reservation_slot;
}

void shim_reservation_clear(void)
{
    unsigned i;

    pthread_mutex_lock(&g_map_epoch_mu);
    for (i = 0; i < SHIM_VA_SLOT_CAP; i++) {
        memset(&g_va_slots[i], 0, sizeof(g_va_slots[i]));
    }
    g_pending_reserve_idx = -1;
    g_active_slot_count = 0;
    memset(&g_reservation_slot, 0, sizeof(g_reservation_slot));
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_reservation_set(CUdeviceptr va, size_t size, size_t alignment,
                          unsigned long long flags)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_pending_reserve_idx >= 0 && shim_slot_index_locked(g_pending_reserve_idx)) {
        idx = g_pending_reserve_idx;
        g_pending_reserve_idx = -1;
    } else {
        idx = shim_slot_alloc_locked();
        if (idx >= 0) {
            g_active_slot_count++;
        }
    }
    if (idx >= 0) {
        g_va_slots[idx].va = va;
        g_va_slots[idx].size = size;
        g_va_slots[idx].alignment = alignment;
        g_va_slots[idx].res_flags = flags;
        shim_legacy_sync_from_slot_locked(idx);
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

int shim_reservation_active(void)
{
    int active;

    pthread_mutex_lock(&g_map_epoch_mu);
    active = g_active_slot_count > 0;
    pthread_mutex_unlock(&g_map_epoch_mu);
    return active;
}

void shim_slot_clear_all(void)
{
    shim_reservation_clear();
}

void shim_slot_free_by_va(CUdeviceptr va)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        shim_slot_free_locked(idx);
    }
    if (g_pending_reserve_idx >= 0 && !shim_slot_index_locked(g_pending_reserve_idx)) {
        g_pending_reserve_idx = -1;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

int shim_slot_va_index(CUdeviceptr va)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    pthread_mutex_unlock(&g_map_epoch_mu);
    return idx;
}

int shim_slot_handle_index(CUmemGenericAllocationHandle handle)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_handle_locked(handle);
    pthread_mutex_unlock(&g_map_epoch_mu);
    return idx;
}

int shim_slot_pending_create_index(void)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_pending_create_locked();
    pthread_mutex_unlock(&g_map_epoch_mu);
    return idx;
}

int shim_slot_get_va_size(CUdeviceptr va, size_t *out_size)
{
    int idx;
    size_t sz = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        sz = g_va_slots[idx].size;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    if (idx < 0 || !out_size) {
        return 0;
    }
    *out_size = sz;
    return 1;
}

void shim_slot_bind_create(CUmemGenericAllocationHandle handle, size_t rounded,
                           unsigned long long flags)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_pending_create_locked();
    if (idx < 0 && g_pending_reserve_idx >= 0) {
        idx = g_pending_reserve_idx;
    }
    if (idx >= 0 && shim_slot_index_locked(idx)) {
        g_va_slots[idx].has_handle = 1;
        g_va_slots[idx].handle = handle;
        g_va_slots[idx].rounded_size = rounded;
        g_va_slots[idx].alloc_flags = flags;
        shim_legacy_sync_from_slot_locked(idx);
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_slot_mark_mapped(CUdeviceptr va)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        g_va_slots[idx].mapped = 1;
        shim_legacy_sync_from_slot_locked(idx);
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_slot_clear_mapped(CUdeviceptr va)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        g_va_slots[idx].mapped = 0;
        shim_legacy_sync_from_slot_locked(idx);
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_slot_clear_handle(CUmemGenericAllocationHandle handle)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_handle_locked(handle);
    if (idx >= 0) {
        g_va_slots[idx].has_handle = 0;
        g_va_slots[idx].handle = NULL;
        g_va_slots[idx].rounded_size = 0;
        g_va_slots[idx].alloc_flags = 0;
        g_va_slots[idx].mapped = 0;
        shim_legacy_sync_from_slot_locked(idx);
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

int shim_slot_is_mapped(CUdeviceptr va)
{
    int idx;
    int mapped = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        mapped = g_va_slots[idx].mapped;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return mapped;
}

int shim_slot_has_handle(CUdeviceptr va)
{
    int idx;
    int has = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        has = g_va_slots[idx].has_handle;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return has;
}

int shim_reserve_begin_slot(void)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_alloc_locked();
    if (idx >= 0) {
        g_pending_reserve_idx = idx;
        g_active_slot_count++;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return idx;
}

void shim_reserve_abort_pending(void)
{
    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_pending_reserve_idx >= 0) {
        shim_slot_free_locked(g_pending_reserve_idx);
        g_pending_reserve_idx = -1;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

int shim_slot_use_packed_handle(CUmemGenericAllocationHandle *out_handle)
{
    int idx;
    int ok = 0;

    if (!out_handle || !shim_pack_enabled_v1()) {
        return 0;
    }
    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_pending_create_locked();
    if (idx < 0 && g_pending_reserve_idx >= 0) {
        idx = g_pending_reserve_idx;
    }
    if (idx >= 0 && shim_slot_index_locked(idx) &&
        shim_pack_is_packed_subva_locked(g_va_slots[idx].va)) {
        CUmemGenericAllocationHandle h = shim_pack_arena_handle_locked();
        if (h) {
            *out_handle = h;
            ok = 1;
        }
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return ok;
}

void shim_slot_set_req_bytes(size_t req)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = g_pending_reserve_idx;
    if (idx < 0) {
        idx = shim_slot_find_pending_create_locked();
    }
    if (idx >= 0 && shim_slot_index_locked(idx)) {
        g_va_slots[idx].req_bytes = req;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_slot_mark_packed_arena(int on)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = g_pending_reserve_idx;
    if (idx < 0) {
        idx = shim_slot_find_pending_create_locked();
    }
    if (idx >= 0 && shim_slot_index_locked(idx)) {
        g_va_slots[idx].packed_arena = on ? 1 : 0;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_slot_mark_packed_by_va(CUdeviceptr va, int on)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        g_va_slots[idx].packed_arena = on ? 1 : 0;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_slot_set_map_offset(CUdeviceptr va, size_t offset)
{
    int idx;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        g_va_slots[idx].map_offset = offset;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

size_t shim_slot_map_offset(CUdeviceptr va)
{
    int idx;
    size_t off = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        off = g_va_slots[idx].map_offset;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return off;
}

int shim_slot_is_packed_subva(CUdeviceptr va)
{
    int idx;
    int sub = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0 && shim_pack_is_packed_subva_locked(g_va_slots[idx].va)) {
        sub = 1;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return sub;
}

size_t shim_slot_req_bytes_for_va(CUdeviceptr va)
{
    int idx;
    size_t req = 0;

    pthread_mutex_lock(&g_map_epoch_mu);
    idx = shim_slot_find_by_va_locked(va);
    if (idx >= 0) {
        req = g_va_slots[idx].req_bytes;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
    return req;
}
