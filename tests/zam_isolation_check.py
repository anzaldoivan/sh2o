#!/usr/bin/env python3
"""Check that the SH2ZAM delegate stays out of the default build of sh2o.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 tests/zam_isolation_check.py <source dir> --zam ON|OFF
           --defs <sh2o COMPILE_DEFINITIONS> --libs <sh2o LINK_LIBRARIES>
           --lib <libsh2o.a> --objects <sh2o object files> --scratch <dir>

CTest gives the arguments (CMakeLists.txt, test zam_isolation). The rules:
1. No .c or .h file under include, src, tests, examples or benchmarks names
   SH2ZAM (<sh2zam/ or sh2z_), except the delegate's own files (ALLOWED).
2. The quoted-include closure of include/sh2o/sh2o.h never reaches
   sh2o/zam_experimental.h, and its only system headers are <stdint.h> and
   <stdbool.h>.
3. CMakeLists.txt defines SH2O_ZAM and names SH2ZAM only inside if(SH2O_ZAM)
   blocks, and the option SH2O_ZAM is OFF by default.
4. With --zam OFF the library is built without SH2O_ZAM and without SH2ZAM:
   the object of src/sh2o_zam.c has no global or undefined symbol and
   libsh2o.a holds no sh2z or sh2o_zam symbol. With --zam ON sh2o is built
   with SH2O_ZAM=1.
Then each plant puts one fault into a copy of include/, src/ and
CMakeLists.txt under the scratch folder (or into the build facts) and must be
caught; the plant in an ALLOWED path must give no problem. Exit code 0: the
tree gives no problem and every plant gives the expected result.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

ALLOWED = ("include/sh2o/zam_experimental.h", "src/sh2o_zam.c", "tests/suite/sh2o_zam_test_suite.c",
           "tests/test_zam.c")
TREE_DIRS = ("include", "src", "tests", "examples", "benchmarks")
UMBRELLA = "include/sh2o/sh2o.h"
ZAM_HEADER = "zam_experimental.h"
SYSTEM_ALLOWED = ("<stdint.h>", "<stdbool.h>")
Z_NAME = re.compile(r"<sh2zam/|\bsh2z_")
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"][^>"]+[>"])', re.M)
CMAKE_DEFINE = re.compile(r"-D\s*SH2O_ZAM\b|\bSH2O_ZAM\s*=|"
                          r"\b(add_compile_definitions|add_definitions|target_compile_definitions)\s*\([^)]*\bSH2O_ZAM\b",
                          re.I | re.S)
CMAKE_PATH = re.compile(r"external/SH2ZAM|SH2ZAM::|sh2zam")
OPTION = re.compile(r'\boption\s*\(\s*SH2O_ZAM\s+"[^"]*"\s*(\S+?)\s*\)', re.I)
TRUE = ("ON", "1", "TRUE", "YES", "Y")


def tree_problems(root):
    """Rules 1 to 3 on the files under root."""
    out = []
    for top in TREE_DIRS:
        for folder, dirs, files in os.walk(os.path.join(root, top)):
            dirs.sort()
            for name in sorted(files):
                if not name.endswith((".c", ".h")):
                    continue
                path = os.path.join(folder, name)
                relative = os.path.relpath(path, root).replace(os.sep, "/")
                if relative in ALLOWED:
                    continue
                with open(path, encoding="utf-8", errors="replace") as handle:
                    if Z_NAME.search(handle.read()):
                        out.append("%s: names SH2ZAM" % relative)
    out += closure_problems(root)
    out += cmake_problems(root)
    return out


def closure_problems(root):
    """Rule 2: the quoted-include closure of the umbrella header."""
    out, seen = [], set()
    include = os.path.join(root, "include")
    todo = [os.path.join(root, UMBRELLA)]
    while todo:
        path = os.path.normpath(todo.pop())
        if path in seen:
            continue
        seen.add(path)
        if os.path.basename(path) == ZAM_HEADER:
            out.append("%s: closure reaches %s" % (UMBRELLA, os.path.relpath(path, root).replace(os.sep, "/")))
        with open(path, encoding="utf-8") as handle:
            text = handle.read()
        for inc in INCLUDE.findall(text):
            if inc.startswith("<"):
                if inc not in SYSTEM_ALLOWED:
                    out.append("%s: system include %s" % (os.path.relpath(path, root).replace(os.sep, "/"), inc))
                continue
            name = inc.strip('"')
            for base in (os.path.dirname(path), include):
                if os.path.isfile(os.path.join(base, name)):
                    todo.append(os.path.join(base, name))
                    break
            else:
                out.append("%s: include %s not found" % (os.path.relpath(path, root).replace(os.sep, "/"), inc))
    return sorted(set(out))


def cmake_problems(root):
    """Rule 3: CMakeLists.txt outside if(SH2O_ZAM) blocks (comments ignored)."""
    out = []
    with open(os.path.join(root, "CMakeLists.txt"), encoding="utf-8") as handle:
        lines = handle.read().split("\n")
    depth, zam_depth, outside = 0, None, []
    for line in lines:
        code = line.split("#", 1)[0]
        if re.match(r"\s*if\s*\(", code, re.I):
            depth += 1
            if zam_depth is None and re.match(r"\s*if\s*\(\s*SH2O_ZAM\s*\)", code, re.I):
                zam_depth = depth
                continue
        if re.match(r"\s*endif\s*\(", code, re.I):
            if zam_depth == depth:
                zam_depth = None
                depth -= 1
                continue
            depth -= 1
        if zam_depth is None:
            outside.append(code)
    text = "\n".join(outside)
    if CMAKE_DEFINE.search(text):
        out.append("CMakeLists.txt: defines SH2O_ZAM outside if(SH2O_ZAM)")
    if CMAKE_PATH.search(text):
        out.append("CMakeLists.txt: names an SH2ZAM path outside if(SH2O_ZAM)")
    option = OPTION.search(text)
    if not option or option.group(1).upper() != "OFF":
        out.append("CMakeLists.txt: option not OFF (option(SH2O_ZAM ...) must default to OFF)")
    return out


def nm(arguments):
    result = subprocess.run(["nm"] + arguments, capture_output=True, text=True, check=True)
    return [line.split()[-1] for line in result.stdout.splitlines() if len(line.split()) >= 2]


def fact_problems(zam, defs, libs):
    """Rule 4 on sh2o's compile definitions and link libraries."""
    out = []
    has_define = any(re.match(r"SH2O_ZAM(=|$)", item) for item in defs.split(";"))
    if zam:
        if "SH2O_ZAM=1" not in defs.split(";"):
            out.append("ON build does not define SH2O_ZAM=1: %s" % defs)
        return out
    if has_define:
        out.append("default build defines SH2O_ZAM: %s" % defs)
    if "sh2zam" in libs.lower():
        out.append("default build links SH2ZAM: %s" % libs)
    return out


def object_problems(lib, objects):
    """Rule 4 on the built default library."""
    out = []
    zam_objects = [path for path in objects.split(";") if os.path.basename(path).startswith("sh2o_zam.c")]
    if len(zam_objects) != 1:
        out.append("zam object not found among %d objects" % len(objects.split(";")))
    else:
        symbols = nm(["-g", "--defined-only", zam_objects[0]]) + nm(["-u", zam_objects[0]])
        # a sanitizer build adds its runtime hooks to every object, the empty one too
        symbols = [name for name in symbols if not re.search(r"(^|_)(asan|ubsan|tsan|msan|sanitizer)(_|$)", name)]
        if symbols:
            out.append("zam object not empty: %s" % " ".join(symbols))
    held = sorted(set(name for name in nm([lib]) if re.search(r"sh2z|sh2o_zam", name)))
    if held:
        out.append("library holds SH2ZAM: %s" % " ".join(held))
    return out


def copy_tree(source, scratch, name):
    root = os.path.join(scratch, name)
    if os.path.isdir(root):
        shutil.rmtree(root)
    for top in ("include", "src"):
        shutil.copytree(os.path.join(source, top), os.path.join(root, top))
    shutil.copy(os.path.join(source, "CMakeLists.txt"), root)
    return root


def edit(path, old, new):
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    if old not in text:
        raise SystemExit("zam_isolation: plant anchor not found in %s: %r" % (path, old))
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text.replace(old, new, 1))


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def plants(source, scratch, args):
    """Each plant: (name, the fault, the phrase it must give; None: no problem)."""
    umbrella_anchor = '#include "sh2o/psyq.h"\n'
    cmake_anchor = "add_library(sh2o::sh2o ALIAS sh2o)\n"
    option_line = OPTION.search(open(os.path.join(source, "CMakeLists.txt"), encoding="utf-8").read()).group(0)
    tree_plants = (
        ("umbrella zam", lambda r: edit(os.path.join(r, UMBRELLA), umbrella_anchor,
                                        umbrella_anchor + '#include "zam_experimental.h"\n'), "closure reaches"),
        ("umbrella system", lambda r: edit(os.path.join(r, UMBRELLA), umbrella_anchor,
                                           umbrella_anchor + "#include <stddef.h>\n"), "system include <stddef.h>"),
        ("cmake define", lambda r: edit(os.path.join(r, "CMakeLists.txt"), cmake_anchor,
                                        cmake_anchor + "target_compile_definitions(sh2o PRIVATE SH2O_ZAM=1)\n"),
         "defines SH2O_ZAM"),
        ("cmake path", lambda r: edit(os.path.join(r, "CMakeLists.txt"), cmake_anchor, cmake_anchor +
                                      "target_include_directories(sh2o PRIVATE external/SH2ZAM/saturn/include)\n"),
         "names an SH2ZAM path"),
        ("src names sh2zam", lambda r: edit(os.path.join(r, "src/sh2o_list.c"), "#include <stdint.h>\n",
                                            "#include <stdint.h>\n#include <sh2zam/sh2zam.h>\n"), "names SH2ZAM"),
        ("allowed path", lambda r: write(os.path.join(r, "tests/test_zam.c"), "#include <sh2zam/sh2zam.h>\n"), None),
        ("option on", lambda r: edit(os.path.join(r, "CMakeLists.txt"), option_line,
                                     re.sub(r"OFF\)$", "ON)", option_line)), "option not OFF"),
    )
    failed = 0
    for name, plant, want in tree_plants:
        root = copy_tree(source, scratch, name.replace(" ", "_"))
        plant(root)
        got = tree_problems(root)
        print("zam_isolation: plant %s: %s" % (name, got))
        if (want is None and got) or (want is not None and not any(want in problem for problem in got)):
            print("zam_isolation: plant %s not caught (want %r)" % (name, want))
            failed += 1
    got = fact_problems(False, "SH2O_ZAM=1", "")
    print("zam_isolation: plant defs with OFF: %s" % got)
    if not any("default build defines SH2O_ZAM" in problem for problem in got):
        print("zam_isolation: plant defs with OFF not caught")
        failed += 1
    return failed, len(tree_plants) + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("source")
    parser.add_argument("--zam", required=True)
    parser.add_argument("--defs", default="")
    parser.add_argument("--libs", default="")
    parser.add_argument("--lib", required=True)
    parser.add_argument("--objects", required=True)
    parser.add_argument("--scratch", required=True)
    args = parser.parse_args()
    args.zam_on = args.zam.upper() in TRUE
    source = os.path.abspath(args.source)
    problems = tree_problems(source) + fact_problems(args.zam_on, args.defs, args.libs)
    if not args.zam_on:
        problems += object_problems(args.lib, args.objects)
    print("zam_isolation: tree (--zam %s): %s" % (args.zam, problems))
    failed, count = plants(source, os.path.abspath(args.scratch), args)
    shutil.rmtree(os.path.abspath(args.scratch), ignore_errors=True)
    if problems or failed:
        print("zam_isolation: FAILED (%d problem(s), %d plant(s) not caught)" % (len(problems), failed))
        return 1
    print("zam_isolation: ok (%d plants caught)" % count)
    return 0


if __name__ == "__main__":
    sys.exit(main())
