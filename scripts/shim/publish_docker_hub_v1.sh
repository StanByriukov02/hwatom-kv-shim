#!/usr/bin/env bash
# Publish gate-12s eval images to Docker Hub (stanbyriukov31/hwatom-kv-shim).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REG="${DOCKERHUB_REGISTRY:-stanbyriukov31/hwatom-kv-shim}"
cd "$ROOT"

if ! command -v docker >/dev/null 2>&1; then
  echo "ABORT: docker not found"
  exit 2
fi

if ! docker info >/dev/null 2>&1; then
  echo "ABORT: docker daemon not reachable"
  exit 2
fi

echo "=== Docker Hub publish -> ${REG} ==="

docker build -f docker/gate-12s/Dockerfile.f1prime -t "${REG}:gate12s-f1prime" .
docker build -f docker/gate-12s/Dockerfile.eval -t "${REG}:gate12s-eval" .

docker tag "${REG}:gate12s-f1prime" "${REG}:t1-leaf-physics-20260525"
docker tag "${REG}:gate12s-eval" "${REG}:eval-moon-xq-20260526"

for t in gate12s-f1prime t1-leaf-physics-20260525 gate12s-eval eval-moon-xq-20260526; do
  echo "--- push ${REG}:${t} ---"
  docker push "${REG}:${t}"
done

echo "OK: pushed 4 tags to ${REG}"
