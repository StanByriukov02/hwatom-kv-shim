# IRON leaf ceiling spec — N1 (layer physics)

**Status:** ACTIVE 2026-05-25  
**workload_id (N2):** `t1_leaf_physics_v1`  
**Pairs with:** `OPERATOR_PARADIGM_RESET_20260525_V1.md` · `DAY_QUESTION_PROTOCOL_20260525_V1.md` · `P1_INTRA_LEAF_POSITION_V1.md`

---

## 0. Boyd × Musk (этот документ)

| Step | Ответ для N1 |
|------|----------------|
| **DESTRUCT** | Требование индустрии: «победа = NVML liberation / vLLM matrix / F1′ slots vs stock». **Отклоняем** как primary. |
| **DELETE** | Не считаем потолок на vLLM pool · не используем `ISO_MAX_SLOTS` slot theater без footnote. |
| **SIMPLIFY** | Один объект: **intra–2 MiB leaf pack** @ известном slice `S`. |
| **CREATE** | Numerator: `eta_leaf`, `eta_iso`, `ceiling_ratio` — ниже. |
| **NOT** | p-adic tree controller product claim (snowmobile **motor** = R&D register). |

---

## 1. Объект (наш слой)

| Term | Symbol | Default (iron) |
|------|--------|----------------|
| Physical leaf (H100 cuMem) | `L` | `2097152` (2 MiB) — `LEAF_BYTES` / `HWATOM_LEAF_BYTES` |
| Logical KV band (iso) | `S` | `524288` (512 KiB) — `HWATOM_ISO_SLICE_BYTES` |
| Placement round | `P(S)` | `iron_placement_round(S)` — ladder min fit ≤ `L` |
| Bands per leaf (max, uniform S) | `k_max` | `⌊L / P(S)⌋` |
| Eval cap (optional) | `K_cap` | `HWATOM_PACK_K_CAP` — structural eval only |

**Source code:** `tools/shim_iron/iron_gate_v1.c` (`iron_placement_round`, `iron_pack_slots_per_leaf`) · `src/shim/shim_pack_v1.c` (`shim_pack_k_cap_max_v1`).

---

## 2. Теоретический потолок (Archimedean — до «снегохода»)

### 2.1 Per-leaf layout efficiency

One **stock** band of size `S` in one leaf:

```
eta_stock_leaf = S / L     (default: 524288 / 2097152 = 0.25 = 25%)
```

**Ideal shim** packs `k_max` disjoint bands of size `S` into one leaf (no internal slack):

```
k_max = floor(L / P(S))
```

For default `S=524288`, `P(S)=524288` (ladder level 2 in `iron_placement_round`):

```
k_max = 2097152 / 524288 = 4
eta_shim_ceiling_leaf = (k_max * S) / L = 1.0 = 100%
```

**Interpretation:** P1 mechanism **cannot** exceed **100%** logical bytes per committed leaf — physics of fitting bands in `L`.

### 2.2 Committed bytes vs logical slots (iso fill)

For `N` logical slots of size `S` each (GQA off, `gqa_h=1`):

| Path | Leaves needed | Committed bytes |
|------|---------------|-----------------|
| **stock** | `N` | `C_stock(N) = N · L` |
| **shim (ideal pack)** | `⌈N / k_max⌉` | `C_shim(N) = ⌈N / k_max⌉ · L` |

**Ideal committed ratio** (large N):

```
C_shim / C_stock → 1/k_max = 0.25   (k_max=4)
```

This is **not** NVML total GPU footprint — only **cuMem committed along iso path**.

### 2.3 Iso @ VRAM budget (harness ceiling reference)

At fixed **budget** `B` (vram curve 50/60/70%), iron stops when `committed ≥ B`.

**Measured reference** (`vram_curve_20260522T060631Z`, `iso_logical_v2`):

| budget % | `committed_stock` | `committed_shim` | `logical_kv_gain_pct` | `cache_liberation_pct` |
|----------|-------------------|------------------|-------------------------|-------------------------|
| 50 | 42.46 GiB | **21.11 GiB** (plateau) | 98 | 50.28 |
| 60 | 50.96 GiB | **21.11 GiB** | 65 | 58.57 |
| 70 | 59.45 GiB | **21.11 GiB** | 42 | 64.49 |

**Fact:** shim **committed plateaus** ~`21112029184` B across 50–70% — not linear in budget.  
**Fact:** at plateau, **logical_kv_gain** and **liberation @ committed** are the **correct** battle numerators — **not** F1′ `resident_slots` (stock can exceed shim there).

---

## 3. Метрики N2 (pre-registered)

| ID | Formula | PASS hint |
|----|---------|-----------|
| **M1 `eta_leaf`** | `(pack_committed_logical per leaf) / L` on uniform micro stress | ≥ **0.70** of `eta_shim_ceiling_leaf` (70% of 100%) |
| **M2 `eta_iso@70`** | `logical_kv_bytes_shim / committed_bytes_shim` at 70% budget | **record**; compare to stock same budget |
| **M3 `ceiling_ratio`** | `eta_leaf_measured / eta_shim_ceiling_leaf` | ≥ **0.70** |
| **M4 `logical_kv_gain@70`** | iron `logical_kv_gain_pct` | ≥ **30%** (below 060631Z 42% → investigate regression) |

**Primary harness (N2):** `iso_logical` stock vs shim @ `HWATOM_ISO_SLICE_BYTES=524288`, `HWATOM_FRAG_UNIFORM_SLICE=1`, budget 50/60/70%, `HWATOM_SHIM_PACK=1`, `HWATOM_PACK_MEGA=1`, **no** `HWATOM_PACK_K_CAP`.

**Secondary (diagnostic):** single-leaf pack unit path / `make -C src/shim test-kcap` — not sufficient alone for G1.

---

## 4. Falsifiers (kill / pivot)

| ID | If true → |
|----|-----------|
| **F1** | `ceiling_ratio < 0.40` on M1 → pack mechanism **not** approaching theory; fix code before public alert |
| **F2** | `logical_kv_gain@70 < 20%` vs reproducible 060631Z → wrong BUILD_ID / regression; no recon push |
| **F3** | Only F1′ resident_slots used as win → **wrong harness** (inward trap); document TABU |
| **F4** | Claim ultrametric / p-adic controller in N3 public text → **TABU** without separate R&D iron |

---

## 5. Stock vs shim — what we claim after N2

| Claim | Allowed | TABU |
|-------|---------|------|
| Intra-leaf pack approaches **100%** @ `S=512KiB`, `k≤4` | After M1/M3 PASS | «Beats vAttention» |
| At iso budget, shim holds **more logical KV** at **lower committed** | After M2/M4 vs 060631Z band | NVML liberation % |
| Composable with external fragmentation layer | Narrative | Substitute MLA/DeltaNet |
| Full 2-adic semantic tree controller | R&D only | N1/N2 |

---

## 6. N2 run recipe (pointer)

Script: `scripts/shim/run_iron_leaf_ceiling_v1.sh` · emitter `emit_leaf_ceiling_gate12_v1.py`

```bash
# stock @ 70%
HWATOM_ISO_SLICE_BYTES=524288 HWATOM_FRAG_UNIFORM_SLICE=1 \
  tools/shim_iron/iron_gate_v1 --bench iso_logical stock --out art/stock_70.json

# shim @ 70% (LD_PRELOAD, pack+mega, stats)
env LD_PRELOAD=src/shim/lib2adic_shim.so HWATOM_SHIM_STATS=1 \
  HWATOM_SHIM_PACK=1 HWATOM_PACK_MEGA=1 \
  tools/shim_iron/iron_gate_v1 --bench iso_logical shim --out art/shim_70.json
```

Emit: `GATE12_LEAF_PHYSICS_stdout.txt` — `LEAF_PHYSICS_OK=yes` when M3+M4 pass.

---

## 7. N2 result (2026-05-25)

**PASS** — `N2_LEAF_PHYSICS_PASS_20260525_V1.md` · art `leaf_physics_20260525T070703Z`.

## 8. DECIDE template (post N2, 3 lines)

1. **ship** / **park** / **kill** — public recon line  
2. Evidence path — artifact dir  
3. Next — R8.3 pluggable **or** R&D snowmobile motor — **one** only  

---

## 9. Constants cheat sheet (agent)

```
L = 2^21 = 2097152
S = 2^19 = 524288   (iso default)
P(S) = 524288       (iron_placement_round for S=512KiB)
k_max = 4
eta_stock_leaf = 0.25
eta_shim_ceiling_leaf = 1.00
ideal C_shim/C_stock = 0.25 (large N)
```

**If `S` changes:** recompute `P(S)` and `k_max` before any PASS claim.
