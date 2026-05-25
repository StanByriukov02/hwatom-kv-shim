#!/usr/bin/env bash
# N2 — leaf physics iron: iso @ 50/60/70% · our measurement contract (not vLLM NVML).
# Canon: IRON_LEAF_CEILING_SPEC_V1.md · workload_id=t1_leaf_physics_v1
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DATE_TAG="$(date -u +%Y%m%dT%H%M%SZ)"
ART="${ART_DIR:-$ROOT/bench_artifacts/leaf_physics_${DATE_TAG}}"
SHIM_SO="${SHIM_SO:-$ROOT/src/shim/lib2adic_shim.so}"
GATE="${GATE:-$ROOT/tools/shim_iron/iron_gate_v1}"
BUILD_ID="${HWATOM_BUILD_ID:-b_shim_20260525a}"

mkdir -p "$ART"
log() { echo "[leaf_physics] $*" | tee -a "$ART/run.log"; }

if ! command -v nvidia-smi >/dev/null 2>&1; then
  echo "LEAF_PHYSICS_ABORT: nvidia-smi missing"
  exit 2
fi

nvidia-smi --query-gpu=name,driver_version,memory.total,memory.used,utilization.gpu \
  --format=csv,noheader | tee "$ART/nvidia_smi.txt"

export HWATOM_ISO_SLICE_BYTES="${HWATOM_ISO_SLICE_BYTES:-524288}"
export HWATOM_LOGICAL_KV_TARGET_BYTES=0
export HWATOM_FRAG_UNIFORM_SLICE=1
export HWATOM_SHIM_PACK=1
export HWATOM_PACK_MEGA=1
unset HWATOM_PACK_K_CAP

log "build shim BUILD_ID=$BUILD_ID"
make -C "$ROOT/src/shim" clean all "HWATOM_BUILD_ID=$BUILD_ID" >>"$ART/run.log" 2>&1
make -C "$ROOT/tools/shim_iron" clean iron_gate_v1 >>"$ART/run.log" 2>&1

python3 - <<PY >"$ART/env.json"
import json, os
from pathlib import Path
smi = Path("$ART/nvidia_smi.txt").read_text().strip().split(", ")
json.dump({
    "workload_id": "t1_leaf_physics_v1",
    "stress_mode": "t1_leaf_physics_v1",
    "spec_id": "IRON_LEAF_CEILING_SPEC_V1",
    "gpu_name": smi[0].strip() if smi else "",
    "driver_version": smi[1].strip() if len(smi) > 1 else "",
    "memory_total_mib": smi[2].strip() if len(smi) > 2 else "",
    "leaf_bytes": 2097152,
    "iso_slice_bytes": int(os.environ.get("HWATOM_ISO_SLICE_BYTES", "524288")),
    "k_max_per_leaf": 4,
    "vram_budget_points_pct": [50, 60, 70],
    "build_id": os.environ.get("HWATOM_BUILD_ID", "b_shim_20260525a"),
}, open(1, "w"), indent=2)
PY

for PCT in 50 60 70; do
  export HWATOM_VRAM_BUDGET_PCT="$PCT"
  log "budget ${PCT}% stock"
  "$GATE" --bench iso_logical stock --out "$ART/stock_${PCT}.json" 2>&1 | tee -a "$ART/run.log"
  log "budget ${PCT}% shim"
  env LD_PRELOAD="$SHIM_SO" HWATOM_SHIM_STATS=1 HWATOM_BUILD_ID="$BUILD_ID" \
    HWATOM_WORKLOAD_ID=t1_leaf_physics_v1 \
    "$GATE" --bench iso_logical shim --out "$ART/shim_${PCT}.json" \
    2>&1 | tee "$ART/shim_${PCT}.log" | tee -a "$ART/run.log"
  grep -h 'SHIM_STATS pack_committed_bytes=' "$ART/shim_${PCT}.log" 2>/dev/null | tail -1 \
    >"$ART/shim_${PCT}_pack_stats.txt" || true
done

log "emit GATE12 leaf physics"
set +e
python3 "$ROOT/scripts/shim/emit_leaf_ceiling_gate12_v1.py" \
  --art-dir "$ART" --env "$ART/env.json" --build-id "$BUILD_ID" \
  --out "$ART/GATE12_LEAF_PHYSICS_stdout.txt" | tee "$ART/emit.log"
EMIT_RC=${PIPESTATUS[0]}
set -e

grep -q 'GATE12_BEGIN' "$ART/GATE12_LEAF_PHYSICS_stdout.txt"
grep -q 'workload_id=t1_leaf_physics_v1' "$ART/GATE12_LEAF_PHYSICS_stdout.txt"
grep -q 'LEAF_PHYSICS_OK=yes' "$ART/GATE12_LEAF_PHYSICS_stdout.txt"

log "LEAF_PHYSICS_N2_PASS art=$ART"
echo "LEAF_PHYSICS_N2_PASS art=$ART workload_id=t1_leaf_physics_v1"
exit "$EMIT_RC"
