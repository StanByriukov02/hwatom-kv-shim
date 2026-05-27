# Docker Hub — public eval images

**Registry:** [hub.docker.com/r/stanbyriukov31/hwatom-kv-shim](https://hub.docker.com/r/stanbyriukov31/hwatom-kv-shim)  
**License:** [LICENSE.md](../LICENSE.md) (Evaluation-Only)  
**Source:** [github.com/StanByriukov02/hwatom-kv-shim](https://github.com/StanByriukov02/hwatom-kv-shim)

---

## Images

| Tag | Dockerfile | What it runs |
|-----|------------|--------------|
| `gate12s-f1prime` | `Dockerfile.f1prime` | Layer A F1-prime GPU iron (GATE12) |
| `t1-leaf-physics-20260525` | same as f1prime | alias @ release tag |
| `gate12s-eval` | `Dockerfile.eval` | eval-cap tier (K≤2/leaf) |
| `eval-moon-xq-20260526` | same as eval | alias @ MOON eval release |

**MOON-Xq full iron** (vLLM 0.21 + TurboQuant + H100) is **not** a single Hub image — use repo recipe:

```bash
git clone https://github.com/StanByriukov02/hwatom-kv-shim.git
cd hwatom-kv-shim && git checkout eval-moon-xq-20260526
bash scripts/moon/recipe_moon_xq_v1.sh path-a-v2
```

---

## Quick pull (Layer A)

```bash
docker pull stanbyriukov31/hwatom-kv-shim:gate12s-f1prime
docker run --rm --gpus all stanbyriukov31/hwatom-kv-shim:gate12s-f1prime
```

Expect `GATE12_BEGIN`, `workload_id=a_gate_v1_kv_microbench`, `A46_DOCKER_OK`.

With artifact mount:

```bash
mkdir -p bench_out
docker run --rm --gpus all \
  -e HWATOM_ART_DIR=/out -v "$(pwd)/bench_out:/out" \
  stanbyriukov31/hwatom-kv-shim:gate12s-f1prime
```

---

## Honest metrics (iron)

| Layer | Result |
|-------|--------|
| Leaf @ 70% budget | **42.2%** liberation · **η_leaf=100%** |
| MOON-Xq bytes | **−58%** · ppl **+0.58%** |
| MOON-Xq tok/s | **~0.86×** FP16 FA2 (**FAIL** speed guard) |

Summary: [results/MOON_XQ_GATE_SUMMARY.txt](../results/MOON_XQ_GATE_SUMMARY.txt)

---

## Publish (maintainer)

Local: `bash scripts/shim/publish_docker_hub_v1.sh`  
CI: `.github/workflows/docker-hub-publish.yml` on version tags (needs `DOCKERHUB_USERNAME` + `DOCKERHUB_TOKEN` secrets).

---

## Falsifier (distribution)

30d after publish: Hub pulls alone **≠** pilot/inbound. Track **substantive** engineering contact, not pull count.
