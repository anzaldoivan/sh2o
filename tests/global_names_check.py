#!/usr/bin/env python3
"""Check that every global symbol sh2o defines is named sh2o_* or sh2oi_*.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/global_names_check.py --cc <C compiler> --scratch <dir>
           --objects <object files> [--objects <object files>]...

CTest gives the arguments (CMakeLists.txt, test global_names): the objects of
the library and of each family program with a suite in tests/suite/ (lists
separated by ";"). Each line of `nm -g --defined-only` with three fields names
a global the object defines; a name that is not sh2o_* or sh2oi_* is a problem
"<source path>: <name>". main is allowed only in a tests/test_*.c object. A
sanitizer build adds its own globals (__odr_asan.<name> with gcc): they are
not ours and are skipped.
Then each plant, an object compiled into the scratch folder and classed as a
library source, is checked with the real objects and must be caught: a bare
function SetRotMatrix (exactly one problem), a bare global variable, a main.
Exit code 0: the objects give no problem and every plant is caught.
"""
import argparse
import os
import re
import subprocess
import sys

PREFIX = re.compile(r"sh2oi?_\w+")
# the runtime globals of a sanitizer build; gcc's __odr_asan.<name> ends in a dot
SANITIZER = re.compile(r"(^|_)(asan|ubsan|tsan|msan|sanitizer)(_|\.|$)")
PLANTS = (
    ("function", "void SetRotMatrix(void);\nvoid SetRotMatrix(void) {}\n", "SetRotMatrix"),
    ("variable", "int planted_count = 1;\n", "planted_count"),
    ("main", "int main(void) { return 0; }\n", "main"),
)


def source_path(obj):
    """CMakeFiles/<target>.dir/<source path>.o -> <source path>."""
    path = obj.replace(os.sep, "/")
    if ".dir/" in path:
        path = path.split(".dir/", 1)[1]
    return re.sub(r"\.(o|obj)$", "", path)


def problems(objects):
    """[(source path, object)] -> ["<source path>: <name>"] for each global not named sh2o_* / sh2oi_*."""
    out = []
    for src, obj in objects:
        result = subprocess.run(["nm", "-g", "--defined-only", obj], capture_output=True, text=True, check=True)
        for line in result.stdout.splitlines():
            fields = line.split()
            if len(fields) != 3:
                continue
            name = fields[2]
            if PREFIX.fullmatch(name) or SANITIZER.search(name):
                continue
            if name == "main" and re.fullmatch(r"tests/test_\w+\.c", src):
                continue
            out.append(f"{src}: {name}")
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc", required=True)
    parser.add_argument("--scratch", required=True)
    parser.add_argument("--objects", action="append", required=True)
    args = parser.parse_args()
    objects = sorted({(source_path(o), o) for group in args.objects for o in group.split(";") if o})
    found = problems(objects)
    print(f"global_names: {len(objects)} objects, {len(found)} problems")
    for problem in found:
        print(f"global_names: {problem}")
    ok = not found
    os.makedirs(args.scratch, exist_ok=True)
    for kind, text, name in PLANTS:
        source = os.path.join(args.scratch, f"planted_{kind}.c")
        obj = source + ".o"
        with open(source, "w") as f:
            f.write(text)
        subprocess.run([args.cc, "-c", source, "-o", obj], check=True)
        got = problems(objects + [("src/planted.c", obj)])
        want = [f"src/planted.c: {name}"]
        caught = got == want if kind == "function" else want[0] in got
        print(f"global_names: plant {kind}: {got} ({'caught' if caught else 'NOT CAUGHT'})")
        ok = ok and caught
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
