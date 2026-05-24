# T1 public repo rename — `hwatom-gate-12s` → `hwatom-kv-shim`

**Date:** 2026-05-24  
**Reason:** discovery (recon bots, Papers with Code, tenant engineers) — name must signal KV / shim / CUDA / H100.

## Policy: topics vs spam

| Earlier (T1 staging) | Now (post-T1 live) |
|----------------------|---------------------|
| Avoid **topic spam** on empty/staging repo | **Curated GitHub Topics** (≤10) — standard metadata facet |
| Minimal surface before claims frozen | T1 tag live; discovery is an explicit goal |

**TABU:** keyword stuffing in README, fake metrics, 30+ topics.  
**OK:** repo name + About + 8–10 real technology topics matching actual artifact.

## GitHub Topics (canonical)

`kv-cache` · `cuda` · `h100` · `gpu-memory` · `inference` · `vllm` · `paged-attention` · `memory-allocator` · `nvidia-gpu` · `llm-inference`

## arXiv frozen URL

Submitted preprint cites **`StanByriukov02/hwatom-gate-12s`**.  
**Do not** change `docs/arxiv/paper/main.tex` / `references.bib` for rename — GitHub redirects old slug to `hwatom-kv-shim`.

## Unchanged

- Tag `t1-eval-20260522`
- Docker image name `hwatom:gate-12s-f1` (eval harness brand)
- GATE12_canonical.txt numbers
