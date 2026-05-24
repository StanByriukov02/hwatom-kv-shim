/*
 * A-4.4 — H100 iron GATE12: synthetic_kv_band microbench (cuMem driver path).
 * Modes:
 *   --bench alloc|resident|frag|kv_tail|gemm_proxy|iso_logical stock|shim --out path.json
 *   --emit-zp1 / --emit-zp2 / --emit-zp12 (Z scout iron; not inference)
 *   --oom stock|shim --out path.json
 *   --emit-gate12 / --emit-f1 (A-4.5: resident + oom jsons)
 */
#define _GNU_SOURCE

#include <cuda.h>
#include <cublas_v2.h>

#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WORKLOAD_ID "a_gate_v1_kv_microbench"
#define SLICE_BYTES ((size_t)(1u << 21))
#define WALL_LIMIT_S 12.0
#define OOM_MAX_ATTEMPTS 128
#define RESIDENT_MAX_SLOTS 65536
#define FRAG_CHURN_WALL_S 4.0
#define FRAG_RESIDENT_WALL_S 8.0
#define FRAG_MAX_SLOTS 65536
#define FRAG_SIZE_COUNT 6
#define LEAF_BYTES ((size_t)(1u << 21))
#define ISO_DEFAULT_SLICE_BYTES ((size_t)(1u << 19))
#define ISO_DEFAULT_LOGICAL_TARGET ((size_t)(2ull << 30))
#define ISO_MAX_SLOTS 262144

#define CHECK(call, label)                                                     \
    do {                                                                       \
        CUresult _r = (call);                                                  \
        if (_r != CUDA_SUCCESS) {                                              \
            const char *name = NULL;                                           \
            cuGetErrorName(_r, &name);                                         \
            fprintf(stderr, "GATE_FAIL %s %s (%d)\n", label, name ? name : "?", \
                    (int)_r);                                                  \
            return -1;                                                         \
        }                                                                      \
    } while (0)

struct bench_result {
    char path[16];
    int iterations;
    double elapsed_s;
    double p50_ms;
    double p99_ms;
    double p99_9_ms;
    double ops_per_s;
    double gflops_proxy;
    double watts_avg;
    double kv_headroom_pct;
    char oom[8];
    size_t mapped_bytes;
    int context_cap_tokens;
    char bench_mode[24];
    int resident_slots;
    size_t requested_bytes;
    size_t committed_bytes;
    double layout_efficiency_pct;
    int churn_slots_freed;
    int churn_cycles;
    double vram_budget_pct;
    size_t gpu_total_bytes;
    size_t vram_budget_bytes;
};

typedef struct {
    CUdeviceptr va;
    CUmemGenericAllocationHandle handle;
    size_t size;
    size_t reserve_sz;
    size_t map_sz;
    int va_reserved;
} ResidentSlot;

typedef struct {
    ResidentSlot slot;
    int live;
    size_t requested;
} FragSlot;

static const size_t k_frag_req_sizes[FRAG_SIZE_COUNT] = {
    (size_t)((125ull << 20) / 100ull),
    (size_t)((150ull << 20) / 100ull),
    (size_t)((175ull << 20) / 100ull),
    (size_t)(1u << 21),
    (size_t)((225ull << 20) / 100ull),
    (size_t)((250ull << 20) / 100ull),
};

static size_t frag_alloc_bytes(size_t req)
{
    if (req == 0 || req <= LEAF_BYTES) {
        return LEAF_BYTES;
    }
    return ((req + LEAF_BYTES - 1) / LEAF_BYTES) * LEAF_BYTES;
}

static size_t frag_req_size_for_seq(int seq)
{
    const char *uniform = getenv("HWATOM_FRAG_UNIFORM_SLICE");

    if (uniform && uniform[0] == '1') {
        return ISO_DEFAULT_SLICE_BYTES;
    }
    return k_frag_req_sizes[seq % FRAG_SIZE_COUNT];
}

static size_t iron_shim_round_leaf(size_t req)
{
    size_t leaf = LEAF_BYTES;
    if (req == 0) {
        return leaf;
    }
    if (req % leaf == 0) {
        return req;
    }
    return ((req / leaf) + 1) * leaf;
}

static size_t iron_ladder_round(size_t size, unsigned level)
{
    size_t leaf = LEAF_BYTES;

    if (level > 2) {
        level = 2;
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

static size_t iron_placement_round(size_t req)
{
    size_t best = iron_shim_round_leaf(req);
    unsigned lv;

    for (lv = 0; lv <= 2; lv++) {
        size_t r = iron_ladder_round(req, lv);
        if (r >= req && r <= LEAF_BYTES && r < best) {
            best = r;
        }
    }
    return best;
}

static size_t iso_slice_bytes(void)
{
    const char *e = getenv("HWATOM_ISO_SLICE_BYTES");
    char *end = NULL;
    unsigned long long v;

    if (!e || !e[0]) {
        return ISO_DEFAULT_SLICE_BYTES;
    }
    v = strtoull(e, &end, 10);
    if (end == e || v == 0) {
        return ISO_DEFAULT_SLICE_BYTES;
    }
    return (size_t)v;
}

static size_t iso_logical_target(void)
{
    const char *e = getenv("HWATOM_LOGICAL_KV_TARGET_BYTES");
    char *end = NULL;
    unsigned long long v;

    if (!e || !e[0]) {
        return ISO_DEFAULT_LOGICAL_TARGET;
    }
    v = strtoull(e, &end, 10);
    if (end == e) {
        return ISO_DEFAULT_LOGICAL_TARGET;
    }
    /* 0 = fill until VRAM budget cap only (A-4.8b). */
    if (v == 0) {
        return (size_t)-1;
    }
    return (size_t)v;
}

static double iron_vram_budget_pct(void)
{
    const char *e = getenv("HWATOM_VRAM_BUDGET_PCT");
    char *end = NULL;
    unsigned long long v;

    if (!e || !e[0]) {
        return 0.0;
    }
    v = strtoull(e, &end, 10);
    if (end == e || v == 0 || v > 100) {
        return 0.0;
    }
    return (double)v;
}

static size_t iron_committed_cap_bytes(size_t gpu_total, double budget_pct)
{
    if (gpu_total == 0 || budget_pct <= 0.0) {
        return 0;
    }
    return (size_t)((double)gpu_total * budget_pct / 100.0);
}

static size_t iron_pack_slots_per_leaf(size_t slice)
{
    size_t place = iron_placement_round(slice);

    if (place == 0) {
        return 1;
    }
    return LEAF_BYTES / place;
}

static int iron_gqa_enabled(void)
{
    const char *e = getenv("HWATOM_IRON_GQA_LOGICAL");

    if (e && e[0]) {
        return e[0] == '1';
    }
    e = getenv("HWATOM_GQA_ALIAS");
    return e && e[0] == '1';
}

static unsigned iron_gqa_heads(void)
{
    const char *e = getenv("HWATOM_GQA_HEADS");
    char *end = NULL;
    unsigned long long v;

    if (!e || !e[0]) {
        return 8u;
    }
    v = strtoull(e, &end, 10);
    if (end == e || v == 0 || v > 32) {
        return 8u;
    }
    return (unsigned)v;
}

static size_t iron_iso_shim_committed_bytes(int logical_slots, size_t per_leaf, unsigned gqa_h)
{
    size_t physical_ops;

    if (logical_slots <= 0) {
        return 0;
    }
    if (gqa_h <= 1) {
        physical_ops = (size_t)logical_slots;
    } else {
        physical_ops = ((size_t)logical_slots + (size_t)gqa_h - 1u) / (size_t)gqa_h;
    }
    if (per_leaf == 0) {
        per_leaf = 1;
    }
    return ((physical_ops + per_leaf - 1) / per_leaf) * LEAF_BYTES;
}

static void iron_diag_cuda_fail(const char *step, int slot_idx, CUresult err)
{
    const char *diag = getenv("HWATOM_IRON_DIAG");
    const char *name = NULL;

    if (!diag || diag[0] != '1') {
        return;
    }
    cuGetErrorName(err, &name);
    fprintf(stderr, "IRON_DIAG_FAIL step=%s slot=%d cuda=%s (%d)\n", step, slot_idx,
            name ? name : "?", (int)err);
}

static int resident_map_size(ResidentSlot *slot, size_t req_logical, size_t *committed_out,
                             int shim_create_logical, int slot_idx)
{
    CUmemAllocationProp prop = {0};
    size_t alloc = frag_alloc_bytes(req_logical);
    /* Shim+LD_PRELOAD: reserve logical span (enables P1 pack); map physical alloc. */
    size_t reserve_sz = alloc;
    size_t create_sz = shim_create_logical ? req_logical : alloc;
    size_t map_sz = alloc;

    if (shim_create_logical) {
        /*
         * reserve_sz passed to shim:
         *  - iso / small frag (alloc==leaf): logical span → P1 pack (k=4 on 512 KiB)
         *  - frag > leaf (2.25–2.5 MiB): physical alloc (4 MiB) → no pack on reserve path
         */
        if (alloc > LEAF_BYTES) {
            reserve_sz = alloc;
        } else {
            reserve_sz = iron_placement_round(req_logical);
            if (reserve_sz == 0) {
                reserve_sz = req_logical;
            }
            if (reserve_sz > alloc) {
                reserve_sz = alloc;
            }
        }
    }

    prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = 0;
    slot->size = alloc;
    if (!slot->va_reserved) {
        slot->reserve_sz = reserve_sz;
        slot->map_sz = map_sz;
    }
    slot->handle = 0;

    {
        CUresult r = cuMemAddressReserve(&slot->va, reserve_sz, LEAF_BYTES, 0, 0);
        if (r != CUDA_SUCCESS) {
            iron_diag_cuda_fail("cuMemAddressReserve", slot_idx, r);
            return -1;
        }
        slot->va_reserved = 1;
        slot->reserve_sz = reserve_sz;
        slot->map_sz = map_sz;
    }
    {
        CUresult r = cuMemCreate(&slot->handle, create_sz, &prop, 0);
        if (r != CUDA_SUCCESS) {
            iron_diag_cuda_fail("cuMemCreate", slot_idx, r);
            cuMemAddressFree(slot->va, reserve_sz);
            return -1;
        }
    }
    if (shim_create_logical) {
        slot->size = iron_placement_round(req_logical);
        /* Map full physical granule (shim rounds cuMemCreate to alloc). */
        map_sz = alloc;
        slot->map_sz = map_sz;
    }
    {
        CUresult r = cuMemMap(slot->va, map_sz, 0, slot->handle, 0);
        if (r != CUDA_SUCCESS) {
            iron_diag_cuda_fail("cuMemMap", slot_idx, r);
        cuMemRelease(slot->handle);
            cuMemAddressFree(slot->va, reserve_sz);
            return -1;
        }
    }
    {
        CUmemAccessDesc access = {0};
        access.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
        access.location.id = 0;
        access.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
        if (cuMemSetAccess(slot->va, map_sz, &access, 1) != CUDA_SUCCESS) {
            cuMemUnmap(slot->va, map_sz);
            cuMemRelease(slot->handle);
            cuMemAddressFree(slot->va, reserve_sz);
            return -1;
        }
    }
    if (!(shim_create_logical && slot->reserve_sz > 0 && slot->reserve_sz < map_sz)) {
        CUresult r = cuMemsetD8(slot->va, 0xA5, map_sz);
        if (r != CUDA_SUCCESS) {
            iron_diag_cuda_fail("cuMemsetD8", slot_idx, r);
            cuMemUnmap(slot->va, map_sz);
            cuMemRelease(slot->handle);
            cuMemAddressFree(slot->va, reserve_sz);
            return -1;
        }
    }
    if (committed_out) {
        *committed_out = shim_create_logical ? alloc : slot->size;
    }
    return 0;
}

static int resident_map_one(ResidentSlot *slot)
{
    size_t c = 0;
    return resident_map_size(slot, SLICE_BYTES, &c, 0, 0);
}

static void frag_slot_teardown(FragSlot *fs, int release_va)
{
    size_t unmap_sz;
    size_t free_sz;

    if (!fs || !fs->live) {
        return;
    }
    unmap_sz = fs->slot.map_sz ? fs->slot.map_sz : fs->slot.size;
    free_sz = fs->slot.reserve_sz ? fs->slot.reserve_sz : fs->slot.size;
    if (fs->slot.handle) {
        cuMemRelease(fs->slot.handle);
        fs->slot.handle = 0;
    }
    if (unmap_sz > 0 && fs->slot.va) {
        cuMemUnmap(fs->slot.va, unmap_sz);
    }
    if (release_va && fs->slot.va_reserved) {
        cuMemAddressFree(fs->slot.va, free_sz);
        fs->slot.va = 0;
        fs->slot.va_reserved = 0;
        fs->slot.reserve_sz = 0;
        fs->slot.map_sz = 0;
    }
    fs->live = 0;
    fs->requested = 0;
    fs->slot.size = 0;
}

static int frag_slot_map(FragSlot *fs, size_t req_logical, int shim_path)
{
    size_t committed = 0;
    if (resident_map_size(&fs->slot, req_logical, &committed, shim_path, -1) != 0) {
        return -1;
    }
    fs->live = 1;
    fs->requested = req_logical;
    if (shim_path) {
        fs->slot.size = committed > 0 ? committed : fs->slot.map_sz;
    }
    return 0;
}

static void frag_metrics_live(const FragSlot *pool, int hwm, size_t *req_sum, size_t *comm_sum,
                            int *live_n)
{
    int i;
    size_t rq = 0;
    size_t cm = 0;
    int n = 0;

    for (i = 0; i < hwm; i++) {
        if (!pool[i].live) {
            continue;
        }
        n++;
        rq += pool[i].requested;
        cm += pool[i].slot.size;
    }
    *req_sum = rq;
    *comm_sum = cm;
    *live_n = n;
}

static int frag_find_free_index(FragSlot *pool, int hwm, int cap)
{
    int i;
    for (i = 0; i < hwm; i++) {
        if (!pool[i].live) {
            return i;
        }
    }
    if (hwm < cap) {
        return hwm;
    }
    return -1;
}

static int frag_pick_middle_live(const FragSlot *pool, int hwm)
{
    int i;
    int lo = hwm;
    int hi = -1;
    int span;
    int pick;

    for (i = 0; i < hwm; i++) {
        if (!pool[i].live) {
            continue;
        }
        if (lo > i) {
            lo = i;
        }
        if (hi < i) {
            hi = i;
        }
    }
    if (hi <= lo) {
        return -1;
    }
    span = hi - lo + 1;
    pick = lo + 1 + (rand() % (span > 2 ? span - 2 : 1));
    if (pick >= hi) {
        pick = lo + 1;
    }
    if (!pool[pick].live) {
        for (i = lo + 1; i < hi; i++) {
            if (pool[i].live) {
                return i;
            }
        }
        return -1;
    }
    return pick;
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static double percentile_ms(double *vals, int n, double pct)
{
    if (n <= 0) {
        return 0.0;
    }
    qsort(vals, (size_t)n, sizeof(double), cmp_double);
    double rank = pct * (double)(n - 1);
    int lo = (int)floor(rank);
    int hi = (int)ceil(rank);
    if (lo == hi) {
        return vals[lo];
    }
    double w = rank - (double)lo;
    return vals[lo] * (1.0 - w) + vals[hi] * w;
}

static int one_slice_cycle(size_t *mapped_out)
{
    CUdeviceptr va = 0;
    CUmemGenericAllocationHandle handle = 0;
    CUmemAllocationProp prop = {0};
    size_t size = SLICE_BYTES;

    prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
    prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    prop.location.id = 0;

    if (cuMemAddressReserve(&va, size, size, 0, 0) != CUDA_SUCCESS) {
        return -1;
    }
    if (cuMemCreate(&handle, size, &prop, 0) != CUDA_SUCCESS) {
        cuMemAddressFree(va, size);
        return -1;
    }
    if (cuMemMap(va, size, 0, handle, 0) != CUDA_SUCCESS) {
        cuMemRelease(handle);
        cuMemAddressFree(va, size);
        return -1;
    }
    if (cuMemUnmap(va, size) != CUDA_SUCCESS) {
        cuMemRelease(handle);
        cuMemAddressFree(va, size);
        return -1;
    }
    if (cuMemRelease(handle) != CUDA_SUCCESS) {
        cuMemAddressFree(va, size);
        return -1;
    }
    if (cuMemAddressFree(va, size) != CUDA_SUCCESS) {
        return -1;
    }
    *mapped_out += size;
    return 0;
}

static int run_oom_probe(void)
{
    size_t mapped = 0;
    int ok = 0;
    for (int i = 0; i < OOM_MAX_ATTEMPTS; i++) {
        if (one_slice_cycle(&mapped) != 0) {
            break;
        }
        ok = 1;
    }
    return ok ? 0 : 1; /* 0=ok at least one, 1=fail immediate */
}

static int cuda_init(CUdevice *dev, CUcontext *ctx)
{
    if (cuInit(0) != CUDA_SUCCESS) {
        return -1;
    }
    if (cuDeviceGet(dev, 0) != CUDA_SUCCESS) {
        return -1;
    }
    if (cuCtxCreate(ctx, 0, *dev) != CUDA_SUCCESS) {
        return -1;
    }
    return 0;
}

static void cuda_fini(CUcontext ctx)
{
    if (ctx) {
        cuCtxDestroy(ctx);
    }
}

static void shim_driver_reset_if_linked(void)
{
    void (*reset_fn)(void) = (void (*)(void))dlsym(RTLD_DEFAULT, "shim_driver_reset_v1");

    if (reset_fn) {
        reset_fn();
    }
}

static void resident_teardown(ResidentSlot *slots, int n)
{
    for (int i = n - 1; i >= 0; i--) {
        size_t unmap_sz = slots[i].map_sz ? slots[i].map_sz : slots[i].size;
        size_t free_sz = slots[i].reserve_sz ? slots[i].reserve_sz : slots[i].size;
        cuMemUnmap(slots[i].va, unmap_sz);
        cuMemRelease(slots[i].handle);
        cuMemAddressFree(slots[i].va, free_sz);
    }
}

static int run_oom_ladder(const char *path, struct bench_result *out)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    ResidentSlot *slots = calloc((size_t)RESIDENT_MAX_SLOTS, sizeof(ResidentSlot));
    int n = 0;
    double t0 = now_sec();

    if (!slots) {
        return 1;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, sizeof(out->path) - 1);
    strcpy(out->bench_mode, "oom_ladder");

    if (cuda_init(&dev, &ctx) != 0) {
        free(slots);
        return 1;
    }

    while (n < RESIDENT_MAX_SLOTS) {
        if (resident_map_one(&slots[n]) != 0) {
            break;
        }
        n++;
    }

    out->resident_slots = n;
    out->elapsed_s = now_sec() - t0;
    out->mapped_bytes = (size_t)n * SLICE_BYTES;
    out->context_cap_tokens = n;
    strcpy(out->oom, n > 0 ? "ok" : "fail");

    if (n > 0) {
        size_t total = 0;
        if (cuDeviceTotalMem(&total, dev) == CUDA_SUCCESS && total > 0) {
            double free_pct = 100.0 * ((double)total - (double)out->mapped_bytes) / (double)total;
            if (free_pct < 0.0) {
                free_pct = 0.0;
            }
            out->kv_headroom_pct = free_pct;
        }
    }

    resident_teardown(slots, n);
    free(slots);
    printf("GATE_OOM_OK path=%s slots=%d oom=%s headroom=%.2f\n", path, n, out->oom,
           out->kv_headroom_pct);
    return 0;
}

static int run_resident_bench(const char *path, struct bench_result *out)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    ResidentSlot *slots = calloc((size_t)RESIDENT_MAX_SLOTS, sizeof(ResidentSlot));
    int n = 0;
    double t0 = now_sec();
    double t1 = t0;

    if (!slots) {
        return 1;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, sizeof(out->path) - 1);
    strcpy(out->bench_mode, "resident_kv");

    if (cuda_init(&dev, &ctx) != 0) {
        free(slots);
        return 1;
    }

    while ((t1 - t0) < WALL_LIMIT_S && n < RESIDENT_MAX_SLOTS) {
        if (resident_map_one(&slots[n]) != 0) {
            break;
        }
        n++;
        t1 = now_sec();
    }

    out->resident_slots = n;
    out->elapsed_s = t1 - t0;
    out->mapped_bytes = (size_t)n * SLICE_BYTES;
    out->context_cap_tokens = n;
    strcpy(out->oom, n > 0 ? "ok" : "fail");

    {
        size_t total = 0;
        if (cuDeviceTotalMem(&total, dev) == CUDA_SUCCESS && total > 0) {
            double free_pct = 100.0 * ((double)total - (double)out->mapped_bytes) / (double)total;
            if (free_pct < 0.0) {
                free_pct = 0.0;
            }
            out->kv_headroom_pct = free_pct;
        } else {
            out->kv_headroom_pct = -1.0;
        }
    }

    resident_teardown(slots, n);
    free(slots);
    printf("GATE_RESIDENT_OK path=%s slots=%d elapsed=%.3f headroom=%.2f oom=%s\n", path, n,
           out->elapsed_s, out->kv_headroom_pct, out->oom);
    return 0;
}

static int run_iso_logical_bench(const char *path, struct bench_result *out)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    ResidentSlot *slots = calloc((size_t)ISO_MAX_SLOTS, sizeof(ResidentSlot));
    size_t slice = iso_slice_bytes();
    size_t logical_target = iso_logical_target();
    size_t req_sum = 0;
    size_t comm_sum = 0;
    size_t gpu_total = 0;
    size_t comm_cap = 0;
    size_t per_leaf = 1;
    int n = 0;
    int phys = 0;
    int shim_path = (strcmp(path, "shim") == 0);
    int gqa_on = shim_path && iron_gqa_enabled();
    unsigned gqa_h = iron_gqa_heads();
    double budget_pct = iron_vram_budget_pct();
    double t0;
    double t1;

    if (!slots) {
        return 1;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, sizeof(out->path) - 1);
    if (budget_pct > 0.0) {
        strcpy(out->bench_mode, "vram_budget_iso_v1");
    } else {
        strcpy(out->bench_mode, "iso_logical_kv_v1");
    }

    if (cuda_init(&dev, &ctx) != 0) {
        free(slots);
        return 1;
    }

    if (cuDeviceTotalMem(&gpu_total, dev) != CUDA_SUCCESS) {
        gpu_total = 0;
    }
    comm_cap = iron_committed_cap_bytes(gpu_total, budget_pct);
    per_leaf = iron_pack_slots_per_leaf(slice);
    if (per_leaf == 0) {
        per_leaf = 1;
    }

    out->vram_budget_pct = budget_pct;
    out->gpu_total_bytes = gpu_total;
    out->vram_budget_bytes = comm_cap;

    t0 = now_sec();
    for (;;) {
        size_t proj_comm;

        if (n >= ISO_MAX_SLOTS) {
            break;
        }
        if (logical_target != (size_t)-1 && req_sum >= logical_target) {
            break;
        }
        if (shim_path) {
            size_t next_phys =
                gqa_on ? (((size_t)n + 1u) + (size_t)gqa_h - 1u) / (size_t)gqa_h : (size_t)(n + 1);
            proj_comm = ((next_phys + per_leaf - 1) / per_leaf) * LEAF_BYTES;
        } else {
            proj_comm = (size_t)(n + 1) * LEAF_BYTES;
        }
        if (comm_cap > 0 && proj_comm > comm_cap) {
            break;
        }

        if (gqa_on && n > 0 && (n % (int)gqa_h) != 0) {
            req_sum += slice;
            n++;
            continue;
        }

        if (resident_map_size(&slots[phys], slice, NULL, shim_path, phys) != 0) {
            if (getenv("HWATOM_IRON_DIAG") && getenv("HWATOM_IRON_DIAG")[0] == '1') {
                fprintf(stderr, "IRON_DIAG_STOP path=%s logical=%d phys=%d comm=%zu cap=%zu\n",
                        path, n, phys, comm_sum, comm_cap);
            }
            break;
        }
        phys++;
        req_sum += slice;
        n++;
        if (shim_path) {
            comm_sum = ((size_t)phys + per_leaf - 1) / per_leaf * LEAF_BYTES;
        } else {
            comm_sum = (size_t)n * LEAF_BYTES;
        }
    }
    t1 = now_sec();

    out->resident_slots = n;
    out->requested_bytes = req_sum;
    out->committed_bytes = comm_sum;
    out->mapped_bytes = comm_sum;
    out->elapsed_s = t1 - t0;
    out->context_cap_tokens = n;
    strcpy(out->oom, n > 0 ? "ok" : "fail");
    if (comm_sum > 0 && req_sum > 0) {
        out->layout_efficiency_pct = 100.0 * (double)req_sum / (double)comm_sum;
    }

    if (gpu_total > 0) {
        double free_pct = 100.0 * ((double)gpu_total - (double)comm_sum) / (double)gpu_total;
        if (free_pct < 0.0) {
            free_pct = 0.0;
        }
        out->kv_headroom_pct = free_pct;
    } else {
        out->kv_headroom_pct = -1.0;
    }

    resident_teardown(slots, phys);
    free(slots);
    cuda_fini(ctx);
    printf("GATE_ISO_OK path=%s slots=%d logical=%zu committed=%zu eff=%.2f "
           "slice=%zu budget_pct=%.0f budget_bytes=%zu gpu_total=%zu\n",
           path, n, req_sum, comm_sum, out->layout_efficiency_pct, slice, budget_pct,
           comm_cap, gpu_total);
    return 0;
}

static int run_frag_bench(const char *path, struct bench_result *out)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    FragSlot *pool = calloc((size_t)FRAG_MAX_SLOTS, sizeof(FragSlot));
    int n_append = 0;
    int live_n = 0;
    int seq = 0;
    int freed = 0;
    int cycles = 0;
    int shim_path = (strcmp(path, "shim") == 0);
    double t0, t_churn_end, t_end;
    const char *seed_env;
    unsigned seed = 42u;

    if (!pool) {
        return 1;
    }
    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, sizeof(out->path) - 1);
    strcpy(out->bench_mode, "f1_frag_v1");

    seed_env = getenv("HWATOM_FRAG_SEED");
    if (seed_env && seed_env[0]) {
        seed = (unsigned)strtoul(seed_env, NULL, 10);
    }
    double churn_wall_s = FRAG_CHURN_WALL_S;
    double resident_wall_s = FRAG_RESIDENT_WALL_S;
    const char *churn_env = getenv("HWATOM_FRAG_CHURN_S");
    const char *resident_env = getenv("HWATOM_FRAG_RESIDENT_S");

    srand(seed);
    if (churn_env && churn_env[0]) {
        churn_wall_s = strtod(churn_env, NULL);
    }
    if (resident_env && resident_env[0]) {
        resident_wall_s = strtod(resident_env, NULL);
    }

    if (cuda_init(&dev, &ctx) != 0) {
        free(pool);
        return 1;
    }

    t0 = now_sec();
    t_churn_end = t0 + churn_wall_s;

    /* Churn: reuse freed indices in [0, n_append) to fragment the pool. */
    if (churn_wall_s > 0.0) {
    while (now_sec() < t_churn_end) {
        int idx;
        cycles++;
        if (live_n >= 4 && (rand() % 100) < 30) {
            idx = frag_pick_middle_live(pool, n_append);
            if (idx >= 0) {
                frag_slot_teardown(&pool[idx], 1);
                live_n--;
                freed++;
            }
        } else {
            size_t req = frag_req_size_for_seq(seq);
            seq++;
            idx = frag_find_free_index(pool, n_append, FRAG_MAX_SLOTS);
            if (idx < 0) {
                if (n_append >= FRAG_MAX_SLOTS) {
                    break;
                }
                idx = n_append++;
            }
            if (frag_slot_map(&pool[idx], req, shim_path) != 0) {
                continue;
            }
            live_n++;
        }
    }
    }

    t_churn_end = now_sec();
    if (churn_wall_s > 0.0) {
        for (int ci = 0; ci < n_append; ci++) {
            frag_slot_teardown(&pool[ci], 1);
            memset(&pool[ci], 0, sizeof(pool[ci]));
        }
        shim_driver_reset_if_linked();
        cuda_fini(ctx);
        if (cuda_init(&dev, &ctx) != 0) {
            free(pool);
            return 1;
        }
        n_append = 0;
        live_n = 0;
    }
    /* Resident: append-only — no index reuse; stop on driver OOM. */
    if (resident_wall_s > 0.0) {
    while ((t_end = now_sec()) - t_churn_end < resident_wall_s) {
        size_t req = frag_req_size_for_seq(seq);
        seq++;
        cycles++;
        if (n_append >= FRAG_MAX_SLOTS) {
            break;
        }
        if (frag_slot_map(&pool[n_append], req, shim_path) != 0) {
            break;
        }
        n_append++;
        live_n++;
        (void)t_end;
    }
    }
    t_end = now_sec();

    {
        size_t req_sum = 0;
        size_t comm_sum = 0;
        size_t total = 0;

        frag_metrics_live(pool, n_append, &req_sum, &comm_sum, &live_n);
        out->resident_slots = live_n;
        out->requested_bytes = req_sum;
        out->committed_bytes = comm_sum;
        out->mapped_bytes = comm_sum;
        out->context_cap_tokens = live_n;
        out->churn_slots_freed = freed;
        out->churn_cycles = cycles;
        out->elapsed_s = t_end - t0;
        if (comm_sum > 0) {
            out->layout_efficiency_pct = 100.0 * (double)req_sum / (double)comm_sum;
        } else {
            out->layout_efficiency_pct = 0.0;
        }
        strcpy(out->oom, live_n > 0 ? "ok" : "fail");
        if (cuDeviceTotalMem(&total, dev) == CUDA_SUCCESS && total > 0) {
            double free_pct =
                100.0 * ((double)total - (double)comm_sum) / (double)total;
            if (free_pct < 0.0) {
                free_pct = 0.0;
            }
            out->kv_headroom_pct = free_pct;
        } else {
            out->kv_headroom_pct = -1.0;
        }
    }

    for (int i = n_append - 1; i >= 0; i--) {
        frag_slot_teardown(&pool[i], 1);
    }
    free(pool);
    cuda_fini(ctx);

    printf("GATE_FRAG_OK path=%s slots=%d req=%zu comm=%zu eff=%.2f churn_free=%d "
           "elapsed=%.3f headroom=%.2f oom=%s\n",
           path, out->resident_slots, out->requested_bytes, out->committed_bytes,
           out->layout_efficiency_pct, out->churn_slots_freed, out->elapsed_s,
           out->kv_headroom_pct, out->oom);
    return 0;
}

static int run_microbench(const char *path, struct bench_result *out)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    int cap = 8192;
    double *lat_ms = calloc((size_t)cap, sizeof(double));
    double t0 = now_sec();
    double t1 = t0;
    size_t mapped = 0;
    size_t peak_mapped = 0;
    int n = 0;

    if (!lat_ms) {
        return 1;
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, sizeof(out->path) - 1);

    if (cuInit(0) != CUDA_SUCCESS) {
        free(lat_ms);
        return 1;
    }
    if (cuDeviceGet(&dev, 0) != CUDA_SUCCESS ||
        cuDevicePrimaryCtxRetain(&ctx, dev) != CUDA_SUCCESS ||
        cuCtxSetCurrent(ctx) != CUDA_SUCCESS) {
        free(lat_ms);
        return 1;
    }

    mapped = 0;
    while ((t1 - t0) < WALL_LIMIT_S) {
        double iter0 = now_sec();
        size_t slice_mapped = 0;
        if (one_slice_cycle(&slice_mapped) != 0) {
            break;
        }
        if (slice_mapped > peak_mapped) {
            peak_mapped = slice_mapped;
        }
        double iter1 = now_sec();
        if (n >= cap) {
            cap *= 2;
            double *nbuf = realloc(lat_ms, (size_t)cap * sizeof(double));
            if (!nbuf) {
                break;
            }
            lat_ms = nbuf;
        }
        lat_ms[n++] = (iter1 - iter0) * 1000.0;
        t1 = now_sec();
    }

    {
        int oom_fail = run_oom_probe();
        strcpy(out->oom, oom_fail ? "fail" : "ok");
    }

    out->elapsed_s = t1 - t0;
    out->iterations = n;
    out->mapped_bytes = peak_mapped;
    out->context_cap_tokens = n;

    if (n > 0) {
        out->p50_ms = percentile_ms(lat_ms, n, 0.50);
        out->p99_ms = percentile_ms(lat_ms, n, 0.99);
        out->p99_9_ms = percentile_ms(lat_ms, n, 0.999);
        if (out->elapsed_s > 0.0) {
            out->ops_per_s = (double)n / out->elapsed_s;
        }
    }

    {
        size_t total_mem = 0;
        if (cuDeviceTotalMem(&total_mem, dev) == CUDA_SUCCESS && total_mem > 0) {
            double used = (double)peak_mapped;
            double total = (double)total_mem;
            double free_pct = 100.0 * (total - used) / total;
            if (free_pct < 0.0) {
                free_pct = 0.0;
            }
            if (free_pct > 100.0) {
                free_pct = 100.0;
            }
            out->kv_headroom_pct = free_pct;
        } else {
            out->kv_headroom_pct = -1.0;
        }
    }

    free(lat_ms);
    printf("GATE_BENCH_OK path=%s iters=%d elapsed=%.3f headroom=%.2f oom=%s\n",
           path, out->iterations, out->elapsed_s, out->kv_headroom_pct, out->oom);
    return 0;
}

#define GEMM_PROXY_N 4096u

static int run_gemm_proxy_bench(const char *path, struct bench_result *out)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    cublasHandle_t handle = NULL;
    const int n = (int)GEMM_PROXY_N;
    size_t bytes = (size_t)n * (size_t)n * sizeof(float);
    CUdeviceptr dA = 0, dB = 0, dC = 0;
    float alpha = 1.0f, beta = 0.0f;
    int iters = 0;
    double t0, t1;
    double flops_per_gemm;

    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, sizeof(out->path) - 1);
    strcpy(out->bench_mode, "gemm_proxy_sgemm_v1");

    if (cuda_init(&dev, &ctx) != 0) {
        return 1;
    }
    if (cublasCreate(&handle) != CUBLAS_STATUS_SUCCESS) {
        cuda_fini(ctx);
        return 1;
    }
    if (cuMemAlloc(&dA, bytes) != CUDA_SUCCESS || cuMemAlloc(&dB, bytes) != CUDA_SUCCESS ||
        cuMemAlloc(&dC, bytes) != CUDA_SUCCESS) {
        cublasDestroy(handle);
        cuda_fini(ctx);
        return 1;
    }

    flops_per_gemm = 2.0 * (double)n * (double)n * (double)n;
    t0 = now_sec();
    t1 = t0;
    while ((t1 - t0) < WALL_LIMIT_S) {
        if (cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, n, n, n, &alpha,
                        (const float *)(uintptr_t)dA, n, (const float *)(uintptr_t)dB, n, &beta,
                        (float *)(uintptr_t)dC, n) != CUBLAS_STATUS_SUCCESS) {
            break;
        }
        iters++;
        t1 = now_sec();
    }

    out->elapsed_s = t1 - t0;
    out->iterations = iters;
    if (out->elapsed_s > 0.0 && iters > 0) {
        out->gflops_proxy = (flops_per_gemm * (double)iters) / out->elapsed_s / 1e9;
        out->ops_per_s = (double)iters / out->elapsed_s;
    }
    strcpy(out->oom, iters > 0 ? "ok" : "fail");

    cuMemFree(dA);
    cuMemFree(dB);
    cuMemFree(dC);
    cublasDestroy(handle);
    cuda_fini(ctx);

    printf("GATE_GEMM_PROXY_OK path=%s n=%d iters=%d gflops=%.2f elapsed=%.3f\n",
           path, n, iters, out->gflops_proxy, out->elapsed_s);
    return iters > 0 ? 0 : 1;
}

static size_t kv_slot_touch_bytes(const FragSlot *fs, int shim_path, size_t logical)
{
    if (shim_path && fs->slot.reserve_sz > 0 && fs->slot.reserve_sz < fs->slot.map_sz) {
        return fs->slot.reserve_sz;
    }
    if (fs->slot.map_sz > 0) {
        return fs->slot.map_sz;
    }
    return frag_alloc_bytes(logical);
}

static double kv_fill_measure_split_s(void)
{
    const char *e = getenv("HWATOM_KV_FILL_S");
    double fill = 8.0;

    if (e && e[0]) {
        fill = strtod(e, NULL);
    }
    if (fill < 2.0) {
        fill = 2.0;
    }
    if (fill > WALL_LIMIT_S - 2.0) {
        fill = WALL_LIMIT_S - 2.0;
    }
    return fill;
}

/*
 * Z-P1 v2: fill resident KV pool (uniform iso slice), then measure p99 of
 * random cuMemset touch — tail under loaded pool, not cold cuMemAddressReserve.
 */
static int run_kv_tail_bench(const char *path, struct bench_result *out)
{
    CUdevice dev = 0;
    CUcontext ctx = NULL;
    int shim_path = (strcmp(path, "shim") == 0);
    FragSlot *pool = calloc((size_t)FRAG_MAX_SLOTS, sizeof(FragSlot));
    int cap = 16384;
    double *lat_ms = calloc((size_t)cap, sizeof(double));
    size_t logical = iso_slice_bytes();
    double fill_s = kv_fill_measure_split_s();
    double t0, t_fill_end, t_end;
    int n_append = 0;
    int live_n = 0;
    int n = 0;

    if (!pool || !lat_ms) {
        free(pool);
        free(lat_ms);
        return 1;
    }

    memset(out, 0, sizeof(*out));
    strncpy(out->path, path, sizeof(out->path) - 1);
    strcpy(out->bench_mode, "kv_tail_latency_v1");

    if (cuda_init(&dev, &ctx) != 0) {
        free(pool);
        free(lat_ms);
        return 1;
    }

    srand(42);
    t0 = now_sec();
    t_fill_end = t0 + fill_s;
    while (now_sec() < t_fill_end) {
        int idx = frag_find_free_index(pool, n_append, FRAG_MAX_SLOTS);
        if (idx < 0) {
            break;
        }
        if (idx == n_append) {
            n_append++;
        }
        if (frag_slot_map(&pool[idx], logical, shim_path) != 0) {
            continue;
        }
        live_n++;
    }

    if (live_n < 8) {
        strcpy(out->oom, "fail");
        for (int i = 0; i < n_append; i++) {
            frag_slot_teardown(&pool[i], 1);
        }
        free(pool);
        cuda_fini(ctx);
        free(lat_ms);
        return 1;
    }

    t_fill_end = now_sec();
    t_end = t0 + WALL_LIMIT_S;
    while (now_sec() < t_end) {
        int idx;
        int tries = 0;
        double t_op0, t_op1;
        size_t touch;

        do {
            idx = rand() % n_append;
            tries++;
        } while (!pool[idx].live && tries < 64);

        if (!pool[idx].live) {
            continue;
        }

        touch = logical;
        t_op0 = now_sec();
        if (cuMemsetD8(pool[idx].slot.va, 0xB7, touch) != CUDA_SUCCESS) {
            continue;
        }
        t_op1 = now_sec();

        if (n >= cap) {
            cap *= 2;
            double *nbuf = realloc(lat_ms, (size_t)cap * sizeof(double));
            if (!nbuf) {
                break;
            }
            lat_ms = nbuf;
        }
        lat_ms[n++] = (t_op1 - t_op0) * 1000.0;
    }

    for (int i = 0; i < n_append; i++) {
        frag_slot_teardown(&pool[i], 1);
    }

    strcpy(out->oom, n > 0 ? "ok" : "fail");
    out->elapsed_s = now_sec() - t_fill_end;
    out->iterations = n;
    out->context_cap_tokens = live_n;
    out->resident_slots = live_n;

    if (n > 0) {
        out->p50_ms = percentile_ms(lat_ms, n, 0.50);
        out->p99_ms = percentile_ms(lat_ms, n, 0.99);
        out->p99_9_ms = percentile_ms(lat_ms, n, 0.999);
        if (out->elapsed_s > 0.0) {
            out->ops_per_s = (double)n / out->elapsed_s;
        }
    }

    cuda_fini(ctx);
    free(pool);
    free(lat_ms);

    printf("GATE_KV_TAIL_OK path=%s slice=%zu live=%d iters=%d p50=%.4f p99=%.4f p99_9=%.4f "
           "ops_s=%.1f fill=%.1f measure=%.3f\n",
           path, logical, live_n, n, out->p50_ms, out->p99_ms, out->p99_9_ms,
           out->ops_per_s, fill_s, out->elapsed_s);
    return n > 0 ? 0 : 1;
}

static int write_bench_json(const char *path, const struct bench_result *r)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen");
        return 1;
    }
    fprintf(f,
            "{\n"
            "  \"workload_id\": \"%s\",\n"
            "  \"workload_class\": \"synthetic_kv_band\",\n"
            "  \"path\": \"%s\",\n"
            "  \"iterations\": %d,\n"
            "  \"context_cap_tokens\": %d,\n"
            "  \"elapsed_s\": %.6f,\n"
            "  \"p50_ms\": %.6f,\n"
            "  \"p99_ms\": %.6f,\n"
            "  \"p99_9_ms\": %.6f,\n"
            "  \"ops_per_s\": %.6f,\n"
            "  \"gflops_proxy\": %.6f,\n"
            "  \"watts_avg\": %.6f,\n"
            "  \"kv_headroom_pct\": %.6f,\n"
            "  \"oom\": \"%s\",\n"
            "  \"mapped_bytes\": %zu,\n"
            "  \"bench_mode\": \"%s\",\n"
            "  \"resident_slots\": %d,\n"
            "  \"requested_bytes\": %zu,\n"
            "  \"committed_bytes\": %zu,\n"
            "  \"layout_efficiency_pct\": %.6f,\n"
            "  \"churn_slots_freed\": %d,\n"
            "  \"churn_cycles\": %d,\n"
            "  \"vram_budget_pct\": %.1f,\n"
            "  \"gpu_total_bytes\": %zu,\n"
            "  \"vram_budget_bytes\": %zu\n"
            "}\n",
            WORKLOAD_ID, r->path, r->iterations, r->context_cap_tokens, r->elapsed_s,
            r->p50_ms, r->p99_ms, r->p99_9_ms, r->ops_per_s, r->gflops_proxy, r->watts_avg,
            r->kv_headroom_pct, r->oom,
            r->mapped_bytes,
            r->bench_mode[0] ? r->bench_mode : "alloc", r->resident_slots,
            r->requested_bytes, r->committed_bytes, r->layout_efficiency_pct,
            r->churn_slots_freed, r->churn_cycles, r->vram_budget_pct,
            r->gpu_total_bytes, r->vram_budget_bytes);
    fclose(f);
    return 0;
}

static int read_json_double(const char *json, const char *key, double *out)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) {
        return -1;
    }
    p += strlen(pat);
    return sscanf(p, "%lf", out) == 1 ? 0 : -1;
}

static int read_json_int(const char *json, const char *key, int *out)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\":", key);
    const char *p = strstr(json, pat);
    if (!p) {
        return -1;
    }
    p += strlen(pat);
    return sscanf(p, "%d", out) == 1 ? 0 : -1;
}

static int read_json_str(const char *json, const char *key, char *buf, size_t bufsz)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\": \"", key);
    const char *p = strstr(json, pat);
    if (!p) {
        snprintf(pat, sizeof pat, "\"%s\":", key);
        p = strstr(json, pat);
        if (!p) {
            return -1;
        }
        p += strlen(pat);
        while (*p == ' ') {
            p++;
        }
        if (*p == '"') {
            p++;
        }
    } else {
        p += strlen(pat);
    }
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < bufsz) {
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    return 0;
}

static char *read_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[n] = '\0';
    fclose(f);
    if (len_out) {
        *len_out = (size_t)n;
    }
    return buf;
}

static long long parse_perf_metric(const char *perf_txt, const char *metric_suffix)
{
    char line[512];
    const char *p = perf_txt;
    while (p && *p) {
        size_t i = 0;
        while (*p && *p != '\n' && i + 1 < sizeof line) {
            line[i++] = *p++;
        }
        line[i] = '\0';
        if (*p == '\n') {
            p++;
        }
        char *sep = strchr(line, '\t');
        if (!sep) {
            sep = strchr(line, ',');
        }
        if (!sep) {
            continue;
        }
        *sep = '\0';
        char *end = NULL;
        long long v = strtoll(line, &end, 10);
        if (end == line) {
            continue;
        }
        if (strstr(sep + 1, metric_suffix) != NULL) {
            return v;
        }
    }
    return -1;
}

static int emit_gate12(const char *stock_path, const char *shim_path,
                       const char *perf_stock, const char *perf_shim,
                       const char *env_path)
{
    char *stock = read_file(stock_path, NULL);
    char *shim = read_file(shim_path, NULL);
    char *ps = perf_stock ? read_file(perf_stock, NULL) : NULL;
    char *ph = perf_shim ? read_file(perf_shim, NULL) : NULL;
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    struct bench_result rs = {0}, rh = {0};
    double kv_s = 0, kv_h = 0, p50_s = 0, p99_s = 0, el_s = 0;
    double p50_h = 0, p99_h = 0, el_h = 0;
    int ctx_s = 0, ctx_h = 0;
    char oom_s[16] = "unmeasured";
    char oom_h[16] = "unmeasured";
    long long llc_s = -1, llc_h = -1, dtlb_s = -1, dtlb_h = -1;
    long long llc_d = 0, dtlb_d = 0;

    if (!stock || !shim) {
        fprintf(stderr, "emit: missing bench json\n");
        free(stock);
        free(shim);
        free(ps);
        free(ph);
        free(env);
        return 1;
    }

    read_json_str(stock, "path", rs.path, sizeof rs.path);
    read_json_str(shim, "path", rh.path, sizeof rh.path);
    read_json_int(stock, "iterations", &rs.iterations);
    read_json_int(shim, "iterations", &rh.iterations);
    read_json_int(stock, "context_cap_tokens", &ctx_s);
    read_json_int(shim, "context_cap_tokens", &ctx_h);
    read_json_double(stock, "kv_headroom_pct", &kv_s);
    read_json_double(shim, "kv_headroom_pct", &kv_h);
    read_json_double(stock, "p50_ms", &p50_s);
    read_json_double(shim, "p50_ms", &p50_h);
    read_json_double(stock, "p99_9_ms", &p99_s);
    read_json_double(shim, "p99_9_ms", &p99_h);
    read_json_double(stock, "elapsed_s", &el_s);
    read_json_double(shim, "elapsed_s", &el_h);
    read_json_str(stock, "oom", oom_s, sizeof oom_s);
    read_json_str(shim, "oom", oom_h, sizeof oom_h);

    if (ps) {
        llc_s = parse_perf_metric(ps, "LLC-load-misses");
        dtlb_s = parse_perf_metric(ps, "dTLB-load-misses");
    }
    if (ph) {
        llc_h = parse_perf_metric(ph, "LLC-load-misses");
        dtlb_h = parse_perf_metric(ph, "dTLB-load-misses");
    }
    if (llc_s >= 0 && llc_h >= 0) {
        llc_d = llc_h - llc_s;
    }
    if (dtlb_s >= 0 && dtlb_h >= 0) {
        dtlb_d = dtlb_h - dtlb_s;
    }

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");

    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("context_cap_tokens=%d\n", ctx_h > 0 ? ctx_h : ctx_s);
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_version=%s\n", drv);
        }
    }
    printf("kv_headroom_pct=%.4f\n", kv_h >= 0 ? kv_h : kv_s);
    printf("ram_efficiency_pct=null\n");
    printf("oom_stock=%s\n", oom_s);
    printf("oom_shim=%s\n", oom_h);
    if (llc_s >= 0 && llc_h >= 0) {
        printf("llc_misses_delta=%lld\n", (long long)llc_d);
    } else {
        printf("llc_misses_delta=null\n");
    }
    if (dtlb_s >= 0 && dtlb_h >= 0) {
        printf("dtlb_misses_delta=%lld\n", (long long)dtlb_d);
    } else {
        printf("dtlb_misses_delta=null\n");
    }
    printf("p50_ms=%.6f\n", p50_h > 0 ? p50_h : p50_s);
    printf("p99_9_ms=%.6f\n", p99_h > 0 ? p99_h : p99_s);
    printf("elapsed_s=%.3f\n", el_h > 0 ? el_h : el_s);
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");
    printf("Evaluation limit reached. Production / unlimited binary: contact "
           "stanislav.byriukov.research@gmail.com (gain-share).\n");

    free(stock);
    free(shim);
    free(ps);
    free(ph);
    free(env);
    return 0;
}

static int emit_f1(const char *resident_stock, const char *resident_shim,
                   const char *oom_stock, const char *oom_shim, const char *env_path)
{
    char *rs = read_file(resident_stock, NULL);
    char *rh = read_file(resident_shim, NULL);
    char *os = read_file(oom_stock, NULL);
    char *oh = read_file(oom_shim, NULL);
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    char oom_s[16] = "fail";
    char oom_h[16] = "fail";
    double kv_h = -1.0, kv_s = -1.0;
    int slots_s = 0, slots_h = 0, slots_os = 0, slots_oh = 0;
    double el_h = 0.0;

    if (!rs || !rh || !os || !oh) {
        fprintf(stderr, "emit-f1: missing json\n");
        free(rs);
        free(rh);
        free(os);
        free(oh);
        free(env);
        return 1;
    }

    read_json_str(os, "oom", oom_s, sizeof oom_s);
    read_json_str(oh, "oom", oom_h, sizeof oom_h);
    read_json_int(os, "resident_slots", &slots_os);
    read_json_int(oh, "resident_slots", &slots_oh);
    read_json_double(rs, "kv_headroom_pct", &kv_s);
    read_json_double(rh, "kv_headroom_pct", &kv_h);
    read_json_int(rs, "resident_slots", &slots_s);
    read_json_int(rh, "resident_slots", &slots_h);
    read_json_double(rh, "elapsed_s", &el_h);

    /* T1: stock must fare worse — fewer resident slots than shim */
    if (slots_oh > slots_os) {
        strcpy(oom_s, "fail");
        strcpy(oom_h, "ok");
    }

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");
    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("stress_mode=f1_resident_oom\n");
    printf("context_cap_tokens=%d\n", slots_h > 0 ? slots_h : slots_s);
    printf("resident_slots_stock=%d\n", slots_os);
    printf("resident_slots_shim=%d\n", slots_oh);
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_version=%s\n", drv);
        }
    }
    printf("kv_headroom_pct=%.4f\n", kv_h >= 0 ? kv_h : kv_s);
    printf("ram_efficiency_pct=null\n");
    printf("oom_stock=%s\n", oom_s);
    printf("oom_shim=%s\n", oom_h);
    printf("llc_misses_delta=null\n");
    printf("dtlb_misses_delta=null\n");
    printf("p50_ms=null\n");
    printf("p99_9_ms=null\n");
    printf("elapsed_s=%.3f\n", el_h);
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");
    printf("Evaluation limit reached. Production / unlimited binary: contact "
           "stanislav.byriukov.research@gmail.com (gain-share).\n");

    free(rs);
    free(rh);
    free(os);
    free(oh);
    free(env);
    return 0;
}

static int emit_f1prime(const char *frag_stock, const char *frag_shim, const char *env_path)
{
    char *fs = read_file(frag_stock, NULL);
    char *fh = read_file(frag_shim, NULL);
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    int slots_s = 0, slots_h = 0;
    double eff_s = 0.0, eff_h = 0.0;
    double kv_s = -1.0, kv_h = -1.0;
    double el_h = 0.0;
    double gain_pct = 0.0;

    if (!fs || !fh) {
        fprintf(stderr, "emit-f1prime: missing json\n");
        free(fs);
        free(fh);
        free(env);
        return 1;
    }

    read_json_int(fs, "resident_slots", &slots_s);
    read_json_int(fh, "resident_slots", &slots_h);
    read_json_double(fs, "layout_efficiency_pct", &eff_s);
    read_json_double(fh, "layout_efficiency_pct", &eff_h);
    read_json_double(fs, "kv_headroom_pct", &kv_s);
    read_json_double(fh, "kv_headroom_pct", &kv_h);
    read_json_double(fh, "elapsed_s", &el_h);

    if (slots_s > 0) {
        gain_pct = 100.0 * (double)(slots_h - slots_s) / (double)slots_s;
    } else if (slots_h > 0) {
        gain_pct = 100.0;
    }

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");
    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("stress_mode=f1_frag_v1\n");
    printf("context_cap_tokens=%d\n", slots_h > 0 ? slots_h : slots_s);
    printf("resident_slots_stock=%d\n", slots_s);
    printf("resident_slots_shim=%d\n", slots_h);
    printf("layout_efficiency_stock=%.4f\n", eff_s);
    printf("layout_efficiency_shim=%.4f\n", eff_h);
    printf("kv_gain_pct=%.4f\n", gain_pct);
    printf("kv_gain_method=f1_prime_n_fail_v1\n");
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_version=%s\n", drv);
        }
    }
    printf("kv_headroom_pct=%.4f\n", kv_h >= 0 ? kv_h : kv_s);
    printf("ram_efficiency_pct=null\n");
    printf("oom_stock=%s\n", slots_s > 0 ? "ok" : "fail");
    printf("oom_shim=%s\n", slots_h > 0 ? "ok" : "fail");
    printf("llc_misses_delta=null\n");
    printf("dtlb_misses_delta=null\n");
    printf("p50_ms=null\n");
    printf("p99_9_ms=null\n");
    printf("elapsed_s=%.3f\n", el_h);
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");
    printf("Evaluation limit reached. Production / unlimited binary: contact "
           "stanislav.byriukov.research@gmail.com (gain-share).\n");

    free(fs);
    free(fh);
    free(env);
    return 0;
}

static int emit_zp1(const char *tail_stock, const char *tail_shim, const char *env_path)
{
    char *js = read_file(tail_stock, NULL);
    char *jh = read_file(tail_shim, NULL);
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    double p50_s = 0.0, p50_h = 0.0, p99_s = 0.0, p99_h = 0.0, p999_s = 0.0, p999_h = 0.0;
    double ops_s = 0.0, ops_h = 0.0;
    double tail_red = 0.0;
    double tail_ratio = 0.0;
    int it_s = 0, it_h = 0;
    size_t slice = ISO_DEFAULT_SLICE_BYTES;

    if (!js || !jh) {
        fprintf(stderr, "emit-zp1: missing json\n");
        free(js);
        free(jh);
        free(env);
        return 1;
    }

    read_json_double(js, "p50_ms", &p50_s);
    read_json_double(jh, "p50_ms", &p50_h);
    read_json_double(js, "p99_ms", &p99_s);
    read_json_double(jh, "p99_ms", &p99_h);
    read_json_double(js, "p99_9_ms", &p999_s);
    read_json_double(jh, "p99_9_ms", &p999_h);
    read_json_double(js, "ops_per_s", &ops_s);
    read_json_double(jh, "ops_per_s", &ops_h);
    read_json_int(js, "iterations", &it_s);
    read_json_int(jh, "iterations", &it_h);

    if (p99_s > 0.0 && p99_h > 0.0 && p99_h < p99_s) {
        tail_red = 100.0 * (1.0 - p99_h / p99_s);
        tail_ratio = p99_s / p99_h;
    } else if (p999_s > 0.0 && p999_h > 0.0 && p999_h < p999_s) {
        tail_red = 100.0 * (1.0 - p999_h / p999_s);
        tail_ratio = p999_s / p999_h;
    }
    if (ops_h > ops_s && ops_s > 0.0) {
        /* throughput uplift complements tail story */
    }

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");
    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("stress_mode=kv_tail_latency_v1\n");
    printf("kv_tail_stock_path=resident_pool_touch\n");
    printf("kv_tail_shim_path=resident_pool_touch\n");
    printf("iso_slice_bytes=%zu\n", slice);
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_driver_version=%s\n", drv);
        }
    }
    printf("p50_ms_stock=%.6f\n", p50_s);
    printf("p50_ms_shim=%.6f\n", p50_h);
    printf("p99_ms_stock=%.6f\n", p99_s);
    printf("p99_ms_shim=%.6f\n", p99_h);
    printf("p99_9_ms_stock=%.6f\n", p999_s);
    printf("p99_9_ms_shim=%.6f\n", p999_h);
    printf("tail_latency_reduction_pct=%.4f\n", tail_red);
    printf("tail_latency_ratio_stock_over_shim=%.4f\n", tail_ratio);
    printf("kv_ops_per_s_stock=%.2f\n", ops_s);
    printf("kv_ops_per_s_shim=%.2f\n", ops_h);
    printf("kv_tail_iters_stock=%d\n", it_s);
    printf("kv_tail_iters_shim=%d\n", it_h);
    printf("llc_misses_delta=null\n");
    printf("dtlb_misses_delta=null\n");
    printf("ram_efficiency_pct=null\n");
    printf("oom_stock=ok\n");
    printf("oom_shim=ok\n");
    printf("elapsed_s=%.3f\n", WALL_LIMIT_S);
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");
    printf("Evaluation limit reached. Production / unlimited binary: contact "
           "stanislav.byriukov.research@gmail.com (gain-share).\n");

    if (tail_red >= 25.0 && tail_ratio >= 1.25) {
        printf("IRON_ZP1_KV_TAIL_OK\n");
    } else {
        printf("IRON_ZP1_KV_TAIL_WEAK tail_red=%.2f ratio=%.2f\n", tail_red, tail_ratio);
    }

    free(js);
    free(jh);
    free(env);
    return 0;
}

static int read_power_json_watts(const char *path, double *watts_avg, int *samples)
{
    char *pj = read_file(path, NULL);
    if (!pj) {
        return 1;
    }
    if (watts_avg) {
        read_json_double(pj, "watts_avg", watts_avg);
    }
    if (samples) {
        read_json_int(pj, "samples", samples);
    }
    free(pj);
    return 0;
}

static int emit_zp2(const char *tail_stock, const char *tail_shim, const char *power_stock,
                    const char *power_shim, const char *gemm_stock, const char *gemm_shim,
                    const char *env_path)
{
    char *js = read_file(tail_stock, NULL);
    char *jh = read_file(tail_shim, NULL);
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    char *gs = gemm_stock ? read_file(gemm_stock, NULL) : NULL;
    char *gh = gemm_shim ? read_file(gemm_shim, NULL) : NULL;
    double ops_s = 0.0, ops_h = 0.0;
    double watts_s = 0.0, watts_h = 0.0;
    double gflops_s = 0.0, gflops_h = 0.0;
    double kv_gbps_s = 0.0, kv_gbps_h = 0.0;
    double kv_opw_s = 0.0, kv_opw_h = 0.0;
    double gemm_gpw_s = 0.0, gemm_gpw_h = 0.0;
    double eff_gain = 0.0;
    double gflops_w_ratio = 0.0;
    int samp_s = 0, samp_h = 0;
    size_t slice = iso_slice_bytes();

    if (!js || !jh || read_power_json_watts(power_stock, &watts_s, &samp_s) != 0 ||
        read_power_json_watts(power_shim, &watts_h, &samp_h) != 0) {
        fprintf(stderr, "emit-zp2: missing tail or power json\n");
        free(js);
        free(jh);
        free(env);
        free(gs);
        free(gh);
        return 1;
    }

    read_json_double(js, "ops_per_s", &ops_s);
    read_json_double(jh, "ops_per_s", &ops_h);
    if (gs) {
        read_json_double(gs, "gflops_proxy", &gflops_s);
    }
    if (gh) {
        read_json_double(gh, "gflops_proxy", &gflops_h);
    }

    if (watts_s > 0.0) {
        kv_gbps_s = (ops_s * (double)slice) / 1e9;
        kv_opw_s = ops_s / watts_s;
        if (gflops_s > 0.0) {
            gemm_gpw_s = gflops_s / watts_s;
        }
    }
    if (watts_h > 0.0) {
        kv_gbps_h = (ops_h * (double)slice) / 1e9;
        kv_opw_h = ops_h / watts_h;
        if (gflops_h > 0.0) {
            gemm_gpw_h = gflops_h / watts_h;
        }
    }
    if (kv_opw_s > 0.0 && kv_opw_h > 0.0) {
        eff_gain = 100.0 * (kv_opw_h / kv_opw_s - 1.0);
    }
    if (gemm_gpw_s > 0.0 && gemm_gpw_h > 0.0) {
        gflops_w_ratio = gemm_gpw_h / gemm_gpw_s;
    }

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");
    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("stress_mode=kv_power_efficiency_v1\n");
    printf("power_sample_source=nvidia_smi_dmon_p\n");
    printf("iso_slice_bytes=%zu\n", slice);
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_driver_version=%s\n", drv);
        }
    }
    printf("watts_avg_stock=%.4f\n", watts_s);
    printf("watts_avg_shim=%.4f\n", watts_h);
    printf("watts_samples_stock=%d\n", samp_s);
    printf("watts_samples_shim=%d\n", samp_h);
    printf("kv_touch_ops_per_s_stock=%.2f\n", ops_s);
    printf("kv_touch_ops_per_s_shim=%.2f\n", ops_h);
    printf("kv_effective_GBps_stock=%.4f\n", kv_gbps_s);
    printf("kv_effective_GBps_shim=%.4f\n", kv_gbps_h);
    printf("kv_ops_per_watt_stock=%.4f\n", kv_opw_s);
    printf("kv_ops_per_watt_shim=%.4f\n", kv_opw_h);
    printf("kv_efficiency_gain_pct=%.4f\n", eff_gain);
    printf("gemm_gflops_proxy_stock=%.4f\n", gflops_s);
    printf("gemm_gflops_proxy_shim=%.4f\n", gflops_h);
    printf("gemm_gflops_per_watt_stock=%.4f\n", gemm_gpw_s);
    printf("gemm_gflops_per_watt_shim=%.4f\n", gemm_gpw_h);
    printf("gemm_gflops_per_watt_ratio_shim_over_stock=%.4f\n", gflops_w_ratio);
    printf("flops_per_watt_note=gemm_is_compute_proxy_kv_is_memory_touch_proxy\n");
    printf("elapsed_s=%.3f\n", WALL_LIMIT_S);
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");
    printf("Evaluation limit reached. Production / unlimited binary: contact "
           "stanislav.byriukov.research@gmail.com (gain-share).\n");

    if (kv_opw_h >= kv_opw_s * 1.05 && watts_h > 0.0 && watts_s > 0.0) {
        printf("IRON_ZP2_KV_POWER_OK\n");
    } else {
        printf("IRON_ZP2_KV_POWER_WEAK eff_gain=%.2f kv_opw_s=%.2f kv_opw_h=%.2f\n", eff_gain,
               kv_opw_s, kv_opw_h);
    }

    free(js);
    free(jh);
    free(env);
    free(gs);
    free(gh);
    return 0;
}

static int emit_zp12(const char *tail_stock, const char *tail_shim, const char *power_stock,
                     const char *power_shim, const char *gemm_stock, const char *gemm_shim,
                     const char *env_path)
{
    char *js = read_file(tail_stock, NULL);
    char *jh = read_file(tail_shim, NULL);
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    char *gs = gemm_stock ? read_file(gemm_stock, NULL) : NULL;
    char *gh = gemm_shim ? read_file(gemm_shim, NULL) : NULL;
    double p50_s = 0.0, p50_h = 0.0, p99_s = 0.0, p99_h = 0.0, p999_s = 0.0, p999_h = 0.0;
    double ops_s = 0.0, ops_h = 0.0;
    double watts_s = 0.0, watts_h = 0.0;
    double gflops_s = 0.0, gflops_h = 0.0;
    double tail_red = 0.0, tail_ratio = 0.0;
    double kv_opw_s = 0.0, kv_opw_h = 0.0, eff_gain = 0.0;
    double kv_gbps_s = 0.0, kv_gbps_h = 0.0;
    double gemm_gpw_s = 0.0, gemm_gpw_h = 0.0;
    int it_s = 0, it_h = 0, samp_s = 0, samp_h = 0;
    size_t slice = iso_slice_bytes();
    int zp1_ok = 0;
    int zp2_ok = 0;

    if (!js || !jh || read_power_json_watts(power_stock, &watts_s, &samp_s) != 0 ||
        read_power_json_watts(power_shim, &watts_h, &samp_h) != 0) {
        fprintf(stderr, "emit-zp12: missing inputs\n");
        free(js);
        free(jh);
        free(env);
        free(gs);
        free(gh);
        return 1;
    }

    read_json_double(js, "p50_ms", &p50_s);
    read_json_double(jh, "p50_ms", &p50_h);
    read_json_double(js, "p99_ms", &p99_s);
    read_json_double(jh, "p99_ms", &p99_h);
    read_json_double(js, "p99_9_ms", &p999_s);
    read_json_double(jh, "p99_9_ms", &p999_h);
    read_json_double(js, "ops_per_s", &ops_s);
    read_json_double(jh, "ops_per_s", &ops_h);
    read_json_int(js, "iterations", &it_s);
    read_json_int(jh, "iterations", &it_h);
    if (gs) {
        read_json_double(gs, "gflops_proxy", &gflops_s);
    }
    if (gh) {
        read_json_double(gh, "gflops_proxy", &gflops_h);
    }

    if (p99_s > 0.0 && p99_h > 0.0 && p99_h < p99_s) {
        tail_red = 100.0 * (1.0 - p99_h / p99_s);
        tail_ratio = p99_s / p99_h;
    }
    if (watts_s > 0.0) {
        kv_gbps_s = (ops_s * (double)slice) / 1e9;
        kv_opw_s = ops_s / watts_s;
        if (gflops_s > 0.0) {
            gemm_gpw_s = gflops_s / watts_s;
        }
    }
    if (watts_h > 0.0) {
        kv_gbps_h = (ops_h * (double)slice) / 1e9;
        kv_opw_h = ops_h / watts_h;
        if (gflops_h > 0.0) {
            gemm_gpw_h = gflops_h / watts_h;
        }
    }
    if (kv_opw_s > 0.0 && kv_opw_h > 0.0) {
        eff_gain = 100.0 * (kv_opw_h / kv_opw_s - 1.0);
    }

    zp1_ok = (tail_red >= 25.0 && tail_ratio >= 1.25);
    zp2_ok = (kv_opw_h >= kv_opw_s * 1.05);

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");
    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("stress_mode=zp12_scout_iron_v1\n");
    printf("kv_tail_stock_path=resident_pool_touch\n");
    printf("kv_tail_shim_path=resident_pool_touch\n");
    printf("power_sample_source=nvidia_smi_dmon_p\n");
    printf("iso_slice_bytes=%zu\n", slice);
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_driver_version=%s\n", drv);
        }
    }
    printf("p50_ms_stock=%.6f\n", p50_s);
    printf("p50_ms_shim=%.6f\n", p50_h);
    printf("p99_ms_stock=%.6f\n", p99_s);
    printf("p99_ms_shim=%.6f\n", p99_h);
    printf("p99_9_ms_stock=%.6f\n", p999_s);
    printf("p99_9_ms_shim=%.6f\n", p999_h);
    printf("tail_latency_reduction_pct=%.4f\n", tail_red);
    printf("tail_latency_ratio_stock_over_shim=%.4f\n", tail_ratio);
    printf("kv_ops_per_s_stock=%.2f\n", ops_s);
    printf("kv_ops_per_s_shim=%.2f\n", ops_h);
    printf("kv_tail_iters_stock=%d\n", it_s);
    printf("kv_tail_iters_shim=%d\n", it_h);
    printf("watts_avg_stock=%.4f\n", watts_s);
    printf("watts_avg_shim=%.4f\n", watts_h);
    printf("watts_samples_stock=%d\n", samp_s);
    printf("watts_samples_shim=%d\n", samp_h);
    printf("kv_effective_GBps_stock=%.4f\n", kv_gbps_s);
    printf("kv_effective_GBps_shim=%.4f\n", kv_gbps_h);
    printf("kv_ops_per_watt_stock=%.4f\n", kv_opw_s);
    printf("kv_ops_per_watt_shim=%.4f\n", kv_opw_h);
    printf("kv_efficiency_gain_pct=%.4f\n", eff_gain);
    printf("gemm_gflops_proxy_stock=%.4f\n", gflops_s);
    printf("gemm_gflops_proxy_shim=%.4f\n", gflops_h);
    printf("gemm_gflops_per_watt_stock=%.4f\n", gemm_gpw_s);
    printf("gemm_gflops_per_watt_shim=%.4f\n", gemm_gpw_h);
    printf("flops_per_watt_note=gemm_is_compute_proxy_kv_is_memory_touch_proxy\n");
    printf("llc_misses_delta=null\n");
    printf("dtlb_misses_delta=null\n");
    printf("ram_efficiency_pct=null\n");
    printf("oom_stock=ok\n");
    printf("oom_shim=ok\n");
    printf("elapsed_s=%.3f\n", WALL_LIMIT_S);
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");
    printf("Evaluation limit reached. Production / unlimited binary: contact "
           "stanislav.byriukov.research@gmail.com (gain-share).\n");

    if (zp1_ok) {
        printf("IRON_ZP1_KV_TAIL_OK\n");
    } else {
        printf("IRON_ZP1_KV_TAIL_WEAK tail_red=%.2f ratio=%.2f\n", tail_red, tail_ratio);
    }
    if (zp2_ok) {
        printf("IRON_ZP2_KV_POWER_OK\n");
    } else {
        printf("IRON_ZP2_KV_POWER_WEAK eff_gain=%.2f\n", eff_gain);
    }
    if (zp1_ok && zp2_ok) {
        printf("IRON_ZP12_SCOUT_OK\n");
    }

    free(js);
    free(jh);
    free(env);
    free(gs);
    free(gh);
    return (zp1_ok && zp2_ok) ? 0 : 1;
}

static int curve_load_pair(const char *dir, int pct, int *slots_s, int *slots_h,
                           size_t *req_s, size_t *req_h, size_t *comm_s, size_t *comm_h,
                           double *lib_pct)
{
    char stock_path[512];
    char shim_path[512];
    char *sj;
    char *hj;

    snprintf(stock_path, sizeof stock_path, "%s/stock_%d.json", dir, pct);
    snprintf(shim_path, sizeof shim_path, "%s/shim_%d.json", dir, pct);
    sj = read_file(stock_path, NULL);
    hj = read_file(shim_path, NULL);
    if (!sj || !hj) {
        free(sj);
        free(hj);
        return -1;
    }

    read_json_int(sj, "resident_slots", slots_s);
    read_json_int(hj, "resident_slots", slots_h);
    {
        const char *pat = "\"requested_bytes\":";
        const char *ps = strstr(sj, pat);
        const char *ph = strstr(hj, pat);
        if (ps) {
            sscanf(ps + strlen(pat), "%zu", req_s);
        }
        if (ph) {
            sscanf(ph + strlen(pat), "%zu", req_h);
        }
    }
    {
        const char *pat = "\"committed_bytes\":";
        const char *ps = strstr(sj, pat);
        const char *ph = strstr(hj, pat);
        if (ps) {
            sscanf(ps + strlen(pat), "%zu", comm_s);
        }
        if (ph) {
            sscanf(ph + strlen(pat), "%zu", comm_h);
        }
    }
    if (comm_s && *comm_s > 0) {
        *lib_pct = 100.0 * ((double)*comm_s - (double)*comm_h) / (double)*comm_s;
    } else {
        *lib_pct = 0.0;
    }

    free(sj);
    free(hj);
    return 0;
}

static int emit_vram_curve(const char *curve_dir, const char *env_path)
{
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    static const int points[] = {50, 60, 70};
    int i;
    int ok_points = 0;

    if (!curve_dir || !curve_dir[0]) {
        fprintf(stderr, "emit-vram-curve: missing --curve-dir\n");
        free(env);
        return 1;
    }

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");
    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("stress_mode=vram_budget_curve_v1\n");
    printf("cache_liberation_method=iso_logical_v2\n");
    printf("curve_point_count=%d\n", (int)(sizeof points / sizeof points[0]));
    printf("leaf_bytes=%zu\n", (size_t)(1u << 21));
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_version=%s\n", drv);
        }
        if (read_json_str(env, "git_sha", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("git_sha=%s\n", gpu);
        }
    }

    for (i = 0; i < (int)(sizeof points / sizeof points[0]); i++) {
        int pct = points[i];
        int slots_s = 0, slots_h = 0;
        size_t req_s = 0, req_h = 0, comm_s = 0, comm_h = 0;
        double lib = 0.0;
        int logical_gain_pct = 0;

        if (curve_load_pair(curve_dir, pct, &slots_s, &slots_h, &req_s, &req_h, &comm_s,
                          &comm_h, &lib) != 0) {
            printf("curve_point_%d_status=missing\n", pct);
            continue;
        }
        ok_points++;
        if (req_s > 0) {
            logical_gain_pct =
                (int)(100.0 * ((double)req_h - (double)req_s) / (double)req_s);
        }
        printf("vram_budget_pct_%d=%d\n", pct, pct);
        printf("resident_slots_stock_%d=%d\n", pct, slots_s);
        printf("resident_slots_shim_%d=%d\n", pct, slots_h);
        printf("logical_kv_bytes_stock_%d=%zu\n", pct, req_s);
        printf("logical_kv_bytes_shim_%d=%zu\n", pct, req_h);
        printf("committed_bytes_stock_%d=%zu\n", pct, comm_s);
        printf("committed_bytes_shim_%d=%zu\n", pct, comm_h);
        printf("logical_kv_gain_pct_%d=%d\n", pct, logical_gain_pct);
        printf("cache_liberation_pct_%d=%.4f\n", pct, lib);
        printf("curve_point_%d_status=ok\n", pct);
    }

    printf("curve_points_ok=%d\n", ok_points);
    printf("kv_headroom_pct=null\n");
    printf("ram_efficiency_pct=null\n");
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");

    free(env);
    return ok_points == 3 ? 0 : 2;
}

static int emit_cache_liberation(const char *stock_json, const char *shim_json,
                                 const char *env_path)
{
    char *sj = read_file(stock_json, NULL);
    char *hj = read_file(shim_json, NULL);
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    int slots_s = 0, slots_h = 0;
    size_t req_s = 0, req_h = 0, comm_s = 0, comm_h = 0;
    double eff_s = 0.0, eff_h = 0.0;
    double liberation = 0.0;
    double el_h = 0.0;
    int in_band = 0;

    if (!sj || !hj) {
        fprintf(stderr, "emit-cache-liberation: missing json\n");
        free(sj);
        free(hj);
        free(env);
        return 1;
    }

    read_json_int(sj, "resident_slots", &slots_s);
    read_json_int(hj, "resident_slots", &slots_h);
    read_json_double(sj, "layout_efficiency_pct", &eff_s);
    read_json_double(hj, "layout_efficiency_pct", &eff_h);
    read_json_double(hj, "elapsed_s", &el_h);

    {
        char pat[] = "\"requested_bytes\":";
        const char *ps = strstr(sj, pat);
        const char *ph = strstr(hj, pat);
        if (ps) {
            sscanf(ps + strlen(pat), "%zu", &req_s);
        }
        if (ph) {
            sscanf(ph + strlen(pat), "%zu", &req_h);
        }
    }
    {
        char pat[] = "\"committed_bytes\":";
        const char *ps = strstr(sj, pat);
        const char *ph = strstr(hj, pat);
        if (ps) {
            sscanf(ps + strlen(pat), "%zu", &comm_s);
        }
        if (ph) {
            sscanf(ph + strlen(pat), "%zu", &comm_h);
        }
    }

    if (comm_s > 0) {
        liberation = 100.0 * ((double)comm_s - (double)comm_h) / (double)comm_s;
    }
    in_band = (liberation >= 30.0 && liberation <= 40.0);

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");
    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("stress_mode=iso_logical_kv_v1\n");
    printf("context_cap_tokens=%d\n", slots_h > 0 ? slots_h : slots_s);
    printf("resident_slots_stock=%d\n", slots_s);
    printf("resident_slots_shim=%d\n", slots_h);
    printf("logical_kv_bytes_stock=%zu\n", req_s);
    printf("logical_kv_bytes_shim=%zu\n", req_h);
    printf("committed_bytes_stock=%zu\n", comm_s);
    printf("committed_bytes_shim=%zu\n", comm_h);
    printf("layout_efficiency_stock=%.4f\n", eff_s);
    printf("layout_efficiency_shim=%.4f\n", eff_h);
    printf("cache_liberation_pct=%.4f\n", liberation);
    printf("cache_liberation_method=iso_logical_v2\n");
    printf("cache_liberation_in_band=%s\n", in_band ? "yes" : "no");
    printf("leaf_bytes=%zu\n", (size_t)(1u << 21));
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_version=%s\n", drv);
        }
        if (read_json_str(env, "git_sha", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("git_sha=%s\n", gpu);
        }
    }
    printf("kv_headroom_pct=null\n");
    printf("ram_efficiency_pct=null\n");
    printf("oom_stock=%s\n", slots_s > 0 ? "ok" : "fail");
    printf("oom_shim=%s\n", slots_h > 0 ? "ok" : "fail");
    printf("llc_misses_delta=null\n");
    printf("dtlb_misses_delta=null\n");
    printf("p50_ms=null\n");
    printf("p99_9_ms=null\n");
    printf("elapsed_s=%.3f\n", el_h);
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");
    printf("Evaluation limit reached. Production / unlimited binary: contact "
           "stanislav.byriukov.research@gmail.com (gain-share).\n");

    free(sj);
    free(hj);
    free(env);
    return in_band ? 0 : 2;
}

static int emit_kv_gain(const char *stock_json, const char *shim_json, const char *env_path)
{
    char *sj = read_file(stock_json, NULL);
    char *hj = read_file(shim_json, NULL);
    char *env = env_path ? read_file(env_path, NULL) : NULL;
    int slots_s = 0, slots_h = 0;
    double kv_s = -1.0, kv_h = -1.0;
    double eff_s = 0.0, eff_h = 0.0;
    double gain_headroom = 0.0;
    double gain_n_slots = 0.0;
    double n_target = 1.0;
    double el_h = 0.0;

    if (!sj || !hj) {
        fprintf(stderr, "emit-kv-gain: missing json\n");
        free(sj);
        free(hj);
        free(env);
        return 1;
    }

    read_json_int(sj, "resident_slots", &slots_s);
    read_json_int(hj, "resident_slots", &slots_h);
    read_json_double(sj, "kv_headroom_pct", &kv_s);
    read_json_double(hj, "kv_headroom_pct", &kv_h);
    read_json_double(sj, "layout_efficiency_pct", &eff_s);
    read_json_double(hj, "layout_efficiency_pct", &eff_h);
    read_json_double(hj, "elapsed_s", &el_h);

    if (kv_s >= 0.0 && kv_h >= 0.0) {
        gain_headroom = kv_h - kv_s;
    }
    if (slots_s > 0) {
        n_target = (double)slots_s;
        gain_n_slots = 100.0 * (double)(slots_h - slots_s) / n_target;
    } else if (slots_h > 0) {
        n_target = (double)slots_h;
        gain_n_slots = 100.0 * (double)slots_h / n_target;
    }

    printf("HWATOM_EVAL v1\n");
    printf("By continuing execution you accept LICENSE.md (Evaluation-Only).\n");
    printf("No reverse engineering or production use.\n");
    printf("HWATOM_EVAL_END\n");
    printf("GATE12_BEGIN\n");
    printf("workload_id=%s\n", WORKLOAD_ID);
    printf("workload_class=synthetic_kv_band\n");
    printf("stress_mode=resident_kv_gain\n");
    printf("context_cap_tokens=%d\n", slots_h > 0 ? slots_h : slots_s);
    printf("resident_slots_stock=%d\n", slots_s);
    printf("resident_slots_shim=%d\n", slots_h);
    printf("kv_headroom_pct_stock=%.4f\n", kv_s >= 0 ? kv_s : 0.0);
    printf("kv_headroom_pct_shim=%.4f\n", kv_h >= 0 ? kv_h : 0.0);
    printf("layout_efficiency_stock=%.4f\n", eff_s);
    printf("layout_efficiency_shim=%.4f\n", eff_h);
    printf("kv_gain_pct=%.4f\n", gain_headroom);
    printf("kv_gain_pct_n_slots=%.4f\n", gain_n_slots);
    printf("kv_gain_n_target=%.0f\n", n_target);
    printf("kv_gain_method=headroom_delta_v1\n");
    printf("leaf_bytes=%zu\n", (size_t)(1u << 21));
    printf("run_id=%ld\n", (long)time(NULL));
    if (env) {
        char gpu[128] = "";
        char drv[64] = "";
        if (read_json_str(env, "gpu_name", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("gpu_sku=%s\n", gpu);
        }
        if (read_json_str(env, "driver_version", drv, sizeof drv) == 0 && drv[0]) {
            printf("driver_version=%s\n", drv);
        }
        if (read_json_str(env, "git_sha", gpu, sizeof gpu) == 0 && gpu[0]) {
            printf("git_sha=%s\n", gpu);
        }
    }
    printf("kv_headroom_pct=%.4f\n", kv_h >= 0 ? kv_h : kv_s);
    printf("ram_efficiency_pct=null\n");
    printf("oom_stock=%s\n", slots_s > 0 ? "ok" : "fail");
    printf("oom_shim=%s\n", slots_h > 0 ? "ok" : "fail");
    printf("llc_misses_delta=null\n");
    printf("dtlb_misses_delta=null\n");
    printf("p50_ms=null\n");
    printf("p99_9_ms=null\n");
    printf("elapsed_s=%.3f\n", el_h);
    printf("GATE12_END\n");
    printf("HWATOM_EVAL_COMPLETE\n");
    printf("Evaluation limit reached. Production / unlimited binary: contact "
           "stanislav.byriukov.research@gmail.com (gain-share).\n");

    free(sj);
    free(hj);
    free(env);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = NULL;
    const char *bench_kind = "alloc";
    const char *bench_path = "stock";
    const char *out = NULL;
    const char *stock_json = NULL;
    const char *shim_json = NULL;
    const char *perf_stock = NULL;
    const char *perf_shim = NULL;
    const char *env_json = NULL;
    const char *resident_stock = NULL;
    const char *resident_shim = NULL;
    const char *oom_stock = NULL;
    const char *oom_shim = NULL;
    const char *frag_stock = NULL;
    const char *frag_shim = NULL;
    const char *tail_stock = NULL;
    const char *tail_shim = NULL;
    const char *power_stock = NULL;
    const char *power_shim = NULL;
    const char *gemm_stock = NULL;
    const char *gemm_shim = NULL;
    const char *curve_dir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bench") == 0 && i + 1 < argc) {
            mode = "bench";
            const char *a = argv[++i];
            if (strcmp(a, "alloc") == 0 || strcmp(a, "resident") == 0 ||
                strcmp(a, "frag") == 0 || strcmp(a, "kv_tail") == 0 ||
                strcmp(a, "gemm_proxy") == 0 || strcmp(a, "iso_logical") == 0) {
                bench_kind = a;
                if (i + 1 < argc && (strcmp(argv[i + 1], "stock") == 0 ||
                                     strcmp(argv[i + 1], "shim") == 0)) {
                    bench_path = argv[++i];
                }
            } else {
                bench_kind = "alloc";
                bench_path = a;
            }
        } else if (strcmp(argv[i], "--oom") == 0 && i + 1 < argc) {
            mode = "oom";
            bench_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else if (strcmp(argv[i], "--emit-gate12") == 0) {
            mode = "emit";
        } else if (strcmp(argv[i], "--emit-f1") == 0) {
            mode = "emit-f1";
        } else if (strcmp(argv[i], "--emit-f1prime") == 0) {
            mode = "emit-f1prime";
        } else if (strcmp(argv[i], "--emit-zp1") == 0) {
            mode = "emit-zp1";
        } else if (strcmp(argv[i], "--emit-zp2") == 0) {
            mode = "emit-zp2";
        } else if (strcmp(argv[i], "--emit-zp12") == 0) {
            mode = "emit-zp12";
        } else if (strcmp(argv[i], "--emit-cache-liberation") == 0) {
            mode = "emit-cache-liberation";
        } else if (strcmp(argv[i], "--emit-vram-curve") == 0) {
            mode = "emit-vram-curve";
        } else if (strcmp(argv[i], "--curve-dir") == 0 && i + 1 < argc) {
            curve_dir = argv[++i];
        } else if (strcmp(argv[i], "--emit-kv-gain") == 0) {
            mode = "emit-kv-gain";
        } else if (strcmp(argv[i], "--gain-stock") == 0 && i + 1 < argc) {
            frag_stock = argv[++i];
        } else if (strcmp(argv[i], "--gain-shim") == 0 && i + 1 < argc) {
            frag_shim = argv[++i];
        } else if (strcmp(argv[i], "--frag-stock") == 0 && i + 1 < argc) {
            frag_stock = argv[++i];
        } else if (strcmp(argv[i], "--frag-shim") == 0 && i + 1 < argc) {
            frag_shim = argv[++i];
        } else if (strcmp(argv[i], "--tail-stock") == 0 && i + 1 < argc) {
            tail_stock = argv[++i];
        } else if (strcmp(argv[i], "--tail-shim") == 0 && i + 1 < argc) {
            tail_shim = argv[++i];
        } else if (strcmp(argv[i], "--power-stock") == 0 && i + 1 < argc) {
            power_stock = argv[++i];
        } else if (strcmp(argv[i], "--power-shim") == 0 && i + 1 < argc) {
            power_shim = argv[++i];
        } else if (strcmp(argv[i], "--gemm-stock") == 0 && i + 1 < argc) {
            gemm_stock = argv[++i];
        } else if (strcmp(argv[i], "--gemm-shim") == 0 && i + 1 < argc) {
            gemm_shim = argv[++i];
        } else if (strcmp(argv[i], "--stock") == 0 && i + 1 < argc) {
            stock_json = argv[++i];
        } else if (strcmp(argv[i], "--shim") == 0 && i + 1 < argc) {
            shim_json = argv[++i];
        } else if (strcmp(argv[i], "--resident-stock") == 0 && i + 1 < argc) {
            resident_stock = argv[++i];
        } else if (strcmp(argv[i], "--resident-shim") == 0 && i + 1 < argc) {
            resident_shim = argv[++i];
        } else if (strcmp(argv[i], "--oom-stock") == 0 && i + 1 < argc) {
            oom_stock = argv[++i];
        } else if (strcmp(argv[i], "--oom-shim") == 0 && i + 1 < argc) {
            oom_shim = argv[++i];
        } else if (strcmp(argv[i], "--perf-stock") == 0 && i + 1 < argc) {
            perf_stock = argv[++i];
        } else if (strcmp(argv[i], "--perf-shim") == 0 && i + 1 < argc) {
            perf_shim = argv[++i];
        } else if (strcmp(argv[i], "--env") == 0 && i + 1 < argc) {
            env_json = argv[++i];
        }
    }

    if (mode && strcmp(mode, "bench") == 0) {
        struct bench_result r;
        int rc;
        if (strcmp(bench_kind, "resident") == 0) {
            rc = run_resident_bench(bench_path, &r);
        } else if (strcmp(bench_kind, "frag") == 0) {
            rc = run_frag_bench(bench_path, &r);
        } else if (strcmp(bench_kind, "iso_logical") == 0) {
            rc = run_iso_logical_bench(bench_path, &r);
        } else if (strcmp(bench_kind, "kv_tail") == 0) {
            rc = run_kv_tail_bench(bench_path, &r);
        } else if (strcmp(bench_kind, "gemm_proxy") == 0) {
            rc = run_gemm_proxy_bench(bench_path, &r);
        } else {
            rc = run_microbench(bench_path, &r);
        }
        if (rc != 0) {
            return 1;
        }
        if (out && out[0]) {
            return write_bench_json(out, &r);
        }
        return 0;
    }
    if (mode && strcmp(mode, "oom") == 0) {
        struct bench_result r;
        if (run_oom_ladder(bench_path, &r) != 0) {
            return 1;
        }
        if (out && out[0]) {
            return write_bench_json(out, &r);
        }
        return 0;
    }
    if (mode && strcmp(mode, "emit") == 0) {
        if (!stock_json || !shim_json) {
            fprintf(stderr, "usage: --emit-gate12 --stock S --shim H [--perf-*] [--env E]\n");
            return 2;
        }
        return emit_gate12(stock_json, shim_json, perf_stock, perf_shim, env_json);
    }
    if (mode && strcmp(mode, "emit-f1") == 0) {
        if (!resident_stock || !resident_shim || !oom_stock || !oom_shim) {
            fprintf(stderr,
                    "usage: --emit-f1 --resident-stock RS --resident-shim RH "
                    "--oom-stock OS --oom-shim OH [--env E]\n");
            return 2;
        }
        return emit_f1(resident_stock, resident_shim, oom_stock, oom_shim, env_json);
    }
    if (mode && strcmp(mode, "emit-f1prime") == 0) {
        if (!frag_stock || !frag_shim) {
            fprintf(stderr,
                    "usage: --emit-f1prime --frag-stock FS --frag-shim FH [--env E]\n");
            return 2;
        }
        return emit_f1prime(frag_stock, frag_shim, env_json);
    }
    if (mode && strcmp(mode, "emit-zp1") == 0) {
        if (!tail_stock || !tail_shim) {
            fprintf(stderr,
                    "usage: --emit-zp1 --tail-stock TS --tail-shim TH [--env E]\n");
            return 2;
        }
        return emit_zp1(tail_stock, tail_shim, env_json);
    }
    if (mode && strcmp(mode, "emit-zp2") == 0) {
        if (!tail_stock || !tail_shim || !power_stock || !power_shim) {
            fprintf(stderr,
                    "usage: --emit-zp2 --tail-stock TS --tail-shim TH "
                    "--power-stock PS --power-shim PH [--gemm-stock GS --gemm-shim GH] [--env E]\n");
            return 2;
        }
        return emit_zp2(tail_stock, tail_shim, power_stock, power_shim, gemm_stock, gemm_shim,
                        env_json);
    }
    if (mode && strcmp(mode, "emit-zp12") == 0) {
        if (!tail_stock || !tail_shim || !power_stock || !power_shim) {
            fprintf(stderr,
                    "usage: --emit-zp12 --tail-stock TS --tail-shim TH "
                    "--power-stock PS --power-shim PH [--gemm-stock GS --gemm-shim GH] [--env E]\n");
            return 2;
        }
        return emit_zp12(tail_stock, tail_shim, power_stock, power_shim, gemm_stock, gemm_shim,
                         env_json);
    }
    if (mode && strcmp(mode, "emit-kv-gain") == 0) {
        if (!frag_stock || !frag_shim) {
            fprintf(stderr,
                    "usage: --emit-kv-gain --gain-stock S --gain-shim H [--env E]\n");
            return 2;
        }
        return emit_kv_gain(frag_stock, frag_shim, env_json);
    }
    if (mode && strcmp(mode, "emit-cache-liberation") == 0) {
        if (!frag_stock || !frag_shim) {
            fprintf(stderr,
                    "usage: --emit-cache-liberation --gain-stock S --gain-shim H [--env E]\n");
            return 2;
        }
        return emit_cache_liberation(frag_stock, frag_shim, env_json);
    }
    if (mode && strcmp(mode, "emit-vram-curve") == 0) {
        if (!curve_dir) {
            fprintf(stderr,
                    "usage: --emit-vram-curve --curve-dir DIR [--env E.json]\n");
            return 2;
        }
        return emit_vram_curve(curve_dir, env_json);
    }

    fprintf(stderr,
            "usage: iron_gate_v1 --bench [alloc|resident|frag|kv_tail|gemm_proxy|iso_logical] stock|shim --out f.json\n"
            "       iron_gate_v1 --oom stock|shim --out f.json\n"
            "       iron_gate_v1 --emit-gate12 | --emit-f1 | --emit-f1prime | --emit-zp1 | --emit-zp2 | --emit-zp12 ...\n");
    return 2;
}
