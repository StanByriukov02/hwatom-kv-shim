# Eval shim — structural K-cap (not license theater)

**Status:** SHIPPED in code · public Docker builds `lib2adic_shim_eval.so`

## Two binaries

| Artifact | Build | K per 2 MiB leaf |
|----------|-------|------------------|
| `lib2adic_shim.so` | `make -C src/shim all` | **unlimited** (lab / iron / leaf physics) |
| `lib2adic_shim_eval.so` | `make -C src/shim eval` | **max 2 slots** baked in; `HWATOM_PACK_K_CAP=0` **ignored** |

`docker/gate-12s/Dockerfile.f1prime` and `Dockerfile.eval` copy **eval** `.so` as `lib2adic_shim.so` in the image.

## Verify

```bash
make -C src/shim eval
strings src/shim/lib2adic_shim_eval.so | grep -q HWATOM_EVAL_SHIM_BUILD || true
# SHIM_STATS line includes: eval_shim=1 k_cap=2
```

## Honesty

Anyone with **source** can `make all` and get full K. Structural gap = **public prebuilt eval artifact**, not secrecy. Licensed tier = full `.so` + integration under contract.

Iron tags (`t1-leaf-physics-20260525`) = **lab build**, not eval docker image.
