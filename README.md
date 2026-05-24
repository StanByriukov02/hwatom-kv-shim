# Hardware Atom — T1 evaluation (CUDA KV / gate-12s)

**License:** [LICENSE.md](LICENSE.md) (Evaluation-Only) · **Results:** [results/GATE12_canonical.txt](results/GATE12_canonical.txt) · **Paper:** [docs/arxiv/paper/main.pdf](docs/arxiv/paper/main.pdf)

Reproducible evaluation of a user-space CUDA memory shim (2 MiB granularity, H100-class). Synthetic microbenchmark — not production vLLM inference.

| | |
|--|--|
| **Tag** | `t1-eval-20260522` |
| **Canonical @ 70% VRAM** | **42.204%** cache liberation, **+131%** logical KV vs stock |
| **Layer B** | vLLM integration — roadmap in [README_T1_EVAL.md](README_T1_EVAL.md) |
| **Contact** | stanislav.byriukov.research@gmail.com |

## Quick repro

`ash
git clone https://github.com/StanByriukov02/hwatom-gate-12s.git
cd hwatom-gate-12s
git checkout t1-eval-20260522
bash scripts/shim/run_docker_a46_gate12_f1prime_v1.sh
`

Full narrative: **[README_T1_EVAL.md](README_T1_EVAL.md)**
