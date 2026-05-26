#!/usr/bin/env python3
"""Parse vLLM logs for cuMem reserve sizes + SHIM_STATS (B unlock probes)."""
from __future__ import annotations

import argparse
import re
import sys

RESERVE_RE = re.compile(
    r"SHIM_TRACE held cuMemAddressReserve size=(\d+)", re.MULTILINE
)
STATS_V2_RE = re.compile(
    r"SHIM_STATS stats_v=2 build_id=(\S+) pack_committed_bytes=(\d+) "
    r"pack_committed_peak_bytes=(\d+) pack_committed_fini_bytes=(\d+) "
    r"mega_reserve_count=(\d+) mega_leaves_used=(\d+)"
)
STATS_FULL_RE = re.compile(
    r"SHIM_STATS pack_committed_bytes=(\d+) mega_reserve_count=(\d+) mega_leaves_used=(\d+)"
)
STATS_RE = re.compile(r"SHIM_STATS pack_committed_bytes=(\d+)")


def parse_log(path: str) -> dict:
    text = open(path, encoding="utf-8", errors="replace").read()
    sizes = [int(m.group(1)) for m in RESERVE_RE.finditer(text)]
    v2 = STATS_V2_RE.findall(text)
    if v2:
        best = max(v2, key=lambda t: int(t[1]))
        pack = int(best[1])
        mega_rc = int(best[4])
        mega_lv = int(best[5])
        build_id = best[0]
    else:
        build_id = ""
    full = STATS_FULL_RE.findall(text)
    if not v2 and full:
        best = max(full, key=lambda t: int(t[0]))
        pack = int(best[0])
        mega_rc = int(best[1])
        mega_lv = int(best[2])
    elif not v2:
        stats = [int(x) for x in STATS_RE.findall(text)]
        pack = max(stats) if stats else 0
        mega_rc = 0
        mega_lv = 0
    out = {
        "n_reserve_trace": len(sizes),
        "reserve_bytes_sum": sum(sizes),
        "reserve_bytes_max": max(sizes) if sizes else 0,
        "shim_pack_committed_bytes": pack,
        "mega_reserve_count": mega_rc,
        "mega_leaves_used": mega_lv,
    }
    if build_id:
        out["shim_build_id"] = build_id
        out["stats_v"] = 2
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("logfile")
    args = ap.parse_args()
    import json

    print(json.dumps(parse_log(args.logfile), indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
