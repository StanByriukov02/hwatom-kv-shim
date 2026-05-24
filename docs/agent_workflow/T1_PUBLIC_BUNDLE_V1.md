# T1 public bundle — что в открытом доступе (A-4.7)

**Status:** ready to ship (2026-05-22)  
**Contract:** `SHIM_INVIOLABLE_CONTRACT_V1.md` · **License:** root `LICENSE.md` (Evaluation-Only)

---

## Docker: один продукт или отдельные сборки?

| Артефакт | Назначение | Кому |
|----------|------------|------|
| **`docker/gate-12s/Dockerfile.f1prime`** → image `hwatom:gate-12s-f1` | **T1 evaluation** — единый публичный repro для clone/build/run | Любой оценщик по `LICENSE.md` |
| **Iron scripts** (`scripts/shim/run_iron_*.sh`) | Тот же workload **вне** Docker (lab/H100) | Operator / advanced repro |
| **Production / unlimited binary** | **Не** этот Dockerfile | Отдельный договор (email в `HWATOM_EVAL` banner) |
| **T2** (`integrations/vllm/`, LD_PRELOAD на **их** vLLM) | **Не** в public v1 | Partner integration path |
| **T3** (eBPF, UVM.ko, DKMS) | **Запрещено** в public v1 | Partner internal only |

**Ответ:** собранный Docker — это **готовый evaluation-продукт T1** для партнёра/провайдера под Evaluation License (скачал → `docker build` → `docker run --gpus 1` → `GATE12` + баннер). Это **не** «production shim в проде».

### Кастом под контракт (обязательно помнить)

| Rule | Detail |
|------|--------|
| Workload | Тот же класс (`f1_frag_v1` / iso curve, `workload_id=a_gate_v1_kv_microbench`) |
| Tag/env/SLA | Допустимы (`hwatom:gate-12s-<partner>`, `HWATOM_*`) |
| Claim | **Новый claim → новый GATE12** + методология; нельзя молча приравнять eval к Licensed/Production |
| Production | **Другой** artifact и договор — не этот public Dockerfile |

Полная таблица: `DOCKER_TIER_AND_PROTECTION_V1.md`.

### Два Dockerfile

| File | Role |
|------|------|
| `docker/gate-12s/Dockerfile` | **Scratch / CI** — dry gate, no GPU repro |
| `docker/gate-12s/Dockerfile.f1prime` | **T1 GPU repro** — партнёрский путь |

**Base `nvidia/cuda` на f1prime:** repro у провайдера + parity с iron (A-4.6); scratch не заменяет GPU proof — см. `DOCKER_TIER_AND_PROTECTION_V1.md` §3.

### Защита eval (свойства, не time/token)

**Сейчас:** LICENSE + `HWATOM_EVAL` + узкий образ (только iron+shim+F1′) + **нет** T2/T3/vLLM/eBPF.  
**После T1:** eval vs licensed tag, optional `HWATOM_PACK_K_CAP` — `COMMERCIAL_NOTES_CONTRACTUAL_K_TIER_V1.md` (**PARK_POST_T1**). Сегодня pack+mega в eval **включены** для честного repro; урезание K = отдельный engineering+legal шаг, не забыто.

---

## Открыто (public repo v1)

| Path | Content |
|------|---------|
| `LICENSE.md` | Evaluation-Only |
| `README_T1_EVAL.md` | Lead narrative + repro one-liner |
| `src/shim/` | `lib2adic_shim.so` sources |
| `tools/shim_iron/iron_gate_v1.c` | GATE12 emitter |
| `docker/gate-12s/` | Dockerfile.f1prime + entrypoint |
| `scripts/shim/run_docker_a46_gate12_f1prime_v1.sh` | Host build/run helper |
| `results/GATE12_canonical.txt` | Paste-friendly canonical stdout |
| `results/NODE4b_CACHE_LIBERATION_MEASURED_V1.txt` | Measured % @ 70% budget |
| `docs/agent_workflow/A49_CLAIM_PROTOCOL_*.md` | Claim discipline |
| `docs/agent_workflow/A47_T1_SHIM_EVIDENCE_CLOSEOUT_V1.md` | Evidence index |

## Закрыто / TABU в public

| TABU | Reason |
|------|--------|
| T3 eBPF / `.ko` / DKMS | Contract tier T3 |
| vLLM inside gate README as primary claim | Track B |
| Z>3, industry FLOPS/W, inference P99 | No population baseline |
| `HWATOM_GQA_ALIAS=1` path | Unstable; matrix says logical only |
| Fabricated `results/*` | C-129 |
| Production keys, operator H100 creds | Security |

## Scout annex (optional, not T1 lead)

| Path | Role |
|------|------|
| `bench_artifacts/zp1_*`, `zp12_*` | Z friction / parser fields |
| `results/GATE12_ZP12_*.txt` | Annex only |
| `Z_P12_IRON_CLOSEOUT_V1.md` | Honest PASS/WEAK |

---

## Canonical measured claims (H100 pull `20260522`)

| Claim | Source | Value |
|-------|--------|-------|
| F1′ uniform resident | `gate12_f1prime_20260522T064350Z` | stock **30202** / shim **30248** slots, eff **100%** |
| Docker repro | `docker_a46_20260522T064616Z` | `A46_DOCKER_OK`, shim **29877** slots |
| VRAM @ **70%** budget | `vram_curve_20260522T062629Z` | liberation **42.2%**, logical gain **+131%**, shim **65536** slots |
| Node 4b anchor | same curve @ 70% | **42.2%** ∈ 30–40% band (upper touch) |

---

## Repro (operator)

```bash
# Docker (primary public path)
bash scripts/shim/run_docker_a46_gate12_f1prime_v1.sh

# Iron host (optional)
bash scripts/shim/run_iron_a45b_f1prime_v1.sh
bash scripts/shim/run_iron_a48b_vram_curve_v1.sh
```

---

## Before `git push` (checklist)

- [x] `LICENSE.md` at root
- [x] `README_T1_EVAL.md` — Layer A vs Layer B roadmap, no Z>3 / no inference P99 lead, no internal node jargon
- [x] `results/GATE12_canonical.txt` from measured artifacts
- [x] Node 4b from `cache_liberation_pct_70`, not headline-only
- [ ] Operator: confirm private vs public repo remote
- [ ] No secrets in `bench_artifacts/` commit (use manifest + selective copy)

**Two contours (eval vs licensed):** `TWO_CONTOUR_EVAL_OPEN_VS_PARTNER_CLOSED_V1.md`

**Scope gate:** T1 ship = **FIX_NOW**. B/vLLM = **PARK**. Z-P1/P2 = **annex**.
