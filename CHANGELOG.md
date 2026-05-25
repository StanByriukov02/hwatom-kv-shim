# Changelog — public eval releases

Format: dated tags on [hwatom-kv-shim](https://github.com/StanByriukov02/hwatom-kv-shim). Private dev: `hardware_atom`.

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
