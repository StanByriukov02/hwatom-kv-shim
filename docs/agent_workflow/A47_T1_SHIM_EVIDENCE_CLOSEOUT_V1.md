# A-4.7 / T1 — shim evidence closeout (2026-05-22)

## What we claim (partnership band)

| Claim | Evidence | Status |
|-------|----------|--------|
| Shim survives F1′ resident stress (uniform) | `IRON_A45B_F1PRIME_OK`, stock ~30202 / shim ~30248 | **OK** |
| Layout efficiency 100% on uniform + pack + mega | A-4.5b `eff_shim=100` | **OK** |
| VRAM budget curve 50/60/70% (iso) | `IRON_A48B_VRAM_CURVE_OK`, shim 65536 @ 70% | **OK** |
| Docker repro gate-12s | `IRON_A46_DOCKER_OK`, slot check | **OK** |
| Claim protocol / liberation method | `A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md` | **OK** |
| Mixed frag ladder (production-like sizes) | `frag_shim_mixed.json`, no INVALID wall | **OK** (count tuning optional) |
| GQA iso + budget | `run_iron_a214b_gqa_iso_v1.sh` (`IRON_GQA_LOGICAL=1`) | **claim OK** — не заявлять `HWATOM_GQA_ALIAS=1` |

## What we do **not** claim without extra work

- In-process churn → resident recovery in **one** `iron_gate_v1` process (use **two processes** in scripts).
- GQA until curve matches non-GQA band.
- Mixed frag slot **count** parity with stock (efficiency story is separate).
- T2–T4 (vLLM wedge, full canon) — no H100 required for T1 bundle.

## Churn / VA poisoning (operator FAQ)

**Суть:** CUDA VA space после churn в том же процессе не возвращается к «как до churn» через `shim_driver_reset_v1()` — это лимит драйвера/процесса, не «shim не работает».

**Чинить для claim?** Для **F1′ / T1 / Docker / curve** — **нет**, если resident метрики сняты в **отдельном процессе** (`run_iron_a45b_f1prime_v1.sh`). Churn-only pass = stress artifact.

**Чинить для продукта (A-layer)?** Уже в скриптах: churn = один вызов `iron_gate_v1`, resident = следующий вызов (новый процесс). Docker entrypoint так же.

**vLLM / long-lived allocator (B-layer):** отдельный эпик, **не** блокер T1. Не смешивать с claim «F1′ iron + gate-12s Docker».

**Без починки in-process можно выкладывать?** **Да**, с явной методологией (два процесса). **Нет** — если заявлять «churn + resident в одном процессе без reset».

## Scope discipline

На каждый баг/хвост в vault — блок **`## Scope gate`** по `SCOPE_GATE_THREE_QUESTIONS_V1.md` (не mdc, тот же MD что факты).

## Before public push

- [x] Root `LICENSE.md` (Evaluation-Only) — pairs with `HWATOM_EVAL` in GATE12 stdout
- [x] `README_T1_EVAL.md` — lead narrative; **не** обещает Z>3, inference P99, FLOPS/W win
- [x] `results/GATE12_canonical.txt` + `results/NODE4b_CACHE_LIBERATION_MEASURED_V1.txt`
- [x] `docs/agent_workflow/T1_PUBLIC_BUNDLE_V1.md` — open/closed + Docker tier answer
- [ ] Operator: choose public vs private remote; scrub secrets from `bench_artifacts/` commit

## Artifact bundle (T1)

```
bench_artifacts/gate12_f1prime_*/     # F1′ + mixed + uniform
bench_artifacts/vram_curve_*/         # A-4.8b JSON + GATE12 emit
bench_artifacts/docker_a46_*/         # container stdout + GATE12
docs/agent_workflow/A45D_*.md
docs/agent_workflow/A46_*.md
docs/agent_workflow/A48B_*.md
docs/agent_workflow/A49_*.md
scripts/shim/run_iron_a45b_f1prime_v1.sh
scripts/shim/run_iron_a48b_vram_curve_v1.sh
scripts/shim/run_docker_a46_gate12_f1prime_v1.sh
```

## Node 4b (30–40% cache liberation)

Protocol: `2026-05-22_NODE4b-kv-gain-measurement-v1.md` (repo search) — package **measured** `cache_liberation_pct_*` from A-4.8b emit, not headline-only %.
