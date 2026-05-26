# Release 0.2.0 — eval bundle (2026-05-26)

**Git tag:** `eval-moon-xq-20260526` · **semver:** `0.2.0` (see [VERSION](../VERSION))  
**Not:** production license · not Tier3 EOD PASS · not full-K public shim

---

## What shipped

| Track | Deliverable |
|-------|-------------|
| **MOON-Xq** | Iron + receipts: bytes −58%, quality PASS, tok/s ~0.86× |
| **Layer A shim** | `stats_v=2`, eval binary `lib2adic_shim_eval.so` (K≤2/leaf) |
| **Repro** | `docs/MOON_XQ_EVAL.md`, `integrations/vllm/recipe_moon_xq_v1.sh` |
| **Recon** | `results/MOON_XQ_GATE_SUMMARY.txt`, `paperswithcode.yaml` row, N4 release notes |

**Index:** [EVAL_BUNDLE_INDEX.md](EVAL_BUNDLE_INDEX.md)

---

## Who this is for

| Audience | Gets | Does not get |
|----------|------|----------------|
| **Senior GPU engineer** | PASS/FAIL receipts, repro commands, TABU | Tier3 win headline |
| **Recon / bots** | tag, keywords, `workload_id`, machine summary txt | NVML pool % claim |
| **Partner pilot** | bytes numerator + pilot prereg link | full-K `.so` |
| **Forker** | eval-capped docker + open scripts | your iron %, SLA, licensed build |

---

## Rebuild commands

```bash
make -C src/shim eval          # public docker
make -C src/shim all           # lab iron full K
bash integrations/vllm/recipe_moon_xq_v1.sh path-a-v2   # H100 + docker
```
