#!/usr/bin/env bash
# Eval-cap tier — F1-prime repro with HWATOM_PACK_K_CAP (structural, not time throttle).
set -euo pipefail

HWATOM_DIR="${HWATOM_DIR:-/opt/hwatom}"
IRON="${HWATOM_DIR}/iron_gate_v1"
SHIM="${HWATOM_DIR}/lib2adic_shim.so"
ART="${HWATOM_ART_DIR:-/tmp/gate12_eval_cap}"

mkdir -p "$ART"
export HWATOM_FRAG_CHURN_S="${HWATOM_FRAG_CHURN_S:-4}"
export HWATOM_FRAG_RESIDENT_S="${HWATOM_FRAG_RESIDENT_S:-8}"
export HWATOM_FRAG_UNIFORM_SLICE="${HWATOM_FRAG_UNIFORM_SLICE:-1}"
export HWATOM_PACK_K_CAP="${HWATOM_PACK_K_CAP:-2}"
export HWATOM_SHIM_PACK=1
export HWATOM_PACK_MEGA=1

if ! command -v nvidia-smi >/dev/null 2>&1; then
  echo "EVAL_CAP_ABORT: nvidia-smi missing (need --gpus all)"
  exit 2
fi

nvidia-smi --query-gpu=name,driver_version --format=csv,noheader | head -1 | tee "$ART/nvidia_smi.txt"

test -x "$IRON"
test -f "$SHIM"

echo "=== eval-cap docker F1-frag stock (resident) ==="
HWATOM_FRAG_CHURN_S=0 HWATOM_FRAG_RESIDENT_S=8 "$IRON" --bench frag stock --out "$ART/frag_stock.json"

echo "=== eval-cap docker F1-frag shim (PACK_K_CAP=$HWATOM_PACK_K_CAP) ==="
HWATOM_FRAG_CHURN_S=0 HWATOM_FRAG_RESIDENT_S=8 HWATOM_FRAG_UNIFORM_SLICE=1 \
  env LD_PRELOAD="$SHIM" HWATOM_BUILD_ID="${HWATOM_BUILD_ID:-gate12s_eval}" \
  HWATOM_WORKLOAD_ID="${HWATOM_EVAL_WORKLOAD_ID:-a_gate_v1_kv_microbench}" \
  "$IRON" --bench frag shim --out "$ART/frag_shim.json" 2>&1 | tee "$ART/frag_shim.log"

echo "=== eval-cap docker GATE12 emit ==="
"$IRON" --emit-f1prime \
  --frag-stock "$ART/frag_stock.json" \
  --frag-shim "$ART/frag_shim.json" | tee "$ART/GATE12_F1PRIME_stdout.txt"

grep -q 'GATE12_BEGIN' "$ART/GATE12_F1PRIME_stdout.txt"
grep -q 'workload_id=a_gate_v1_kv_microbench' "$ART/GATE12_F1PRIME_stdout.txt"

SLOTS_S="$(grep '^resident_slots_stock=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
SLOTS_H="$(grep '^resident_slots_shim=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
echo "EVAL_CAP_METRICS pack_k_cap=$HWATOM_PACK_K_CAP slots_stock=$SLOTS_S slots_shim=$SLOTS_H"
echo "HWATOM_BUILD_ID=${HWATOM_BUILD_ID:-unset}"
echo "HWATOM_EVAL_TIER=${HWATOM_EVAL_TIER:-eval_cap_v1}"
echo "EVAL_CAP_DOCKER_OK art=$ART"
