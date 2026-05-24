/* A-2.5 v1.1 — cuMemCreate / Map / Unmap / Release (per-VA slot table). */
#include "shim_internal.h"

#include <stdio.h>

shim_alloc_slot_t g_alloc_slot;

void shim_alloc_clear(void)
{
    shim_slot_clear_all();
}

CUresult shim_create_validate(size_t size)
{
    if (!shim_epoch_held()) {
        shim_trace("reject", "cuMemCreate", "no_epoch");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (shim_slot_pending_create_index() < 0) {
        shim_trace("reject", "cuMemCreate", "no_pending_va");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (size == 0) {
        shim_trace("reject", "cuMemCreate", "size_zero");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    return CUDA_SUCCESS;
}

size_t shim_create_round_size(size_t size)
{
    size_t leaf = shim_leaf_bytes_v1();
    size_t rounded = shim_round_leaf_size(size);

    (void)leaf;
    if (rounded < shim_leaf_bytes_v1()) {
        rounded = shim_leaf_bytes_v1();
    }
    return rounded;
}

int shim_pack_enabled_v1(void)
{
    const char *e = getenv("HWATOM_SHIM_PACK");
    if (!e || !e[0]) {
        return 1;
    }
    return e[0] != '0';
}

void shim_create_log(size_t req_size, size_t rounded, unsigned long long flags)
{
    char detail[96];
    snprintf(detail, sizeof(detail), "req=%zu rounded=%zu flags=%llu", req_size,
             rounded, (unsigned long long)flags);
    shim_trace("held", "cuMemCreate", detail);
}

void shim_create_commit(CUmemGenericAllocationHandle handle, size_t rounded,
                        unsigned long long flags)
{
    shim_slot_bind_create(handle, rounded, flags);
    shim_trace("commit", "cuMemCreate", "ok");
}

CUresult shim_map_validate(CUdeviceptr ptr, size_t size, size_t offset)
{
    size_t va_size = 0;

    if (!shim_epoch_held()) {
        shim_trace("reject", "cuMemMap", "no_epoch");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (shim_slot_va_index(ptr) < 0) {
        shim_trace("reject", "cuMemMap", "va_unknown");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (!shim_slot_has_handle(ptr)) {
        shim_trace("reject", "cuMemMap", "no_handle");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (shim_slot_is_packed_subva(ptr)) {
        return CUDA_SUCCESS;
    }
    if (!shim_slot_get_va_size(ptr, &va_size)) {
        shim_trace("reject", "cuMemMap", "va_mismatch");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (offset + size > va_size) {
        shim_trace("reject", "cuMemMap", "range_overflow");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (size == 0) {
        shim_trace("reject", "cuMemMap", "size_zero");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    return CUDA_SUCCESS;
}

void shim_map_log(CUdeviceptr ptr, size_t size, size_t offset)
{
    char detail[128];
    snprintf(detail, sizeof(detail), "ptr=%llu size=%zu off=%zu", (unsigned long long)ptr,
             size, offset);
    shim_trace("held", "cuMemMap", detail);
}

void shim_map_mark_mapped(CUdeviceptr ptr)
{
    shim_slot_mark_mapped(ptr);
    shim_trace("commit", "cuMemMap", "ok");
}

CUresult shim_unmap_validate(CUdeviceptr ptr, size_t size)
{
    if (!shim_epoch_held()) {
        shim_trace("reject", "cuMemUnmap", "no_epoch");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (!shim_slot_is_mapped(ptr)) {
        shim_trace("held", "cuMemUnmap", "not_mapped_idempotent");
        return CUDA_SUCCESS;
    }
    if (shim_slot_va_index(ptr) < 0) {
        shim_trace("reject", "cuMemUnmap", "va_mismatch");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (size == 0) {
        shim_trace("reject", "cuMemUnmap", "size_zero");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    return CUDA_SUCCESS;
}

void shim_unmap_clear_mapped(CUdeviceptr ptr)
{
    shim_slot_clear_mapped(ptr);
    shim_trace("held", "cuMemUnmap", "ok");
}

CUresult shim_release_validate(CUmemGenericAllocationHandle handle)
{
    if (!shim_epoch_held()) {
        shim_trace("reject", "cuMemRelease", "no_epoch");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (shim_slot_handle_index(handle) < 0) {
        shim_trace("reject", "cuMemRelease", "bad_handle");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    return CUDA_SUCCESS;
}

void shim_release_clear(CUmemGenericAllocationHandle handle)
{
    shim_slot_clear_handle(handle);
    shim_trace("held", "cuMemRelease", "ok");
}

const shim_alloc_slot_t *shim_alloc_snapshot(void)
{
    return &g_alloc_slot;
}
