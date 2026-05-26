/*
 * lib2adic_shim.so — T1 cuMem interposer (A-2.3 stub, pass-through + epoch + log).
 * Contract: SHIM-INVIOLABLE-A-v1.0 · spec A-2.1 parts 1–3.
 */
#define _GNU_SOURCE

#include "shim_internal.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/* RTLD_NEXT misses libcuda in vLLM Docker workers; dlopen fallback for B12 smoke. */
static void *shim_dlsym_driver(const char *sym)
{
    void *p = dlsym(RTLD_NEXT, sym);
    if (p) {
        return p;
    }
    static void *cuda = NULL;
    if (!cuda) {
        cuda = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!cuda) {
            cuda = dlopen("libcuda.so", RTLD_NOW | RTLD_LOCAL);
        }
    }
    if (cuda) {
        p = dlsym(cuda, sym);
    }
    return p;
}

#define RESOLVE(sym, fn_type)                                                    \
    do {                                                                         \
        if (!real_##sym) {                                                       \
            real_##sym = (fn_type)shim_dlsym_driver(#sym);                       \
            if (!real_##sym) {                                                   \
                fprintf(stderr, "SHIM_FATAL dlsym " #sym " failed\n");          \
            }                                                                    \
        }                                                                        \
    } while (0)

typedef CUresult (*cuMemAddressReserve_fn)(CUdeviceptr *, size_t, size_t,
                                           CUdeviceptr, unsigned long long);
typedef CUresult (*cuMemAddressFree_fn)(CUdeviceptr, size_t);
typedef CUresult (*cuMemCreate_fn)(CUmemGenericAllocationHandle *, size_t,
                                   const CUmemAllocationProp *, unsigned long long);
typedef CUresult (*cuMemRelease_fn)(CUmemGenericAllocationHandle);
typedef CUresult (*cuMemMap_fn)(CUdeviceptr, size_t, size_t,
                                CUmemGenericAllocationHandle, unsigned long long);
typedef CUresult (*cuMemUnmap_fn)(CUdeviceptr, size_t);
typedef CUresult (*cuMemSetAccess_fn)(CUdeviceptr, size_t, const CUmemAccessDesc *,
                                      size_t);
typedef CUresult (*cuMemGetAllocationGranularity_fn)(
    size_t *, const CUmemAllocationProp *, CUmemAllocationGranularity_flags);

static cuMemAddressReserve_fn real_cuMemAddressReserve;
static cuMemAddressFree_fn real_cuMemAddressFree;
static cuMemCreate_fn real_cuMemCreate;
static cuMemRelease_fn real_cuMemRelease;
static cuMemMap_fn real_cuMemMap;
static cuMemUnmap_fn real_cuMemUnmap;
static cuMemSetAccess_fn real_cuMemSetAccess;
static cuMemGetAllocationGranularity_fn real_cuMemGetAllocationGranularity;

CUresult cuMemAddressReserve(CUdeviceptr *ptr, size_t size, size_t alignment,
                             CUdeviceptr addr, unsigned long long flags)
{
    hwatom_shim_emit_identity_once();
    shim_reserve_request_t req = {
        .size = size,
        .alignment = alignment,
        .flags = flags,
        .hint_addr = addr,
    };
    size_t reserve_sz = size;
    CUresult v = shim_reserve_validate(&req);
    if (v != CUDA_SUCCESS) {
        return v;
    }
    v = shim_reserve_begin();
    if (v != CUDA_SUCCESS) {
        return v;
    }
    shim_reserve_log_fields(&req);
    shim_slot_set_req_bytes(size);

    {
        size_t gqa_map = 0;

        if (ptr && shim_gqa_try_alias_reserve(ptr, size, &gqa_map)) {
            req.size = gqa_map;
            shim_reserve_commit(*ptr, &req);
            shim_slot_mark_packed_by_va(*ptr, 1);
            shim_slot_set_map_offset(*ptr, shim_pack_map_offset_for_va(*ptr));
            return CUDA_SUCCESS;
        }
    }

    if (size > shim_leaf_bytes_v1()) {
        shim_pack_flush();
    }

    if (shim_pack_enabled_v1() && ptr && size <= shim_leaf_bytes_v1()) {
        size_t map_sz = 0;
        CUdeviceptr pack_va = 0;

        if (shim_pack_try_reserve(&pack_va, size, &map_sz)) {
            *ptr = pack_va;
            req.size = map_sz;
            shim_reserve_commit(pack_va, &req);
            shim_slot_mark_packed_by_va(pack_va, 1);
            shim_slot_set_map_offset(pack_va, shim_pack_map_offset_for_va(pack_va));
            if (shim_gqa_enabled_v1()) {
                size_t off = shim_pack_map_offset_for_va(pack_va);
                shim_gqa_register_leaf(pack_va - (CUdeviceptr)off, size);
            }
            return CUDA_SUCCESS;
        }
        if (shim_pack_take_mega_leaf(&pack_va, &map_sz)) {
            *ptr = pack_va;
            req.size = map_sz;
            shim_reserve_commit(pack_va, &req);
            shim_slot_mark_packed_by_va(pack_va, 1);
            shim_slot_set_map_offset(pack_va, shim_pack_map_offset_for_va(pack_va));
            if (shim_gqa_enabled_v1()) {
                shim_gqa_register_leaf(pack_va, size);
            }
            return CUDA_SUCCESS;
        }
    }

    if (shim_pack_enabled_v1() && size <= shim_leaf_bytes_v1()) {
        reserve_sz = shim_pack_new_reserve_size();
    }
    RESOLVE(cuMemAddressReserve, cuMemAddressReserve_fn);
    CUresult r = real_cuMemAddressReserve(ptr, reserve_sz, alignment, addr, flags);
    if (r == CUDA_SUCCESS && ptr) {
        req.size = reserve_sz;
        shim_reserve_commit(*ptr, &req);
        if (shim_gqa_enabled_v1() && size <= shim_leaf_bytes_v1()) {
            shim_gqa_register_leaf(*ptr, size);
        }
        if (shim_pack_enabled_v1() && shim_pack_mega_enabled_v1() &&
            reserve_sz >= shim_pack_new_reserve_size()) {
            shim_pack_mega_open(*ptr, reserve_sz);
        }
    } else {
        shim_reserve_rollback();
    }
    return r;
}

CUresult cuMemAddressFree(CUdeviceptr ptr, size_t size)
{
    if (shim_slot_va_index(ptr) < 0) {
        shim_trace("reject", "cuMemAddressFree", "va_unknown");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (shim_slot_is_packed_subva(ptr)) {
        shim_slot_free_by_va(ptr);
        shim_trace("exit", "cuMemAddressFree", "packed_sub");
        return CUDA_SUCCESS;
    }
    char detail[64];
    snprintf(detail, sizeof(detail), "ptr=%llu size=%zu", (unsigned long long)ptr,
             size);
    shim_trace("held", "cuMemAddressFree", detail);
    RESOLVE(cuMemAddressFree, cuMemAddressFree_fn);
    CUresult r = real_cuMemAddressFree(ptr, size);
    if (r == CUDA_SUCCESS) {
        shim_slot_free_by_va(ptr);
        shim_trace("exit", "cuMemAddressFree", "ok");
    }
    return r;
}

CUresult cuMemCreate(CUmemGenericAllocationHandle *handle, size_t size,
                     const CUmemAllocationProp *prop, unsigned long long flags)
{
    CUresult v;
    size_t rounded;
    CUmemGenericAllocationHandle packed = NULL;

    (void)prop;
    if (shim_slot_use_packed_handle(&packed)) {
        rounded = shim_leaf_bytes_v1();
        if (handle) {
            *handle = packed;
        }
        shim_pack_retain_handle();
        shim_create_commit(packed, rounded, flags);
        return CUDA_SUCCESS;
    }
    v = shim_create_validate(size);
    if (v != CUDA_SUCCESS) {
        return v;
    }
    rounded = shim_create_round_size(size);
    shim_create_log(size, rounded, flags);
    RESOLVE(cuMemCreate, cuMemCreate_fn);
    v = real_cuMemCreate(handle, rounded, prop, flags);
    if (v == CUDA_SUCCESS && handle) {
        const shim_reservation_slot_t *rs;

        shim_create_commit(*handle, rounded, flags);
        if (shim_pack_enabled_v1()) {
            rs = shim_reservation_snapshot();
            if (rs && rs->active && rs->va) {
                shim_pack_open_arena(rs->va, rounded, *handle);
            }
        }
    }
    return v;
}

CUresult cuMemRelease(CUmemGenericAllocationHandle handle)
{
    CUresult v = shim_release_validate(handle);

    if (v != CUDA_SUCCESS) {
        return v;
    }
    if (shim_pack_release_ref(handle)) {
        shim_release_clear(handle);
        return CUDA_SUCCESS;
    }
    RESOLVE(cuMemRelease, cuMemRelease_fn);
    v = real_cuMemRelease(handle);
    if (v == CUDA_SUCCESS) {
        shim_release_clear(handle);
    }
    return v;
}

CUresult cuMemMap(CUdeviceptr ptr, size_t size, size_t offset,
                  CUmemGenericAllocationHandle handle, unsigned long long flags)
{
    CUresult v = shim_map_validate(ptr, size, offset);

    (void)handle;
    (void)flags;
    if (v != CUDA_SUCCESS) {
        return v;
    }
    if (shim_slot_is_packed_subva(ptr)) {
        shim_map_log(ptr, size, offset);
        shim_map_mark_mapped(ptr);
        return CUDA_SUCCESS;
    }
    {
        size_t hoff = shim_slot_map_offset(ptr);
        shim_map_log(ptr, size, offset + hoff);
        RESOLVE(cuMemMap, cuMemMap_fn);
        v = real_cuMemMap(ptr, size, offset + hoff, handle, flags);
    }
    if (v == CUDA_SUCCESS) {
        shim_map_mark_mapped(ptr);
        if (!shim_slot_is_packed_subva(ptr) && shim_pack_arena_active()) {
            size_t req = shim_slot_req_bytes_for_va(ptr);
            if (req > 0) {
                shim_pack_bump_usage(req);
            }
        }
    }
    return v;
}

CUresult cuMemUnmap(CUdeviceptr ptr, size_t size)
{
    CUresult v = shim_unmap_validate(ptr, size);

    if (v != CUDA_SUCCESS) {
        return v;
    }
    if (shim_slot_is_packed_subva(ptr)) {
        shim_unmap_clear_mapped(ptr);
        return CUDA_SUCCESS;
    }
    RESOLVE(cuMemUnmap, cuMemUnmap_fn);
    v = real_cuMemUnmap(ptr, size);
    if (v == CUDA_SUCCESS) {
        shim_unmap_clear_mapped(ptr);
    }
    return v;
}

CUresult cuMemSetAccess(CUdeviceptr ptr, size_t size, const CUmemAccessDesc *desc,
                        size_t count)
{
    char detail[64];
    size_t acc_sz = size;

    if (shim_slot_is_packed_subva(ptr)) {
        shim_trace("held", "cuMemSetAccess", "packed_sub_skip");
        return CUDA_SUCCESS;
    }
    /*
     * P1 iso: iron may pass logical span (< 2 MiB); driver requires granule-aligned
     * SetAccess on the mapped physical leaf at arena base.
     */
    if (shim_pack_arena_active() && acc_sz > 0 && acc_sz < shim_leaf_bytes_v1()) {
        acc_sz = shim_leaf_bytes_v1();
    }
    snprintf(detail, sizeof(detail), "ptr=%llu count=%zu acc=%zu",
             (unsigned long long)ptr, count, acc_sz);
    shim_trace("held", "cuMemSetAccess", detail);
    RESOLVE(cuMemSetAccess, cuMemSetAccess_fn);
    return real_cuMemSetAccess(ptr, acc_sz, desc, count);
}

void shim_driver_reset_v1(void)
{
    shim_pack_flush();
    shim_pack_reset();
    shim_gqa_reset();
    shim_reservation_clear();
}

static void hwatom_shim_emit_stats_line(void)
{
    const char *build_id = hwatom_shim_build_id();
    size_t peak = shim_pack_committed_peak_bytes();
    size_t fini = shim_pack_committed_bytes();
    unsigned mega_rc = shim_pack_mega_reserve_count();
    unsigned mega_lv = shim_pack_mega_leaf_used();
    const char *json = getenv("HWATOM_SHIM_STATS_JSON");

    if (json && json[0] && json[0] != '0') {
        if (shim_2adic_enabled_v1()) {
            fprintf(stderr,
                    "{\"type\":\"SHIM_STATS\",\"stats_v\":2,\"build_id\":\"%s\","
                    "\"eval_shim\":%d,\"k_cap\":%u,"
                    "\"pack_committed_peak_bytes\":%zu,"
                    "\"pack_committed_fini_bytes\":%zu,"
                    "\"mega_reserve_count\":%u,\"mega_leaves_used\":%u,"
                    "\"shim_2adic_bands\":%u}\n",
                    build_id, shim_pack_eval_build_v1(), shim_pack_k_cap_max_v1(),
                    peak, fini, mega_rc, mega_lv, shim_2adic_band_count());
        } else {
            fprintf(stderr,
                    "{\"type\":\"SHIM_STATS\",\"stats_v\":2,\"build_id\":\"%s\","
                    "\"eval_shim\":%d,\"k_cap\":%u,"
                    "\"pack_committed_peak_bytes\":%zu,"
                    "\"pack_committed_fini_bytes\":%zu,"
                    "\"mega_reserve_count\":%u,\"mega_leaves_used\":%u}\n",
                    build_id, shim_pack_eval_build_v1(), shim_pack_k_cap_max_v1(),
                    peak, fini, mega_rc, mega_lv);
        }
        return;
    }
    if (shim_2adic_enabled_v1()) {
        fprintf(stderr,
                "SHIM_STATS stats_v=2 build_id=%s eval_shim=%d k_cap=%u "
                "pack_committed_bytes=%zu pack_committed_peak_bytes=%zu "
                "pack_committed_fini_bytes=%zu mega_reserve_count=%u "
                "mega_leaves_used=%u shim_2adic_bands=%u\n",
                build_id, shim_pack_eval_build_v1(), shim_pack_k_cap_max_v1(), peak,
                peak, fini, mega_rc, mega_lv, shim_2adic_band_count());
    } else {
        fprintf(stderr,
                "SHIM_STATS stats_v=2 build_id=%s eval_shim=%d k_cap=%u "
                "pack_committed_bytes=%zu pack_committed_peak_bytes=%zu "
                "pack_committed_fini_bytes=%zu mega_reserve_count=%u "
                "mega_leaves_used=%u\n",
                build_id, shim_pack_eval_build_v1(), shim_pack_k_cap_max_v1(), peak,
                peak, fini, mega_rc, mega_lv);
    }
}

void __attribute__((destructor)) hwatom_shim_fini(void)
{
    const char *e = getenv("HWATOM_SHIM_STATS");
    if (!e || e[0] == '0') {
        return;
    }
    hwatom_shim_emit_stats_line();
}

CUresult cuMemGetAllocationGranularity(size_t *granularity,
                                       const CUmemAllocationProp *prop,
                                       CUmemAllocationGranularity_flags option)
{
    char detail[48];
    snprintf(detail, sizeof(detail), "option=%d", (int)option);
    shim_trace("pass", "cuMemGetAllocationGranularity", detail);
    RESOLVE(cuMemGetAllocationGranularity, cuMemGetAllocationGranularity_fn);
    CUresult r = real_cuMemGetAllocationGranularity(granularity, prop, option);
    if (granularity && shim_log_enabled()) {
        fprintf(stderr, "SHIM_TRACE gran value=%zu option=%d\n", *granularity,
                (int)option);
    }
    if (prop && shim_log_enabled()) {
        shim_log_granularity_both(prop);
    }
    return r;
}
