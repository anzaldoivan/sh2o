#!/usr/bin/env python3
"""Check of the private input variables, as a script (CMake COMMAND form).

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage:
  python3 tests/private_inputs.py <source folder>
  python3 tests/private_inputs.py --ctest-self-test <ctest> <build folder> <test name> <source folder>

The first form does what tests/private_inputs.c does. A path in the
repository prints an error and gives exit code 1. A variable that is not set,
or a path that is not there, prints why and gives exit code 77 (CTest shows
the test as skipped). Exit code 0 when each input is there.

The second form runs CTest on the test <test name> three times: with no
variable set, with a path that is not there, and with a path in the
repository. CTest must show Skipped, Skipped and Failed.
"""
import os
import subprocess
import sys

SKIP, FAIL = 77, 1
# The variables, in the order of docs/EXACTNESS.md.
VARIABLES = ("SH2O_PS1_LIBGTE_OBJECTS", "SH2O_PS1_LINK_MAP", "SH2O_PS1_DATA", "SH2O_PS1_GTE_RECORDINGS")


def below(path, root):
    """True if path is root or a path below root."""
    return path == root or path.startswith(root.rstrip("/") + "/")


def check(variable, root):
    """0 when the input is there, SKIP or FAIL otherwise; each result prints a line."""
    value = os.environ.get(variable, "")
    if not value:
        print("skipped: %s is not set" % variable)
        return SKIP
    real_root = os.path.realpath(root)
    if not os.path.exists(value):
        full = os.path.abspath(value)
        if below(full, real_root) or below(full, os.path.abspath(root)):
            print("error: %s=%s is in the repository %s; keep private inputs outside it" % (variable, value, real_root))
            return FAIL
        print("skipped: %s=%s is not there" % (variable, value))
        return SKIP
    if below(os.path.realpath(value), real_root):
        print("error: %s=%s is in the repository %s; keep private inputs outside it" % (variable, value, real_root))
        return FAIL
    print("found: %s=%s" % (variable, value))
    return 0


def run_check(root):
    result = 0
    for variable in VARIABLES:
        status = check(variable, root)
        if status == FAIL:
            result = FAIL
        elif status == SKIP and result == 0:
            result = SKIP
    return result


def self_test(ctest, build, name, root):
    absent = os.path.join(os.path.dirname(os.path.realpath(root)), "sh2o-private-input-that-is-not-there")
    if os.path.exists(absent):
        print("error: %s must not exist for this test" % absent)
        return FAIL
    cases = (("no variable set", None, "Skipped", "skipped: SH2O_PS1_GTE_RECORDINGS is not set"),
             ("a path that is not there", absent, "Skipped", "is not there"),
             ("a path in the repository", os.path.join(root, "tests"), "Failed", "is in the repository"))
    result = 0
    for label, value, status, line in cases:
        env = {key: text for key, text in os.environ.items() if key not in VARIABLES}
        if value is not None:
            env["SH2O_PS1_GTE_RECORDINGS"] = value
        output = subprocess.run([ctest, "--test-dir", build, "-V", "-R", "^%s$" % name], env=env,
                                capture_output=True, text=True).stdout
        found = [text for text in output.splitlines() if "Test #" in text and name in text]
        ok = len(found) == 1 and ("***" + status) in found[0] and line in output
        print("%s: %s: %s" % ("ok" if ok else "FAIL", label, found[0].strip() if found else "no result line"))
        if not ok:
            print(output)
            result = FAIL
    return result


def main(argv):
    if len(argv) == 2:
        return run_check(argv[1])
    if len(argv) == 6 and argv[1] == "--ctest-self-test":
        return self_test(*argv[2:])
    print(__doc__)
    return FAIL


if __name__ == "__main__":
    sys.exit(main(sys.argv))
