# Hardware Atom — evaluation release (Layer A)

**License:** [LICENSE.md](LICENSE.md) — **Evaluation-Only.** Production, fleet, and unlimited use require a separate agreement.

---

## What this repository is

**Layer A (this release)** proves a **memory-packing mechanism** on NVIDIA GPUs using CUDA virtual memory (`cuMem*`), measured with a **fixed synthetic workload** in Docker.

**Layer B (next)** applies the same mechanism to **real inference stacks** — starting with **vLLM** on the path where KV cache actually uses `cuMem*` (not the default `cudaMalloc` bypass).

If you only run the Docker eval here, you are validating **the physics of packing** — not yet your production vLLM fleet.

**MOON-Xq (bytes-KV · vLLM TurboQuant):** separate composable eval — [docs/MOON_XQ_EVAL.md](docs/MOON_XQ_EVAL.md) · tag `eval-moon-xq-20260526` · [bundle index](docs/EVAL_BUNDLE_INDEX.md).

---

## What Layer A proves today

**Environment:** NVIDIA H100-class GPU, driver band **~550.x**, **512 KiB** logical KV slices inside **2 MiB** CUDA granularity.

| Result (synthetic) | Tag | Where |
|------------------|-----|--------|
| F1′ + VRAM curve @70% (legacy lead) | `t1-eval-20260522` | `results/GATE12_canonical.txt` |
| **Intra–2 MiB leaf ceiling** (`eta_leaf=100%`, iso @ budget) | **`t1-leaf-physics-20260525`** | **`results/GATE12_leaf_physics_v1.txt`** |
| Reproducible **Docker** run (`A46_DOCKER_OK`) | `t1-eval-20260522` | `docker/gate-12s/Dockerfile.f1prime` |
| **Iron leaf physics** (`LEAF_PHYSICS_OK=yes`) | `t1-leaf-physics-20260525` | `scripts/shim/run_iron_leaf_ceiling_v1.sh` |

**Leaf physics @70% budget (tag `t1-leaf-physics-20260525`, `workload_id=t1_leaf_physics_v1`):** **42.20%** cache liberation @ committed, **131%** logical KV gain, **100%** layout efficiency (at theoretical leaf ceiling). Contract: [IRON_LEAF_CEILING_SPEC_V1.md](docs/agent_workflow/IRON_LEAF_CEILING_SPEC_V1.md).

**Do not headline** `resident_slots_shim` vs stock — slot count is diagnostic, not the battle metric.

**Methodology:** [A49 claim protocol](docs/agent_workflow/A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md) · leaf iron [N2 pass](docs/agent_workflow/N2_LEAF_PHYSICS_PASS_20260525_V1.md) (vault mirror; public: CHANGELOG).

**Shim stats (engineers):** set `HWATOM_SHIM_STATS=1` on vLLM/cumem runs; parse with `scripts/vllm/b21_parse_cumem_trace_v1.py`. Line format **v2** since `eval-moon-xq-20260526` — see [CHANGELOG.md](CHANGELOG.md).

---

## What this release does **not** claim

- **vLLM / production serving** throughput, P99 latency, or fleet-wide savings  
- That `LD_PRELOAD` alone on **stock** vLLM (cudaMalloc path) will show the same numbers  
- Industry FLOPS/W or Z-score rankings  
- Kernel modules, eBPF, or custom NVIDIA UVM drivers (partner-only tracks)

---

## What comes next (Layer B — production inference)

**Status:** documented in-repo, **measured claims not published yet.**

| Stage | What ships | What you can check now |
|-------|------------|------------------------|
| **1 — Integration** | vLLM (and similar) on **cumem / sleep-mode** path + documented env matrix | Roadmap only — [Layer B public roadmap](docs/agent_workflow/B_LAYER_PUBLIC_ROADMAP_V1.md) |
| **2 — Smoke** | Public recipe: prove shim hooks `cuMem*` on a real server image | Not in tag `t1-eval-20260522` |
| **3 — Iron** | Fair **stock vs shim** pairing on **one** production-class workload; metrics from **NVML + GATE12**, not PyTorch alone | Not in public v1 |
| **4 — Provider headline** (only after iron) | **More effective KV capacity** at the same **committed VRAM**; **tokens/s** must stay ≥ baseline | No public % until H100 iron closes |

**Audiences we optimize for:**

| If you are… | Start here |
|-------------|------------|
| **GPU cloud tenant** (bare-metal / SSH / your container) | Eval Docker (Layer A) → pilot image with cumem flags (Layer B) |
| **Enterprise platform** (NVIDIA / large bank / regulated fleet) | Mechanism proof (A) → **PyTorch pluggable allocator** path, not prod `LD_PRELOAD` |
| **Research / recon** | Do **not** map T1 synthetic **42%** to vLLM; watch repo for **vLLM integration** tags and new `workload_id` |

**Track B (engineers):** [public roadmap](docs/agent_workflow/B_LAYER_PUBLIC_ROADMAP_V1.md) — full implementation vault ships with a future **Track B** git tag, not in this eval release. **Do not** map Layer A Table 1 metrics to vLLM until a separate `workload_id=b_vllm_*` canonical exists (contract: `SHIM_INVIOLABLE_CONTRACT_V1.md` §8.2).

---

## Quick repro (Docker — Layer A)

Requires Linux, NVIDIA GPU, Docker with GPU support.

**Public Docker** ships **`lib2adic_shim_eval.so`** (structural **K_cap=2** per 2 MiB leaf). Full-K lab shim: `make -C src/shim all` — used for iron tags like `t1-leaf-physics-20260525`. See [docs/EVAL_SHIM_KCAP_V1.md](docs/EVAL_SHIM_KCAP_V1.md).

```bash
git clone https://github.com/StanByriukov02/hwatom-kv-shim.git
cd hwatom-kv-shim
git checkout t1-eval-20260522
bash scripts/shim/run_docker_a46_gate12_f1prime_v1.sh
```

Expect `GATE12_BEGIN` … `HWATOM_EVAL` banner … `A46_DOCKER_OK`. Shim stats: `eval_shim=1` `k_cap=2`.

Manual build:

```bash
docker build --no-cache -f docker/gate-12s/Dockerfile.f1prime -t hwatom:gate-12s-f1 .
docker run --rm --gpus all -e HWATOM_ART_DIR=/out -v "$(pwd)/bench_out:/out" hwatom:gate-12s-f1
```

---

## Open vs licensed (two contours)

| | **Public eval (this repo)** | **Licensed / partnership** |
|---|---------------------------|---------------------------|
| **Goal** | Trust + reproduce mechanism | Production integration + fleet |
| **Workload** | Synthetic `cuMem` microbench | Your inference stack (e.g. vLLM) |
| **Artifact** | `hwatom:gate-12s-f1`, open shim sources | Full-K binary, matrix rows, SLA — **not** in public v1 |
| **Legal** | [LICENSE.md](LICENSE.md) Evaluation-Only | Separate contract |

Details: [Two contours](docs/agent_workflow/TWO_CONTOUR_EVAL_OPEN_VS_PARTNER_CLOSED_V1.md) · [Docker tiers](docs/agent_workflow/DOCKER_TIER_AND_PROTECTION_V1.md) · [T1 bundle index](docs/agent_workflow/T1_PUBLIC_BUNDLE_V1.md).

---

## For monitoring / recon (keywords)

`hardware-atom` `cuMem` `KV-cache` `memory-packing` `gate-12s` `cache-liberation` `VRAM` **`t1_leaf_physics_v1`** **`t1-leaf-physics-20260525`** **`intra-leaf`** **`layout_efficiency`** **`eta_leaf_ceiling_ratio`** `internal-fragmentation` `2 MiB leaf`

**Layer B vLLM / NVML metrics:** not valid until announced with `workload_id=b_vllm_*` and a separate tag.

**Recon path:** [N3_RECON_ALERT_PATH_20260525_V1.md](docs/agent_workflow/N3_RECON_ALERT_PATH_20260525_V1.md) (private vault; public: tag + `results/GATE12_leaf_physics_v1.txt`).

---

## Contact

Production / partnership: **stanislav.byriukov.research@gmail.com**
