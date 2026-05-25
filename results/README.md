# Results — T1 canonical outputs

Populated from **measured** H100 iron + Docker runs (`bench_artifacts/h100_pull_20260522/`). Do not hand-edit metric values.

| File | Purpose |
|------|---------|
| `GATE12_canonical.txt` | Public paste: F1′ + VRAM curve @70% + Docker (`t1-eval-20260522`) |
| **`GATE12_leaf_physics_v1.txt`** | **Leaf ceiling iron** (`t1-leaf-physics-20260525`, `workload_id=t1_leaf_physics_v1`) |
| `NODE4b_CACHE_LIBERATION_MEASURED_V1.txt` | Node 4b anchor: **42.204%** @ 70% budget (iso curve) |
| `GATE12_ZP12_*.txt` | Optional scout annex (Z friction); **not** T1 lead |
| `iron_*.txt` / `docker_*.txt` | Run pointers (if present) |

Repro:

```bash
bash scripts/shim/run_docker_a46_gate12_f1prime_v1.sh
bash scripts/shim/run_iron_a48b_vram_curve_v1.sh
bash scripts/shim/run_iron_leaf_ceiling_v1.sh   # tag t1-leaf-physics-20260525
```

Bundle index: `docs/agent_workflow/T1_PUBLIC_BUNDLE_V1.md`.
