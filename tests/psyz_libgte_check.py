#!/usr/bin/env python3
"""Run the psyz libgte test against sh2o.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/psyz_libgte_check.py <cc> <source dir> <work dir> [<psyz dir>]

CTest gives the arguments (CMakeLists.txt, test psyz_libgte). The psyz dir is
<source dir>/external/psyz when not given. The script compiles the psyz
test_libgte.c and ztest.c in place, unchanged (-std=gnu11 -w -D__psyz, psyz
headers with -isystem), then links them with tests/psyz/sh2o_libgte_shim.c and
each src/ C file (-std=c11 -Wall -Wextra -Werror). It runs the program with
--verbose and reads the result of each case: PASS is pass, FAIL is differs,
SKIP is notrun with the name after "sh2o lacks". The test file must have
CASES ZTEST cases. The case list in docs/PSYQ.md (the fenced block after the
heading "## The psyz libgte test") must give the same class for each case and
the same name for each notrun case, and the class counts must be COUNTS.
The GTE model (scripts/gte_model.py) on the input state of the differs case
must give sh2o's FLAG, not psyz's. Each mutant of the gte and psyq families of
tests/mutant.py is built (sh2o side only, the psyz objects again) and run: the
mutants that make a running case change class must be CAUGHT, the others
NOT_REACHED. Each shim plant (PLANTS, in a copy of tests/psyz/) must make a
running case change class. The work dir is removed at the end.
Exit code 0: all hold; 77 when the psyz checkout is not there, but 1 when
SH2O_REQUIRE_PSYZ is 1.
"""
import glob
import os
import re
import shutil
import subprocess
import sys

CASES = 225
COUNTS = {"pass": 116, "differs": 1, "notrun": 108}
HEADING = "## The psyz libgte test"
RESULT = re.compile(r"^gte::(\S+) (PASS|FAIL|SKIP)\n((?:   .*\n)*)", re.M)
CLASS = {"PASS": "pass", "FAIL": "differs", "SKIP": "notrun"}
# The mutants of tests/mutant.py (families gte and psyq) that make a running case fail; NOT_REACHED gives
# for each other mutant the code that no running case reaches.
CAUGHT = {"shift_11", "shift_13", "unr_index_off_by_one", "dropped_sxy_push", "dropped_sz_push",
          "dropped_colour_push", "back_colour_shift", "rot_matrix_swapped", "transpose_dropped_store",
          "rot_trans_no_sf", "apply_matrix_swapped", "apply_matrix_lv_sign", "comp_matrix_sign",
          "colour_matrix_base", "far_colour_shift", "geom_offset_shift", "geom_screen_plus_one",
          "light_matrix_base", "trans_matrix_swapped", "trans_matrix_store"}
NOT_REACHED = {
    "clamp_off_by_one": "RTPS/RTPT SX2/SY2 saturation: the cases that saturate the screen (rtps_screen_saturation, "
                        "rot_trans_pers*_set_geom_offset) are notrun (RotTransPers, RotTransPers3)",
    "swapped_flag_bit": "RTPS/RTPT SX2/SY2 saturation flags: the one case that reads them (rtps_screen_saturation) "
                        "is notrun (RotTransPers)",
    "pop_matrix_bit": "PopMatrix: only notrun cases call it (push_pop_matrix and two more, sh2o lacks ReadRotMatrix)",
    "push_matrix_top": "PushMatrix: only notrun cases call it (push_pop_matrix, push_matrix_holds_20_entries)",
    "fog_no_break_hook": "the divide break hook: the shim sets no sh2o_psyq_break, and the running SetFogNearFar "
                         "cases divide by no zero (h 256, far - near >= 100 or an early return)",
    "ratan_index_off_by_one": "ratan2: test_libgte.c calls it in no case (tests/psyz_ratan2_sweep.py covers it)",
    "init_geom_screen": "InitGeom: only notrun cases call it (the RotTransPers and RotTransPers3 cases)",
    "dpct_reads_rgbc": "DPCT: the one running case (dpct_flags_far_color_difference_saturation) has RGB0-2 and the "
                       "RGBC colour all 0, so RGBC gives the same input as RGB0",
    "dpct_fifo_not_advanced": "DPCT: the one running case has RGB0-2 all 0, so each FIFO entry gives the same input",
}
# Shim plants: {name: (old text, new text)} in a copy of tests/psyz/sh2o_libgte_shim.c; each must make a
# running case fail.
PLANTS = {
    "ldv0_wrong_register": ("    shim_wr(1,shim_pair(v->vz,v->pad));", "    shim_wr(2,shim_pair(v->vz,v->pad));"),
    "set_trans_matrix_swapped": ("    shim_to_matrix(&s,m);\n    sh2o_psyq_SetTransMatrix(",
                                 "    shim_to_matrix(&s,m);\n    s.t[0]=(int32_t)m->t[1];\n"
                                 "    s.t[1]=(int32_t)m->t[0];\n    sh2o_psyq_SetTransMatrix("),
}
# The differs case and its input state for the GTE model (scripts/gte_model.py): {register: value}, command.
ARBITRATED = "rtps_ir0_flags_fraction_above_limit"
ARBITER_STATE = {0: (20 << 16) | 10, 1: 30, 32: 0x1000, 34: 0x1000, 36: 0x1000, 39: 1000,
                 56: 160 << 16, 57: 120 << 16, 58: 1000, 59: 0, 60: 0x1000001}
ARBITER_CMD = 0x180001


def run(command):
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit("psyz_libgte: %s failed:\n%s%s" % (command[0], result.stdout, result.stderr))
    return result.stdout


def doc_cases(source):
    """{case: (class, note)} of the fenced block after HEADING in docs/PSYQ.md."""
    with open(os.path.join(source, "docs", "PSYQ.md"), encoding="utf-8") as handle:
        text = handle.read()
    after = text.split("\n" + HEADING + "\n", 1)
    if len(after) != 2:
        sys.exit("psyz_libgte: docs/PSYQ.md has no heading '%s'" % HEADING)
    block = after[1].split("\n```\n", 2)
    if len(block) < 3:
        sys.exit("psyz_libgte: docs/PSYQ.md has no fenced case list after '%s'" % HEADING)
    cases = {}
    for line in block[1].splitlines():
        words = line.split(" ", 2)
        if len(words) < 2 or words[1] not in COUNTS or words[0] in cases:
            sys.exit("psyz_libgte: docs/PSYQ.md case line '%s' is not '<case> <class> [<note>]', "
                     "or the case is there two times" % line)
        cases[words[0]] = (words[1], words[2] if len(words) == 3 else "")
    return cases


def main(argv):
    if len(argv) not in (4, 5):
        sys.exit(__doc__)
    cc, source, work = argv[1:4]
    psyz = argv[4] if len(argv) == 5 else os.path.join(source, "external", "psyz")
    test_c = os.path.join(psyz, "psyz", "tests", "test_libgte.c")
    if not os.path.exists(test_c):
        if os.environ.get("SH2O_REQUIRE_PSYZ") == "1":
            print("psyz_libgte: required (SH2O_REQUIRE_PSYZ=1), %s is not checked out" % test_c)
            return 1
        print("psyz_libgte: skipped, %s is not checked out" % test_c)
        return 77
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    try:
        return check(cc, source, work, psyz, test_c)
    finally:
        shutil.rmtree(work, ignore_errors=True)


def check(cc, source, work, psyz, test_c):
    with open(test_c, encoding="utf-8") as handle:
        count = len(re.findall(r"^ZTEST\(", handle.read(), re.M))
    if count != CASES:
        print("FAIL %s has %d ZTEST cases, want %d" % (test_c, count, CASES))
        return 1
    flags = ["-D__psyz", "-isystem", os.path.join(psyz, "psyz", "include"),
             "-isystem", os.path.join(psyz, "ztest")]
    objects = []
    for name, path in (("test_libgte.o", test_c), ("ztest.o", os.path.join(psyz, "ztest", "ztest.c"))):
        objects.append(os.path.join(work, name))
        run([cc, "-std=gnu11", "-w", "-O1", *flags, "-c", path, "-o", objects[-1]])
    got, bodies = build_and_run(cc, flags, objects, source, os.path.join(source, "tests", "psyz"),
                                os.path.join(work, "psyz_libgte"))
    want = doc_cases(source)
    fail = 0
    if len(got) != CASES:
        print("FAIL the run gave %d case results, want %d" % (len(got), CASES))
        fail = 1
    for name in sorted(set(got) | set(want)):
        if name not in want:
            print("FAIL %s: %s in the run, not in docs/PSYQ.md" % (name, got[name][0]))
            fail = 1
        elif name not in got:
            print("FAIL %s: in docs/PSYQ.md, not in the run" % name)
            fail = 1
        elif got[name][0] != want[name][0] or (got[name][0] == "notrun" and got[name][1] != want[name][1]):
            print("FAIL %s: run %s, docs/PSYQ.md %s" % (name, " ".join(got[name]).strip(),
                                                       " ".join(want[name]).strip()))
            fail = 1
    counts = {key: sum(1 for value in got.values() if value[0] == key) for key in COUNTS}
    print("psyz_libgte: %d cases: %s" % (len(got), ", ".join("%d %s" % (counts[k], k) for k in COUNTS)))
    if counts != COUNTS:
        print("FAIL counts: want %s" % ", ".join("%d %s" % (COUNTS[k], k) for k in COUNTS))
        fail = 1
    fail |= arbitrate(source, bodies.get(ARBITRATED, ""))
    fail |= mutants(cc, flags, objects, source, work, want)
    fail |= plants(cc, flags, objects, source, work, want)
    print("psyz_libgte: %s" % ("FAIL" if fail else "PASS"))
    return fail


def build_and_run(cc, flags, objects, root, shim, exe, warnings=("-Wall", "-Wextra", "-Werror")):
    """Link the sh2o side of root (src/, include/) and the shim folder with the psyz objects, run it.
    Return ({case: (class, note)}, {case: result text})."""
    run([cc, "-std=c11", *warnings, "-O1", *flags,
         "-I", os.path.join(root, "include"), "-I", os.path.join(root, "src"),
         os.path.join(shim, "sh2o_libgte_shim.c"),
         *sorted(glob.glob(os.path.join(root, "src", "*.c"))), *objects, "-o", exe])
    result = subprocess.run([exe, "--verbose", "--output=plain"], capture_output=True, text=True)
    got, bodies = {}, {}
    for match in RESULT.finditer(result.stdout):
        name, word, body = match.groups()
        lacks = re.search(r"sh2o lacks (.*)", body)
        got[name] = (CLASS[word], lacks.group(1) if word == "SKIP" and lacks else "")
        bodies[name] = body
    return got, bodies


def changed(got, want):
    """The running cases (not notrun in docs/PSYQ.md) whose class in got is not the docs class."""
    return sorted(name for name, (kind, _) in want.items()
                  if kind != "notrun" and got.get(name, ("missing",))[0] != kind)


def arbitrate(source, body):
    """The GTE model gives the FLAG of ARBITRATED: it must be sh2o's value, not psyz's."""
    match = re.search(r"Line \d+  Expected: (\d+) \(0x[0-9a-fA-F]+\)  Actual: (\d+)", body)
    if not match:
        print("FAIL arbiter: no expected/actual FLAG in the result of %s" % ARBITRATED)
        return 1
    psyz_flag, sh2o_flag = int(match.group(1)), int(match.group(2))
    sys.path.insert(0, os.path.join(source, "scripts"))
    import gte_model
    regs = [0] * 64
    for reg, value in ARBITER_STATE.items():
        regs[reg] = value
    flag = gte_model.step(regs, ARBITER_CMD)[63]
    print("psyz_libgte: arbiter %s FLAG: psyz %Xh, sh2o %Xh, GTE model %Xh"
          % (ARBITRATED, psyz_flag, sh2o_flag, flag))
    if flag != sh2o_flag or flag == psyz_flag:
        print("FAIL arbiter: the GTE model FLAG must be sh2o's and not psyz's")
        return 1
    return 0


def mutants(cc, flags, objects, source, work, want):
    """Each mutant of the gte and psyq families of tests/mutant.py: CAUGHT when a running case changes class."""
    sys.path.insert(0, os.path.join(source, "tests"))
    import mutant
    caught = set()
    for family in ("gte", "psyq"):
        for name in mutant.MUTANTS[family]:
            root = os.path.join(work, "mutant_" + name)
            mutant.main(["mutant.py", family, name, source, root])
            # -w: a mutant can leave a variable unused (apply_matrix_swapped)
            got, _ = build_and_run(cc, flags, objects, root, os.path.join(source, "tests", "psyz"),
                                   os.path.join(root, "psyz_libgte"), ("-w",))
            count = len(changed(got, want))
            if count:
                caught.add(name)
            print("psyz_libgte: mutant %s %s: %d running cases fail" % (family, name, count))
            shutil.rmtree(root, ignore_errors=True)
    print("psyz_libgte: caught %s" % " ".join(sorted(caught)))
    for name in sorted(NOT_REACHED):
        print("psyz_libgte: not reached %s: %s" % (name, NOT_REACHED[name]))
    every = set(mutant.MUTANTS["gte"]) | set(mutant.MUTANTS["psyq"])
    if caught != CAUGHT or every - caught != set(NOT_REACHED):
        print("FAIL mutants: caught set is not the pin (CAUGHT %s; NOT_REACHED %s)"
              % (" ".join(sorted(CAUGHT)), " ".join(sorted(NOT_REACHED))))
        return 1
    return 0


def plants(cc, flags, objects, source, work, want):
    """Each shim plant (a wrong mapping in a copy of tests/psyz/) must make a running case fail."""
    fail = 0
    for name, (old, new) in PLANTS.items():
        shim = os.path.join(work, "plant_" + name)
        shutil.copytree(os.path.join(source, "tests", "psyz"), shim)
        path = os.path.join(shim, "sh2o_libgte_shim.c")
        with open(path, encoding="utf-8") as handle:
            text = handle.read()
        if text.count(old) != 1:
            print("FAIL plant %s: the old text occurs %d times, not 1" % (name, text.count(old)))
            fail = 1
            continue
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text.replace(old, new))
        got, _ = build_and_run(cc, flags, objects, source, shim, os.path.join(shim, "psyz_libgte"))
        count = len(changed(got, want))
        print("psyz_libgte: plant %s: %d running cases fail" % (name, count))
        if not count:
            print("FAIL plant %s: no running case fails" % name)
            fail = 1
    return fail


if __name__ == "__main__":
    sys.exit(main(sys.argv))
