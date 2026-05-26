#!/usr/bin/env python3
"""MOON-representation Tier 1 — KV byte bounds + cos proxy (no GPU).

Hypothesis H0: >=30% KV bytes/token reduction vs FP16 @ long decode, with
Tier-2 cos floor 0.940 plausible (conservative literature proxies, not iron).

Outputs JSON receipt to stdout; optional --out path.
"""
from __future__ import annotations

import argparse
import json
import time
from dataclasses import asdict, dataclass
from typing import Any

# EOD-signed gates
BYTES_REDUCTION_MIN = 0.30
COS_PROXY_MIN = 0.940

MODELS: dict[str, dict[str, int | float]] = {
    "Qwen/Qwen2.5-7B-Instruct": {
        "num_layers": 28,
        "num_kv_heads": 4,
        "head_dim": 128,
    },
    "deepseek-ai/DeepSeek-V2-Lite": {
        # Tier-1 MLA bound: published ~5.7% KV vs standard (≈94% reduction) — use conservative 70% for Xm compare
        "num_layers": 27,
        "num_kv_heads": 16,
        "head_dim": 128,
        "mla_kv_factor": 0.25,  # conservative vs FP16 full K+V (not headline 94%)
    },
}


@dataclass
class Profile:
    id: str
    path: str  # Xq | Xm
    k_bits: float
    v_bits: float
    layers_fp16: int
    cos_proxy: float
    cos_note: str


def fp16_bytes_per_token(layers: int, kv_heads: int, head_dim: int) -> int:
    per_layer = 2 * kv_heads * head_dim * 2  # K+V, FP16
    return layers * per_layer


def quant_bytes_per_token(
    layers: int,
    kv_heads: int,
    head_dim: int,
    k_bits: float,
    v_bits: float,
    layers_fp16: int,
) -> int:
    q_layers = layers - layers_fp16
    fp_layers = layers_fp16
    per_layer_q = kv_heads * head_dim * ((k_bits + v_bits) / 8.0)
    per_layer_fp = 2 * kv_heads * head_dim * 2
    return int(q_layers * per_layer_q + fp_layers * per_layer_fp)


def reduction(baseline: int, compressed: int) -> float:
    if baseline <= 0:
        return 0.0
    return 1.0 - (compressed / baseline)


def eval_profiles(model_key: str) -> dict[str, Any]:
    cfg = MODELS[model_key]
    L = int(cfg["num_layers"])
    nh = int(cfg["num_kv_heads"])
    hd = int(cfg["head_dim"])
    baseline = fp16_bytes_per_token(L, nh, hd)

    profiles: list[Profile] = [
        Profile(
            id="xq_k4v8_all",
            path="Xq",
            k_bits=4,
            v_bits=8,
            layers_fp16=0,
            cos_proxy=0.965,
            cos_note="TurboQuant-class K4V8 + rotation (conservative lit lower bound)",
        ),
        Profile(
            id="xq_k4v8_boundary2",
            path="Xq",
            k_bits=4,
            v_bits=8,
            layers_fp16=2,
            cos_proxy=0.968,
            cos_note="2 boundary layers FP16 (PDF boundary-layer pattern)",
        ),
        Profile(
            id="xq_qjl_k1v8",
            path="Xq",
            k_bits=1,
            v_bits=8,
            layers_fp16=2,
            cos_proxy=0.945,
            cos_note="QJL 1-bit keys + 8-bit V; rotation required — tight vs 0.940",
        ),
        Profile(
            id="xq_k4v4_aggressive",
            path="Xq",
            k_bits=4,
            v_bits=4,
            layers_fp16=0,
            cos_proxy=0.928,
            cos_note="Aggressive — likely FAIL Tier 2 cos; included as falsifier",
        ),
    ]

    rows = []
    for p in profiles:
        comp = quant_bytes_per_token(L, nh, hd, p.k_bits, p.v_bits, p.layers_fp16)
        red = reduction(baseline, comp)
        h0_ok = red >= BYTES_REDUCTION_MIN
        cos_ok = p.cos_proxy >= COS_PROXY_MIN
        tier1_profile_pass = h0_ok and cos_ok
        rows.append(
            {
                **asdict(p),
                "bytes_per_token_fp16": baseline,
                "bytes_per_token_profile": comp,
                "reduction_frac": round(red, 4),
                "h0_bytes_pass": h0_ok,
                "cos_proxy_pass": cos_ok,
                "tier1_profile_pass": tier1_profile_pass,
            }
        )

    mla_factor = cfg.get("mla_kv_factor")
    mla_row = None
    if mla_factor is not None:
        mla_bytes = int(baseline * float(mla_factor))
        mla_red = reduction(baseline, mla_bytes)
        mla_row = {
            "id": "xm_mla_conservative",
            "path": "Xm",
            "mla_kv_factor": mla_factor,
            "bytes_per_token_fp16": baseline,
            "bytes_per_token_mla": mla_bytes,
            "reduction_frac": round(mla_red, 4),
            "h0_bytes_pass": mla_red >= BYTES_REDUCTION_MIN,
            "cos_proxy": 0.955,
            "cos_note": "MLA latent — structural; iron cos TBD on DeepSeek stack",
            "tier1_profile_pass": mla_red >= BYTES_REDUCTION_MIN,
        }

    passing_xq = [r for r in rows if r["tier1_profile_pass"]]
    best_xq = max(rows, key=lambda r: (r["tier1_profile_pass"], r["reduction_frac"]))

    path_x = "UNDECIDED"
    path_reason = ""
    if passing_xq:
        path_x = "MOON-Xq"
        path_reason = f"{len(passing_xq)} quant profile(s) meet H0+ cos proxy; best={best_xq['id']}"
    elif mla_row and mla_row.get("tier1_profile_pass"):
        path_x = "MOON-Xm"
        path_reason = "quant profiles fail cos or bytes; MLA conservative bound passes H0"
    else:
        path_x = "MOON-X?"
        path_reason = "no profile passes Tier1 bounds — PARK or new path"

    if mla_row and mla_row["tier1_profile_pass"] and passing_xq:
        path_x = "MOON-Xq"
        path_reason += f"; Xm also viable (mla red={mla_row['reduction_frac']}) — default Xq for iron stack"

    return {
        "model": model_key,
        "baseline_bytes_per_token": baseline,
        "ctx_32k_kv_gib_fp16": round(baseline * 32768 / (1024**3), 3),
        "profiles_xq": rows,
        "profile_xm": mla_row,
        "path_x_recommendation": path_x,
        "path_x_reason": path_reason,
        "tier1_pass": bool(passing_xq or (mla_row and mla_row.get("tier1_profile_pass"))),
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=str, default="", help="write JSON receipt path")
    ap.add_argument("--model", type=str, default="Qwen/Qwen2.5-7B-Instruct")
    args = ap.parse_args()

    t0 = time.perf_counter()
    primary = eval_profiles(args.model)
    secondary = eval_profiles("deepseek-ai/DeepSeek-V2-Lite")

    receipt: dict[str, Any] = {
        "workload_id": "MOON-REP-TIER1-20260526",
        "layer": "MOON-rep",
        "numerator": "kv_bytes_per_token",
        "gates_reference": {
            "bytes_reduction_min": BYTES_REDUCTION_MIN,
            "cos_proxy_min": COS_PROXY_MIN,
        },
        "elapsed_sec": round(time.perf_counter() - t0, 4),
        "primary_model": primary,
        "xm_compare": secondary,
        "tier1_pass": primary["tier1_pass"],
        "path_x_sign_proposal": primary["path_x_recommendation"],
        "path_x_reason": primary["path_x_reason"],
        "tabu": "GPU not used; cos_proxy is not Tier2 iron",
        "next": "Tier2 cos on layer if tier1_pass else PARK representation sprint",
    }

    text = json.dumps(receipt, indent=2)
    print(text)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(text)


if __name__ == "__main__":
    main()
