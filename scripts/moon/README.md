# MOON-representation iron scripts

**Spec:** `docs/agent_workflow/MOON_REPRESENTATION_RD_SPEC_20260525_V1.md`  
**Public guide:** `docs/MOON_XQ_EVAL.md`

| Script | Purpose |
|--------|---------|
| `tier1_moon_rep_bounds_v1.py` | NumPy bytes bounds (no GPU) |
| `tier2_moon_rep_cos_v1.py` | Key cos sim |
| `path_a_tier3_vllm_iron_v1.py` | vLLM TurboQuant Tier3 iron |
| `path_b_hb_fa2_parity_v1.py` | H-B: FA2 parity tok/s falsifier |

**Docker repro:** `integrations/vllm/recipe_moon_xq_v1.sh`
