#!/usr/bin/env bash
# A-4.5c — F1′ iron: mixed-size churn + resident (H100).
# A-4.5d: churn and resident are separate iron_gate_v1 processes (lines below = fresh OS process each).
# In-process churn+resident in ONE iron binary is not used for F1'/T1 metrics.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DATE_TAG="$(date -u +%Y%m%dT%H%M%SZ)"
ART="$ROOT/bench_artifacts/gate12_f1prime_${DATE_TAG}"
SHIM_SO="$ROOT/src/shim/lib2adic_shim.so"
GATE="$ROOT/tools/shim_iron/iron_gate_v1"
RESULTS="$ROOT/results"

mkdir -p "$ART" "$RESULTS"

echo "=== A-4.5c F1-prime iron (frag) ==="
echo "ART=$ART"

if ! command -v nvidia-smi >/dev/null 2>&1; then
  echo "IRON_ABORT: nvidia-smi not found"
  exit 2
fi

nvidia-smi --query-gpu=name,driver_version,memory.total,memory.used,utilization.gpu \
  --format=csv,noheader | tee "$ART/nvidia_smi.txt"

export GPU_LINE="$(head -1 "$ART/nvidia_smi.txt" | tr -d '\r')"
python3 - <<'PY' > "$ART/env.json"
import json, os
parts = os.environ["GPU_LINE"].split(", ")
json.dump({
    "gpu_name": parts[0].strip() if len(parts) > 0 else "",
    "driver_version": parts[1].strip() if len(parts) > 1 else "",
    "memory_total_mib": parts[2].strip() if len(parts) > 2 else "",
    "memory_used_mib": parts[3].strip() if len(parts) > 3 else "",
    "utilization_gpu_pct": parts[4].strip() if len(parts) > 4 else "",
}, open(1, "w"), indent=2)
PY

cd "$ROOT/src/shim"
make clean
make

cd "$ROOT/tools/shim_iron"
make clean
make iron_gate_v1
test -x "$GATE"

FS="$ART/frag_stock.json"
FH="$ART/frag_shim.json"

echo "--- A-4.5d churn-only (4s) stock ---"
HWATOM_FRAG_CHURN_S=4 HWATOM_FRAG_RESIDENT_S=0 "$GATE" --bench frag stock --out "$ART/frag_stock_churn.json" || true

echo "--- A-4.5d churn-only (4s) shim ---"
HWATOM_FRAG_CHURN_S=4 HWATOM_FRAG_RESIDENT_S=0 HWATOM_SHIM_PACK=1 HWATOM_PACK_MEGA=1 \
  env LD_PRELOAD="$SHIM_SO" "$GATE" --bench frag shim --out "$ART/frag_shim_churn.json" || true

echo "--- F1-frag stock (resident 8s, post-reset) ---"
HWATOM_FRAG_CHURN_S=0 HWATOM_FRAG_RESIDENT_S=8 "$GATE" --bench frag stock --out "$FS"

echo "--- F1-frag shim MIXED ladder (resident 8s, pack+mega) ---"
HWATOM_FRAG_CHURN_S=0 HWATOM_FRAG_RESIDENT_S=8 HWATOM_FRAG_UNIFORM_SLICE=0 HWATOM_SHIM_LOG=0 \
  HWATOM_FRAG_SEED=42 HWATOM_SHIM_PACK=1 HWATOM_PACK_MEGA=1 \
  env LD_PRELOAD="$SHIM_SO" "$GATE" --bench frag shim --out "$ART/frag_shim_mixed.json" || true

echo "--- F1-frag shim UNIFORM 512K (partnership resident metric) ---"
HWATOM_FRAG_CHURN_S=0 HWATOM_FRAG_RESIDENT_S=8 HWATOM_FRAG_UNIFORM_SLICE=1 HWATOM_SHIM_LOG=0 \
  HWATOM_FRAG_SEED=42 HWATOM_SHIM_PACK=1 HWATOM_PACK_MEGA=1 \
  env LD_PRELOAD="$SHIM_SO" "$GATE" --bench frag shim --out "$FH"

echo "--- F1-prime GATE12 stdout ---"
"$GATE" --emit-f1prime --frag-stock "$FS" --frag-shim "$FH" --env "$ART/env.json" \
  | tee "$ART/GATE12_F1PRIME_stdout.txt"

grep -q 'GATE12_BEGIN' "$ART/GATE12_F1PRIME_stdout.txt"
grep -q 'stress_mode=f1_frag_v1' "$ART/GATE12_F1PRIME_stdout.txt"

SLOTS_S="$(grep '^resident_slots_stock=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
SLOTS_H="$(grep '^resident_slots_shim=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
EFF_S="$(grep '^layout_efficiency_stock=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
EFF_H="$(grep '^layout_efficiency_shim=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
GAIN="$(grep '^kv_gain_pct=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
OOM_S="$(grep '^oom_stock=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"
OOM_H="$(grep '^oom_shim=' "$ART/GATE12_F1PRIME_stdout.txt" | cut -d= -f2)"

echo "A45B_METRICS slots_stock=$SLOTS_S slots_shim=$SLOTS_H eff_stock=$EFF_S eff_shim=$EFF_H gain=$GAIN oom_stock=$OOM_S oom_shim=$OOM_H"

export SLOTS_S SLOTS_H EFF_S EFF_H GAIN OOM_S OOM_H
python3 - <<'PY'
import os, sys
slots_s = int(os.environ["SLOTS_S"])
slots_h = int(os.environ["SLOTS_H"])
eff_s = float(os.environ["EFF_S"])
eff_h = float(os.environ["EFF_H"])
gain = float(os.environ["GAIN"])
oom_s = os.environ["OOM_S"]
oom_h = os.environ["OOM_H"]
ok = True
obs4 = (slots_h > slots_s) or (eff_h > eff_s + 1e-6)
if not obs4:
    print(f"A45B_FAIL: OBS-4 false slots {slots_h}<={slots_s} eff {eff_h}<={eff_s}")
    ok = False
if slots_h <= 0:
    print("A45B_FAIL: shim zero slots")
    ok = False
if slots_s == 0 and slots_h > 0:
    print(f"A45B_NOTE: stock_total_fail shim_survives slots_h={slots_h} (T1 flip)")
if slots_h > slots_s:
    print(f"A45B_NOTE: shim_slot_gain={100.0*(slots_h-slots_s)/max(slots_s,1):.1f}%")
if not ok:
    sys.exit(1)
print(f"A45B_OK: F1-prime OBS-4 met gain_pct={gain} slots_shim={slots_h} slots_stock={slots_s}")
PY

cp "$ART/GATE12_F1PRIME_stdout.txt" "$RESULTS/gate12_f1prime_stdout_${DATE_TAG}.txt"
{
  echo "iron_a45b_close: $DATE_TAG"
  echo "artifact_dir: $ART"
} | tee "$RESULTS/iron_a45b_${DATE_TAG}.txt"

echo "IRON_A45B_F1PRIME_OK"
