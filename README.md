# H100 LLM KV-cache evaluation — cuMem leaf packing + vLLM TurboQuant

Reproducible **evaluation-only** artifacts for NVIDIA H100 inference memory:

1. **CUDA `cuMem` leaf packing** — pack more logical KV bands per stock 2 MiB page (user-space shim).
2. **vLLM 0.21 + TurboQuant (`turboquant_4bit_nc`)** — measure KV **bytes/token**, quality, and decode speed on Qwen2.5-7B.

*Internal codenames (Git tags only):* `gate12s` / Layer A = leaf pack · `MOON-Xq` = TurboQuant bytes track · tag `eval-moon-xq-20260526`.

![docker build](https://github.com/StanByriukov02/hwatom-kv-shim/actions/workflows/docker-build.yml/badge.svg)

**Eval release 0.2.0:** tag `eval-moon-xq-20260526` · [RELEASE_0.2.0.md](docs/RELEASE_0.2.0.md) · [MOON_XQ_EVAL.md](docs/MOON_XQ_EVAL.md) · [MOON_XQ_GATE_SUMMARY.txt](results/MOON_XQ_GATE_SUMMARY.txt)

**Leaf pack (iso iron):** **42.2%** VRAM liberation @ 70% budget · **100%** intra-leaf layout efficiency · Docker repro on Hub.

**vLLM TurboQuant KV (2026-05-26 iron):** **−58%** KV bytes · ppl **+0.58%** · NIAH deep prefill **PASS** · decode **~0.86×** vs FP16 (**slower** — stated explicitly).

**License:** [LICENSE.md](LICENSE.md) (Evaluation-Only) · **Measured:** [GATE12_canonical.txt](results/GATE12_canonical.txt) · [GATE12_leaf_physics_v1.txt](results/GATE12_leaf_physics_v1.txt) · **Changelog:** [CHANGELOG.md](CHANGELOG.md)

| | |
|--|--|
| **Tags** | `t1-eval-20260522` · `t1-leaf-physics-20260525` · `eval-moon-xq-20260526` |
| **Latest iron** | `workload_id=t1_leaf_physics_v1` · `LEAF_PHYSICS_OK=yes` |
| **TurboQuant workload id** | `MOON-REP-Xq-VLLM-01` (internal) |
| **Leaf repro** | Docker Hub `gate12s-f1prime` — [README_T1_EVAL.md](README_T1_EVAL.md) |
| **vLLM TurboQuant repro** | `bash scripts/moon/recipe_moon_xq_v1.sh path-a-v2` (GPU + vLLM 0.21) |
| **Leaf iron** | `bash scripts/shim/run_iron_leaf_ceiling_v1.sh` |
| **@ 70% VRAM budget** | **42.20%** liberation · **131%** logical KV · **η_leaf=100%** |
| **Contact (production)** | stanislav.byriukov.research@gmail.com |
| **Docker Hub** | [`stanbyriukov31/hwatom-kv-shim`](https://hub.docker.com/r/stanbyriukov31/hwatom-kv-shim) · [DOCKERHUB.md](docs/DOCKERHUB.md) |

---

## Start here

Full narrative, disclaimers, Layer B roadmap: **[README_T1_EVAL.md](README_T1_EVAL.md)**

vLLM TurboQuant KV bytes: **[docs/MOON_XQ_EVAL.md](docs/MOON_XQ_EVAL.md)** · index **[docs/EVAL_BUNDLE_INDEX.md](docs/EVAL_BUNDLE_INDEX.md)**

Claim discipline: [docs/agent_workflow/A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md](docs/agent_workflow/A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md)

Bundle index: [docs/agent_workflow/T1_PUBLIC_BUNDLE_V1.md](docs/agent_workflow/T1_PUBLIC_BUNDLE_V1.md)

---

## Quick repro (Docker — Layer A)

**Pull (no clone):**

```bash
docker pull stanbyriukov31/hwatom-kv-shim:gate12s-f1prime
docker run --rm --gpus all stanbyriukov31/hwatom-kv-shim:gate12s-f1prime
```

**From source:**

```bash
git clone https://github.com/StanByriukov02/hwatom-kv-shim.git
cd hwatom-kv-shim
git checkout eval-moon-xq-20260526
bash scripts/shim/run_docker_a46_gate12_f1prime_v1.sh
```

Expect `GATE12_BEGIN`, `workload_id=a_gate_v1_kv_microbench`, `stress_mode=f1_frag_v1`, and `A46_DOCKER_OK`.

Manual build:

```bash
docker build --no-cache -f docker/gate-12s/Dockerfile.f1prime -t hwatom:gate-12s-f1 .
docker run --rm --gpus all -e HWATOM_ART_DIR=/out -v "$(pwd)/bench_out:/out" hwatom:gate-12s-f1
```

Eval-cap image (K≤2/leaf baked in):

```bash
docker build -f docker/gate-12s/Dockerfile.eval -t hwatom:gate-12s-eval .
```

---

## Quick repro (vLLM TurboQuant KV · H100-class GPU)

```bash
git checkout eval-moon-xq-20260526
cat results/MOON_XQ_GATE_SUMMARY.txt
bash scripts/moon/recipe_moon_xq_v1.sh path-a-v2
bash scripts/moon/recipe_moon_xq_v1.sh hb-fa2
```

Requires `vllm/vllm-openai:v0.21.0` and HuggingFace weights for `Qwen/Qwen2.5-7B-Instruct`.

---

## Repository layout

| Path | Role |
|------|------|
| `src/shim/` | `lib2adic_shim.so` sources (+ `make eval` → eval cap) |
| `tools/shim_iron/` | `iron_gate_v1` benchmark + GATE12 emitter |
| `docker/gate-12s/` | Evaluation images (f1prime + eval cap) |
| `scripts/shim/` | Layer A Docker + iron helpers |
| `scripts/moon/` | MOON-Xq iron + recipe |
| `results/` | Canonical GATE12 + MOON summary |
| `docs/` | MOON + eval bundle docs |
| `docs/agent_workflow/` | Claim protocol, iron receipts |

---

## Tiers

| Tier | In this repo |
|------|----------------|
| **T1** | Shim + gate-12s Docker + GATE12 metrics |
| **vLLM TurboQuant track** (`MOON-Xq` tag) | KV bytes iron + vLLM repro (upstream quant, not our kernel) |
| **T2** | Not shipped (customer vLLM integration — partnership) |
| **T3** | Not shipped (eBPF / kernel — contract only) |

Eval Docker is **scope-limited** (license + synthetic workload). Production/unlimited binaries require a separate agreement — [DOCKER_TIER_AND_PROTECTION_V1.md](docs/agent_workflow/DOCKER_TIER_AND_PROTECTION_V1.md).

---

## What we do not claim

- Inference **P99** or tokens/s **win** for MOON layer (~0.86× measured)
- Industry **Z-scores** or FLOPS/W leadership
- Tier3 PASS headline
- eBPF / kernel modules
- vLLM preload pool % or NVML liberation for MOON

Optional scout annex: `results/GATE12_ZP12_*.txt` (not lead narrative).

**Track B (planned):** vLLM / production-stack integration — separate release.

---

## Author

Stanislav Byriukov · Independent Researcher · stanislav.byriukov.research@gmail.com
