#define _GNU_SOURCE
#include "shim_internal.h"

#include <dlfcn.h>
#include <stdio.h>

typedef CUresult (*cuMemGetAllocationGranularity_fn)(
    size_t *, const CUmemAllocationProp *, CUmemAllocationGranularity_flags);

void shim_log_granularity_both(const CUmemAllocationProp *prop)
{
    if (!shim_log_enabled() || !prop) {
        return;
    }
    cuMemGetAllocationGranularity_fn real =
        (cuMemGetAllocationGranularity_fn)dlsym(RTLD_NEXT,
                                              "cuMemGetAllocationGranularity");
    if (!real) {
        return;
    }
    size_t min_g = 0;
    size_t rec_g = 0;
    if (real(&min_g, prop, CU_MEM_ALLOC_GRANULARITY_MINIMUM) == CUDA_SUCCESS) {
        fprintf(stderr, "SHIM_TRACE gran MINIMUM value=%zu\n", min_g);
    }
    if (real(&rec_g, prop, CU_MEM_ALLOC_GRANULARITY_RECOMMENDED) == CUDA_SUCCESS) {
        fprintf(stderr, "SHIM_TRACE gran RECOMMENDED value=%zu\n", rec_g);
    }
}
