#!/usr/bin/env python3
"""Check that no run of lines of sh2o's sources is a copy of a pinned submodule.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/zam_copy_check.py <source dir> [<pin path>]

CTest gives the arguments (CMakeLists.txt, tests zam_copy and psyz_copy). The
pin path is external/SH2ZAM (the default) or external/psyz; the output lines
start with zam_copy or psyz_copy. Each file is
normalised: comment lines starting with # are dropped in .py, .sh, .toml, .mk,
Makefile and files without an extension; elsewhere /* */ comments are blanked
(their newlines kept) and // comments cut; whitespace is collapsed and empty
lines are dropped. A problem is a run of LIMIT or more consecutive normalised
lines of a .c, .h or .py file under include, src, tests, scripts, benchmarks or
examples that equals a run of a text blob (no NUL byte) of the pin at the
gitlink of the source dir. LIMIT is 6: runs of up to 5 lines (closing
braces, includes, short idioms) can be equal by chance; the longest today is 4.
Then the plant: the first LIMIT normalised lines of a pin .c or .h file as one
more source must be caught, its first LIMIT - 1 lines must not.
Exit code 0: no problem and the plant gives the expected result; 77 when the
pin is not checked out, but 1 for external/psyz when SH2O_REQUIRE_PSYZ is 1.
"""
import os
import re
import subprocess
import sys

LIMIT = 6
PIN = "external/SH2ZAM"
TOPS = ("include", "src", "tests", "scripts", "benchmarks", "examples")
HASH_EXTS = (".py", ".sh", ".toml", ".mk", "")


def normalise(text, name):
    """[(line number, line)] of text less comments and blank lines; whitespace collapsed."""
    hashc = os.path.splitext(name)[1].lower() in HASH_EXTS or os.path.basename(name) == "Makefile"
    if not hashc:
        text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    out = []
    for number, line in enumerate(text.splitlines(), 1):
        if hashc:
            if line.strip().startswith("#"):
                continue
        else:
            line = re.sub(r"//.*", "", line)
        line = " ".join(line.split())
        if line:
            out.append((number, line))
    return out


def pin_texts(source, where=PIN):
    """{path: text} of each blob of the pin at the gitlink, or None when the pin is not checked out."""
    stage = subprocess.run(["git", "-C", source, "ls-files", "--stage", "--", where], capture_output=True,
                           text=True).stdout.split()
    pin = os.path.join(source, where)
    if len(stage) < 2 or stage[0] != "160000" or not os.path.exists(os.path.join(pin, ".git")):
        return None
    rows = subprocess.run(["git", "-C", pin, "ls-tree", "-r", stage[1]], capture_output=True, text=True,
                          check=True).stdout.splitlines()
    blobs = [(meta.split()[2], path) for meta, path in (row.split("\t", 1) for row in rows)
             if meta.split()[1] == "blob"]
    out = subprocess.run(["git", "-C", pin, "cat-file", "--batch"], capture_output=True, check=True,
                         input="".join(f"{sha}\n" for sha, _ in blobs).encode()).stdout
    texts, at = {}, 0
    for _, path in blobs:
        nl = out.index(b"\n", at)
        size = int(out[at:nl].split()[2])
        data = out[nl + 1:nl + 1 + size]
        at = nl + 2 + size
        if b"\0" not in data:
            texts[path] = data.decode("utf-8", "replace")
    return texts


def own_texts(source):
    """{path: text} of each .c, .h and .py file under TOPS."""
    texts = {}
    for top in TOPS:
        for root, dirs, files in os.walk(os.path.join(source, top)):
            dirs[:] = sorted(d for d in dirs if d != "__pycache__")
            for name in sorted(files):
                if name.endswith((".c", ".h", ".py")):
                    path = os.path.join(root, name)
                    with open(path, encoding="utf-8", errors="replace") as f:
                        texts[os.path.relpath(path, source).replace(os.sep, "/")] = f.read()
    return texts


def pin_windows(pins):
    """[set of the k-line runs of the pin for k = 1..LIMIT]; the last maps each run to (pin path, first line)."""
    sets = [set() for _ in range(LIMIT - 1)] + [{}]
    for path in sorted(pins):
        lines = normalise(pins[path], path)
        words = [line for _, line in lines]
        for k in range(1, LIMIT + 1):
            for i in range(len(words) - k + 1):
                run = tuple(words[i:i + k])
                if k < LIMIT:
                    sets[k - 1].add(run)
                else:
                    sets[k - 1].setdefault(run, (path, lines[i][0]))
    return sets


def problems(own, windows):
    """(problems, longest run up to LIMIT): one problem per own run of LIMIT lines equal to a pin run."""
    out, longest = [], 0
    for path in sorted(own):
        lines = normalise(own[path], path)
        words = [line for _, line in lines]
        for k in range(longest + 1, LIMIT + 1):
            if not any(tuple(words[i:i + k]) in windows[k - 1] for i in range(len(words) - k + 1)):
                break
            longest = k
        for i in range(len(words) - LIMIT + 1):
            hit = windows[LIMIT - 1].get(tuple(words[i:i + LIMIT]))
            if hit:
                out.append(f"{path}:{lines[i][0]}: {LIMIT} lines equal {hit[0]}:{hit[1]}")
    return out, longest


def main(argv):
    if len(argv) not in (2, 3):
        sys.exit(__doc__)
    source, where = argv[1], argv[2] if len(argv) == 3 else PIN
    tag = "psyz_copy" if where == "external/psyz" else "zam_copy"
    pins = pin_texts(source, where)
    if pins is None and tag == "psyz_copy" and os.environ.get("SH2O_REQUIRE_PSYZ") == "1":
        print(f"{tag}: required (SH2O_REQUIRE_PSYZ=1), {where} is not checked out in {source}")
        return 1
    if pins is None:
        print(f"{tag}: skipped, {where} is not checked out in {source}")
        return 77
    windows = pin_windows(pins)
    own = own_texts(source)
    found, longest = problems(own, windows)
    print(f"{tag}: {len(own)} files against {len(pins)} pin texts, longest equal run "
          f"{longest}{'+' if longest == LIMIT else ''} lines (limit {LIMIT}), {len(found)} problems")
    for problem in found:
        print(f"{tag}: {problem}")
    ok = not found
    name, lines = next((p, normalise(t, p)) for p, t in sorted(pins.items())
                       if p.endswith((".c", ".h")) and len(normalise(t, p)) >= LIMIT)
    for n, want in ((LIMIT, True), (LIMIT - 1, False)):
        plant = dict(own)
        plant["src/planted.c"] = "".join(line + "\n" for _, line in lines[:n])
        got = [p for p in problems(plant, windows)[0] if p.startswith("src/planted.c:")]
        print(f"{tag}: plant {n} lines of {name}: {len(got)} problems "
              f"({'as expected' if bool(got) == want else 'NOT AS EXPECTED'})")
        ok = ok and bool(got) == want
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
