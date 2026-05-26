# MOON-Xq eval + shim 0.2.0 — `eval-moon-xq-20260526`

**semver:** 0.2.0 · **workload_id:** `MOON-REP-Xq-VLLM-01` · **model:** Qwen2.5-7B-Instruct · **stack:** vLLM 0.21 TurboQuant

## PASS (iron)

| Gate | Result |
|------|--------|
| KV bytes reduction | **−58%** (preset aligned) |
| ppl drift | **+0.58%** |
| NIAH deep prefill | **PASS** |
| tok/s vs FP16 (FA2 parity) | **0.86×** — guard FAIL |

## Shim 0.2.0

- `SHIM_STATS stats_v=2` · `eval_shim=1` · `k_cap=2` in public docker
- Full-K lab: `make -C src/shim all` (tag `t1-leaf-physics-20260525` unchanged)

## Repro

```bash
git checkout eval-moon-xq-20260526
cat results/MOON_XQ_GATE_SUMMARY.txt
# MOON: bash integrations/vllm/recipe_moon_xq_v1.sh path-a-v2
# pack: bash scripts/shim/run_iron_leaf_ceiling_v1.sh  # full-K lab shim
```

## TABU

- Tier3 PASS · vLLM pool % · beat vAttention without shared trace

## Keywords (recon)

`MOON-Xq` `turboquant` `bytes-per-token` `eval-moon-xq-20260526` `MOON-REP-Xq-VLLM-01` `vllm-0.21` `KV-cache` `cuMem` `intra-leaf` `gate-12s` `shim-stats-v2` `eval-shim-k-cap`
