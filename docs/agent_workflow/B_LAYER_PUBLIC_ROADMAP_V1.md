# Layer B — public roadmap (Track B, not in tag `t1-eval-20260522`)

**Status:** planned · **Public eval today:** Layer A only (synthetic GATE12 + Docker)

This file replaces internal B-vault links in the public repo. Detailed engineering maps stay in the private development tree until a **Track B** release tag ships.

---

## Stages (what will ship later)

| Stage | Deliverable | Public today? |
|-------|-------------|---------------|
| **1 — Integration** | vLLM on **cuMem / sleep-mode** path; env matrix for pins | No — partnership / next tag |
| **2 — Smoke** | Recipe proving shim hooks `cuMem*` on a real server image | No |
| **3 — Iron** | Fair stock vs shim on one production-class workload; NVML + GATE12 | No public % until iron closes |
| **4 — Headline** (after iron) | More effective KV at same committed VRAM; tokens/s ≥ baseline | No |

---

## What to use now

| Need | Path |
|------|------|
| Reproduce Layer A | `README_T1_EVAL.md` · tag `t1-eval-20260522` |
| Claim discipline | `A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md` |
| Eval vs licensed | `TWO_CONTOUR_EVAL_OPEN_VS_PARTNER_CLOSED_V1.md` |
| Paper source | `docs/arxiv/paper/main.tex` |

**Contact for Track B pilot:** stanislav.byriukov.research@gmail.com
