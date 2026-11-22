#!/usr/bin/env python3
"""Check the sh2o code and comment style (docs/STYLE.md).

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/style_check.py [PATH ...]

With no PATH, the script checks the public sh2o paths only: the C files in
include/ src/ tests/ examples/ benchmarks/, the scripts in scripts/, and the
public Markdown files. It prints one line per problem, as
path:line: rule: text, and exits 1 if it finds a problem.
"""
import fnmatch
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C_DIRS = ("include", "src", "tests", "examples", "benchmarks")
LIBRARY_DIRS = ("include", "src")
SCRIPT_DIRS = ("scripts",)
MD_ROOT = ("README.md", "NOTICE.md", "CONTRIBUTING.md", "CONTRIBUTING-KERNELS.md",
           "CODE_OF_CONDUCT.md")
MD_DIRS = ("docs", "benchmarks", "examples")
# The private repository lists its public files here. When the list exists,
# a Markdown file in MD_DIRS that no entry matches is private and not checked.
PUBLIC_LIST = os.path.join(REPO, "config", "public-paths.txt")

COPYRIGHT = " * Copyright (c) 2026 sh2o Contributors. MIT License."
CREDIT = re.compile(r"^ \* .*\bSH2ZAM\b.*\bSH4ZAM\b")
# The SH-2 model harness is GPL (it includes GPL code), so its files have the
# SPDX line, a title and the sh2o copyright line without the MIT words.
GPL_DIR = "benchmarks/sh2-model/"
GPL_SPDX = "/* SPDX-License-Identifier: GPL-2.0-or-later"
GPL_COPYRIGHT = " * Copyright (c) 2026 sh2o Contributors."
SCRIPT_COPYRIGHT = "Copyright (c) 2026 sh2o Contributors. MIT License."
KEYWORD_SPACE = re.compile(r"\b(if|for|while|switch)\s+\(")
COMMA_SPACE = re.compile(r",[ ]+\S")
POINTER_LEFT = re.compile(r"\b(?:[A-Za-z_]\w*_t|char|int|short|long|void|bool|unsigned|signed)\*+[ ]+[A-Za-z_(]")
MARKER = re.compile(r"\b(TODO|FIXME|XXX)\b")
DOXYGEN_TAG = re.compile(r"[\\@](param|returns?|note|warning|file|author|defgroup|ingroup|name)\b")
ASM_START = re.compile(r"\b(__asm__|asm|__asm)\b")
ASM_FORM = re.compile(r"\b__asm__ volatile\(")
MAC_USE = re.compile(r"\b(mac\.[lw]|dmuls\.l|dmulu\.l|mul\.l|muls\.w|mulu\.w|muls|mulu|clrmac)\b")
MAC_S_DEPENDENT = re.compile(r"\bmac\.[lw]\b")
SR_S_HANDLED = re.compile(r"\bstc\s+sr\b|\bclrs\b|\bsets\b")
# Files whose MAC.W kernels assume SR.S = 0 and do not read or set SR. Each
# kernel's contract tag gives the assumption and the known gap; a kernel that
# checks SR.S gets a new contract tag and a file that is not in this set. The
# k suite's test mutants copy the kernels' assumption. The prepared MVMVA kernel states the same assumption; its
# caller (src/sh2o_gte_prepared.c) reads SR.S and does not call it when S is set.
SR_S_ASSUMED = {"include/sh2o/inline/sh2/k_sh2.inl.h", "tests/suite/sh2o_k_test_suite.c",
                "include/sh2o/inline/sh2/gte_prepared_sh2.inl.h"}
# Function lines that start with these macros are static (sh2o/cdefs.h).
STATIC_MACROS = ("SH2OI_INLINE", "SH2OI_FORCE_INLINE")
FLOAT = re.compile(r"\b(float|double)\b|\b\d+\.\d*(e[+-]?\d+)?f?\b")
LIBC_CALL = re.compile(r"\b(malloc|calloc|realloc|free|memcpy|memset|memmove|memcmp|strlen|"
                       r"printf|fprintf|sprintf|snprintf|puts|abort|exit|abs|labs|div|ldiv)\s*\(")
LIBRARY_HEADERS = {"stdbool.h", "stddef.h", "stdint.h"}
# The one library file that includes SH2ZAM (sh2o/zam_experimental.h), only when SH2O_ZAM=1.
ZAM_SOURCE = "src/sh2o_zam.c"
EM_DASH = "—"
EMOJI = re.compile("[\U0001F000-\U0001FAFF☀-➿️‍]")


def strip_code(lines):
    """Blank comments, strings and chars. Return the code lines, the numbers of
    lines with a // comment, and per line whether it starts in a comment."""
    code, slashes, starts_in_comment = [], [], []
    in_comment = False
    for number, line in enumerate(lines, 1):
        starts_in_comment.append(in_comment)
        out, i = [], 0
        while i < len(line):
            if in_comment:
                if line.startswith("*/", i):
                    in_comment = False
                    out.append("  ")
                    i += 2
                else:
                    out.append(" ")
                    i += 1
                continue
            if line.startswith("/*", i):
                in_comment = True
                out.append("  ")
                i += 2
                continue
            if line.startswith("//", i):
                slashes.append(number)
                break
            ch = line[i]
            if ch in "\"'":
                end = i + 1
                while end < len(line) and line[end] != ch:
                    end += 2 if line[end] == "\\" else 1
                out.append(ch + " " * max(0, min(end, len(line)) - i - 1) + ch)
                i = end + 1
                continue
            out.append(ch)
            i += 1
        code.append("".join(out))
    return code, slashes, starts_in_comment


def asm_blocks(code):
    """Yield (first, last) line numbers of each inline-assembly statement."""
    number = 0
    while number < len(code):
        match = ASM_START.search(code[number])
        if not match:
            number += 1
            continue
        first, depth, seen = number, 0, False
        while number < len(code):
            text = code[number][match.start():] if number == first else code[number]
            depth += text.count("(") - text.count(")")
            seen = seen or "(" in text
            if seen and depth <= 0:
                break
            number += 1
        yield first + 1, number + 1
        number += 1


def expected_guard(path):
    stem = os.path.splitext(os.path.basename(path))[0].upper()
    if stem.endswith(".INL"):
        stem = stem[:-4] + "_INL"
    if stem == "SH2O":
        return "SH2O_H"
    if stem.startswith("SH2O_"):
        stem = stem[5:]
    return "SH2O_%s_H" % stem


def check_gpl_banner(lines):
    if len(lines) < 3 or lines[0] != GPL_SPDX:
        return 1, "banner line 1 is not '%s'" % GPL_SPDX
    if not (lines[1].startswith(" * ") and lines[1].endswith(".")):
        return 2, "banner line 2 is not ' * <title>.'"
    if lines[2] != GPL_COPYRIGHT:
        return 3, "banner line 3 is not '%s'" % GPL_COPYRIGHT.strip()
    return None


def check_banner(lines):
    if len(lines) < 5:
        return 1, "file has no 5-line banner"
    if not (lines[0].startswith("/* ") and lines[0].endswith(".")):
        return 1, "banner line 1 is not '/* <title>.'"
    if lines[1] != " *":
        return 2, "banner line 2 is not ' *'"
    if lines[2] != COPYRIGHT:
        return 3, "banner line 3 is not '%s'" % COPYRIGHT.strip()
    if not CREDIT.match(lines[3]):
        return 4, "banner line 4 does not credit SH2ZAM and SH4ZAM"
    if lines[4] != " */":
        return 5, "banner line 5 is not ' */'"
    return None


def check_names(code, path, library_header, public):
    """Public names use sh2o_/SH2O_, internal names use sh2oi_/SH2OI_. A
    static function in include/ may be public (sh2o_) or internal (sh2oi_)."""
    problems = []
    for number, text in enumerate(code, 1):
        define = re.match(r"#define\s+(\w+)", text)
        if define and library_header:
            name = define.group(1)
            if not name.startswith(("SH2O_", "SH2OI_")) and name != "SH2O_H":
                problems.append((number, "name", "macro %s has no SH2O_ prefix" % name))
        typedef = re.match(r"typedef\b.*?\b(\w+)\s*;", text)
        if typedef and library_header and not typedef.group(1).startswith("sh2o"):
            problems.append((number, "name", "type %s has no sh2o_ prefix" % typedef.group(1)))
        function = re.match(r"((?:static\s+|inline\s+|const\s+|extern\s+)*)[A-Za-z_][\w ]*?[ *]+(\w+)\(", text)
        if function and not text.startswith((" ", "#", "typedef", "return")):
            name = function.group(2)
            if "static" in function.group(1) or text.startswith(STATIC_MACROS):
                if public and not name.startswith(("sh2o_", "sh2oi_")):
                    problems.append((number, "name", "static function %s has no sh2o_ or sh2oi_ prefix" % name))
                elif not public and not name.startswith("sh2oi_"):
                    problems.append((number, "name", "internal function %s has no sh2oi_ prefix" % name))
            elif not name.startswith("sh2o_"):
                problems.append((number, "name", "public function %s has no sh2o_ prefix" % name))
    return problems


def asm_literals(lines, code, first, last):
    """The string literals of the lines first..last, comments left out (the
    code lines keep each literal's quotes at the same columns)."""
    literals = []
    for number in range(first, last + 1):
        for match in re.finditer(r'"[ ]*"', code[number - 1]):
            literals.append(lines[number - 1][match.start() + 1:match.end() - 1])
    return literals


def check_asm(lines, code, relative):
    problems = []
    mac_s, handled = False, False
    for first, last in asm_blocks(code):
        literals = asm_literals(lines, code, first, last)
        mac_s = mac_s or any(MAC_S_DEPENDENT.search(text) for text in literals)
        handled = handled or any(SR_S_HANDLED.search(text) for text in literals)
        block = "\n".join(lines[first - 1:last])
        if not ASM_FORM.search(lines[first - 1]):
            problems.append((first, "asm", "write inline assembly as '__asm__ volatile('"))
        for number in range(first, last + 1):
            for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', lines[number - 1]):
                if literal.count("\\n") > 1 or ";" in literal:
                    problems.append((number, "asm", "more than one instruction in one string"))
        if MAC_USE.search(block) and not ('"mach"' in block and '"macl"' in block):
            problems.append((first, "asm", "MAC or multiply code without \"mach\",\"macl\" clobbers"))
    if mac_s and not handled and relative not in SR_S_ASSUMED:
        problems.append((1, "asm", "MAC.L or MAC.W without an SR.S check (stc sr, clrs or sets)"))
    return problems


def check_c(path):
    relative = os.path.relpath(path, REPO)
    top = relative.split(os.sep)[0]
    library = top in LIBRARY_DIRS
    public = top == "include"
    with open(path, encoding="utf-8") as handle:
        whole = handle.read()
    lines = whole.split("\n")
    code, slashes, starts_in_comment = strip_code(lines)
    problems = []
    gpl = relative.replace(os.sep, "/").startswith(GPL_DIR)
    found = check_gpl_banner(lines) if gpl else check_banner(lines)
    if found:
        problems.append((found[0], "banner", found[1]))
    if path.endswith(".h"):
        guard = expected_guard(path)
        index = next((i for i, text in enumerate(code) if text.startswith("#ifndef")), None)
        if index is None or code[index].split()[1:2] != [guard] or \
                code[index + 1].strip() != "#define " + guard:
            problems.append((index + 1 if index is not None else 1, "guard",
                             "header guard is not '#ifndef %s' then '#define %s'" % (guard, guard)))
    for number in slashes:
        problems.append((number, "comment", "// comment; use /* */"))
    for number, line in enumerate(lines, 1):
        if "/**" in line and not public:
            problems.append((number, "doc-comment", "/** doc comments are for public headers only"))
        elif "/**" in line and not re.search(r"/\*\* \\brief |/\*\*< ", line):
            problems.append((number, "doc-comment", "use '/** \\brief' or '/**<'"))
        if public and DOXYGEN_TAG.search(line):
            problems.append((number, "doc-comment", "only \\brief and /**< are used"))
    asm = set()
    for first, last in asm_blocks(code):
        asm.update(range(first, last + 1))
    for number, (line, text) in enumerate(zip(lines, code), 1):
        if "\t" in line:
            problems.append((number, "tab", "tab character"))
        if line.rstrip() != line:
            problems.append((number, "space", "space at the end of the line"))
        if MARKER.search(line):
            problems.append((number, "marker", "TODO, FIXME or XXX"))
        if KEYWORD_SPACE.search(text):
            problems.append((number, "keyword-space", "write 'if(', not 'if ('"))
        if POINTER_LEFT.search(text):
            problems.append((number, "pointer", "put '*' next to the name, not the type"))
        if number not in asm and COMMA_SPACE.search(text):
            problems.append((number, "comma-space", "no space after a comma"))
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if text.strip() and not starts_in_comment[number - 1] and indent % 4:
            problems.append((number, "indent", "indent of %d spaces; use steps of 4" % indent))
        if library:
            include = re.match(r"#include\s+<([^>]+)>", line)
            if include and include.group(1) not in LIBRARY_HEADERS and \
                    not (relative == ZAM_SOURCE and include.group(1).startswith("sh2zam/")):
                problems.append((number, "freestanding", "library code includes <%s>" % include.group(1)))
            if FLOAT.search(text):
                problems.append((number, "freestanding", "floating point in library code"))
            if LIBC_CALL.search(text):
                problems.append((number, "freestanding", "C library call in library code"))
    if library:
        problems += check_names(code, path, public or path.endswith(".h"), public)
    problems += check_asm(lines, code, relative.replace(os.sep, "/"))
    return problems


def check_script(path):
    with open(path, encoding="utf-8") as handle:
        head = handle.read(600)
    if SCRIPT_COPYRIGHT not in head:
        return [(1, "banner", "script has no '%s' line near the top" % SCRIPT_COPYRIGHT)]
    return []


def check_md(path):
    problems = []
    with open(path, encoding="utf-8") as handle:
        for number, line in enumerate(handle, 1):
            if EM_DASH in line:
                problems.append((number, "em-dash", "em dash"))
            if EMOJI.search(line):
                problems.append((number, "emoji", "emoji"))
    return problems


def walk(root):
    if os.path.isfile(root):
        yield root
        return
    for directory, subdirs, files in os.walk(root):
        subdirs[:] = sorted(subdirs)
        for name in sorted(files):
            yield os.path.join(directory, name)


def public_entries():
    if not os.path.isfile(PUBLIC_LIST):
        return None
    with open(PUBLIC_LIST, encoding="utf-8") as handle:
        return [line.strip() for line in handle if line.strip() and not line.lstrip().startswith("#")]


def listed(entries, relative):
    if entries is None:
        return True
    return any(relative.startswith(entry) if entry.endswith("/") else fnmatch.fnmatchcase(relative, entry)
               for entry in entries)


def default_paths():
    paths, entries = [], public_entries()
    for top in C_DIRS + SCRIPT_DIRS:
        paths += list(walk(os.path.join(REPO, top))) if os.path.isdir(os.path.join(REPO, top)) else []
    paths += [os.path.join(REPO, name) for name in MD_ROOT if os.path.isfile(os.path.join(REPO, name))]
    for top in MD_DIRS:
        for path in walk(os.path.join(REPO, top)) if os.path.isdir(os.path.join(REPO, top)) else []:
            relative = os.path.relpath(path, REPO).replace(os.sep, "/")
            if path.endswith(".md") and listed(entries, relative):
                paths.append(path)
    return sorted(set(paths))


def main(argv):
    paths = []
    for root in argv[1:] or [None]:
        paths += default_paths() if root is None else list(walk(root))
    count = 0
    for path in paths:
        top = os.path.relpath(path, REPO).split(os.sep)[0]
        if path.endswith((".c", ".h")):
            problems = check_c(path)
        elif top in SCRIPT_DIRS and path.endswith((".py", ".sh")):
            problems = check_script(path)
        elif path.endswith(".md"):
            problems = check_md(path)
        else:
            continue
        for number, rule, text in problems:
            print("%s:%d: %s: %s" % (os.path.relpath(path, REPO), number, rule, text))
            count += 1
    print("style_check: %d file(s), %d problem(s)" % (len(paths), count))
    return 1 if count else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
