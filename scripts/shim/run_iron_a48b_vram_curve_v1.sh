#!/usr/bin/env bash
# A-4.8b — VRAM budget curve at 50/60/70% committed cap (iso-logical fill).
set -eu
set -o pipefail 2>/dev/null || true

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DATE_TAG="$(date -u +%Y%m%dT%H%M%SZ)"
ART="$ROOT/bench_artifacts/vram_curve_${DATE_TAG}"
SHIM_SO="$ROOT/src/shim/lib2adic_shim.so"
GATE="$ROOT/tools/shim_iron/iron_gate_v1"
RESULTS="$ROOT/results"
SPEC_ID="NODE-4b-VRAM-BUDGET-CURVE-v1"

mkdir -p "$ART" "$RESULTS"

echo "=== A-4.8b VRAM budget curve iron (50/60/70%) ==="
echo "ART=$ART"

if ! command -v nvidia-smi >/dev/null 2>&1; then
  echo "IRON_ABORT: nvidia-smi not found"
  exit 2
fi

GIT_SHA="$(cd "$ROOT" && git rev-parse HEAD 2>/dev/null || echo unknown)"
nvidia-smi --query-gpu=name,driver_version,memory.total,memory.used,utilization.gpu \
  --format=csv,noheader | tee "$ART/nvidia_smi.txt"

export GPU_LINE="$(head -1 "$ART/nvidia_smi.txt" | tr -d '\r')"
export GIT_SHA
export HWATOM_ISO_SLICE_BYTES="${HWATOM_ISO_SLICE_BYTES:-524288}"
export HWATOM_LOGICAL_KV_TARGET_BYTES=0
export HWATOM_SHIM_PACK="${HWATOM_SHIM_PACK:-1}"
export HWATOM_PACK_MEGA="${HWATOM_PACK_MEGA:-1}"
export HWATOM_GQA_ALIAS="${HWATOM_GQA_ALIAS:-0}"
export HWATOM_IRON_GQA_LOGICAL="${HWATOM_IRON_GQA_LOGICAL:-0}"
export HWATOM_GQA_HEADS="${HWATOM_GQA_HEADS:-8}"

python3 - <<'PY' > "$ART/env.json"
import json, os
parts = os.environ["GPU_LINE"].split(", ")
json.dump({
    "workload_id": "a_gate_v1_kv_microbench",
    "workload_class": "synthetic_kv_band",
    "stress_mode": "vram_budget_curve_v1",
    "gpu_name": parts[0].strip() if len(parts) > 0 else "",
    "driver_version": parts[1].strip() if len(parts) > 1 else "",
    "memory_total_mib": parts[2].strip() if len(parts) > 2 else "",
    "memory_used_mib": parts[3].strip() if len(parts) > 3 else "",
    "utilization_gpu_pct": parts[4].strip() if len(parts) > 4 else "",
    "git_sha": os.environ.get("GIT_SHA", "unknown"),
    "leaf_bytes": 2097152,
    "iso_slice_bytes": int(os.environ.get("HWATOM_ISO_SLICE_BYTES", "524288")),
    "vram_budget_points_pct": [50, 60, 70],
    "spec_id": os.environ.get("SPEC_ID", "NODE-4b-VRAM-BUDGET-CURVE-v1"),
    "repro": "bash scripts/shim/run_iron_a48b_vram_curve_v1.sh",
}, open(1, "w"), indent=2)
PY

cd "$ROOT/src/shim"
make clean
make

cd "$ROOT/tools/shim_iron"
make clean
make iron_gate_v1
test -x "$GATE"

for PCT in 50 60 70; do
  export HWATOM_VRAM_BUDGET_PCT="$PCT"
  echo "--- budget ${PCT}% stock ---"
  "$GATE" --bench iso_logical stock --out "$ART/stock_${PCT}.json"
  echo "--- budget ${PCT}% shim ---"
  HWATOM_SHIM_LOG=0 env LD_PRELOAD="$SHIM_SO" \
    "$GATE" --bench iso_logical shim --out "$ART/shim_${PCT}.json"
done

echo "--- emit VRAM curve GATE12 ---"
set +e
"$GATE" --emit-vram-curve --curve-dir "$ART" --env "$ART/env.json" \
  | tee "$ART/GATE12_VRAM_CURVE_stdout.txt"
EMIT_RC=$?
set -e

grep -q 'GATE12_BEGIN' "$ART/GATE12_VRAM_CURVE_stdout.txt"
grep -q 'stress_mode=vram_budget_curve_v1' "$ART/GATE12_VRAM_CURVE_stdout.txt"
grep -q 'curve_points_ok=3' "$ART/GATE12_VRAM_CURVE_stdout.txt"

OK50="$(grep '^curve_point_50_status=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"
OK60="$(grep '^curve_point_60_status=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"
OK70="$(grep '^curve_point_70_status=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"
LIB50="$(grep '^cache_liberation_pct_50=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"
LIB60="$(grep '^cache_liberation_pct_60=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"
LIB70="$(grep '^cache_liberation_pct_70=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"
GAIN50="$(grep '^logical_kv_gain_pct_50=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"
GAIN60="$(grep '^logical_kv_gain_pct_60=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"
GAIN70="$(grep '^logical_kv_gain_pct_70=' "$ART/GATE12_VRAM_CURVE_stdout.txt" | cut -d= -f2)"

echo "curve: 50% lib=${LIB50}% gain=${GAIN50}% ${OK50} | 60% lib=${LIB60}% gain=${GAIN60}% ${OK60} | 70% lib=${LIB70}% gain=${GAIN70}% ${OK70}"

cp "$ART/GATE12_VRAM_CURVE_stdout.txt" "$RESULTS/vram_curve_stdout_${DATE_TAG}.txt"

if [ "$EMIT_RC" -eq 0 ] && [ "$OK50" = "ok" ] && [ "$OK60" = "ok" ] && [ "$OK70" = "ok" ]; then
  echo "IRON_A48B_VRAM_CURVE_OK"
  exit 0
fi

echo "IRON_A48B_VRAM_CURVE_FAIL rc=$EMIT_RC"
exit "${EMIT_RC:-1}"
