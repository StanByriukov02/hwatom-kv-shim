# Pilot numerator — pre-registered (partner Phase B)

**Status:** **ACTIVE** — sign before partner run · **not** a public tag bump  
**Parent:** `WEDGE_PARTNER_EVAL_OPEN_20260525_V1.md` · `B_CONC_18_COMMODITY_INFERENCE_GATE_V1.md`  
**Post-2026-05-25 evidence:** B₁ PARK · MOON R12 coupling yes / delivery 0% · `DAY_ENTROPY_REGISTER_20260525_V1.md`

**Format:** markdown basis for 1-page PDF/email — export is operator step, not agent theater.

---

## 0. Why this doc exists (shepherd, not sheep)

| Sheep game | Shepherd game |
|------------|---------------|
| «Match our 131% / 42% on your vLLM @0.85» | «Run **this** protocol on **your** cumem/sleep matrix» |
| NVML total / token proxy when pool pre-sized | **pack_committed** + reserve geometry + tok/s @ **fixed committed** |
| Win vAttention headline | **Composable** internal leaf layer — joint trace or no battle claim |

**Locked fact (2026-05-25):** shim **visible** in cumem trace; **0%** uplift on vLLM NVML/logical_kv_proxy @ preload — **not** a reason for another annex util sweep.

---

## 1. Preconditions (partner must confirm in writing)

| # | Requirement | If false |
|---|-------------|----------|
| P1 | vLLM **≥0.14**, **≠0.17.0** (B-CONC-09) | **ABORT** — wrong stack |
| P2 | **Phase B:** `--enable-sleep-mode` / cumem KV path active | Phase A = **null test** (B-CONC-01) |
| P3 | `PYTORCH_ALLOC_CONF=expandable_segments:False` (B-CONC-10) | **ABORT** |
| P4 | TP=1, single H100 (B-CONC-07) | PARK multi-GPU |
| P5 | Partner names `workload_id` + image digest + `build_id` | No GATE12 without audit row |

Ref: `B12_ENV_MATRIX_V1.md`

---

## 2. Primary numerator (G1 — pre-registered)

**At fixed `committed_bytes` cap** (partner states cap; we do not move goalposts mid-run):

| Field | Definition | Source tool |
|-------|------------|-------------|
| **`pack_committed_bytes`** | Max `SHIM_STATS pack_committed_bytes=` in shim arm log | `b21_parse_cumem_trace_v1.py` |
| **`reserve_bytes_max`** | Max `cuMemAddressReserve size=` in shim trace | same + vLLM log |
| **`mega_reserve_count`** | `SHIM_STATS mega_reserve_count=` | fini line |
| **`effective_kv_gain_pct`** | `(logical_kv_proxy_shim − logical_kv_proxy_stock) / stock × 100` **only if** P2–P5 true | `b21_gate12_engine_v1.py` measure window |

**PASS G1 (pilot):** `effective_kv_gain_pct ≥ 5.0` **OR** (`pack_committed_bytes` shows ≥10% lower bytes for same token load — operator defines paired run).

**Honesty slot:** if `effective_kv_gain_pct = 0` but `pack_committed_bytes` ≫ 0 and `reserve_bytes_max` ≥ 1 GiB → report **「coupling without pool proxy uplift」** — **not** FAIL mechanism, **FAIL** sheep metric.

---

## 3. Guard rails (G2–G5)

| Gate | Criterion | FAIL |
|------|-----------|------|
| **G2 tok/s** | shim ≥ stock −0.5% (B22 class) | regression |
| **G3 path** | `n_reserve_trace` shim > stock in cumem parse | cudaMalloc bypass |
| **G4 stack** | Their image tag recorded | wrong workload |
| **G5 audit** | `GATE12_BEGIN…END` in artifact dir | screenshot-only |

---

## 4. TABU on pilot slide (copy-paste)

- «**42% / 131%** on your fleet» (T1 / iso only)  
- «**NVML liberation**» on vLLM preload pool  
- «Beat **vAttention**» without one shared trace  
- «**LD_PRELOAD** on any commercial image» without Phase B checklist  
- Fleet N% from `t1-leaf-physics` tag alone  

---

## 5. What Layer A already proved (no re-H100 needed)

| Claim | Tag |
|-------|-----|
| Leaf pack ceiling on iso @ budget | `t1-leaf-physics-20260525` · `LEAF_PHYSICS_OK=yes` |
| tok/s not broken @ 0.5B | B22 class |
| Mechanism hooks cumem | annex A + R12 coupling |

**Partner repro (2 h, no vLLM):**

```bash
git checkout t1-leaf-physics-20260525
bash scripts/shim/run_iron_leaf_ceiling_v1.sh
```

---

## 6. Pilot run package (when inbound — needs H100)

| Step | Script class | New GPU? |
|------|--------------|----------|
| 1 | Partner confirms P1–P5 | no |
| 2 | `run_iron_b21_gate12_v1.sh` or annex on **their** digest | **yes** — one paired run |
| 3 | `emit_*` + update wedge §4 slot | no |

**Until inbound:** **no** proactive B-CONC-18 H100 (DECIDE · entropy register).

---

## 7. Result slot (fill after pilot)

| Field | Value |
|-------|-------|
| `workload_id` | _pending_ |
| `build_id` | _pending_ |
| `effective_kv_gain_pct` | _pending_ |
| `pack_committed_bytes` | _pending_ |
| `reserve_bytes_max` | _pending_ |
| `tok/s_delta_pct` | _pending_ |
| **Pilot PASS** | _pending_ |

---

## 8. Export to PDF (operator)

1. Print §0–§5 + §7 empty slot from this file.  
2. Attach `results/GATE12_leaf_physics_v1.txt`.  
3. Do **not** attach MOON 131% as vLLM claim.

**Next doc session (separate):** Gemini Deep Research R&D PDF filter — `GEMINI_*_FILTERED` · `B_RD_FUTURE_REGISTER` — not mixed into this numerator.
