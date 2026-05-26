# Public eval bundle index

Single map for engineers, partners, and recon bots.

---

## Layers

| Layer | Doc | Repro | Canonical result |
|-------|-----|-------|------------------|
| **A — pack / leaf** | [README_T1_EVAL.md](../README_T1_EVAL.md) | `scripts/shim/run_iron_leaf_ceiling_v1.sh` | `results/GATE12_leaf_physics_v1.txt` |
| **MOON-Xq — bytes-KV** | [MOON_XQ_EVAL.md](MOON_XQ_EVAL.md) | `integrations/vllm/recipe_moon_xq_v1.sh` | `results/MOON_XQ_GATE_SUMMARY.txt` |
| **B — pluggable / phantom** | `agent_workflow/B_LAYER_PUBLIC_ROADMAP_V1.md` | not shipped | — |

---

## Shim (.so) — engineer-facing stats

| Item | Location |
|------|----------|
| Build | `make -C src/shim` → `lib2adic_shim.so` |
| Stats line v2 | `HWATOM_SHIM_STATS=1` → stderr `SHIM_STATS stats_v=2 ...` |
| JSON option | `HWATOM_SHIM_STATS_JSON=1` |
| Parse cumem + stats | `scripts/vllm/b21_parse_cumem_trace_v1.py` |
| Pilot numerator | `agent_workflow/PILOT_NUMERATOR_PREREG_20260525_V1.md` |

**v2 fields:** `build_id`, `pack_committed_peak_bytes`, `pack_committed_fini_bytes` (peak in `pack_committed_bytes` for backward compat).

---

## MOON iron receipts (JSON)

| File | Content |
|------|---------|
| `agent_workflow/PATH_A_TIER3_VLLM_RECEIPT_20260526_V2.json` | Path A fair iron |
| `agent_workflow/PATH_B_HB_FA2_RECEIPT_20260526_V1.json` | H-B FA2 parity |
| `agent_workflow/TIER1_MOON_REP_RECEIPT_20260526_V1.json` | Tier1 bounds |

---

## Release tags (CHANGELOG)

| Tag / semver | What |
|--------------|------|
| `t1-leaf-physics-20260525` | Layer A leaf iron (full-K lab shim) |
| **`0.2.0`** / `eval-moon-xq-20260526` | MOON-Xq + shim stats v2 + eval K-cap |

See [../CHANGELOG.md](../CHANGELOG.md) · [RELEASE_0.2.0.md](RELEASE_0.2.0.md)

## Recon surfaces (who indexes what)

| Surface | Triggers | File |
|---------|----------|------|
| Git tag API | `eval-moon-xq-20260526` | GitHub release |
| README keywords | bots, search | root `README.md` |
| paperswithcode.yaml | crawlers | repo root |
| N4 release notes | intel probe | `agent_workflow/recon_feeds/N4_*` |
| Machine summary | one-file parse | `results/MOON_XQ_GATE_SUMMARY.txt` |
| TABU honesty | senior engineer trust | `MOON_XQ_EVAL.md` |

**Not primary:** bare semver alone — **tag + workload_id + keywords** (N3 path).
