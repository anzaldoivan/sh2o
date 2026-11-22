#!/usr/bin/env python3
"""SH-2 GTE budget: a config's weights and share over a census report's `gte` -> each name's per-call SH-2 budget.

Copyright (c) 2026 sh2o Contributors. MIT License.

The config (TOML) gives frame_ms, share, share_reason, clock_mhz and a [weight, basis] pair for each command and
function. The census (JSON) gives, under `gte`, the frame count and the largest count per frame of each name.

  k = share x frame_ms x clock / sum(max x weight)      budget(name) = k x weight

Largest frame = the composite of every name at its own `max` (an upper bound of any real frame); totals are sums of
inclusive times (a function and the commands or functions it calls are each counted: nesting overcounts).

usage: gte_budget.py --config TOML --census REPORT [--json OUT] [--markdown]
Library: load(config, census) -> dict (the same as --json)."""
import argparse
import json
import sys
import tomllib
from pathlib import Path


def load_config(path):
    return tomllib.loads(Path(path).read_text(encoding="utf-8"))


def census_gte(path):
    """report.json `gte` -> {commands: {name: max}, functions: {name: max}, frames, max_frames: {name: frame}}."""
    g = json.loads(Path(path).read_text(encoding="utf-8"))["gte"]
    return {"frames": g["frames"],
            "commands": {c["command"]: c["max"] for c in g["commands"]},
            "functions": {f["function"]: f["max"] for f in g["functions"]},
            "max_frames": {**{c["command"]: c["max_frame"] for c in g["commands"]},
                           **{f["function"]: f["max_frame"] for f in g["functions"]}}}


def compute(cfg, gte):
    """The budget dict; a census name without a weight (or a weight without a census name) is an error."""
    errors = []
    rows = []
    for kind in ("commands", "functions"):
        w, counts = cfg[kind], gte[kind]
        errors += [f"{kind}: {n} counted by the census, no weight" for n in sorted(set(counts) - set(w))]
        errors += [f"{kind}: {n} weighted, not in the census" for n in sorted(set(w) - set(counts))]
        for n in sorted(set(w) & set(counts)):
            weight, basis = w[n]
            if not isinstance(weight, int) or weight <= 0:
                errors.append(f"{kind}: {n} weight {weight!r} not a positive int")
                continue
            rows.append({"name": n, "kind": kind[:-1], "weight": weight, "basis": basis, "max": counts[n],
                         "max_frame": gte["max_frames"].get(n)})
    if errors:
        raise ValueError("; ".join(errors))
    units = sum(r["max"] * r["weight"] for r in rows)
    clock = cfg["clock_mhz"] * 1e6
    frame_cycles = cfg["share"] * cfg["frame_ms"] * 1e-3 * clock
    k = frame_cycles / units
    for r in rows:
        r["cycles"] = round(k * r["weight"], 1)
        r["us"] = round(k * r["weight"] / clock * 1e6, 3)
        r["frame_cycles"] = round(k * r["weight"] * r["max"])
    rows.sort(key=lambda r: (-r["frame_cycles"], r["kind"], r["name"]))
    return {"share": cfg["share"], "frame_ms": cfg["frame_ms"], "clock_mhz": cfg["clock_mhz"],
            "budget_ms": round(cfg["share"] * cfg["frame_ms"], 3), "budget_cycles": round(frame_cycles),
            "units": units, "k_cycles_per_unit": round(k, 4), "frames": gte["frames"],
            "largest_frame": "composite: every name at its own max (upper bound of any real frame)",
            "share_reason": cfg["share_reason"], "rows": rows}


def load(config, census):
    return compute(load_config(config), census_gte(census))


def markdown(b):
    out = [f"k = {b['budget_cycles']} cycles / {b['units']} units = {b['k_cycles_per_unit']} cycles per unit "
           f"({b['share']} x {b['frame_ms']} ms x {b['clock_mhz']} MHz; {b['frames']} census frames).", "",
           "| name | kind | weight | max/frame | budget cycles/call | us/call | frame cycles |",
           "|---|---|---|---|---|---|---|"]
    for r in b["rows"]:
        out.append(f"| {r['name']} | {r['kind']} | {r['weight']} | {r['max']} | {r['cycles']} | {r['us']} | "
                   f"{r['frame_cycles']} |")
    out.append(f"| total | | | | | {round(b['budget_cycles'] / (b['clock_mhz']), 1)} | {b['budget_cycles']} |")
    return "\n".join(out) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--config", required=True, help="the weights (TOML)")
    ap.add_argument("--census", required=True, help="the census report (JSON with a `gte` object)")
    ap.add_argument("--json", help="write the budget json here")
    ap.add_argument("--markdown", action="store_true", help="print the doc table")
    a = ap.parse_args(argv)
    b = load(a.config, a.census)
    if a.json:
        Path(a.json).write_text(json.dumps(b, indent=1) + "\n", encoding="utf-8")
    if a.markdown:
        sys.stdout.write(markdown(b))
    else:
        print(f"budget {b['budget_ms']} ms = {b['budget_cycles']} cycles; units {b['units']}; "
              f"k {b['k_cycles_per_unit']} cycles/unit; {len(b['rows'])} names")
    return 0


if __name__ == "__main__":
    sys.exit(main())
