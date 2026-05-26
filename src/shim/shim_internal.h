/* Layer A — lib2adic_shim v1 internal (A-2.3+). Spec: vault A-2.1. */
#ifndef HWATOM_SHIM_INTERNAL_H
#define HWATOM_SHIM_INTERNAL_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* Minimal CUDA Driver types (dry build without full cuda.h). */
typedef int CUresult;
typedef unsigned long long CUdeviceptr;
typedef void *CUmemGenericAllocationHandle;
typedef int CUmemAllocationGranularity_flags;

typedef struct CUmemAllocationProp_st {
    int type;
    int location;
    unsigned int compressionType;
    unsigned int gpuDirectRDMACapable;
    unsigned int usage;
    unsigned int reserved[4];
} CUmemAllocationProp;

typedef struct CUmemAccessDesc_st {
    int location;
    int flags;
} CUmemAccessDesc;

#define CUDA_SUCCESS 0
#define CUDA_ERROR_NOT_PERMITTED 800
/* Driver API granularity flags (CUDA 12.x names). */
#define CU_MEM_ALLOC_GRANULARITY_MINIMUM 0x0
#define CU_MEM_ALLOC_GRANULARITY_RECOMMENDED 0x1

typedef struct shim_reservation_slot_st {
    int active;
    CUdeviceptr va;
    size_t size;
    size_t alignment;
    unsigned long long flags;
} shim_reservation_slot_t;

typedef struct shim_alloc_slot_st {
    int active;
    int mapped;
    CUmemGenericAllocationHandle handle;
    size_t rounded_size;
    unsigned long long flags;
} shim_alloc_slot_t;

extern pthread_mutex_t g_map_epoch_mu;
extern int g_map_epoch_active;
extern shim_reservation_slot_t g_reservation_slot;
extern shim_alloc_slot_t g_alloc_slot;

int shim_epoch_try_enter(const char *op);
void shim_epoch_abort(const char *op);
void shim_epoch_exit(const char *op);
void shim_trace(const char *epoch_tag, const char *op, const char *detail);

size_t shim_leaf_bytes_v1(void);
size_t shim_round_leaf_size(size_t size);
int shim_is_leaf_aligned(size_t size);
size_t shim_leaf_ladder_round(size_t size, unsigned level);
unsigned shim_pack_k_cap_max_v1(void);
int shim_pack_eval_build_v1(void);
size_t shim_placement_round_v1(size_t req);
void shim_pack_reset(void);
void shim_pack_flush(void);
int shim_pack_arena_active(void);
int shim_pack_mega_enabled_v1(void);
size_t shim_pack_new_reserve_size(void);
int shim_pack_need_new_mega_reserve(void);
void shim_pack_mega_open(CUdeviceptr base_va, size_t va_len);
int shim_pack_take_mega_leaf(CUdeviceptr *va_out, size_t *va_map_size);
unsigned shim_pack_mega_leaf_used(void);
unsigned shim_pack_mega_reserve_count(void);
int shim_pack_try_reserve(CUdeviceptr *va_out, size_t req_logical, size_t *va_map_size);
int shim_pack_open_arena(CUdeviceptr va, size_t va_len, CUmemGenericAllocationHandle handle);
CUmemGenericAllocationHandle shim_pack_arena_handle(void);
CUmemGenericAllocationHandle shim_pack_arena_handle_locked(void);
size_t shim_pack_committed_bytes(void);
size_t shim_pack_committed_peak_bytes(void);
size_t shim_pack_map_offset_for_va(CUdeviceptr va);
int shim_pack_is_packed_subva(CUdeviceptr va);
/* Caller must hold g_map_epoch_mu. */
int shim_pack_is_packed_subva_locked(CUdeviceptr va);
void shim_pack_bump_usage(size_t logical_req);
int shim_2adic_enabled_v1(void);
void shim_2adic_reset(void);
unsigned shim_2adic_band_count(void);
unsigned shim_2adic_slots_per_leaf(size_t need_va, size_t leaf_bytes);
size_t shim_2adic_offset_in_leaf(unsigned band_idx, size_t need_va, size_t leaf_bytes);
unsigned shim_2adic_mega_leaf_for_band(unsigned band_idx, unsigned slots_per_leaf);
unsigned shim_2adic_bump_band(void);
void shim_pack_retain_handle(void);
int shim_pack_release_ref(CUmemGenericAllocationHandle handle);

int shim_gqa_enabled_v1(void);
int shim_gqa_try_alias_reserve(CUdeviceptr *va_out, size_t req_logical, size_t *va_map_size);
void shim_gqa_register_leaf(CUdeviceptr base_va, size_t req_logical);
void shim_gqa_reset(void);
void shim_driver_reset_v1(void);

static inline int shim_log_enabled(void)
{
    const char *e = getenv("HWATOM_SHIM_LOG");
    return e && e[0] && e[0] != '0';
}

int shim_epoch_held(void);
typedef struct shim_reserve_request_st {
    size_t size;
    size_t alignment;
    unsigned long long flags;
    CUdeviceptr hint_addr;
} shim_reserve_request_t;

CUresult shim_reserve_validate(const shim_reserve_request_t *req);
CUresult shim_reserve_begin(void);
void shim_reserve_log_fields(const shim_reserve_request_t *req);
void shim_reserve_commit(CUdeviceptr va, const shim_reserve_request_t *req);
void shim_reserve_rollback(void);
const shim_reservation_slot_t *shim_reservation_snapshot(void);
void shim_reservation_clear(void);
void shim_reservation_set(CUdeviceptr va, size_t size, size_t alignment,
                          unsigned long long flags);
int shim_reservation_active(void);
void shim_slot_clear_all(void);
void shim_slot_free_by_va(CUdeviceptr va);
int shim_slot_va_index(CUdeviceptr va);
int shim_slot_handle_index(CUmemGenericAllocationHandle handle);
int shim_slot_pending_create_index(void);
int shim_slot_get_va_size(CUdeviceptr va, size_t *out_size);
void shim_slot_bind_create(CUmemGenericAllocationHandle handle, size_t rounded,
                           unsigned long long flags);
void shim_slot_mark_mapped(CUdeviceptr va);
void shim_slot_clear_mapped(CUdeviceptr va);
void shim_slot_clear_handle(CUmemGenericAllocationHandle handle);
int shim_slot_is_mapped(CUdeviceptr va);
int shim_slot_has_handle(CUdeviceptr va);
int shim_reserve_begin_slot(void);
void shim_reserve_abort_pending(void);
int shim_pack_enabled_v1(void);
int shim_slot_use_packed_handle(CUmemGenericAllocationHandle *out_handle);
void shim_slot_set_req_bytes(size_t req);
void shim_slot_mark_packed_arena(int on);
void shim_slot_mark_packed_by_va(CUdeviceptr va, int on);
void shim_slot_set_map_offset(CUdeviceptr va, size_t offset);
size_t shim_slot_map_offset(CUdeviceptr va);
int shim_slot_is_packed_subva(CUdeviceptr va);
size_t shim_slot_req_bytes_for_va(CUdeviceptr va);
void shim_log_granularity_both(const CUmemAllocationProp *prop);

void shim_alloc_clear(void);
CUresult shim_create_validate(size_t size);
size_t shim_create_round_size(size_t size);
void shim_create_log(size_t req_size, size_t rounded, unsigned long long flags);
void shim_create_commit(CUmemGenericAllocationHandle handle, size_t rounded,
                        unsigned long long flags);
CUresult shim_map_validate(CUdeviceptr ptr, size_t size, size_t offset);
void shim_map_log(CUdeviceptr ptr, size_t size, size_t offset);
void shim_map_mark_mapped(CUdeviceptr ptr);
CUresult shim_unmap_validate(CUdeviceptr ptr, size_t size);
void shim_unmap_clear_mapped(CUdeviceptr ptr);
CUresult shim_release_validate(CUmemGenericAllocationHandle handle);
void shim_release_clear(CUmemGenericAllocationHandle handle);
const shim_alloc_slot_t *shim_alloc_snapshot(void);

/* PATCH-004 / B-CONC-12 */
const char *hwatom_shim_build_id(void);
void hwatom_shim_emit_identity_once(void);

#endif
