# SHIM_INVIOLABLE_CONTRACT_V1 — Layer A proof path (H100)

```yaml
contract_id: SHIM-INVIOLABLE-A-v1.0
status: closed                      # puzzle-close step 9 — 2026-05-21
blocker: BLOCKER-001                # GATE_BLOCKERS: closed (§5 swarm deferred)
vault_canon: docs/HwatomOrgOS/05_OPERATOR_PLAST/
operator_signed: 2026-05-21         # tiers T1/T2/T3 + WORKLOAD OK (tasks 1–3)
swarm_must_must_not: signed           # steps 10–11 — DELEGATION_MAP + vault §5
workload_annex: filled              # §8.1 Layer A + §8.2 Layer B (Node 1 PASS 2026-05-24)
public_repo_skeleton: docs/HwatomOrgOS/05_OPERATOR_PLAST/public_repo_v1_skeleton/
iron_pending: C-113,C-119,C-109     # sec31 + IRON_PENDING_AND_DEFERRED_ACTIVATION_V1
contact_email: stanislav.byriukov.research@gmail.com   # canon: OPERATOR_CONTACT_CANON_V1
```

**Boss:** contract + reproducible engineering proof (`GOAL_SUPREMACY` / vault mirror).  
**Not:** operator convenience, notebook rhetoric, or «ship everything at once».

**§5 swarm MUST/MUST NOT:** intentionally **not** closed on step 9 — finalized during **steps 10–11** (delegation map). Implement may proceed under §1–4, §9, and interim §5.3 hard stops.

---

## 1. Scope (Track A only)

| In scope v0.1 | Out of scope v0.1 |
|---------------|-------------------|
| `:gate-12s` **FROM scratch** Docker/ELF | vLLM inside our gate image |
| H100 **GATE12** stdout metrics | B200 / APTEI (Track C) |
| `lib2adic_shim.so` Driver API path (cuMem) | ASDA/vLLM plugin claims in gate README (Track B) |
| Public repro + inbound license path | Full internal/partner stack in open repo |

**T2 ≠ T1 narrative (Node 1.3.2):** Track B / `workload_id=b_vllm_*` claims **must not** cite T1 `GATE12_canonical.txt`, Table 1 **42.204%**, or `a_gate_v1_kv_microbench` numbers. T1 README / arxiv = synthetic cuMem only (§8.1). B iron file = separate canonical (future `GATE12_B_canonical.txt`).

Vault: `dual-artifact-scratch-vs-vllm-plugin`, `R03_SYNTHESIS_V1` §A, `RELEASE_GATE_A_12SEC_V1`.

---

## 2. Three delivery tiers (C-109) — operator confirmed

| Tier | Surface | Public open repo v1 | Partner / contract |
|------|---------|---------------------|-------------------|
| **T1 — Gate / public** | `cuMem*` via `lib2adic_shim.so` (LD_PRELOAD or static link per implement) | **YES** | YES |
| **T2 — B wedge** | `libpadic.so` on **customer** vLLM (`libasda` = PDF alias only, C-122) | `integrations/vllm/` when B exists | YES |
| **T3 — Partner internal** | UVM.ko / eBPF / DKMS | **NO** in public v1 | YES under agreement |

**Operator 2026-05-21:** T3 **never** in public v1 · T1 = cuMem shim · eBPF not in gate (C-044).

---

## 3. Artifact A (Naked Proof)

| Rule | Requirement |
|------|-------------|
| Image | `FROM scratch` — static ELF (+ minimal runner if needed) |
| Proof | GATE12 stdout per vault `stdout-legal-banner-spec` |
| Claims | Labeled `workload_id` — not production vLLM (C-040, C-104) |
| Forbidden | Python in gate, ZKML, eBPF, `.ko`, clickwrap stdin, fake git |

**Paper skeleton (step 8):** `public_repo_v1_skeleton/` — README split, LICENSE draft, empty GATE12 template.  
**Implement:** copy/adapt skeleton → `hardware_atom` when coding; **no fabricated** `results/`.

---

## 4. Shim technical floor (cuMem v0.1)

| Item | Decision |
|------|----------|
| API | `cuMemAddressReserve` → `cuMemCreate` → `cuMemMap` (PDF-3 v2) |
| Public binary | `lib2adic_shim.so` |
| Map epoch | **Serialised** v1 (sec31 Q2) — no lock-free theater |
| Min granularity | **2 MB leaf** — **iron verified** 2026-05-21 H100 min=rec=2097152 (C-113 resolved) |
| GQA aliasing | A VA policy only — not APTEI/TMEM (C-112) |
| GATE12 transport | **pinned_host_buffer** default; BAR1 optional spike (C-119, sec31 Q6) |
| Multi-GPU | **single_gpu_v1** (C-128, sec31 Q9) |

Iron protocols: vault `IRON_PENDING_AND_DEFERRED_ACTIVATION_V1` + `sec31` § Measurement → action.

---

## 5. Swarm rules (MUST / MUST NOT)

**Status:** `signed` (2026-05-21 steps 10–11).  
**Canon:** `05_OPERATOR_PLAST/_REGISTERS/DELEGATION_MAP_OPERATOR_AGENT_SWARM_V1.md`

### 5.1 Swarm MUST NOT

| | Rule |
|---|------|
| | Violate T1/T2/T3 tier boundaries |
| | Merge `:gate-12s` claims with vLLM/128K README |
| | Fabricate GATE12 % or fake git velocity (C-129) |
| | Python / pyo3 in `:gate-12s` gate image |
| | Edit `SHIM_INVIOLABLE_CONTRACT_V1.md` or WORKLOAD YAML |
| | Enqueue with `iron_pending: false` on partial_resolve C-* |
| | **Start/stop H100** (v1 — operator only) |
| | Assign `lane: iron` packets to swarm |
| | S1-class WBS (score 6–8) without operator+agent |

### 5.2 Swarm MUST

| | Rule |
|---|------|
| | `goal` references `contract_id: SHIM-INVIOLABLE-A-v1.0` |
| | Return `STEP_REPORT.yaml` per packet (implemented / tested / outcome) |
| | Respect `allowlist_paths` in packet |
| | Label logs `workload_id: a_gate_v1_kv_microbench` when gate-related |
| | Follow `PUZZLE_CLOSE_SYSTEM_HOOKS_V1` |
| | Use coder tact session per `SWARM_TOPOLOGY_V1.md` (recon off sprint 1) |
| | Complete **dry** WBS before requesting iron window |

### 5.3 Interim hard stops (always active)

| | Rule |
|---|------|
| **MUST NOT** | Violate T1/T2/T3 tier boundaries |
| **MUST NOT** | Merge `:gate-12s` claims with vLLM/128K README |
| **MUST NOT** | Fabricate GATE12 % or fake git velocity (C-129) |
| **MUST NOT** | Python / pyo3 in `:gate-12s` gate image |

**Friction:** any fail/surprise → `FRICTION_LESSON_PROTOCOL_V1.md` (vault `_REGISTERS/`) before WBS close.
| **MUST NOT** | Enqueue swarm with `iron_pending: false` on partial_resolve C-* |
| **MUST** | Reference `contract_id: SHIM-INVIOLABLE-A-v1.0` in swarm `goal` |
| **MUST** | Label logs `workload_id: a_gate_v1_kv_microbench` |
| **MUST** | Follow `PUZZLE_CLOSE_SYSTEM_HOOKS_V1` |

### 5.4 Fail/retry DEFAULT (operator+agent enforce)

| | Rule |
|---|------|
| **max_retries** | **2** per `packet_id` → 3rd fail **escalate_s1** (crown) |
| **Hard reject** | `fabricated_metrics`, tier/SHIM/README merge — **no** packet_v2 |
| **Lint** | `lint_swarm_step_report_v1.py` + `check_swarm_fail_retry_fsm_v1.py` |
| **Preflight** | `preflight_operator_agent_layer_a_v1.py`; Cursor rule `layer_a_operator_agent_preflight_v1.mdc` |
| **FSM canon** | vault `SWARM_A_FAIL_RETRY_FSM_V1.md` |

---

## 6. Iron-pending (does not reopen BLOCKER-001)

| ID | Status | Trigger |
|----|--------|---------|
| C-113 | partial_resolve | Q1 granularity log → may revise leaf table |
| C-119 | partial_resolve | Q6 A/B → promote BAR1 or keep pinned |
| C-109 | partial_resolve | Q2 + green GATE12 → confirm Driver tier |

BLOCKER-002 (H100 log) separate — `deferred_implement_phase`.

---

## 7. Closure ladder (BLOCKER-001)

| Stage | When | Status |
|-------|------|--------|
| relatively_closed v0.1 | 2026-05-21 AM | ✓ |
| **closed** (puzzle) | step 9 — WORKLOAD annex + operator sign-off | **✓** |
| §5 swarm signed | steps 10–11 | **✓** |
| iron amend | H100 bench | may patch §4 granularity/transport only |

---

## 8. Annex — WORKLOAD (from step 5, operator OK)

### 8.1 Layer A — Track T1 (closed)

```yaml
workload_A_gate: minimal_harness + synthetic_kv_band_microbench
workload_id: a_gate_v1_kv_microbench
model: none_for_gate_v1
context_tokens_12s: bench_derived_fixed_window
readme_arxiv_same: yes
readme_sections:
  - README.md#gate-12s-proof
```

Full record: `05_OPERATOR_PLAST/by_dimension/tests_contours_plans/A/WORKLOAD_A_GATE_DECISION_V1.md`

### 8.2 Layer B — Track T2 (Node 1 PASS 2026-05-24)

```yaml
workload_B_iron: b_vllm_kv_integration_v1
workload_B_smoke: b_gate_v1_vllm_smoke_v1
workload_B_deferred: false
tier: T2
stack: vllm
vllm_image: vllm/vllm-openai:v0.16.0
vllm_version_verified: "0.16.0"
h100_verify_artifact: bench_artifacts/b_node1_h100_verify_20260524/NODE1_H100_VERIFY.txt
injection_primary: Branch_A   # CuMemAllocator + --enable-sleep-mode + LD_PRELOAD
injection_fallback: Branch_B   # integrations/vllm pluggable VMM (Node 8)
constraints:
  tp_size: 1
  processes: 1
  expandable_segments: false
tabu:
  - t1_table1_on_stock_vllm_ld_preload
  - iron_before_b12_phase_b_pass
yaml_canon:
  - docs/agent_workflow/workloads/b_vllm_kv_integration_v1.yaml
  - docs/agent_workflow/workloads/b_gate_v1_vllm_smoke_v1.yaml
decision_doc: docs/agent_workflow/B_WORKLOAD_DECISION_V1.md
callgraph_doc: docs/agent_workflow/B11_VLLM_CALLGRAPH_V1.md  # Node 2 — not gate for annex
readme_sections:
  - README.md#vllm-integration-when-ready
  - README_T1_EVAL.md#track-b
```

**Failure forks:** WORKLOAD § F1 — if T1–T2 fail on iron, revise W1 per protocol (not marketing).  
**B bypass (B-CONC-01):** stock vLLM + LD_PRELOAD alone is **not** shim failure — Node 3 null test.

---

## 6. P1 verification backlog (2026-05-25 — Opus review)

**Canon narrative:** `P1_INTRA_LEAF_POSITION_V1.md`

| Item | Requirement before enterprise / multi-tenant claim |
|------|--------------------------------------------------|
| **Packed-leaf isolation** | `cuMemUnmap` / free of slot A must not corrupt VA/phys segments of B–D in same 2 MiB leaf |
| **Packing trigger** | Document whether pack requires concurrent reserves or serial batch — verify in `shim_pack_v1.c` |
| **Heterogeneous band sizes** | No claim beyond uniform 512 KiB iso until B iron |
| **NCCL / TP≥2** | Single-node only until explicit test; use `NCCL_CUMEM_ENABLE=0` on distributed smoke |
| **Radix / prefix sharing** | **TABU** compat with SGLang Radix until measured — orthogonal layers |

**Plateau @ 70% budget:** `ISO_MAX_SLOTS=65536` is bench configuration — not proof of fundamental shim ceiling (**arxiv** + R8.2 metric).

---

## 9. Vault pointers

| Doc | Role |
|-----|------|
| `P1_INTRA_LEAF_POSITION_V1.md` | strengths/weaknesses + actions |
| `GATE12_PROTOCOL_VALUE_V1.md` | audit contract separate from P1 |
| `ARXIV_V3_REVISION_CHECKLIST_V1.md` | preprint text deltas |
| `GATE_BLOCKERS_V1.md` | BLOCKER-001 = closed |
| `public_repo_v1_skeleton/` | step 8 tree |
| `2026-05-21_A-implementation-chain_V1.md` | node 2 shim, node 6 repo |
| `pdf3-cumem-shim-vs-uvm-hook.md` | mechanism |
| `GATE12_ACCEPTANCE_CRITERIA_V1.md` | iron close 002 |
| `OPERATOR_CONTACT_CANON_V1.md` | LICENSE · README · GATE12 CTA |

---

*§5 editable only after delegation (steps 10–11). Iron may amend §4 rows; not §1–3 tiers without operator.*
