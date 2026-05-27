# Docker Hub — H100 LLM KV-cache evaluation images

**What this is (plain language):** reproducible **evaluation** containers for two related problems on NVIDIA H100:

1. **CUDA memory layout** — pack more logical KV data into stock NVIDIA `cuMem` 2 MiB pages (user-space shim, not a fork of vLLM).
2. **KV cache size on vLLM** — measure **fewer bytes per token** using upstream **vLLM 0.21 + `turboquant_4bit_nc`** on Qwen2.5-7B (quality + throughput measured honestly).

**Registry:** [hub.docker.com/r/stanbyriukov31/hwatom-kv-shim](https://hub.docker.com/r/stanbyriukov31/hwatom-kv-shim)  
**License:** [LICENSE.md](../LICENSE.md) (Evaluation-Only — not a production support offer)  
**Source / receipts:** [github.com/StanByriukov02/hwatom-kv-shim](https://github.com/StanByriukov02/hwatom-kv-shim)

**Internal codename map (for Git tags only):** `MOON-Xq` = vLLM TurboQuant KV-bytes eval track · `Layer A` / `gate12s` = cuMem leaf-pack track.

---

## Who should care

| You are… | Start here |
|----------|------------|
| **CUDA / GPU memory engineer** | `gate12s-f1prime` — leaf packing iron |
| **vLLM / inference engineer** | Git tag `eval-moon-xq-20260526` + [MOON_XQ_EVAL.md](MOON_XQ_EVAL.md) |
| **Automated scanner** | keywords: `cuMem`, `KV cache`, `vLLM`, `turboquant`, `H100`, `long-context`, `Qwen2.5` |

---

## Images on Docker Hub

| Tag | What it is |
|-----|------------|
| `gate12s-f1prime` | **cuMem leaf-pack repro** — run GPU iron, expect `GATE12` / `A46_DOCKER_OK` |
| `t1-leaf-physics-20260525` | same image, frozen release name |
| `gate12s-eval` | eval-tier shim build (K cap baked in) |
| `eval-moon-xq-20260526` | alias for eval image; **full vLLM TurboQuant iron is not one pull** — see below |

### vLLM TurboQuant eval (not a single `docker pull`)

Full stack needs **vLLM 0.21 docker + H100 + weights**:

```bash
git clone https://github.com/StanByriukov02/hwatom-kv-shim.git
cd hwatom-kv-shim && git checkout eval-moon-xq-20260526
bash scripts/moon/recipe_moon_xq_v1.sh path-a-v2
```

---

## Quick start — leaf pack (one command)

```bash
docker pull stanbyriukov31/hwatom-kv-shim:gate12s-f1prime
docker run --rm --gpus all stanbyriukov31/hwatom-kv-shim:gate12s-f1prime
```

Expect `GATE12_BEGIN`, `workload_id=a_gate_v1_kv_microbench`, `A46_DOCKER_OK`.

---

## Measured results (2026-05-26 iron, honest)

| Track | Metric | Result |
|-------|--------|--------|
| **cuMem leaf pack** | VRAM liberation @ 70% fill | **42.2%** · layout efficiency **100%** |
| **vLLM TurboQuant KV** | KV bytes vs FP16 | **−58%** |
| **vLLM TurboQuant KV** | perplexity drift | **+0.58%** |
| **vLLM TurboQuant KV** | decode tok/s vs FP16 FA2 | **~0.86×** (slower — stated explicitly) |

Machine summary: [results/MOON_XQ_GATE_SUMMARY.txt](../results/MOON_XQ_GATE_SUMMARY.txt)

**Not claimed:** OOM fix on your fleet · NVML % win · “beats vLLM” on latency.

---

## Publish (maintainer)

`bash scripts/shim/publish_docker_hub_v1.sh` · CI: `.github/workflows/docker-hub-publish.yml`
