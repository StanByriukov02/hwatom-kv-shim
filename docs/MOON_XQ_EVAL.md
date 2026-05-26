# MOON-Xq evaluation (composable · vLLM TurboQuant)

**License:** same as [LICENSE.md](../LICENSE.md) (Evaluation-Only).  
**Parent:** Layer A [README_T1_EVAL.md](../README_T1_EVAL.md) · pilot numerators [agent_workflow/PILOT_NUMERATOR_PREREG_20260525_V1.md](agent_workflow/PILOT_NUMERATOR_PREREG_20260525_V1.md)

---

## What this is

**MOON-Xq** = measurable **KV bytes/token** reduction + **e2e quality** on a **production-class stack** (vLLM **0.21** + `turboquant_*` KV dtypes), not a custom quant kernel in this repo.

**Orthogonal to Layer A (.so pack):** pack proves **intra-leaf** coupling on `cuMem*`; MOON-Xq proves **fewer bytes per KV token** at long context.

---

## What iron proved (2026-05-26)

| Gate | Result | Receipt |
|------|--------|---------|
| bytes @ preset | **PASS** −58% | `agent_workflow/TIER1_MOON_REP_VERDICT_20260526_V1.md` |
| cos (keys) | **PASS** 0.976 | `TIER2_MOON_REP_VERDICT_20260526_V1.md` |
| ppl drift | **PASS** +0.58% | `PATH_A_TIER3_VLLM_RECEIPT_20260526_V2.json` |
| NIAH (deep prefill) | **PASS** | same |
| tok/s vs FP16 | **FAIL** ~0.86× (FA2 parity) | `PATH_B_HB_FA2_RECEIPT_20260526_V1.json` |

**Headline allowed:** −58% KV bytes · &lt;1% ppl drift · needle retrieval to 32k prefill on Qwen2.5-7B + vLLM 0.21.  
**Headline forbidden:** full Tier3 PASS · beat vLLM · NVML pool % win.

Canonical one-pager: [../results/MOON_XQ_GATE_SUMMARY.txt](../results/MOON_XQ_GATE_SUMMARY.txt)

---

## Repro (H100 · docker)

```bash
# from repo root
bash scripts/moon/recipe_moon_xq_v1.sh path-a-v2
# H-B FA2 parity (tok/s root cause)
bash scripts/moon/recipe_moon_xq_v1.sh hb-fa2
```

Requires: NVIDIA GPU, Docker, HuggingFace cache for `Qwen/Qwen2.5-7B-Instruct`, image `vllm/vllm-openai:v0.21.0`.

Outputs: `docs/agent_workflow/PATH_A_TIER3_VLLM_RECEIPT_*.json` (git-tracked after run).

---

## Partner / buyer numerators

| Primary (shepherd) | Guard |
|--------------------|-------|
| `bytes_per_token` / KV reduction @ context | tok/s ratio vs FP16 (**~0.86×** measured, not 0.97) |
| effective context per GiB KV | ppl drift ≤1% |

Do **not** use vLLM preload pool % or NVML liberation as PASS for this layer.

Full pre-reg: `agent_workflow/PILOT_NUMERATOR_PREREG_20260525_V1.md`

---

## Scripts

| Script | Role |
|--------|------|
| `scripts/moon/path_a_tier3_vllm_iron_v1.py` | Path A Tier3 iron |
| `scripts/moon/path_b_hb_fa2_parity_v1.py` | H-B FA2 falsifier |
| `scripts/moon/tier1_moon_rep_bounds_v1.py` | Tier1 bytes bounds |

See [../scripts/moon/README.md](../scripts/moon/README.md)

---

## What we do not ship here

- Custom MOON quant in upstream vLLM  
- Xm / MLA iron (park until buyer or Xq FAIL)  
- Branch B pluggable allocator (separate track)

Release scope: [RELEASE_0.2.0.md](RELEASE_0.2.0.md) (full strategy notes stay in private dev repo).
