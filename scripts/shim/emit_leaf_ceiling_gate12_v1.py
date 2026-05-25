#!/usr/bin/env python3
"""Emit GATE12 leaf-physics block from N2 iso @ budget artifacts."""
from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

LEAF_BYTES = 1 << 21
SLICE_DEFAULT = 1 << 19
K_MAX_DEFAULT = 4
CEILING_LEAF_PCT = 100.0


def load_json(p: Path) -> dict:
    return json.loads(p.read_text(encoding="utf-8"))


def liberation(stock_c: int, shim_c: int) -> float:
    if stock_c <= 0:
        return 0.0
    return (stock_c - shim_c) / stock_c * 100.0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--art-dir", required=True)
    ap.add_argument("--env", required=True)
    ap.add_argument("--build-id", default="b_shim_20260525a")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    art = Path(args.art_dir)
    env = load_json(Path(args.env))
    points = env.get("vram_budget_points_pct", [50, 60, 70])
    slice_b = int(env.get("iso_slice_bytes", SLICE_DEFAULT))
    k_max = LEAF_BYTES // slice_b if slice_b else K_MAX_DEFAULT

    lines = [
        "HWATOM_EVAL v1",
        "By continuing execution you accept LICENSE.md (Evaluation-Only).",
        "HWATOM_EVAL_END",
        "GATE12_BEGIN",
        f"build_id={args.build_id}",
        "workload_id=t1_leaf_physics_v1",
        "workload_class=synthetic_kv_band",
        "stress_mode=t1_leaf_physics_v1",
        "cache_liberation_method=iso_logical_v2",
        "measurement_contract=IRON_LEAF_CEILING_SPEC_V1",
        f"leaf_bytes={LEAF_BYTES}",
        f"iso_slice_bytes={slice_b}",
        f"k_max_per_leaf={k_max}",
        f"eta_shim_ceiling_leaf_pct={CEILING_LEAF_PCT:.4f}",
        f"curve_point_count={len(points)}",
        f"run_id={int(time.time())}",
    ]
    if env.get("gpu_name"):
        lines.append(f"gpu_sku={env['gpu_name']}")
    if env.get("driver_version"):
        lines.append(f"driver_version={env['driver_version']}")

    ok_n = 0
    eta70 = 0.0
    gain70 = 0
    lib70 = 0.0
    for pct in points:
        sp = art / f"stock_{pct}.json"
        hp = art / f"shim_{pct}.json"
        if not sp.is_file() or not hp.is_file():
            lines.append(f"curve_point_{pct}_status=missing")
            continue
        st = load_json(sp)
        sh = load_json(hp)
        cs = int(st.get("committed_bytes", 0))
        ch = int(sh.get("committed_bytes", 0))
        rs = int(st.get("requested_bytes", 0))
        rh = int(sh.get("requested_bytes", 0))
        eff_h = float(sh.get("layout_efficiency_pct", 0.0))
        lib = liberation(cs, ch)
        gain = int(100.0 * (rh - rs) / rs) if rs else 0
        ratio = eff_h / CEILING_LEAF_PCT if CEILING_LEAF_PCT else 0.0
        ok_n += 1
        lines.extend(
            [
                f"vram_budget_pct_{pct}={pct}",
                f"resident_slots_stock_{pct}={st.get('resident_slots', 0)}",
                f"resident_slots_shim_{pct}={sh.get('resident_slots', 0)}",
                f"logical_kv_bytes_stock_{pct}={rs}",
                f"logical_kv_bytes_shim_{pct}={rh}",
                f"committed_bytes_stock_{pct}={cs}",
                f"committed_bytes_shim_{pct}={ch}",
                f"layout_efficiency_shim_{pct}={eff_h:.4f}",
                f"eta_leaf_ceiling_ratio_{pct}={ratio:.4f}",
                f"logical_kv_gain_pct_{pct}={gain}",
                f"cache_liberation_pct_{pct}={lib:.4f}",
                f"curve_point_{pct}_status=ok",
            ]
        )
        if pct == 70:
            eta70 = eff_h
            gain70 = gain
            lib70 = lib

    lines.append(f"curve_points_ok={ok_n}")
    pass_m3 = ok_n == len(points) and (eta70 / CEILING_LEAF_PCT) >= 0.70
    pass_m4 = gain70 >= 30
    lines.append(f"ceiling_ratio_at_70={eta70 / CEILING_LEAF_PCT:.4f}" if CEILING_LEAF_PCT else "ceiling_ratio_at_70=0")
    lines.append(f"N2_PASS_ceiling_ratio={'yes' if pass_m3 else 'no'}")
    lines.append(f"N2_PASS_logical_kv_gain_70={'yes' if pass_m4 else 'no'}")
    lines.append("GATE12_END")
    lines.append("HWATOM_EVAL_COMPLETE")
    lines.append(f"LEAF_PHYSICS_OK={'yes' if pass_m3 and pass_m4 and ok_n == len(points) else 'no'}")
    lines.append(f"LEAF_PHYSICS_METRICS liberation_70={lib70:.2f} gain_70={gain70}")

    out = Path(args.out)
    text = "\n".join(lines) + "\n"
    out.write_text(text, encoding="utf-8")
    print(text)
    return 0 if pass_m3 and pass_m4 and ok_n == len(points) else 2


if __name__ == "__main__":
    raise SystemExit(main())
