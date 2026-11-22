#!/usr/bin/env python3
"""Check that each pinned submodule points to its pinned commit.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/check_pin.py [--self-test]

PINS gives each submodule and its pin file: external/SH2ZAM with
scripts/sh2zam_pin.txt, external/psyz with scripts/psyz_pin.txt. The pin is
the first non-comment line of the pin file. The script compares it with the
gitlink in HEAD, the gitlink in the index and, when the submodule is checked
out, the commit of the checkout. It exits 1 on a difference. --self-test
gives each submodule a wrong pin and exits 1 if a check does not fail.
"""
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PINS = (("external/SH2ZAM", "scripts/sh2zam_pin.txt"), ("external/psyz", "scripts/psyz_pin.txt"))


def git(*args, cwd=REPO):
    result = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else ""


def check(path, pin, date):
    """1 when HEAD, the index or the checkout of path differs from pin, else 0."""
    found = {
        "HEAD": git("ls-tree", "HEAD", path).split()[2:3],
        "index": git("ls-files", "--stage", path).split()[1:2],
    }
    checkout = os.path.join(REPO, path)
    if os.path.exists(os.path.join(checkout, ".git")):
        found["checkout"] = [git("rev-parse", "HEAD", cwd=checkout)]
    bad = 0
    for where, value in found.items():
        commit = value[0] if value else "(none)"
        if commit != pin:
            print("check_pin: %s has %s %s, the pin is %s (%s)" % (where, path, commit, pin, date))
            bad += 1
    print("check_pin: %s at %s (pinned %s): %s" % (path, pin[:8], date, "FAIL" if bad else "ok"))
    return 1 if bad else 0


def main(argv):
    bad = 0
    for path, pin_file in PINS:
        with open(os.path.join(REPO, pin_file), encoding="ascii") as handle:
            pin, date = next(line.split() for line in handle if line.strip() and not line.startswith("#"))
        if "--self-test" in argv[1:]:
            print("check_pin: self-test, a wrong pin for %s must FAIL" % path)
            if check(path, "0" * 40, "planted") != 1:
                print("check_pin: self-test, the wrong pin for %s was not caught" % path)
                bad += 1
        else:
            bad += check(path, pin, date)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
