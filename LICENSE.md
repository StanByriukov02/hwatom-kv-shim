# Hardware Atom — Evaluation License (T1 / gate-12s)

**Version:** Evaluation-Only v1  
**Contact (production / unlimited):** stanislav.byriukov.research@gmail.com (gain-share)

## Grant

You may build and run the evaluation artifacts in this repository (including `iron_gate_v1`, `lib2adic_shim.so`, and Docker images under `docker/gate-12s/`) **only** to reproduce documented benchmarks (`GATE12_*` stdout, `workload_id=a_gate_v1_kv_microbench`) on hardware you control.

## Restrictions (evaluation)

- **No production use** — not for serving inference, multi-tenant fleets, or customer workloads without a separate written agreement.
- **No reverse engineering** of evaluation binaries beyond what is required to verify reproducibility of published metrics.
- **No removal** of `HWATOM_EVAL` / license banners from tool output when redistributing measurement logs.
- Claims in public materials must match measured `GATE12` fields and disclaimers in `docs/agent_workflow/A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md`.

## Tiers (eval vs licensed)

| Tier | Artifact | Packing / K |
|------|----------|-------------|
| **T1 eval** (`t1-eval-20260522`) | `Dockerfile.f1prime` | Full pack+mega — matches arXiv / `GATE12_canonical.txt` |
| **Eval cap** (`gate-12s-eval`, post-T1) | `Dockerfile.eval` | Structural `HWATOM_PACK_K_CAP` — lower effective slots per leaf; **not** a time throttle |
| **Licensed / production** | Private deliverable (not in public git) | Full K + vLLM integration (Track B) under contract |

Details: `docs/agent_workflow/TWO_CONTOUR_EVAL_OPEN_VS_PARTNER_CLOSED_V1.md`, `docs/agent_workflow/KCAP_LOCAL_BRANCH_V1.md`.

## Production / partnership

Unlimited binaries, integration tiers (T2+), SLA, and commercial terms — contact the email above.  
Tier map: `docs/agent_workflow/SHIM_INVIOLABLE_CONTRACT_V1.md` (T1 public gate · T2 customer vLLM · T3 partner internal).

## Disclaimer

Software provided **as-is** for evaluation. Metrics depend on GPU SKU, driver, CUDA version, and workload class; compare stock vs shim only within the same run and environment.
