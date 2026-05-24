# Docker tiers, protection, and base-image decision

**Status:** operator canon for T1 ship (2026-05-22)  
**Pairs with:** `T1_PUBLIC_BUNDLE_V1.md`, `SHIM_INVIOLABLE_CONTRACT_V1.md`, `COMMERCIAL_NOTES_CONTRACTUAL_K_TIER_V1.md`

---

## 1. Два Docker-артефакта (не путать)

| Dockerfile | Image (typical) | Назначение | GPU |
|------------|-----------------|------------|-----|
| `docker/gate-12s/Dockerfile` | scratch / CI | **Dry gate** — musl `a_gate_v1_kv_microbench` + `gate_launcher` + `.so` | **Нет** (`docker-scratch` CI) |
| `docker/gate-12s/Dockerfile.f1prime` | `hwatom:gate-12s-f1` | **T1 evaluation repro** — `iron_gate_v1` + `lib2adic_shim.so`, F1′ | **Да** (`--gpus all`) |

**Партнёрский repro = `Dockerfile.f1prime`**, не scratch. Scratch = отдельный контракт (минимальный ELF, ADR-001).

---

## 2. Кастом под контракт (не потерять)

| Правило | Содержание |
|---------|------------|
| **Same workload class** | Кастомный image/env/SLA **не меняет** claim без нового `GATE12` с тем же `workload_id` и явной методологией |
| **Base** | `stress_mode=f1_frag_v1` / iso curve — эталон T1 |
| **Tag / env** | Допустимы: `hwatom:gate-12s-<partner>`, `HWATOM_FRAG_*`, budget % — **метрики пересчитываются** |
| **Не смешивать** | Eval stdout **нельзя** автоматически приравнивать к Production / NDA / Licensed tier |
| **Production** | Отдельный договор + бинарь/образ **не** из public eval Dockerfile |

Зафиксировано в `T1_PUBLIC_BUNDLE_V1.md` § Docker.

---

## 3. Почему `nvidia/cuda`, а не `FROM scratch` (данные + выигрыш)

### Как пришли (факты, не лозунг)

| Факт | Источник |
|------|----------|
| Scratch image **без** GPU userland | `docker/gate-12s/Dockerfile` → `FROM scratch`, CI `timeout 12s docker run` без `--gpus` |
| A-4.6 **требует** `nvidia-smi`, `cuMem`, LD_PRELOAD на GPU | `entrypoint_f1prime.sh`, `IRON_A46_DOCKER_OK` на H100 |
| Провайдерский путь = «скачал → `docker run --gpus 1`» | B0.1 WBS, partnership repro |
| Scout notebook «FROM scratch» | Их **эстетика** чистоты; у нас отдельный scratch для CI, **не** для F1′ GPU proof |

### Почему cuda-base **выигрышнее** для T1 цели

1. **Repro rate** — один стандартный путь у CoreWeave-style tenant; scratch на GPU без CUDA base = самосборка драйвера/libc (support hell).
2. **Claim integrity** — Docker цифры **=** iron цифры (тот же `iron_gate_v1` + тот же `.so`), иначе «два разных мира».
3. **Скорость T1** — не блокировать ship на musl+static port `iron_gate_v1` в scratch.
4. **Честность vs scout** — в README явно: scratch = dry/CI; **GPU proof = cuda f1prime** (`Z_SCOUT_IRON_ALIGNMENT_V1.md` § eBPF/scratch).

### Цена (осознанная)

- Больше attack surface в **eval** image (ubuntu base) vs empty rootfs.
- Компенсация: **tier separation** (§4), не «scratch театр» вместо repro.

---

## 4. Защита Docker: по **свойствам**, не по времени/токенам

### Замысел оператора (верно)

Eval/public Docker **не должен** быть тем же продуктом, что PoC/demo/NDA/Production с **полной** силой pack/интеграций — иначе украли образ и раскатили на все инстансы.

### Что уже есть **сейчас** (T1 ship)

| Слой | Механизм | Ограничение |
|------|----------|-------------|
| **Legal** | `LICENSE.md` Evaluation-Only | Нет production / fleet / unlimited |
| **Banner** | `HWATOM_EVAL` в GATE12 stdout | Юридическая маркировка логов |
| **Surface** | В image только `iron_gate_v1` + `.so` + entrypoint | **Нет** vLLM, eBPF, T3, fleet agent, padic runtime |
| **Workload** | Фиксированный F1′ / 8s / synthetic `workload_id` | **Не** inference serving path |
| **Tier map** | T1 public · T2 customer · T3 internal | T3 **запрещён** в public v1 |
| **Scratch split** | Полный алгоритм в repo **≠** «одна кнопка production» | Нужен build + знание env matrix |

### Что **ещё не** в коде (честный gap — `PARK_POST_T1`)

| План | Файл |
|------|------|
| `hwatom:gate-12s-eval` vs `hwatom:gate-12s-licensed` | `COMMERCIAL_NOTES_CONTRACTUAL_K_TIER_V1.md` |
| `HWATOM_PACK_K_CAP` / reduced K в eval `.so` | Там же — **не** time/token throttle |
| Contractual downgrade on breach | Commercial + telemetry |

**Важно:** сегодня `entrypoint_f1prime.sh` включает `HWATOM_SHIM_PACK=1` `HWATOM_PACK_MEGA=1` — **механизм pack в eval = тот же**, что iron proof. Защита сейчас = **legal + scope + отсутствие T2/T3/ serving**, **не** урезанный K в бинарнике. Это **не забыто** — вынесено в commercial K-tier **после** T1 public.

### CUBIN/SASS «dark core» (notebook) vs наш путь

| Модель | T1 public v1 | Licensed / NDA later |
|--------|--------------|----------------------|
| **Opus:** только CUBIN/SASS в `FROM scratch`, без source | **Не используем** для open eval | **Опционально** под договор (closed `.so`) |
| **Наш:** open `src/shim/` + repro Docker + LICENSE | **Да** — доверие = повторяемость | Production binary может быть closed |
| **Обрез eval** | `HWATOM_PACK_K_CAP` (post-push), узкий image, no vLLM/eBPF | Не секретность через SASS на первом теге |

**42% liberation — доказано iron/H100** (`GATE12_canonical`). «Commerce layer later» = **продажа/доставка** full-K + integration + SLA — **не** «ещё не доказали 42%».

Украсть **идею** из public repo: юридически LICENSE; технически на v1 source открыт — **trade-off** Gate A (repro > скрытие). Защита от массового **fleet abuse** eval image = K-cap + licensed tier, не PTX strip в первом public push.

### RE reality (operator honesty)

| Layer | Stops motivated RE + LLM disassembly? |
|-------|-------------------------------------|
| LICENSE | **No** — deters law-abiding production use |
| K-cap | **No** — limits eval utility, not secrecy |
| CUBIN/SASS-only | **No** — raises bar only |
| Open `src/shim/` on public tag | **Zero** technical secrecy |

T1 v1 = **credential**, not strong binary protection. Open source = deliberate trade for repro/arXiv.

**Post-credential (licensed):** closed build, no full source publication, contract/trade secret; optional obfuscation. CUBIN/SASS optional under NDA — not retroactive fix after public source.

### Eval vs Licensed (целевая картина)

```
Public eval (T1)     →  repro + GATE12 + Evaluation license
Partner NDA/demo     →  может совпадать с eval ИЛИ отдельный tag (договор)
Licensed/production  →  другой artifact, SLA, возможно full K + T2 integration
```

Новый claim или «полная сила» на fleet **требует** новый tier + новый GATE12/workload_id.

---

## 5. T3 vs T2/B — порядок

| Tier | Когда | Зависимость |
|------|-------|-------------|
| **T1** | **Сейчас** (ship) | — |
| **T2 / B** | После T1 public; vLLM на **их** процессе | Prerequisite: T1 bundle |
| **T3** | Partner **internal** only; **never** public v1 | **Не** «автоматически после B» в контракте, но **практически** после доказанного runtime path (B) + NDA |

**Коротко:** T3 (eBPF/UVM.ko) **не** реализуем в open repo вместе с T1; **обычно** после T2 wedge, под отдельным agreement — можно параллельно по NDA, но **не** смешивать с public Docker claim.

---

## 6. Scope gate

**FIX_NOW:** T1 eval = `Dockerfile.f1prime` + LICENSE + этот документ (full K repro, frozen canonical).  
**FIX_NOW+20h:** K-cap eval vs licensed image (`COMMERCIAL_NOTES_*`, `T1_RELEASE_AND_KCAP_SEQUENCE_V1.md`).  
**PARK:** T3 в public.  
**TABU:** K-cap до первого push; выдавать eval Docker за production unlimited.
