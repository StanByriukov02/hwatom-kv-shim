# Tier 1 MOON-representation — verdict 2026-05-26

**Status:** **PASS** (bounds only · no GPU)  
**Script:** `scripts/moon/tier1_moon_rep_bounds_v1.py`  
**Receipt:** `TIER1_MOON_REP_RECEIPT_20260526_V1.json`  
**Elapsed:** 0.0003 s (local)

---

## Hypothesis H0 (Tier 1)

Can MOON-representation plausibly hit **≥30%** KV bytes/token vs FP16 @ 32k **and** stay above **cos proxy 0.940** (not iron Tier 2)?

**Answer:** **YES** for TurboQuant-class (MOON-Xq). MLA (Xm) also passes conservative bound on DeepSeek-V2-Lite config.

---

## Primary model: Qwen2.5-7B-Instruct

| Metric | Value |
|--------|-------|
| FP16 KV bytes/token | **57,344** |
| KV @ 32k ctx (FP16) | **~1.75 GiB** |

| Profile | Reduction | cos proxy | Tier1 |
|---------|-------------|-----------|-------|
| xq_k4v8_all | **62.5%** | 0.965 | PASS |
| xq_k4v8_boundary2 | 58.0% | 0.968 | PASS |
| xq_qjl_k1v8 | **66.7%** | 0.945 | PASS |
| xq_k4v4_aggressive | 75.0% | 0.928 | **FAIL** (cos) |

**Falsifier note:** K4V4 shows bytes win can violate cos — do not headline aggressive quant.

---

## Path X — signed from Tier 1

| Field | Value |
|-------|-------|
| **path_X** | **MOON-Xq** (TurboQuant-class quant) |
| **iron_model** | `Qwen/Qwen2.5-7B-Instruct` |
| **workload_id** | `MOON-REP-Xq-01` |
| **72h kill clock** | starts **2026-05-26** (Tier 1 PASS) |
| **Xm** | viable on paper (MLA 75% red @ 0.25 factor) — **not** primary (Qwen stack) |

**Operator override:** one line in chat to switch to Xm.

---

## Next (Tier 2 — needs GPU / small)

- Profile: **xq_k4v8_boundary2** first (safer cos than QJL edge case)
- Gate: **cos_sim ≥ 0.940** on attention keys (iron, not proxy)
- **TABU until Tier 2 design locked:** random H100 annex / vLLM %

---

## Changelog

| Date | Note |
|------|------|
| 2026-05-26 | Tier 1 PASS · path Xq signed |
