# A-4.9 — External claim protocol (cache liberation / KV headroom)

**Date:** 2026-05-22  
**Status:** R&D discipline (not a product ship blocker)  
**Depends on:** A-4.8 iron truth, A-4.8b VRAM curve, A-2.14 ceiling fix

## One-line rule

**Publish what was measured, with the stock class and bottleneck named. Never imply shim committed VRAM scales with budget if the run hit slot/reserve or iron caps.**

## Metrics (definitions)

| Metric | Formula / source | Use |
|--------|------------------|-----|
| `cache_liberation_pct` | `(committed_stock − committed_shim) / committed_stock` at matched logical load | Partnership / C-082 band anchor |
| `logical_kv_gain_pct` | `(logical_shim − logical_stock) / logical_stock` at same budget tier | Hyperscaler “more context per dollar” |
| `layout_efficiency_pct` | `logical / committed` (iso bench) | Mechanism proof (packing) |
| Iso point (A-4.7b) | Fixed 2 GiB logical target | **75%** liberation possible — discrete k=4 vs stock 2 MiB/slice |

## What we may claim externally

1. **Mechanism:** Under uniform 512 KiB iso slice on H100 (2 MiB min granularity), P1 packing places **k=4** logical slices per 2 MiB leaf; multi-slot path does not hang when mega-VA enabled (A-2.14).
2. **Hyperscaler value (preferred narrative):** At the same VRAM budget scenario, shim sustains **higher logical KV** at **lower committed** than stock — cite `logical_kv_gain_pct` **and** both committed numbers from the same `GATE12` line.
3. **C-082 band (30–40%):** Treat as **audit anchor**, not physics max. Post A-2.14 VRAM curve: liberation **19% @ 50%**, **32% @ 60%**, **42% @ 70%** — **70% tier touches upper band**; 50% tier is **below** band because stock has not yet scaled committed, not because packing failed.

### What “k размазан по curve” means for Layer A (not a failure)

- **Fixed k≈4** (uniform iso, 512 KiB) → теоретический потолок liberation **~75%** на **iso point** (2 GiB target) — mechanism proof.
- **Fill-to-budget curve** — другой эксперимент: stock **догоняет** committed по мере роста budget%, shim **раньше упирается** в slot plateau (~65536) → отношение `(stock−shim)/stock` **растёт** с 50→70%, не фикс 30–40% на каждой точке.
- **Для цели A:** мы доказали **(a)** механизм pack, **(b)** fair comparison protocol, **(c)** band anchor на **70%** сценарии, **(d)** честно ниже band на 50% **без** F1b подтасовки. Цель A = **измеримое освобождение + repro**, не «все точки curve ∈ [30,40]».
4. **Iso 75%:** Internal / mechanism doc only unless paired with explicit “uniform iso, fixed logical target” disclaimer (see `FRICTION_A48_CACHE_LIBERATION_75PCT_ISO_V1.md`).

## What we must not claim

| TABU | Why |
|------|-----|
| “Shim uses 70% GPU VRAM committed like stock” | Post A-2.14 shim plateaus at **32 GiB committed / 65536 slots** while 70% budget ≈ **59 GiB** cap — **budget headroom unused** |
| “`logical_kv_gain` drop at 70% means weaker packing” | Pre A-2.14: shim **OOM/slot-bound** (~40k); stock scaled — **artifact** |
| Rename metrics / F1b to force 30–40% | Violates inviolable contract |
| Round liberation up without stock class + budget % + slot ceiling note | Misleading for audit |

## Stock class pairing (required in any external slide)

| Stock class | Shim mode | Liberation expectation |
|-------------|-----------|-------------------------|
| Iso uniform 512 KiB, fill-to-budget (A-4.8b) | P1 pack + mega | 19–42% on curve; gain 131–223% logical |
| Iso fixed 2 GiB logical (A-4.7b) | P1 pack | Up to **~75%** vs stock 2 MiB/slice |
| Frag / OOM / production-like | TBD | **Do not** extrapolate from iso alone |

## Bottleneck checklist (state in report)

- [ ] `resident_slots_shim` vs `ISO_MAX_SLOTS` (65536)
- [ ] `committed_shim` vs `vram_budget_bytes`
- [ ] `IRON_DIAG_FAIL` / `IRON_DIAG_STOP` present?
- [ ] `HWATOM_PACK_MEGA` on for multi-slot production-like tests?

## Suggested external sentence (template)

> On NVIDIA H100 PCIe (driver 550.163), with iso 512 KiB slices and VRAM budget **X%**, stock committed **A**, shim committed **B** (packed), delivering **L%** more logical KV (**not** implying shim fills the full budget cap when slots plateau at **N**).

## Next iron (optional, not A-4.9)

- Raise `ISO_MAX_SLOTS` + re-run curve to test shim vs `comm_cap`.
- GQA / alias path (WBS “deep” A-2.14 hinge) if production stock class differs.
