/* A-2.14b — GQA KV alias: multiple logical heads share one physical leaf (VA policy). */
#include "shim_internal.h"

#include <stdlib.h>
#include <string.h>

#define SHIM_GQA_MAX_HEADS_DEFAULT 8u

typedef struct shim_gqa_bucket_st {
    size_t logical_bytes;
    CUdeviceptr base_va;
    unsigned head_count;
    unsigned max_heads;
} shim_gqa_bucket_t;

static shim_gqa_bucket_t g_gqa_buckets[64];
static unsigned g_gqa_bucket_n;

int shim_gqa_enabled_v1(void)
{
    const char *e = getenv("HWATOM_GQA_ALIAS");
    if (!e || !e[0]) {
        return 0;
    }
    return e[0] != '0';
}

static unsigned shim_gqa_max_heads(void)
{
    const char *e = getenv("HWATOM_GQA_HEADS");
    char *end = NULL;
    unsigned long long v;

    if (!e || !e[0]) {
        return SHIM_GQA_MAX_HEADS_DEFAULT;
    }
    v = strtoull(e, &end, 10);
    if (end == e || v == 0 || v > 32) {
        return SHIM_GQA_MAX_HEADS_DEFAULT;
    }
    return (unsigned)v;
}

/* Return 1 if *va_out is an alias (no new cuMemAddressReserve). */
int shim_gqa_try_alias_reserve(CUdeviceptr *va_out, size_t req_logical, size_t *va_map_size)
{
    size_t leaf = shim_leaf_bytes_v1();
    size_t need = shim_placement_round_v1(req_logical);
    unsigned i;

    if (!shim_gqa_enabled_v1() || !va_out || !va_map_size || need == 0 || need > leaf) {
        return 0;
    }

    pthread_mutex_lock(&g_map_epoch_mu);

    for (i = 0; i < g_gqa_bucket_n; i++) {
        shim_gqa_bucket_t *b = &g_gqa_buckets[i];
        if (b->logical_bytes == need && b->head_count < b->max_heads) {
            /* True GQA: one physical KV mapping; logical heads reuse base VA. */
            (void)b->head_count;
            *va_out = b->base_va;
            *va_map_size = need;
            b->head_count++;
            pthread_mutex_unlock(&g_map_epoch_mu);
            return 1;
        }
    }

    if (g_gqa_bucket_n >= sizeof(g_gqa_buckets) / sizeof(g_gqa_buckets[0])) {
        pthread_mutex_unlock(&g_map_epoch_mu);
        return 0;
    }

    pthread_mutex_unlock(&g_map_epoch_mu);
    return 0;
}

void shim_gqa_register_leaf(CUdeviceptr base_va, size_t req_logical)
{
    size_t need = shim_placement_round_v1(req_logical);
    unsigned max_h = shim_gqa_max_heads();

    if (!shim_gqa_enabled_v1() || need == 0) {
        return;
    }

    pthread_mutex_lock(&g_map_epoch_mu);
    if (g_gqa_bucket_n < sizeof(g_gqa_buckets) / sizeof(g_gqa_buckets[0])) {
        shim_gqa_bucket_t *b = &g_gqa_buckets[g_gqa_bucket_n++];

        memset(b, 0, sizeof(*b));
        b->logical_bytes = need;
        b->base_va = base_va;
        b->head_count = 1;
        b->max_heads = max_h;
    }
    pthread_mutex_unlock(&g_map_epoch_mu);
}

void shim_gqa_reset(void)
{
    pthread_mutex_lock(&g_map_epoch_mu);
    g_gqa_bucket_n = 0;
    memset(g_gqa_buckets, 0, sizeof(g_gqa_buckets));
    pthread_mutex_unlock(&g_map_epoch_mu);
}
