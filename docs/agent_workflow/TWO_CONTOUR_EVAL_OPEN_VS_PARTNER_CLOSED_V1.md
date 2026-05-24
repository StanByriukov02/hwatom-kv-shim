# Two contours — eval-open vs partner-closed

**Status:** operator canon (2026-05-22)  
**Pairs with:** `DOCKER_TIER_AND_PROTECTION_V1.md`, `COMMERCIAL_NOTES_CONTRACTUAL_K_TIER_V1.md`, `T1_RELEASE_AND_KCAP_SEQUENCE_V1.md`, `SHIM_INVIOLABLE_CONTRACT_V1.md`

---

## 0. Да — так и планировалось (уточнение зазора)

| План (vault) | Что имелось в виду |
|--------------|-------------------|
| **Eval** public | Repro + GATE12 + Evaluation license + **ограниченный** образ (K-cap post-push, узкий workload) |
| **Licensed** | **Не** в public git: full-K prod `.so`, fleet integration, SLA |
| **Первый push** | Осознанно **full pack+mega** в eval = **честный** iron/Docker parity (`T1_RELEASE` step 1) |
| **~20h post-push** | `HWATOM_PACK_K_CAP` + tag `gate-12s-eval` |

**Зазор, о котором говорим честно:** в public `hwatom-gate-12s` v1 также лежит **`src/shim/` source** — это **сильнее**, чем «только eval binary». План по **tier/Docker** соблюдён; план «секретность алгоритма» на v1 **не** был целью Gate A.

---

## 1. Contour A — eval-open (public `hwatom-gate-12s`)

### Цель

**Доверие + credential:** «повтори у себя → увидишь 42% на synthetic GATE12».

### Что **в** public git

| Path | Role |
|------|------|
| `src/shim/` | Reference implementation (P1 + mega для repro) |
| `tools/shim_iron/`, `docker/gate-12s/Dockerfile.f1prime` | Repro |
| `results/GATE12_canonical.txt` | Frozen numbers |
| `LICENSE.md` | Evaluation-Only |
| `README_T1_EVAL.md`, paper | Claims + disclaimers |
| `paperswithcode.yaml` | Discovery |

### Что **ограничено** (не секретность, а scope)

| Mechanism | Effect |
|-----------|--------|
| `HWATOM_EVAL` banner | Legal marker on logs |
| Synthetic `workload_id` only | Not vLLM production claim |
| Narrow Docker (no T2/T3/vLLM) | Not fleet product |
| **K-cap** (post-push) | Lower effective K in **eval** `.so` |
| Tag `t1-eval-20260522` | Frozen science; later tags differ |

### Чего eval-open **не обещает**

- Production fleet rights  
- Full-K unlimited on hyperscaler scale  
- Защиту от RE мотивированной лаборатории  

---

## 2. Contour B — partner-closed (commercial / NDA)

### Цель

**Монетизация + усложнение «упереть бесплатно»** для production path — не отмена публичного proof.

### Что **НЕ** в public git (TABU)

| Item | Why closed |
|------|------------|
| `hwatom:gate-12s-licensed` image / prod `.so` | Commercial deliverable |
| Full-K production build без cap | Contract tier |
| `integrations/vllm/` production recipes | Track B |
| T3 eBPF / `.ko` | Contract tier |
| Placement tuning tables, GQA production paths, fleet hooks | Edge + support moat |
| Telemetry / breach downgrade automation | Commercial |
| Partner-specific `HWATOM_*` matrix under SLA | Per-contract |

### Что **в** partner-closed (delivery channel)

- Private registry or signed artifact URL under agreement  
- Stripped symbols, optimized build, driver matrix QA  
- vLLM / TRT / custom PyTorch integration (B-layer)  
- Nsight evidence packs on **their** stack (B-OPT)  
- Support + updates for new driver branches (550→555…)  

### Legal layer (работает на «гуандонов с юристами»)

- Evaluation-Only ≠ production  
- Trade secret / NDA schedules  
- Gain-share / inbound framework (vault legal_protection/A)  
- Contractual K downgrade on breach (`COMMERCIAL_NOTES`)

---

## 3. First principles — как не потерять доверие И не быть наивным

### Нельзя одновременно (выбери честно)

| A | B |
|---|---|
| Полный open source + arXiv repro | Полная секретность от NVIDIA RE |

**Мы выбрали A на v1** — доверие первично. Защита = **слои B + legal + скорость**, не «никто не поймёт».

### Что хотят «упереть» (NVIDIA / Google / Tier-2)

| Они берут бесплатно | Они всё равно покупают / ведут переговоры |
|-------------------|------------------------------------------|
| **Идею** pack под 2 MiB | Maintained **integration** на их stack |
| Повтор experiment class | **Legal** production / fleet |
| LD_PRELOAD pattern | Driver churn QA (555.x) |
| | **Measured** claim на **их** vLLM (B) |
| | Speed to production vs internal 18-month project |

**NVIDIA особенно:** им не нужен «украденный .so» — им нужен **driver**. Риск = **идея уходит в driver**, не клон стартапа. Ответ = **inbound + author position (arxiv)** + partnership path, не секретность eval.

### Моат после open v1 (реалистичный)

1. **Author / prior art** — arXiv + canonical GATE12 + first public tag  
2. **Integration depth (B)** — vLLM pool lifecycle, не только cuMem microbench  
3. **Iron brand** — новые workload только с GATE12, не маркетинг  
4. **Licensed binary** — prod без source в public  
5. **Contract + downgrade** — K-cap / telemetry on breach  
6. **Velocity** — они разберут за недели; ты ship B + licensed за месяцы с поддержкой  

### Что **не** делать (ломает доверие)

- Публичный proof, потом «ой, всё закрыли» без нового тега/документации  
- Fake cripple (12s stop) вместо честного K-cap  
- Скрыть, что eval source = тот же механизм, что licensed  
- Претензия «eval не разберут»  

### Что **можно** усилить без потери trust

| Action | Trust impact |
|--------|----------------|
| Post-push **K-cap** eval image + README «eval ≠ licensed» | **Neutral+** (honest tier) |
| Public: **минимальный** reference shim; advanced paths only in licensed | **Mild−** if sudden — лучше с v1.1 doc «eval = reference build» |
| B-layer public **integration guide** without prod `.so` | **Positive** |
| Watermark / build-id in licensed `.so` | **Neutral** (partner only) |

---

## 4. CUBIN/SASS — место в двух контурах

| Contour | CUBIN/SASS |
|---------|------------|
| **eval-open** | **Не требуется** — cuda image + optional K-cap `.so`; source for repro |
| **partner-closed** | **Опционально** — stripped binary, no PTX in deliverable; **не** замена LICENSE |

SASS raises RE cost; **does not** replace contract or integration moat.

---

## 4b. Licensed build pipeline — **after** public release, **private** only

| Rule | Detail |
|------|--------|
| **When** | **After** public `hwatom-gate-12s` push + tag `t1-eval-20260522` (not blocking day-1) |
| **Where** | Private CI / registry — **never** same commit stream as public eval without review |
| **Artifacts** | `hwatom:gate-12s-licensed` (or partner tag), prod `.so` — **not** in public git |
| **Relation to eval** | Same codebase family; **delivery + license + integration**, not secret physics day-1 |
| **Prerequisite for $** | Inbound NDA + B-layer iron on customer stack |

**Not confused with:** ~20h **K-cap** on **public eval** image (`gate-12s-eval`).

---

## 5. Sequence (aligned with operator)

```
1. Public hwatom-gate-12s + tag t1-eval-20260522  (eval-open, full K repro, open src)
2. arXiv + inbound recon
3. ~20h: K-cap eval tag gate-12s-eval          (eval weaker, documented)
4. **Private** licensed pipeline (build/push .so — **not in git**, separate from public CI)
5. First NDA: partner-closed artifacts
6. Track B: vLLM iron + optional B-OPT Nsight
```

### Why anyone pays if NVIDIA can read the idea (operator Q)

| Fear | Truth |
|------|--------|
| «Увидели код → встроили в driver → я никому не нужен» | **Risk exists** for the *idea class* (pack under granularity). **Not** the whole business. |
| License text stops NVIDIA RE | **No** |
| What still needs a vendor | Production **integration** (B), legal **fleet** rights, **maintenance** across driver branches, **measured** claims on *their* stack, **time-to-field** vs internal queue |
| arXiv + public tag | **Prior art / author** — shifts negotiation; does not ban NVIDIA engineering |
| Your product | **Service + artifact + contract**, not «секрет формулы» on v1 |

**Licensed GQA (partner):** production path for **grouped-query attention** KV layout — **not** in T1 public claims (`GQA_ALIAS` unstable); **closed** tuning + model-aware integration under NDA.

---

## 6. README one-liner (both contours)

```markdown
**Eval (open):** reproduce GATE12 synthetic proof — Evaluation-Only, not production.
**Licensed (closed):** production binary, vLLM integration, fleet tier — contact stanislav.byriukov.research@gmail.com.
```

---

## 6b. Operator mode after public T1 (= explicit 2nd gear)

**Release public eval + arxiv** is **not** «отдых» — переключение на **2–3 передачу**:

| Gear | Work (parallel where possible) |
|------|--------------------------------|
| **2a** | ~20h: K-cap public eval + `gate-12s-eval` tag + README eval≠licensed |
| **2b** | Private licensed pipeline (`.so`/image — **not in git**) |
| **2c** | B-layer: vLLM integration + **iron** on production path |
| **2d** | Inbound: contract template, first NDA thread |
| **3** | B-OPT / GQA prod / fleet — **optional**, not blocking 2b–2c |

**TABU:** pretend idea stays secret; skip iron on B claims.

---

## 7. Contradiction register

| ID | Tension | Resolution |
|----|---------|------------|
| C-RE-01 | Open src vs theft fear | Two contours; moat = B + licensed + legal, not v1 secrecy |
| C-RE-02 | First tag full K vs K-cap story | Sequence doc: proof first, cap second |

---

## Links

- `T1_PUBLIC_REPO_SPLIT_V1.md` — public repo = eval-open tree only  
- `B_LAYER_WBS_VLLM_INTEGRATION_V1.md` — B + B-OPT  
- `DOCKER_TIER_AND_PROTECTION_V1.md` § RE reality
