# SHIM_STATS v2 — engineer reference

Emitted on process exit when `HWATOM_SHIM_STATS=1` (non-zero).

## Text line (default)

```text
SHIM_STATS stats_v=2 build_id=b_shim_20260526a pack_committed_bytes=65131249664 pack_committed_peak_bytes=65131249664 pack_committed_fini_bytes=0 mega_reserve_count=1 mega_leaves_used=32
```

| Field | Meaning |
|-------|---------|
| `build_id` | `HWATOM_BUILD_ID` env or compile-time `HWATOM_BUILD_ID` |
| `pack_committed_bytes` | **Peak** committed bytes (use for pilot G1) |
| `pack_committed_peak_bytes` | Same as peak (explicit) |
| `pack_committed_fini_bytes` | Value at teardown (often 0 after flush) |

## JSON (`HWATOM_SHIM_STATS_JSON=1`)

```json
{"type":"SHIM_STATS","stats_v":2,"build_id":"...","pack_committed_peak_bytes":...,"pack_committed_fini_bytes":0,...}
```

## Parse

```bash
python3 scripts/vllm/b21_parse_cumem_trace_v1.py your_vllm.log
```

Pilot numerator: `docs/agent_workflow/PILOT_NUMERATOR_PREREG_20260525_V1.md`
