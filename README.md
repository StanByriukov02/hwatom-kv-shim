# hwatom-kv-shim — CUDA KV cache shim (H100 eval)

![docker build](https://github.com/StanByriukov02/hwatom-kv-shim/actions/workflows/docker-build.yml/badge.svg)

**Discovery:** `kv-cache` · `cuda` · `cuMem` · `h100` · `gpu-memory` · `inference` · `paged-attention` · user-space shim · vLLM integration (Track B roadmap, not T1 claims).

Pack up to four 512 KiB logical KV slices per 2 MiB CUDA leaf on NVIDIA H100 — **42.2%** measured VRAM liberation at 70% budget fill, reproducible Docker eval. Synthetic `cuMem*` microbench only — not stock vLLM inference.

**License:** [LICENSE.md](LICENSE.md) (Evaluation-Only) · **Measured results:** [results/GATE12_canonical.txt](results/GATE12_canonical.txt) · **Paper source:** [docs/arxiv/paper/main.tex](docs/arxiv/paper/main.tex) (submitted URL: `hwatom-gate-12s` — GitHub redirects to this repo) · **Cite:** [CITATION.cff](CITATION.cff)

Reproducible evaluation of a user-space CUDA memory shim that packs logical KV slices under **2 MiB** driver granularity on **NVIDIA H100**. Synthetic microbenchmark only — not production vLLM inference.

| | |
|--|--|
| **Workload** | `workload_id=a_gate_v1_kv_microbench` |
| **Tag** | `t1-eval-20260522` |
| **Primary repro** | Docker `hwatom:gate-12s-f1` — see [README_T1_EVAL.md](README_T1_EVAL.md) |
| **Canonical @ 70% VRAM budget** | **42.204%** cache liberation, **+131%** logical KV vs stock |
| **Contact (production)** | stanislav.byriukov.research@gmail.com |

---

## Start here

Full narrative, disclaimers, Layer B roadmap: **[README_T1_EVAL.md](README_T1_EVAL.md)**

Claim discipline: [docs/agent_workflow/A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md](docs/agent_workflow/A49_CLAIM_PROTOCOL_CACHE_LIBERATION_V1.md)

Bundle index: [docs/agent_workflow/T1_PUBLIC_BUNDLE_V1.md](docs/agent_workflow/T1_PUBLIC_BUNDLE_V1.md)

---

## Quick repro (Docker)

Linux host with NVIDIA driver and Docker GPU support:

```bash
git clone https://github.com/StanByriukov02/hwatom-kv-shim.git
cd hwatom-kv-shim
git checkout t1-eval-20260522
bash scripts/shim/run_docker_a46_gate12_f1prime_v1.sh
```

Expect `GATE12_BEGIN`, `workload_id=a_gate_v1_kv_microbench`, `stress_mode=f1_frag_v1`, and `A46_DOCKER_OK`.

Manual build:

```bash
docker build --no-cache -f docker/gate-12s/Dockerfile.f1prime -t hwatom:gate-12s-f1 .
docker run --rm --gpus all -e HWATOM_ART_DIR=/out -v "$(pwd)/bench_out:/out" hwatom:gate-12s-f1
```

---

## Repository layout (T1 public)

| Path | Role |
|------|------|
| `src/shim/` | `lib2adic_shim.so` sources |
| `tools/shim_iron/` | `iron_gate_v1` benchmark + GATE12 emitter |
| `docker/gate-12s/` | Evaluation image (`Dockerfile.f1prime`) |
| `scripts/shim/` | Host helpers (Docker + optional iron) |
| `results/` | Canonical GATE12 paste (do not hand-edit numbers) |
| `docs/agent_workflow/` | Claim protocol, evidence index, tier map |
| `docs/arxiv/paper/` | LaTeX source (PDF at arxiv release) (ancillary to code repro) |

---

## Tiers (what this repo is / is not)

| Tier | In this repo |
|------|----------------|
| **T1** | Shim + gate-12s Docker + GATE12 metrics |
| **T2** | Not shipped (customer vLLM integration — partnership) |
| **T3** | Not shipped (eBPF / kernel — contract only) |

Eval Docker is **scope-limited** (license + synthetic workload). Production/unlimited binaries require a separate agreement — [DOCKER_TIER_AND_PROTECTION_V1.md](docs/agent_workflow/DOCKER_TIER_AND_PROTECTION_V1.md).

---

## What we do not claim in T1

- Inference **P99** or tokens/s
- Industry **Z-scores** or FLOPS/W leadership
- vLLM end-to-end benchmarks
- eBPF / kernel modules

Optional scout annex: `results/GATE12_ZP12_*.txt` (not lead narrative).

**Track B (planned):** vLLM / production-stack integration — separate release; **not** in tag `t1-eval-20260522`.

---

## Author

Stanislav Byriukov · Independent Researcher · stanislav.byriukov.research@gmail.com

