/*
 * A-2.8 / A-2.9 — H100 iron probe (real CUDA Driver API).
 * Modes: q1 (granularity) | chain (reserve→create→map→unmap→free→release)
 * Usage: iron_probe_v1 [--mode q1|chain] [--out results/file.json]
 */
#define _GNU_SOURCE

#include <cuda.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(call, label)                                                     \
    do {                                                                       \
        CUresult _r = (call);                                                  \
        if (_r != CUDA_SUCCESS) {                                              \
            const char *name = NULL;                                           \
            cuGetErrorName(_r, &name);                                         \
            fprintf(stderr, "IRON_FAIL %s %s (%d)\n", label, name ? name : "?", \
                    (int)_r);                                                  \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static void write_ts(FILE *f)
{
    time_t t = time(NULL);
    fprintf(f, "\"ts_utc\":\"%ld\"", (long)t);
}

static int run_q1(const char *outpath)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    CUmemAllocationProp prop = {0};
    size_t min_g = 0;
    size_t rec_g = 0;
    FILE *f = stdout;

    if (outpath && outpath[0]) {
        f = fopen(outpath, "w");
        if (!f) {
            perror("fopen");
            return 1;
        }
    }

    CHECK(cuInit(0), "cuInit");
    CHECK(cuDeviceGet(&dev, 0), "cuDeviceGet");
    CHECK(cuDevicePrimaryCtxRetain(&ctx, dev), "cuDevicePrimaryCtxRetain");
    CHECK(cuCtxSetCurrent(ctx), "cuCtxSetCurrent");

    prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = 0;

    CHECK(cuMemGetAllocationGranularity(&min_g, &prop,
                                        CU_MEM_ALLOC_GRANULARITY_MINIMUM),
          "granularity_MINIMUM");
    CHECK(cuMemGetAllocationGranularity(&rec_g, &prop,
                                        CU_MEM_ALLOC_GRANULARITY_RECOMMENDED),
          "granularity_RECOMMENDED");

    fprintf(f, "{\n  ");
    write_ts(f);
    fprintf(f, ",\n  \"mode\":\"q1\",\n");
    fprintf(f, "  \"granularity_minimum_bytes\":%zu,\n", min_g);
    fprintf(f, "  \"granularity_recommended_bytes\":%zu,\n", rec_g);
    fprintf(f, "  \"assumed_leaf_bytes\":%u,\n", 1u << 21);
    fprintf(f, "  \"match_assumed_2mb\":%s\n}\n",
            (min_g == (1u << 21) || rec_g == (1u << 21)) ? "true" : "false");

    if (f != stdout) {
        fclose(f);
    }
    printf("IRON_Q1_OK min=%zu rec=%zu\n", min_g, rec_g);
    return 0;
}

static int run_chain(const char *outpath)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    CUdeviceptr va = 0;
    CUmemGenericAllocationHandle handle = 0;
    CUmemAllocationProp prop = {0};
    size_t size = (size_t)(1u << 21);
    FILE *f = stdout;

    if (outpath && outpath[0]) {
        f = fopen(outpath, "w");
        if (!f) {
            perror("fopen");
            return 1;
        }
    }

    CHECK(cuInit(0), "cuInit");
    CHECK(cuDeviceGet(&dev, 0), "cuDeviceGet");
    CHECK(cuDevicePrimaryCtxRetain(&ctx, dev), "cuDevicePrimaryCtxRetain");
    CHECK(cuCtxSetCurrent(ctx), "cuCtxSetCurrent");

    prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = 0;

    CHECK(cuMemAddressReserve(&va, size, size, 0, 0), "cuMemAddressReserve");
    CHECK(cuMemCreate(&handle, size, &prop, 0), "cuMemCreate");
    CHECK(cuMemMap(va, size, 0, handle, 0), "cuMemMap");
    CHECK(cuMemUnmap(va, size), "cuMemUnmap");
    CHECK(cuMemRelease(handle), "cuMemRelease");
    CHECK(cuMemAddressFree(va, size), "cuMemAddressFree");

    fprintf(f, "{\n  ");
    write_ts(f);
    fprintf(f, ",\n  \"mode\":\"chain\",\n");
    fprintf(f, "  \"reserve_create_map_ok\":true,\n");
    fprintf(f, "  \"va\":\"0x%llx\",\n", (unsigned long long)va);
    fprintf(f, "  \"size_bytes\":%zu\n}\n", size);

    if (f != stdout) {
        fclose(f);
    }
    printf("IRON_CHAIN_OK va=%llu\n", (unsigned long long)va);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = "q1";
    const char *out = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out = argv[++i];
        }
    }

    if (strcmp(mode, "q1") == 0) {
        return run_q1(out);
    }
    if (strcmp(mode, "chain") == 0) {
        return run_chain(out);
    }
    fprintf(stderr, "usage: iron_probe_v1 --mode q1|chain [--out path.json]\n");
    return 2;
}
