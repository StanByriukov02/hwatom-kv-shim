# Hardware Atom — evaluation release (Layer A)

**License:** [LICENSE.md](LICENSE.md) — **Evaluation-Only.** Production, fleet, and unlimited use require a separate agreement.

---

## What this repository is

**Layer A (this release)** proves a **memory-packing mechanism** on NVIDIA GPUs using CUDA virtual memory (`cuMem*`), measured with a **fixed synthetic workload** in Docker.

**Layer B (next)** applies the same mechanism to **real inference stacks** — starting with **vLLM** on the path where KV cache actually uses `cuMem*` (not the default `cudaMalloc` bypass).

If you only run the Docker eval here, you are validating **the physics of packing** — not yet your production vLLM fleet.

---

## What Layer A proves today

**Environment:** NVIDIA H100-class GPU, driver band **~550.x**, **512 KiB** logical KV slices inside **2 MiB** CUDA granularity.

| Result (synthetic, frozen tag `t1-eval-20260522`) | Where |
|--------------------------------------------------|--------|
| More **logical KV** at **lower committed VRAM** vs stock allocator | `results/GATE12_canonical.txt` |
| **~42%** cache liberation @ **70%** VRAM budget, **~2.3×** logical KV vs stock (iso fill) | same |
| Reproducible **Docker** run (`A46_DOCKER_OK`) | `docker/gate-12s/Dockerfile.f1prime` |

**Methodology:** [A49 claim protocol](docs/agent_workflow/A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md).

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
| **1 — Integration** | vLLM (and similar) on **cumem / sleep-mode** path + documented env matrix | [B12 env matrix](docs/agent_workflow/B12_ENV_MATRIX_V1.md) |
| **2 — Smoke** | Public recipe: prove shim hooks `cuMem*` on a real server image | scripts under `scripts/vllm/` (when tagged in repo) |
| **3 — Iron** | Fair **stock vs shim** pairing on **one** production-class workload; metrics from **NVML + GATE12**, not PyTorch alone | Protocol: [B measurement](docs/agent_workflow/B_MEASUREMENT_PROTOCOL_V1.md) |
| **4 — Provider headline** (only after iron) | **More effective KV capacity** at the same **committed VRAM**; **tokens/s** must stay ≥ baseline | No public % until H100 iron closes |

**Audiences we optimize for:**

| If you are… | Start here |
|-------------|------------|
| **GPU cloud tenant** (bare-metal / SSH / your container) | Eval Docker (Layer A) → pilot image with cumem flags (Layer B) |
| **Enterprise platform** (NVIDIA / large bank / regulated fleet) | Mechanism proof (A) → **PyTorch pluggable allocator** path, not prod `LD_PRELOAD` |
| **Research / recon** | Do **not** map T1 synthetic **42%** to vLLM; watch repo for **vLLM integration** tags and new `workload_id` |

**Deep implementation map (for engineers):** [B layer chain](docs/agent_workflow/B_LAYER_IMPLEMENT_CHAIN_V1.md) · [External adoption](docs/agent_workflow/B_EXTERNAL_ADOPTION_STRATEGY_V1.md).

---

## Quick repro (Docker — Layer A)

Requires Linux, NVIDIA GPU, Docker with GPU support.

```bash
git clone https://github.com/StanByriukov02/hwatom-gate-12s.git
cd hwatom-gate-12s
git checkout t1-eval-20260522
bash scripts/shim/run_docker_a46_gate12_f1prime_v1.sh
```

Expect `GATE12_BEGIN` … `HWATOM_EVAL` banner … `A46_DOCKER_OK`.

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

`hardware-atom` `cuMem` `KV-cache` `vLLM` `memory-packing` `gate-12s` `cache-liberation` `VRAM` `inference-integration` — **Layer B vLLM metrics:** not valid until announced with new tag and `workload_id`.

---

## Contact

Production / partnership: **stanislav.byriukov.research@gmail.com**
