#!/usr/bin/env python3
"""Iron H-B — FA2 parity falsifier for tok/s gap (MOON-REP-HB-01).

Falsifier:
  H-B: If baseline is forced to FA2 (same as TurboQuant), tok/s ratio TQ/baseline
  rises toward >=0.97 → gap was FA downgrade, not KV quant.
  If ratio stays ~0.83-0.88 → real quant/decode cost on this bench.

Arms (same bench as Path A v2):
  1. kv=auto, FA default (reference)
  2. kv=auto, flash_attn_version=2 (parity)
  3. kv=turboquant_4bit_nc (TQ, FA2 forced by vLLM)
"""
from __future__ import annotations

import argparse
import gc
import json
import os
import time
from typing import Any

os.environ.setdefault("VLLM_WORKER_MULTIPROC_METHOD", "spawn")

MODEL_ID = "Qwen/Qwen2.5-7B-Instruct"
TOK_S_GATE = 0.97


def run_tok_s(llm: Any, tok: Any, ctx_target: int, decode_n: int) -> dict:
    from vllm import SamplingParams

    filler = "Word "
    prompt = (filler * (ctx_target // 2))[: ctx_target * 3]
    enc = tok.encode(prompt, truncation=True, max_length=ctx_target)
    prompt = tok.decode(enc)
    sp = SamplingParams(max_tokens=decode_n, temperature=0.0)
    _ = llm.generate([prompt], sp)
    t0 = time.perf_counter()
    _ = llm.generate([prompt], sp)
    elapsed = time.perf_counter() - t0
    return {
        "ctx_tokens": len(enc),
        "decode_tokens": decode_n,
        "elapsed_s": round(elapsed, 3),
        "tok_s": round(decode_n / elapsed, 2) if elapsed > 0 else 0.0,
    }


def make_llm(
    kv_dtype: str,
    max_model_len: int,
    flash_attn_version: int | None,
) -> Any:
    from vllm import LLM
    from vllm.config import AttentionConfig

    kwargs: dict[str, Any] = {
        "model": MODEL_ID,
        "kv_cache_dtype": kv_dtype,
        "max_model_len": max_model_len,
        "enforce_eager": False,
        "gpu_memory_utilization": 0.90,
        "trust_remote_code": True,
    }
    if flash_attn_version is not None:
        kwargs["attention_config"] = AttentionConfig(flash_attn_version=flash_attn_version)
    return LLM(**kwargs)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--ctx-toks", type=int, default=8192)
    ap.add_argument("--decode-tokens", type=int, default=48)
    ap.add_argument("--max-model-len", type=int, default=12288)
    args = ap.parse_args()

    import torch
    from transformers import AutoTokenizer

    tok = AutoTokenizer.from_pretrained(MODEL_ID, trust_remote_code=True)
    gpu = torch.cuda.get_device_name(0) if torch.cuda.is_available() else "none"
    t0 = time.perf_counter()

    arms = [
        {
            "arm": "baseline_fa_default",
            "kv_cache_dtype": "auto",
            "flash_attn_version": None,
        },
        {
            "arm": "baseline_fa2_parity",
            "kv_cache_dtype": "auto",
            "flash_attn_version": 2,
        },
        {
            "arm": "turboquant_4bit_nc",
            "kv_cache_dtype": "turboquant_4bit_nc",
            "flash_attn_version": None,
        },
    ]

    results: list[dict[str, Any]] = []
    for spec in arms:
        print(f"Loading {spec['arm']}...", flush=True)
        llm = make_llm(
            spec["kv_cache_dtype"],
            args.max_model_len,
            spec["flash_attn_version"],
        )
        row = {
            **spec,
            "tok_s": run_tok_s(llm, tok, args.ctx_toks, args.decode_tokens),
        }
        results.append(row)
        del llm
        gc.collect()
        torch.cuda.empty_cache()

    by_name = {r["arm"]: r for r in results}
    fa_def = by_name["baseline_fa_default"]["tok_s"]["tok_s"]
    fa2 = by_name["baseline_fa2_parity"]["tok_s"]["tok_s"]
    tq = by_name["turboquant_4bit_nc"]["tok_s"]["tok_s"]

    ratio_tq_vs_fa_def = tq / fa_def if fa_def > 0 else 0.0
    ratio_tq_vs_fa2 = tq / fa2 if fa2 > 0 else 0.0
    ratio_fa2_vs_fa_def = fa2 / fa_def if fa_def > 0 else 0.0

    # Path A v2 reference
    path_a_v2_ratio = 0.8346

    if ratio_tq_vs_fa2 >= TOK_S_GATE:
        hb_verdict = "H_B_FALSIFIED_FA_ONLY"
        hb_meaning = "Gap collapses at FA2 parity — tok/s gate failure was mostly FA downgrade."
    elif abs(ratio_tq_vs_fa2 - path_a_v2_ratio) < 0.05:
        hb_verdict = "H_B_CONFIRMED_QUANT_COST"
        hb_meaning = "Gap unchanged at FA2 parity — ~12-17% is real TQ/KV cost on this bench."
    else:
        hb_verdict = "H_B_PARTIAL"
        hb_meaning = "FA2 parity moves ratio but does not fully explain gap — mixed cause."

    receipt: dict[str, Any] = {
        "workload_id": "MOON-REP-HB-01",
        "investigate": "H-B FA2 parity tok/s",
        "falsifier": {
            "hypothesis": "tok/s gap is FA2 downgrade not KV quant",
            "kill_if": f"ratio_tq_vs_fa2_baseline >= {TOK_S_GATE}",
            "confirm_if": "ratio_tq_vs_fa2 within 5pp of path_a_v2 (~0.835)",
        },
        "model": MODEL_ID,
        "gpu": gpu,
        "bench": {
            "ctx_toks": args.ctx_toks,
            "decode_tokens": args.decode_tokens,
            "max_model_len": args.max_model_len,
            "enforce_eager": False,
        },
        "arms": results,
        "analysis": {
            "tok_s_baseline_fa_default": fa_def,
            "tok_s_baseline_fa2": fa2,
            "tok_s_turboquant_4bit_nc": tq,
            "ratio_tq_vs_fa_default": round(ratio_tq_vs_fa_def, 4),
            "ratio_tq_vs_fa2_parity": round(ratio_tq_vs_fa2, 4),
            "ratio_fa2_vs_fa_default": round(ratio_fa2_vs_fa_def, 4),
            "path_a_v2_ratio_reference": path_a_v2_ratio,
            "hb_verdict": hb_verdict,
            "hb_meaning": hb_meaning,
            "tok_s_gate_pass_at_fa2_parity": ratio_tq_vs_fa2 >= TOK_S_GATE,
        },
        "elapsed_sec": round(time.perf_counter() - t0, 1),
    }

    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(receipt, f, indent=2)
    print(json.dumps(receipt["analysis"], indent=2))
    raise SystemExit(0)


if __name__ == "__main__":
    main()
