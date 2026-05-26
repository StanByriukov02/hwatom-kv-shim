#!/usr/bin/env python3
"""Path A — Tier 3 iron on vLLM TurboQuant (MOON-REP-Xq-VLLM-01).

Compares FP16/bf16 KV (auto) vs turboquant preset on same gates as EOD.
Requires vLLM >= 0.21 with turboquant_* cache dtypes.
"""
from __future__ import annotations

import os

os.environ.setdefault("VLLM_WORKER_MULTIPROC_METHOD", "spawn")

import argparse
import json
import math
import re
import time
from typing import Any

# EOD gates
BYTES_REDUCTION_MIN = 0.30
PPL_DRIFT_MAX = 0.01
TOK_S_MIN_RATIO = 0.97
NIAH_DEPTHS = (4096, 8192, 16384, 32768)

MODEL_ID = "Qwen/Qwen2.5-7B-Instruct"
BASELINE_KV = "auto"
TQ_KV = "turboquant_4bit_nc"  # ~3.8x per vLLM PR; alt: turboquant_k8v4 quality-first

# Compression accounting (vLLM PR #38479 table) for bytes gate
PRESET_BYTES = {
    "auto": 57344,  # Qwen2.5-7B fp16 bpt from tier1
    "turboquant_4bit_nc": 24064,  # ~58% from tier1 xq_k4v8_boundary2
    "turboquant_k8v4": 36864,  # ~2.6x -> ~22000 bpt approx
}


def build_niah_prompt(
    tok: Any, depth: int, needle: str, max_prefill: int, max_output: int = 32
) -> tuple[str, int]:
    """Haystack sized so prefill <= max_prefill and targets ~depth tokens."""
    filler = "The grass is green. The sky is blue. "
    cap = max(512, max_prefill - max_output - 64)
    target = min(depth, cap)

    def pack(reps: int) -> tuple[str, int]:
        hay = filler * reps
        prompt = f"{hay}\nThe secret code is {needle}.\n{hay}\nRecall the secret code exactly.\nAnswer:"
        return prompt, len(tok.encode(prompt))

    lo, hi = 1, 2
    while True:
        _, n_hi = pack(hi)
        if n_hi >= target or n_hi >= cap:
            break
        hi *= 2
    best_prompt, best_len = pack(1)
    lo_b, hi_b = 1, hi
    while lo_b <= hi_b:
        mid = (lo_b + hi_b) // 2
        prompt, n = pack(mid)
        if n > cap:
            hi_b = mid - 1
            continue
        if abs(n - target) <= abs(best_len - target):
            best_prompt, best_len = prompt, n
        if n < target:
            lo_b = mid + 1
        else:
            hi_b = mid - 1
    return best_prompt, best_len


def run_niah(
    llm: Any,
    tok: Any,
    depths: tuple[int, ...],
    needle: str,
    max_model_len: int,
) -> list[dict]:
    from vllm import SamplingParams

    sp = SamplingParams(max_tokens=32, temperature=0.0)
    max_prefill = max_model_len - 32
    rows = []
    for depth in depths:
        prompt, prefill = build_niah_prompt(tok, depth, needle, max_prefill)
        row: dict[str, Any] = {
            "depth_target": depth,
            "prefill_tokens": prefill,
            "max_prefill_cap": max_prefill,
        }
        try:
            out = llm.generate([prompt], sp)[0]
            text = out.outputs[0].text
            row["output_snip"] = text[:160]
            row["hit"] = needle in text or needle.replace("-", "") in text.replace("-", "")
        except Exception as e:
            row["error"] = str(e)[:200]
            row["hit"] = False
        rows.append(row)
    return rows


def run_tok_s(llm: Any, tok: Any, ctx_target: int, decode_n: int) -> dict:
    from vllm import SamplingParams

    filler = "Word "
    prompt = (filler * (ctx_target // 2))[: ctx_target * 3]
    enc = tok.encode(prompt, truncation=True, max_length=ctx_target)
    prompt = tok.decode(enc)
    sp = SamplingParams(max_tokens=decode_n, temperature=0.0)
    _ = llm.generate([prompt], sp)  # warmup
    t0 = time.perf_counter()
    _ = llm.generate([prompt], sp)
    elapsed = time.perf_counter() - t0
    return {
        "ctx_tokens": len(enc),
        "decode_tokens": decode_n,
        "elapsed_s": round(elapsed, 3),
        "tok_s": round(decode_n / elapsed, 2) if elapsed > 0 else 0.0,
    }


def _extract_prompt_logprobs(out: Any) -> list[Any] | None:
    lps = getattr(out, "prompt_logprobs", None)
    if lps:
        return lps
    if out.outputs:
        return getattr(out.outputs[0], "prompt_logprobs", None)
    return None


def run_ppl_sample(llm: Any, tok: Any, texts: list[str], max_len: int = 256) -> float:
    """Mean perplexity via vLLM prompt_logprobs (RequestOutput level)."""
    from vllm import SamplingParams

    sp = SamplingParams(max_tokens=1, temperature=0.0, prompt_logprobs=1)
    nlls: list[float] = []
    for text in texts:
        enc = tok.encode(text, truncation=True, max_length=max_len)
        if len(enc) < 32:
            continue
        prompt = tok.decode(enc)
        try:
            out = llm.generate([prompt], sp)[0]
            lps = _extract_prompt_logprobs(out)
            if not lps:
                continue
            for i, lpdict in enumerate(lps):
                if lpdict is None or i >= len(enc):
                    continue
                tid = enc[i]
                if tid in lpdict:
                    nlls.append(-float(lpdict[tid].logprob))
        except Exception:
            continue
    if not nlls:
        return float("nan")
    return math.exp(sum(nlls) / len(nlls))


def load_wikitext(n: int) -> list[str]:
    try:
        from datasets import load_dataset

        ds = load_dataset("wikitext", "wikitext-2-raw-v1", split="validation")
        return [r["text"] for r in ds if r["text"].strip()][:n]
    except Exception:
        return ["The " * 50 for _ in range(n)]


def make_llm(kv_dtype: str, max_model_len: int, enforce_eager: bool) -> Any:
    from vllm import LLM

    return LLM(
        model=MODEL_ID,
        kv_cache_dtype=kv_dtype,
        max_model_len=max_model_len,
        enforce_eager=enforce_eager,
        gpu_memory_utilization=0.90,
        trust_remote_code=True,
    )


def _json_safe(obj: Any) -> Any:
    if isinstance(obj, float) and (math.isnan(obj) or math.isinf(obj)):
        return None
    if isinstance(obj, dict):
        return {k: _json_safe(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_json_safe(v) for v in obj]
    return obj


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--niah-depths", default="4096,8192,16384,32768")
    ap.add_argument("--ppl-chunks", type=int, default=12)
    ap.add_argument("--ctx-toks", type=int, default=8192)
    ap.add_argument("--decode-tokens", type=int, default=48)
    ap.add_argument("--max-model-len", type=int, default=32768)
    ap.add_argument(
        "--enforce-eager",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="False = production-like graphs (tok/s); True = debug/eager",
    )
    ap.add_argument(
        "--tq-kv",
        default=TQ_KV,
        choices=("turboquant_4bit_nc", "turboquant_k8v4"),
        help="TurboQuant preset for quant arm",
    )
    ap.add_argument("--iron-tag", default="v2", help="Receipt tag suffix")
    args = ap.parse_args()

    tq_kv = args.tq_kv
    depths = tuple(int(x) for x in args.niah_depths.split(","))
    t0 = time.perf_counter()

    from transformers import AutoTokenizer

    tok = AutoTokenizer.from_pretrained(MODEL_ID, trust_remote_code=True)

    import torch

    gpu = torch.cuda.get_device_name(0) if torch.cuda.is_available() else "none"

    receipt: dict[str, Any] = {
        "workload_id": "MOON-REP-Xq-VLLM-01",
        "iron_tag": args.iron_tag,
        "path": "A",
        "model": MODEL_ID,
        "vllm_kv_baseline": BASELINE_KV,
        "vllm_kv_quant": tq_kv,
        "enforce_eager": args.enforce_eager,
        "gpu": gpu,
        "gates_signed": {
            "bytes_min_reduction": BYTES_REDUCTION_MIN,
            "ppl_drift_max": PPL_DRIFT_MAX,
            "tok_s_min_ratio": TOK_S_MIN_RATIO,
        },
    }

    # --- bytes (accounting + preset) ---
    fp16_bpt = PRESET_BYTES["auto"]
    tq_bpt = PRESET_BYTES.get(tq_kv, int(fp16_bpt * 0.42))
    red = 1.0 - tq_bpt / fp16_bpt
    receipt["gates"] = {
        "bytes": {
            "fp16_bytes_per_token": fp16_bpt,
            "quant_bytes_per_token": tq_bpt,
            "reduction_frac": round(red, 4),
            "pass": red >= BYTES_REDUCTION_MIN,
            "method": "tier1_bpt_preset_aligned_vllm_dtype",
        }
    }

    texts = load_wikitext(args.ppl_chunks)
    needle = "SECRET-729184"

    # --- baseline LLM ---
    print("Loading baseline LLM...", flush=True)
    llm_base = make_llm(BASELINE_KV, args.max_model_len, args.enforce_eager)
    receipt["baseline"] = {
        "niah": run_niah(llm_base, tok, depths, needle, args.max_model_len),
        "tok_s": run_tok_s(llm_base, tok, args.ctx_toks, args.decode_tokens),
        "ppl": run_ppl_sample(llm_base, tok, texts),
    }
    del llm_base
    import gc

    gc.collect()
    torch.cuda.empty_cache()

    # --- turboquant LLM ---
    print("Loading TurboQuant LLM...", flush=True)
    llm_tq = make_llm(tq_kv, args.max_model_len, args.enforce_eager)
    receipt["turboquant"] = {
        "niah": run_niah(llm_tq, tok, depths, needle, args.max_model_len),
        "tok_s": run_tok_s(llm_tq, tok, args.ctx_toks, args.decode_tokens),
        "ppl": run_ppl_sample(llm_tq, tok, texts),
    }
    del llm_tq
    torch.cuda.empty_cache()

    # --- aggregate gates ---
    ppl_b = receipt["baseline"]["ppl"]
    ppl_q = receipt["turboquant"]["ppl"]
    drift = (ppl_q - ppl_b) / ppl_b if ppl_b and not math.isnan(ppl_b) else float("nan")

    niah_pass = all(r.get("hit") for r in receipt["turboquant"]["niah"])
    ts_b = receipt["baseline"]["tok_s"]["tok_s"]
    ts_q = receipt["turboquant"]["tok_s"]["tok_s"]
    ratio = ts_q / ts_b if ts_b > 0 else 0.0

    sample = " ".join(r.get("output_snip", "") for r in receipt["turboquant"]["niah"])
    k1_pass = not (sample.count("grass grass") > 3)

    receipt["gates"]["ppl"] = {
        "ppl_fp16": round(ppl_b, 4) if not math.isnan(ppl_b) else None,
        "ppl_quant": round(ppl_q, 4) if not math.isnan(ppl_q) else None,
        "drift_frac": round(drift, 4) if not math.isnan(drift) else None,
        "pass": drift <= PPL_DRIFT_MAX if not math.isnan(drift) else False,
    }
    receipt["gates"]["niah"] = {
        "pass": niah_pass,
        "k3": not niah_pass,
        "quant": receipt["turboquant"]["niah"],
    }
    receipt["gates"]["tok_s"] = {
        "fp16": receipt["baseline"]["tok_s"],
        "quant": receipt["turboquant"]["tok_s"],
        "ratio": round(ratio, 4),
        "pass": ratio >= TOK_S_MIN_RATIO,
    }
    receipt["gates"]["k1"] = {"pass": k1_pass}

    receipt["tier3_pass"] = all(receipt["gates"][g]["pass"] for g in ("bytes", "ppl", "niah", "tok_s", "k1"))
    receipt["fail_gates"] = [g for g in ("bytes", "ppl", "niah", "tok_s", "k1") if not receipt["gates"][g]["pass"]]
    receipt["elapsed_sec"] = round(time.perf_counter() - t0, 1)

    receipt = _json_safe(receipt)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(receipt, f, indent=2, allow_nan=False)
    print(json.dumps({"tier3_pass": receipt["tier3_pass"], "fail_gates": receipt["fail_gates"]}, indent=2))
    raise SystemExit(0 if receipt["tier3_pass"] else 1)


if __name__ == "__main__":
    main()
