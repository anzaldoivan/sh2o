#!/usr/bin/env python3
"""The CI job "4.7 archive = game libgte": fetch the PsyQ 4.7 archive, then report its libgte against the game's.

Copyright (c) 2026 sh2o Contributors. MIT License.

fetch [--dir build/psyq47]: scripts/psyq_lnk.py fetch. On its exit code 77 the line
"4.7 archive = game libgte: skipped: <reason>" follows its lines (and goes to $GITHUB_STEP_SUMMARY when set).
report <zip> <junit.xml>...: one junit per build, written by ctest -L psyq47. Prints (and appends as Markdown to
$GITHUB_STEP_SUMMARY when set) the label with its caveat, the archive's stamp byte, one line per body of
tests/psyq_game_check.py BODIES (and SetDQA, SetDQB, outside the count), and the match count. A body is matched when
its digest equals the file's (tests/psyq47_digests.py) and every psyq47 test passed in every build: each model test
covers all bodies and stops at the first difference. Exit code 1 unless every test of every junit passed (none
failed, skipped or not run) and each junit has one.
Only names, counts, statuses, the stamp byte and the first 8 hex of digests are printed.
"""
import argparse
import contextlib
import io
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "tests"))
import psyq_lnk as pl  # noqa: E402
import psyq47_digests as dg  # noqa: E402
import psyq_game_check as gc  # noqa: E402

LABEL = "4.7 archive = game libgte"
CAVEAT = ("PSY-Q 4.7's libgte equals the game's 4.5 code body by body, apart from the version word (0x47 against "
          "the game's 0x45); 4 bodies (InitGeom, PopMatrix, PushMatrix, ratan2) are compared at masked relocation "
          "fields.")


def summary(lines):
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a") as f:
            f.write("\n".join(lines) + "\n\n")


def fetch(dirpath):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        rc = pl.fetch(dirpath)
    text = out.getvalue().splitlines()
    for line in text:
        print(line)
    if rc != pl.SKIP:
        return rc
    reason = text[-1].removeprefix("fetch: ") if text else "no reason given"
    line = f"{LABEL}: skipped: {reason}"
    print(line)
    summary([line])
    return pl.SKIP


def junit(path):
    """(build, {test: status}) of one ctest junit; status is passed, failed or not run."""
    tests = {}
    if not Path(path).is_file():
        return Path(path).parent.name, tests
    for case in ET.parse(path).getroot().iter("testcase"):
        name = case.get("name")
        if case.find("failure") is not None or case.get("status") == "fail":
            tests[name] = "failed"
        elif case.find("skipped") is not None or case.get("status") != "run":
            tests[name] = "not run"
        else:
            tests[name] = "passed"
    return Path(path).parent.name, tests


def report(zp, paths):
    runs = [junit(p) for p in paths]
    bad = [f"{b}/{t} {s}" for b, tests in runs for t, s in sorted(tests.items()) if s != "passed"]
    bad += [f"{b}: no psyq47 test" for b, tests in runs if not tests]
    passed = not bad and bool(runs)
    lib, rows = dg.routine_results(zp)
    stamp = pl.stamp(pl.module_of(pl.definers(lib), "InitGeom"))
    total = pl.text_total(lib)
    builds = " and ".join(b for b, _t in runs)
    body_lines, matched = [], 0
    for name, d, _want, how, _mod, _sites in rows:
        if name not in gc.BODIES:
            body_lines.append(f"{name}: digest {d[:8]} {how}; outside the count")
            continue
        ok = how != "differs" and passed
        matched += ok
        if passed:
            run = f"0 differences in both builds ({builds})"
        else:
            run = "not shown (" + ", ".join(bad) + ")"
        body_lines.append(f"{name}: digest {d[:8]} {how}; run {run}; matched {'yes' if ok else 'no'}")
    head = [f"{LABEL}", CAVEAT, f"stamp byte: {stamp:#04x}"]
    count = f"match count: {matched} of {total} libgte functions"
    for line in head + body_lines + [count]:
        print(line)
    summary([f"### {LABEL}", "", CAVEAT, "", f"Stamp byte: `{stamp:#04x}`", ""]
            + [f"- {line}" for line in body_lines] + ["", f"**{count}**"])
    if not passed:
        print(f"{LABEL}: FAIL: " + ", ".join(bad or ["no junit"]))
        return 1
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch")
    f.add_argument("--dir", default="build/psyq47")
    r = sub.add_parser("report")
    r.add_argument("zip")
    r.add_argument("junit", nargs="+")
    a = ap.parse_args(argv)
    if a.cmd == "fetch":
        return fetch(a.dir)
    return report(a.zip, a.junit)


if __name__ == "__main__":
    sys.exit(main())
