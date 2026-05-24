/* A-2.4 — cuMemAddressReserve path (validate · epoch · reservation slot). */
#include "shim_internal.h"

#include <stdio.h>

static int shim_is_power_of_two(size_t x)
{
    return x != 0 && (x & (x - 1)) == 0;
}

CUresult shim_reserve_validate(const shim_reserve_request_t *req)
{
    if (!req || req->size == 0) {
        shim_trace("reject", "cuMemAddressReserve", "size_zero");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    if (req->alignment != 0 && !shim_is_power_of_two(req->alignment)) {
        shim_trace("reject", "cuMemAddressReserve", "align_not_pow2");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    return CUDA_SUCCESS;
}

CUresult shim_reserve_begin(void)
{
    if (shim_reserve_begin_slot() < 0) {
        shim_trace("reject", "cuMemAddressReserve", "slot_cap");
        return CUDA_ERROR_NOT_PERMITTED;
    }
    return CUDA_SUCCESS;
}

void shim_reserve_log_fields(const shim_reserve_request_t *req)
{
    char detail[128];
    if (!req) {
        return;
    }
    snprintf(detail, sizeof(detail), "size=%zu align=%zu flags=%llu hint=%llu",
             req->size, req->alignment, (unsigned long long)req->flags,
             (unsigned long long)req->hint_addr);
    shim_trace("held", "cuMemAddressReserve", detail);
}

void shim_reserve_commit(CUdeviceptr va, const shim_reserve_request_t *req)
{
    if (!req) {
        return;
    }
    shim_reservation_set(va, req->size, req->alignment, req->flags);
    char detail[64];
    snprintf(detail, sizeof(detail), "va=%llu", (unsigned long long)va);
    shim_trace("commit", "cuMemAddressReserve", detail);
}

void shim_reserve_rollback(void)
{
    shim_reserve_abort_pending();
    shim_trace("abort", "cuMemAddressReserve", "driver_fail");
}
