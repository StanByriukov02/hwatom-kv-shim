#!/usr/bin/env bash
# A-4.6 — in-container F1-prime (A-4.5d two-phase + uniform resident metrics).
set -euo pipefail

HWATOM_DIR="${HWATOM_DIR:-/opt/hwatom}"
IRON="${HWATOM_DIR}/iron_gate_v1"
SHIM="${HWATOM_DIR}/lib2adic_shim.so"
ART="${HWATOM_ART_DIR:-/tmp/gate12_f1prime}"

mkdir -p "$ART"
export HWATOM_FRAG_CHURN_S="${HWATOM_FRAG_CHURN_S:-4}"
export HWATOM_FRAG_RESIDENT_S="${HWATOM_FRAG_RESIDENT_S:-8}"
export HWATOM_FRAG_UNIFORM_SLICE="${HWATOM_FRAG_UNIFORM_SLICE:-1}"

if ! command -v nvidia-smi >/dev/null 2>&1; then
  echo "A46_ABORT: nvidia-smi missing (need --gpus all)"
  exit 2
fi

nvidia-smi --query-gpu=name,driver_version --format=csv,noheader | head -1 | tee "$ART/nvidia_smi.txt"

test -x "$IRON"
test -f "$SHIM"

echo "=== A-4.6 docker churn-only stock ==="
HWATOM_FRAG_RESIDENT_S=0 "$IRON" --bench frag stock --out "$ART/frag_stock_churn.json" || true

echo "=== A-4.6 docker churn-only shim ==="
HWATOM_FRAG_RESIDENT_S=0 HWATOM_SHIM_PACK=1 HWATOM_PACK_MEGA=1 \
  env LD_PRELOAD="$SHIM" "$IRON" --bench frag shim --out "$ART/frag_shim_churn.json" || true

echo "=== A-4.6 docker F1-frag stock (resident) ==="
HWATOM_FRAG_CHURN_S=0 HWATOM_FRAG_RESIDENT_S=8 "$IRON" --bench frag stock --out "$ART/frag_stock.json"

echo "=== A-4.6 docker F1-frag shim (pack+mega, uniform 512K) ==="
HWATOM_FRAG_CHURN_S=0 HWATOM_FRAG_RESIDENT_S=8 HWATOM_FRAG_UNIFORM_SLICE=1 \
  HWATOM_SHIM_PACK=1 HWATOM_PACK_MEGA=1 \
  env LD_PRELOAD="$SHIM" "$IRON" --bench frag shim --out "$ART/frag_shim.json"

echo "=== A-4.6 docker GATE12 emit ==="
"$IRON" --emit-f1prime \
  --frag-stock "$ART/frag_stock.json" \
  --frag-shim "$ART/frag_shim.json" | tee "$ART/GATE12_F1PRIME_stdout.txt"

grep -q 'GATE12_BEGIN' "$ART/GATE12_F1PRIME_stdout.txt"
grep -q 'stress_mode=f1_frag_v1' "$ART/GATE12_F1PRIME_stdout.txt"
grep -q 'workload_id=a_gate_v1_kv_microbench' "$ART/GATE12_F1PRIME_stdout.txt"

SLOTS_S="$(grep '^resident_slots_stock=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
SLOTS_H="$(grep '^resident_slots_shim=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
echo "A46_METRICS slots_stock=$SLOTS_S slots_shim=$SLOTS_H"

echo "A46_DOCKER_OK art=$ART"
