#!/usr/bin/env bash
# MOON-Xq eval recipes (vLLM 0.21 docker). Usage: recipe_moon_xq_v1.sh <path-a-v2|hb-fa2>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE="${VLLM_IMAGE:-vllm/vllm-openai:v0.21.0}"
MODE="${1:-path-a-v2}"

run_py() {
  local out="$1" script="$2"
  shift 2
  docker run --rm --gpus all --entrypoint python3 \
    -v "$ROOT:/workspace" \
    -v "${HF_HOME:-$HOME/.cache/huggingface}:/root/.cache/huggingface" \
    -e HF_HOME=/root/.cache/huggingface \
    -e VLLM_WORKER_MULTIPROC_METHOD=spawn \
    "$IMAGE" \
    "/workspace/scripts/moon/$script" --out "/workspace/$out" "$@"
}

case "$MODE" in
  path-a-v2)
    run_py docs/agent_workflow/PATH_A_TIER3_VLLM_RECEIPT_20260526_V2.json \
      path_a_tier3_vllm_iron_v1.py --iron-tag v2 --no-enforce-eager --tq-kv turboquant_4bit_nc
    ;;
  hb-fa2)
    run_py docs/agent_workflow/PATH_B_HB_FA2_RECEIPT_20260526_V1.json \
      path_b_hb_fa2_parity_v1.py
    ;;
  *)
    echo "usage: $0 path-a-v2|hb-fa2" >&2
    exit 2
    ;;
esac

echo "OK: $MODE"
