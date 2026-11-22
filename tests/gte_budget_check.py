#!/usr/bin/env python3
"""Check the GTE budget script (scripts/gte_budget.py) on synthetic inputs.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/gte_budget_check.py --scratch <dir>
The formula and the rows, the refusal of a census name without a weight and of a weight that is not a positive int,
then one run of the script with --json and --markdown on a TOML and a report.json written to the scratch folder."""
import argparse
import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "gte_budget.py"
spec = importlib.util.spec_from_file_location("gte_budget", SCRIPT)
gb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gb)

CFG = {"frame_ms": 33.3, "share": 0.25, "clock_mhz": 28.6364, "share_reason": "r",
       "commands": {"MVMVA": [8, "c"]}, "functions": {"f": [1, "m"], "g": [20, "prior"]}}
GTE = {"frames": 3, "commands": {"MVMVA": 10}, "functions": {"f": 100, "g": 2},
       "max_frames": {"MVMVA": 1, "f": 2, "g": 0}}

TOML = """frame_ms = 33.3
share = 0.25
clock_mhz = 28.6364
share_reason = "r"

[commands]
MVMVA = [8, "c"]

[functions]
f = [1, "m"]
g = [20, "prior"]
"""
REPORT = {"gte": {"frames": 3,
                  "commands": [{"command": "MVMVA", "max": 10, "max_frame": 1}],
                  "functions": [{"function": "f", "max": 100, "max_frame": 2},
                                {"function": "g", "max": 2, "max_frame": 0}]}}
SCRATCH = None


class BudgetTests(unittest.TestCase):
    def test_k_and_rows(self):
        b = gb.compute(CFG, GTE)
        units = 10 * 8 + 100 * 1 + 2 * 20
        k = 0.25 * 33.3e-3 * 28.6364e6 / units
        self.assertEqual(b["units"], units)
        self.assertAlmostEqual(b["k_cycles_per_unit"], round(k, 4))
        r = {x["name"]: x for x in b["rows"]}
        self.assertAlmostEqual(r["MVMVA"]["cycles"], round(8 * k, 1))
        self.assertAlmostEqual(sum(x["frame_cycles"] for x in b["rows"]), b["budget_cycles"], delta=3)

    def test_unweighted_census_name_refused(self):
        with self.assertRaises(ValueError):
            gb.compute(CFG, {**GTE, "functions": {**GTE["functions"], "h": 1}})
        with self.assertRaises(ValueError):
            gb.compute({**CFG, "functions": {"f": [0, "zero"], "g": [20, "p"]}}, GTE)

    def test_cli(self):
        config = SCRATCH / "budget.toml"
        census = SCRATCH / "report.json"
        out = SCRATCH / "budget.json"
        config.write_text(TOML, encoding="utf-8")
        census.write_text(json.dumps(REPORT), encoding="utf-8")
        out.unlink(missing_ok=True)
        run = subprocess.run([sys.executable, str(SCRIPT), "--config", str(config), "--census", str(census),
                              "--json", str(out), "--markdown"], capture_output=True, text=True, check=True)
        expected = gb.compute(CFG, GTE)
        self.assertEqual(json.loads(out.read_text(encoding="utf-8")), expected)
        self.assertEqual(run.stdout, gb.markdown(expected))
        lines = run.stdout.splitlines()
        self.assertEqual(len(lines), 4 + 3 + 1)
        self.assertTrue(lines[0].startswith(f"k = {expected['budget_cycles']} cycles / 220 units"))
        self.assertEqual(lines[-1].split(" | ")[-1], f"{expected['budget_cycles']} |")
        bad = subprocess.run([sys.executable, str(SCRIPT), "--config", str(config)], capture_output=True)
        self.assertNotEqual(bad.returncode, 0)


def main():
    global SCRATCH
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--scratch", required=True, help="a folder for the synthetic inputs and the output")
    a = ap.parse_args()
    SCRATCH = Path(a.scratch)
    SCRATCH.mkdir(parents=True, exist_ok=True)
    result = unittest.main(argv=[sys.argv[0]], exit=False, verbosity=1).result
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
