# Changelog — public eval releases

Format: dated tags on [hwatom-kv-shim](https://github.com/StanByriukov02/hwatom-kv-shim). Private dev: `hardware_atom`.

---

## [0.2.0] = [eval-moon-xq-20260526] — 2026-05-26

**Semver:** `VERSION` → **0.2.0** · **Release notes:** `docs/RELEASE_0.2.0.md` · **Recon:** `docs/agent_workflow/recon_feeds/N4_RELEASE_NOTES_eval-moon-xq-20260526.md`

**Layer:** MOON-Xq (KV bytes/token · vLLM TurboQuant) — **composable** with Layer A pack, **not** a replacement.

### Added

- Public eval guide: `docs/MOON_XQ_EVAL.md`
- Bundle index: `docs/EVAL_BUNDLE_INDEX.md`
- Iron scripts: `scripts/moon/path_a_tier3_vllm_iron_v1.py`, `path_b_hb_fa2_parity_v1.py`
- Docker recipe: `integrations/vllm/recipe_moon_xq_v1.sh`
- Canonical summary: `results/MOON_XQ_GATE_SUMMARY.txt`
- Iron receipts under `docs/agent_workflow/PATH_*_20260526_*.json`

### Shim stats v2 (Layer A `.so`)

- `SHIM_STATS stats_v=2` with `build_id`, `pack_committed_peak_bytes`, `pack_committed_fini_bytes`
- `pack_committed_bytes` at fini = **peak** (fixes teardown=0 false zero)
- Optional `HWATOM_SHIM_STATS_JSON=1` one-line JSON
- Parser: `scripts/vllm/b21_parse_cumem_trace_v1.py` accepts v1 and v2 lines

Rebuild lab: `make -C src/shim all` · public docker: `make -C src/shim eval` → `lib2adic_shim_eval.so`

### Eval K-cap (structural)

- Public Docker images use **eval** shim: **K≤2** slots/leaf baked in (`HWATOM_EVAL_SHIM_BUILD`)
- Full-K `lib2adic_shim.so` remains for lab iron (`make all`) — canonical `t1-leaf-physics` repro
- `SHIM_STATS` adds `eval_shim=` `k_cap=`

### Measured (H100 PCIe, vLLM 0.21, Qwen2.5-7B-Instruct)

| Claim | Status |
|-------|--------|
| KV bytes −58% @ preset | **PASS** |
| ppl drift +0.58% | **PASS** |
| NIAH deep prefill | **PASS** |
| tok/s vs FP16 @ FA2 parity | **0.86× FAIL** (EOD 0.97 guard) |

### TABU

- Tier3 PASS headline · vLLM pool % · NVML liberation for MOON layer

---

## [t1-leaf-physics-20260525] — 2026-05-25

**Layer:** intra–2 MiB CUDA leaf (cuMem pack), **not** vLLM / NVML.

### Added

- `workload_id=t1_leaf_physics_v1` — iron harness `iso_logical_v2` @ VRAM budgets 50/60/70%
- Canonical GATE12: `results/GATE12_leaf_physics_v1.txt` (`LEAF_PHYSICS_OK=yes`)
- Measurement contract: `docs/agent_workflow/IRON_LEAF_CEILING_SPEC_V1.md`
- Repro: `scripts/shim/run_iron_leaf_ceiling_v1.sh`

### Measured (H100 PCIe, driver 550.163.01, `build_id=b_shim_20260525a`)

| Metric @70% budget | Value |
|--------------------|-------|
| `layout_efficiency_shim` (η_leaf) | **100%** (ceiling) |
| `eta_leaf_ceiling_ratio` | **1.00** |
| `logical_kv_gain_pct` | **131%** |
| `cache_liberation_pct` @ committed | **42.20%** |

### TABU in this release (do not misread)

- **Not** claiming vLLM throughput, P99, or NVML liberation %
- **Not** claiming `resident_slots_shim` as battle win (diagnostic only)
- **Not** replacing tag `t1-eval-20260522` — complementary workload

### Composable

Two-layer fragmentation: this release proves **internal leaf packing**; external page layers (e.g. vAttention-class) are a **different layer** — see `B_LAYER_PUBLIC_ROADMAP_V1.md`.

---

## [t1-eval-20260522] — 2026-05-22

Frozen Layer A eval: F1′ + VRAM curve @70%. See `results/GATE12_canonical.txt`.
