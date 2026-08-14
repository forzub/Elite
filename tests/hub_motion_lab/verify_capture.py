#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path


def as_float(row, key):
    try:
        return float(row[key])
    except (KeyError, TypeError, ValueError):
        raise ValueError(f"missing/invalid column {key!r}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify an end-to-end Hub Motion Lab presentation CSV capture."
    )
    parser.add_argument("csv", type=Path)
    parser.add_argument("--warmup-rows", type=int, default=120)
    parser.add_argument("--max-fast-error-m", type=float, default=0.005)
    parser.add_argument("--max-slow-error-m", type=float, default=0.001)
    parser.add_argument("--max-match-delayed-error-m", type=float, default=0.001)
    parser.add_argument("--max-clamped-oldest-fraction", type=float, default=0.001)
    parser.add_argument("--max-clamped-newest-fraction", type=float, default=0.001)
    parser.add_argument("--min-bracket-fraction", type=float, default=0.999)
    args = parser.parse_args()

    with args.csv.open(newline="", encoding="utf-8-sig") as f:
        rows = list(csv.DictReader(f))

    if len(rows) <= args.warmup_rows + 100:
        print("[FAIL] capture is too short", file=sys.stderr)
        return 1

    rows = rows[args.warmup_rows:]
    n = len(rows)

    try:
        clamped_oldest = sum(as_float(r, "clamped_oldest") != 0.0 for r in rows)
        clamped_newest = sum(as_float(r, "clamped_newest") != 0.0 for r in rows)
        bracket = sum(as_float(r, "has_bracket") != 0.0 for r in rows)
        fast_max = max(abs(as_float(r, "fast_error_m")) for r in rows)
        slow_max = max(abs(as_float(r, "slow_error_m")) for r in rows)
        match_max = max(
            abs(as_float(r, "match_vs_delayed_player_error_m")) for r in rows
        )
        alpha = [as_float(r, "alpha") for r in rows]
        remainder = [as_float(r, "player_prediction_remainder_ms") for r in rows]
    except ValueError as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        return 1

    failures = []
    oldest_clamp_fraction = clamped_oldest / n
    newest_clamp_fraction = clamped_newest / n
    bracket_fraction = bracket / n

    if oldest_clamp_fraction > args.max_clamped_oldest_fraction:
        failures.append(
            f"clamped_oldest fraction {oldest_clamp_fraction:.6f} > "
            f"{args.max_clamped_oldest_fraction:.6f}"
        )
    if newest_clamp_fraction > args.max_clamped_newest_fraction:
        failures.append(
            f"clamped_newest fraction {newest_clamp_fraction:.6f} > "
            f"{args.max_clamped_newest_fraction:.6f}"
        )
    if bracket_fraction < args.min_bracket_fraction:
        failures.append(
            f"bracket fraction {bracket_fraction:.6f} < {args.min_bracket_fraction:.6f}"
        )
    if fast_max > args.max_fast_error_m:
        failures.append(f"FAST max error {fast_max:.9f} m")
    if slow_max > args.max_slow_error_m:
        failures.append(f"SLOW max error {slow_max:.9f} m")
    if match_max > args.max_match_delayed_error_m:
        failures.append(f"MATCH delayed max error {match_max:.9f} m")
    if min(alpha) > 0.10 or max(alpha) < 0.90:
        failures.append(
            f"interpolation alpha does not traverse bracket: "
            f"min={min(alpha):.6f} max={max(alpha):.6f}"
        )
    if min(remainder) < -1.0e-6 or max(remainder) > 20.01:
        failures.append(
            f"fractional prediction remainder outside [0,20] ms: "
            f"min={min(remainder):.6f} max={max(remainder):.6f}"
        )

    print(f"rows={n}")
    print(f"clamped_oldest={clamped_oldest} ({oldest_clamp_fraction:.6%})")
    print(f"clamped_newest={clamped_newest} ({newest_clamp_fraction:.6%})")
    print(f"bracket={bracket} ({bracket_fraction:.6%})")
    print(f"alpha=[{min(alpha):.6f}, {max(alpha):.6f}]")
    print(f"slow_max_error_m={slow_max:.9f}")
    print(f"fast_max_error_m={fast_max:.9f}")
    print(f"match_delayed_max_error_m={match_max:.9f}")
    print(f"prediction_remainder_ms=[{min(remainder):.6f}, {max(remainder):.6f}]")

    if failures:
        print("[FAIL] Hub Motion Lab capture regression", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print("[PASS] Hub Motion Lab capture regression")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
