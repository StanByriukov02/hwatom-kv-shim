# Packing Logical KV Slices Under CUDA 2 MiB Granularity: Measured VRAM Liberation on H100

**Status:** Draft v0.2 — canonical metrics frozen 2026-05-22  
**Author:** Stanislav Byriukov (Independent Researcher)  
**Email:** stanislav.byriukov.research@gmail.com  
**ORCID:** linked on arXiv account (sufficient)  
**arXiv:** primary **cs.DS**; optional cross-list cs.DC / cs.LG at metadata step  
**Code:** `github.com/StanByriukov02/hwatom-gate-12s` tag `t1-eval-20260522`  
**Workload:** `workload_id=a_gate_v1_kv_microbench`

---

## Abstract

Large language model (LLM) serving systems allocate GPU memory through the CUDA virtual memory management APIs (`cuMemAddressReserve`, `cuMemCreate`, `cuMemMap`). On NVIDIA H100, we measure a **minimum allocation granularity of 2 MiB** per physical mapping, while application-level key–value (KV) cache slices are often **much smaller** (e.g. 512 KiB logical bands). This mismatch commits excess VRAM per logical slot under a stock allocator path.

We present a **user-space shim** (`lib2adic_shim.so`) that packs multiple logical KV slices into shared 2 MiB leaves without changing application source code (LD_PRELOAD on the CUDA driver path). We evaluate on a **synthetic KV-band microbenchmark**—not production vLLM inference—with reproducible **GATE12** stdout metrics.

On NVIDIA H100 PCIe (driver 550.163), iso uniform 512 KiB slices, and a **70% VRAM budget** fill scenario, the shim reduces **committed** GPU memory by **42.2%** while delivering **131% higher budget-curve logical KV** than stock at the same budget tier. A resident stress test (F1′, 8 s wall) achieves **100%** layout efficiency vs **70.3%** stock. Results are reproduced inside a published Docker gate image (`hwatom:gate-12s-f1`). **We do not claim** inference tail-latency, FLOPS/W, or industry Z-score anomalies; runtime integration on customer vLLM is deferred to future work.

**Keywords:** KV cache, CUDA, GPU memory, memory granularity, LLM serving, reproducible benchmark

---

## 1. Introduction

KV cache footprint dominates VRAM in long-context LLM inference. Allocator behavior at the CUDA driver boundary—not only high-level frameworks—determines how many logical slots fit under a memory budget.

**Problem.** When the platform enforces ~2 MiB minimum mapping granularity, reserving one small logical KV slice per 2 MiB leaf wastes committed memory. Frameworks may request 512 KiB logical regions; the driver path still pays multi-megabyte physical commitment per slot in our stock baseline.

**Approach.** A shim intercepts `cuMem*` on the allocation path, reserves logical address spans, and packs multiple logical slots per 2 MiB leaf (packing factor k≈4 for 512 KiB slices). A mega-VA mode reduces repeated `cuMemAddressReserve` pressure in multi-slot benches.

**Contributions (measurement-backed only).**

1. **Mechanism** with iron-verified 2 MiB minimum granularity on H100 and P1 packing for uniform 512 KiB iso slices.
2. **VRAM budget curve** (50/60/70% caps) with explicit stock-class pairing per A49 claim protocol.
3. **Reproducible artifact:** open evaluation repo, Docker gate, fixed `workload_id`, canonical GATE12 transcript.

---

## 2. Background and related work

**CUDA virtual memory.** Modern CUDA allows granular virtual address management separate from physical allocation [cite:cuda_prog_guide]. Our work operates at this boundary via LD_PRELOAD—not kernel modules.

**LLM KV cache systems.** Serving systems such as vLLM introduced paged KV attention to improve throughput [cite:vllm]. **We do not benchmark vLLM** in this paper; we cite them only as motivation for KV memory pressure.

**GPU memory context.** Accelerator memory pressure and pooling are surveyed in [cite:sze2017efficient]. Our shim is **policy-specific** to 2 MiB CUDA leaves and measured KV-band workloads—not a general allocator.

**Author measurement line.** Prior work [cite:byriukov2026tal] documents iron GPU receipts (energy/latency) on H100-class hardware; it does **not** evaluate KV packing or CUDA `cuMem*` interception.

---

## 3. Design

### 3.1 Stock baseline

Stock path: one `cuMemAddressReserve` + `cuMemCreate` + `cuMemMap` per logical slot at **2 MiB** physical size for ≤512 KiB requests (iron `frag_alloc_bytes`).

### 3.2 Shim packing (P1 + mega)

- **P1 pack:** up to k logical 512 KiB slices share one 2 MiB leaf where `iron_placement_round` allows.
- **Mega-VA:** amortizes VA reservation across many slots in resident pools.
- **Evaluation-only** delivery: `lib2adic_shim.so` + `iron_gate_v1` driver; production tiers under separate license.

### 3.3 Workload (synthetic)

| Field | Value |
|-------|-------|
| `workload_id` | `a_gate_v1_kv_microbench` |
| `workload_class` | `synthetic_kv_band` |
| F1′ stress | `stress_mode=f1_frag_v1`, uniform 512 KiB, 8 s resident |
| Budget curve | `stress_mode=vram_budget_curve_v1`, iso fill 50/60/70% |

**TABU:** Calling this “vLLM evaluation.”

---

## 4. Methodology

**Hardware:** NVIDIA H100 PCIe, driver **550.163.01**, May 2026 iron window.

**Comparison:** stock vs shim (`LD_PRELOAD=lib2adic_shim.so`, `HWATOM_SHIM_PACK=1`, `HWATOM_PACK_MEGA=1`) in **separate processes** for resident metrics (churn/resident split—see A45D).

**Outputs:** JSON per bench + **GATE12** stdout (human- and machine-parseable). Metrics defined in A49:

- `cache_liberation_pct = (committed_stock − committed_shim) / committed_stock`
- `logical_kv_gain_pct = (logical_shim − logical_stock) / logical_stock`
- `layout_efficiency_pct = logical / committed`

**Docker repro:** `docker/gate-12s/Dockerfile.f1prime`, `A46_DOCKER_OK`.

---

## 5. Results

**Table 1 — Canonical H100 (2026-05-22, `results/GATE12_canonical.txt`)**

| Scenario | Metric | Stock | Shim |
|----------|--------|-------|------|
| VRAM @ **70%** budget | `cache_liberation_pct` | — | **42.2%** |
| VRAM @ **70%** budget | `logical_kv_gain_pct` | — | **+131%** |
| VRAM @ **70%** budget | `committed_bytes` | 59.45 GB | 34.36 GB |
| VRAM @ **70%** budget | `resident_slots` | 28,348 | 65,536 (plateau) |
| F1′ uniform 8 s | `resident_slots` | 30,202 | 30,248 |
| F1′ uniform 8 s | `layout_efficiency_pct` | 70.3% | **100%** |
| Docker F1′ | `resident_slots_shim` | — | 29,877 |

**Curve context (honest):** @50% budget, liberation **19.1%** (stock not yet scaled committed); @60% **32.6%**. Band anchor 30–40% applies to **70%** tier, not every point.

**Mechanism ceiling:** iso fixed 2 GiB logical target → up to **~75%** liberation (k=4), internal doc only—not mixed with curve headline without disclaimer.

---

## 6. Limitations

1. **Synthetic workload** — not end-to-end transformer inference or vLLM P99 latency.
2. **Single GPU SKU** — H100 PCIe; other GPUs need re-measurement (granularity may differ).
3. **Slot plateau** — shim hits `ISO_MAX_SLOTS=65536` before exhausting 70% committed cap; do not imply shim fills entire budget.
4. **Single-node** — no multi-GPU NCCL study (deferred).
5. **GQA alias path** excluded — unstable on iron; logical GQA mode optional annex only.

---

## 7. Reproducibility and artifact

```bash
git clone https://github.com/StanByriukov02/hwatom-gate-12s.git
cd hwatom-gate-12s
cd hardware_atom
git checkout t1-eval-20260522
bash scripts/shim/run_docker_a46_gate12_f1prime_v1.sh
```

Expect `GATE12_BEGIN`, `workload_id=a_gate_v1_kv_microbench`, `A46_DOCKER_OK`.

**License:** Evaluation-Only (`LICENSE.md`). Production/unlimited binaries require separate agreement (contact in GATE12 banner).

**Post-repo note:** public eval Docker may add structural `HWATOM_PACK_K_CAP` after initial tag; **this paper’s Table 1** remains tied to tag `t1-eval-20260522` iron/H100 measurements.

---

## 8. AI use and author responsibility

**Structure assisted by Cursor Agent; all benchmarks run on H100 by the author.** Generative tools did not execute GPU experiments and did not alter GATE12 metrics or Table 1 values.

---

## 9. References

See `references.bib`. Compile with standard BibTeX; no proprietary bibliography.

---

## Appendix A — GATE12 excerpt (70% curve)

```
cache_liberation_pct_70=42.2040
logical_kv_gain_pct_70=131
committed_bytes_stock_70=59450064896
committed_bytes_shim_70=34359738368
resident_slots_shim_70=65536
```

Source artifact: `bench_artifacts/h100_pull_20260522/vram_curve_20260522T062629Z/`
