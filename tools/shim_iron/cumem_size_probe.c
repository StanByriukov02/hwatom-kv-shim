#include <cuda.h>
#include <stdio.h>

int main(void)
{
    CUdevice d;
    CUcontext ctx;
    CUmemAllocationProp prop = {0};
    CUmemGenericAllocationHandle h;
    CUresult r;
    const char *name = NULL;

    cuInit(0);
    cuDeviceGet(&d, 0);
    cuCtxCreate(&ctx, 0, d);
    prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = 0;

    r = cuMemCreate(&h, (size_t)((125ull << 20) / 100ull), &prop, 0);
    cuGetErrorName(r, &name);
    printf("create_1p25MiB=%d %s\n", (int)r, name ? name : "?");

    r = cuMemCreate(&h, (size_t)(1u << 21), &prop, 0);
    cuGetErrorName(r, &name);
    printf("create_2MiB=%d %s\n", (int)r, name ? name : "?");
    return 0;
}
