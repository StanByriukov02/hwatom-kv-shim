#!/usr/bin/env bash
# A-4.6 — build hwatom:gate-12s-f1 and run F1-prime e2e with GPU.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DATE_TAG="$(date -u +%Y%m%dT%H%M%SZ)"
IMAGE="${HWATOM_GATE_IMAGE:-hwatom:gate-12s-f1}"
ART="$ROOT/bench_artifacts/docker_a46_${DATE_TAG}"
RESULTS="$ROOT/results"

mkdir -p "$ART" "$RESULTS"

echo "=== A-4.6 Docker gate-12s F1-prime e2e ==="
echo "IMAGE=$IMAGE ART=$ART"

if ! command -v docker >/dev/null 2>&1; then
  echo "A46_ABORT: docker not found"
  exit 2
fi

if ! docker info >/dev/null 2>&1; then
  echo "A46_ABORT: docker daemon not reachable"
  exit 2
fi

cd "$ROOT"
docker build --no-cache -f docker/gate-12s/Dockerfile.f1prime -t "$IMAGE" .

echo "--- docker run (--gpus all) ---"
docker run --rm --gpus all \
  -e HWATOM_ART_DIR=/out \
  -v "$ART:/out" \
  "$IMAGE" 2>&1 | tee "$ART/container_stdout.txt"

test -f "$ART/GATE12_F1PRIME_stdout.txt"

grep -q 'GATE12_BEGIN' "$ART/GATE12_F1PRIME_stdout.txt"
grep -q 'stress_mode=f1_frag_v1' "$ART/GATE12_F1PRIME_stdout.txt"
grep -q 'workload_id=a_gate_v1_kv_microbench' "$ART/GATE12_F1PRIME_stdout.txt"
grep -q 'A46_DOCKER_OK' "$ART/container_stdout.txt"

GATE12_OUT="$ART/GATE12_F1PRIME_stdout.txt"
SLOTS_S="$(grep -h '^resident_slots_stock=' "$GATE12_OUT" "$ART/container_stdout.txt" 2>/dev/null | head -1 | cut -d= -f2 || true)"
SLOTS_H="$(grep -h '^resident_slots_shim=' "$GATE12_OUT" "$ART/container_stdout.txt" 2>/dev/null | head -1 | cut -d= -f2 || true)"
if [ -n "$SLOTS_H" ] && [ "$SLOTS_H" -gt 0 ] 2>/dev/null; then
  echo "A46_SLOT_CHECK_OK shim_slots=$SLOTS_H stock_slots=$SLOTS_S"
else
  echo "A46_SLOT_CHECK_FAIL shim_slots=$SLOTS_H"
  exit 1
fi

{
  echo "docker_a46_close: $DATE_TAG"
  echo "image: $IMAGE"
  echo "artifact_dir: $ART"
} | tee "$RESULTS/docker_a46_${DATE_TAG}.txt"

echo "IRON_A46_DOCKER_OK"
