#!/usr/bin/env python3
"""Check that a tree holds only files and text that may be public.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/check_public.py [--repo DIR] [TREE]
       python3 scripts/check_public.py --self-test

TREE is a directory or a git revision of --repo (default: this repository).
The private files are always read from this checkout.
With no TREE, the script checks HEAD. When config/public-paths.txt exists,
the script checks only the files that the list selects at HEAD; with a TREE,
every file of the tree must match an entry of the list.

The script fails on:
- a path outside config/public-paths.txt (when the list exists);
- a denied path: the generic agent files, the deny list of
  config/sh2o-publish.toml and the files of .claude/pa3-managed.json;
- text that cites a work-tracking id: a task id, a report id, a backlog or
  technique id, a phase id, or the name of a denied file or of a file that
  the names list of config/sh2o-publish.toml gives (a name its any_case list
  also gives matches in any case, with an optional space, _ or - at each
  break: SoulRE also matches soulre, Soul_RE and SOUL-RE);
- in a comment, or in a Markdown or text file: a rule id or the name of a
  language model or an agent. The root file AGENTS.md, and the token
  AGENTS.md in any file, pass the agent-name check.

When the private files do not exist (in the public repository), the
script checks the generic agent files and the ids only. It prints one line
per problem, as path:line: rule: text, and exits 1 if it finds a problem.
"""
import fnmatch
import json
import os
import re
import subprocess
import sys
import tempfile
import tomllib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIST = "config/public-paths.txt"
CONFIG = "config/sh2o-publish.toml"
MANAGED = ".claude/pa3-managed.json"
# This file names the patterns it looks for, so its own text is not scanned.
SELF = "scripts/check_public.py"
AGENT_FILES = ("CLAUDE.md", "CLAUDE.local.md", ".claude/", ".claude-state/", ".mcp.json", ".run/")

ANYWHERE = (
    ("task-id", re.compile(r"\bT\d{1,3}(?:\.c\d+)?\b")),
    ("report-id", re.compile(r"\bR\d+\.\d+-\d{3}\b|\bR\d{3}\b")),
    ("backlog-id", re.compile(r"\bB-\d{3}\b")),
    ("technique-id", re.compile(r"\bC\d{4}\b")),
    ("phase-id", re.compile(r"\b\d+\.\d+/T\d+\b|\b[Pp]hase \d+\.\d+\b")),
)
IN_COMMENTS = (
    ("rule-id", re.compile(r"\b[HMPX]\d{1,2}\b")),
    ("model-word", re.compile(r"\b(?:claude|anthropic|opus|sonnet|haiku|fable|chatgpt|gpt-?\d\w*|llms?|"
                              r"copilot|codex|gemini|subagents?|agents?)\b", re.I)),
)
# The root AGENTS.md is public and addresses agents: the model-word rule skips that file, and the token
# AGENTS.md in any file.
AGENTS_FILE = "AGENTS.md"
AGENTS_TOKEN = re.compile(r"\bAGENTS\.md\b")
C_COMMENT = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
HASH_COMMENT = re.compile(r"(?:^|\s)#[^\n]*")
PROSE = (".md", ".txt", ".rst")
C_LIKE = (".c", ".h", ".cpp", ".hpp", ".s", ".S")


def git(repo, *args, data=None):
    """Run git in repo. With data, feed it on stdin and return bytes."""
    result = subprocess.run(["git", "-C", repo, *args], capture_output=True,
                            input=data.encode() if data is not None else None)
    if result.returncode != 0:
        raise SystemExit("check_public: git %s: %s" % (" ".join(args[:2]), result.stderr.decode().strip()))
    return result.stdout if data is not None else result.stdout.decode()


def entries(text):
    return [line.strip() for line in text.splitlines() if line.strip() and not line.lstrip().startswith("#")]


def matches(patterns, path):
    for entry in patterns:
        if entry.endswith("/") and path.startswith(entry):
            return True
        if not entry.endswith("/") and fnmatch.fnmatchcase(path, entry):
            return True
    return False


def read_private(repo):
    """Return (the list or None, the deny entries, the denied file names)."""
    def text(rel):
        path = os.path.join(repo, rel)
        if os.path.isfile(path):
            with open(path, encoding="utf-8") as handle:
                return handle.read()
        return None
    listed = text(LIST)
    deny = list(AGENT_FILES)
    config = text(CONFIG)
    extra, any_case = [], set()
    if config is not None:
        table = tomllib.loads(config).get("public", {})
        deny += table.get("deny", [])
        extra = table.get("names", [])
        any_case = set(table.get("any_case", []))
    managed = text(MANAGED)
    if managed is not None:
        deny += sorted(json.loads(managed).get("files", {}))
    names = sorted({os.path.basename(entry.rstrip("/")) for entry in deny if "." in os.path.basename(entry)} | set(extra))
    return (entries(listed) if listed is not None else None), deny, [(name, name in any_case) for name in names]


BREAK = re.compile(r"[\s_-]+|(?<=[a-z])(?=[A-Z])|(?<=[A-Za-z])(?=\d)|(?<=\d)(?=[A-Za-z])")


def name_parts(name):
    """Return the parts of a name between its breaks: separators, a lower to
    upper case change, a letter to digit change."""
    return [part for part in BREAK.split(name) if part]


def name_pattern(name, any_case):
    if not any_case:
        return re.escape(name)
    return "(?i:%s)" % r"[\s_-]?".join(re.escape(part) for part in name_parts(name))


def tree_files(repo, tree):
    """Return {path: text or None} for a directory or a git revision of repo."""
    files = {}
    if tree is not None and os.path.isdir(tree):
        for directory, subdirs, names in os.walk(tree):
            subdirs[:] = sorted(name for name in subdirs if name != ".git")
            for name in sorted(names):
                path = os.path.join(directory, name)
                with open(path, "rb") as handle:
                    files[os.path.relpath(path, tree).replace(os.sep, "/")] = handle.read()
        return files
    rev = tree or "HEAD"
    blobs = []
    for record in git(repo, "ls-tree", "-r", "-z", rev).split("\0"):
        if record:
            meta, path = record.split("\t", 1)
            mode, _, sha = meta.split()
            files[path] = None
            if mode != "160000":
                blobs.append((path, sha))
    if blobs:
        out = git(repo, "cat-file", "--batch", data="\n".join(sha for _, sha in blobs) + "\n")
        position = 0
        for path, _ in blobs:
            end = out.index(b"\n", position)
            size = int(out[position:end].split()[2])
            files[path] = out[end + 1:end + 1 + size]
            position = end + 1 + size + 1
    return files


def comments(path, text):
    """Return the comment text of a source file, with its newlines kept."""
    if path.endswith(PROSE):
        return text
    pattern = C_COMMENT if path.endswith(C_LIKE) else HASH_COMMENT
    kept = [" " if ch != "\n" else ch for ch in text]
    for match in pattern.finditer(text):
        kept[match.start():match.end()] = text[match.start():match.end()]
    return "".join(kept)


def scan(path, text, names):
    problems = []
    rules = [(rule, pattern, text) for rule, pattern in ANYWHERE]
    if names:
        named = re.compile(r"(?<![\w.-])(?:%s)(?![\w-])" % "|".join(name_pattern(*name) for name in names))
        rules.append(("private-file-name", named, text))
    scoped = comments(path, text)
    unnamed = AGENTS_TOKEN.sub(lambda match: " " * len(match.group(0)), scoped)
    for rule, pattern in IN_COMMENTS:
        if rule != "model-word":
            rules.append((rule, pattern, scoped))
        elif path != AGENTS_FILE:
            rules.append((rule, pattern, unnamed))
    for rule, pattern, body in rules:
        for match in pattern.finditer(body):
            line = body.count("\n", 0, match.start()) + 1
            problems.append((line, rule, match.group(0)))
    return sorted(problems)


def check(private, repo, tree):
    """Check tree (a directory, a revision of repo, or None for HEAD) with the
    private files of the checkout private."""
    listed, deny, names = read_private(private)
    files = tree_files(repo, tree)
    problems = []
    for path in sorted(files):
        if tree is None and listed is not None and not matches(listed, path):
            continue
        if matches(deny, path):
            problems.append((path, 0, "denied-path", "never public"))
            continue
        if listed is not None and not matches(listed, path):
            problems.append((path, 0, "unlisted-path", "matched by no entry of %s" % LIST))
            continue
        data = files[path]
        if data is None or path == SELF or b"\0" in data:
            continue
        text = data.decode("utf-8", errors="replace")
        problems += [(path, line, rule, found) for line, rule, found in scan(path, text, names)]
    checked = sum(1 for path in files if not (tree is None and listed is not None and not matches(listed, path)))
    return checked, problems


def report(checked, problems):
    for path, line, rule, found in problems:
        print("%s:%d: %s: %s" % (path, line, rule, found))
    print("check_public: %d file(s), %d problem(s)" % (checked, len(problems)))
    return 1 if problems else 0


def self_test():
    """Plant one case of each class in a fixture tree and expect its rule,
    then expect no problem in a clean file."""
    cases = {
        "notes/plan.txt": ("unlisted-path", "a file outside the list\n"),
        "secret/key.txt": ("denied-path", "a denied file\n"),
        "tools/helper.py": ("denied-path", "a managed file\n"),
        "src/task.c": ("task-id", "/* Done in T12. */\n"),
        "src/coder.c": ("task-id", "int x; /* see T12.c3 */\n"),
        "docs/report.md": ("report-id", "As found in R3.1-016.\n"),
        "docs/short.md": ("report-id", "As found in R557.\n"),
        "docs/backlog.md": ("backlog-id", "Closes B-017.\n"),
        "docs/technique.md": ("technique-id", "Uses C0206.\n"),
        "src/rule.c": ("rule-id", "int y; // keep M27\n"),
        "scripts/phase.py": ("phase-id", "VALUE = 1  # from 2.5/T10\n"),
        "docs/notes.md": ("private-file-name", "See PLAN_NOTES.md.\n"),
        "docs/named.md": ("private-file-name", "See NOTES_PLAN.\n"),
        "docs/anycase.md": ("private-file-name", "Ported from moon_fish 3.\n"),
        "src/model.c": ("model-word", "/* Written with Claude. */\n"),
        "include/agent.h": ("model-word", "/* The agent wrote this. */\n"),
    }
    clean = {"src/clean.c": "/* A clean file: R0 and T are fine, and so is the code below. */\nint M27;\n",
             "docs/case.md": "Only the any_case names match in lower case: notes_plan, moonfish3x.\n",
             "AGENTS.md": "Agents: use the package. An agent never copies one file.\n",
             "docs/cite.md": "Read AGENTS.md before you use the library.\n"}
    listed = "AGENTS.md\nsrc/\ninclude/\ndocs/\nscripts/\ntools/\nsecret/\n"
    failures = 0
    with tempfile.TemporaryDirectory() as root:
        for rel, body in [(LIST, listed), (CONFIG, '[public]\ndeny = ["secret/"]\nnames = ["NOTES_PLAN", "MoonFish3"]\n'
                                                   'any_case = ["MoonFish3"]\n'),
                          (MANAGED, json.dumps({"files": {"tools/helper.py": {}, "PLAN_NOTES.md": {}}}))]:
            os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
            with open(os.path.join(root, rel), "w", encoding="utf-8") as handle:
                handle.write(body)
        for rel, (rule, body) in list(cases.items()) + [(rel, (None, body)) for rel, body in clean.items()]:
            tree = os.path.join(root, "tree")
            if os.path.isdir(tree):
                subprocess.run(["rm", "-rf", tree], check=True)
            os.makedirs(os.path.dirname(os.path.join(tree, rel)))
            with open(os.path.join(tree, rel), "w", encoding="utf-8") as handle:
                handle.write(body)
            found = {problem[2] for problem in check(root, root, tree)[1]}
            good = rule in found if rule else not found
            failures += not good
            print("self-test %s: %s: expected %s, found %s" % ("ok" if good else "FAIL", rel, rule or "nothing",
                                                                ", ".join(sorted(found)) or "nothing"))
    planted, planted_failures = self_test_private()
    failures += planted_failures
    print("check_public self-test: %d case(s), %d failure(s)" % (len(cases) + len(clean) + planted, failures))
    return 1 if failures else 0


def variants(name):
    """Return a name as written, in lower and upper case, and with each
    separator (none, space, _, -) at its breaks."""
    parts = name_parts(name)
    found = [name, name.lower(), name.upper()] + [sep.join(parts).lower() for sep in ("", " ", "_", "-")]
    return sorted(set(found))


def self_test_private():
    """Plant each any_case name of this checkout's private config, in each of
    its variants, at run time, so that no public file holds a name. Skip when
    the config is absent, as in the public repository."""
    path = os.path.join(REPO, CONFIG)
    if not os.path.isfile(path):
        print("self-test skip: private names: no %s" % CONFIG)
        return 0, 0
    with open(path, encoding="utf-8") as handle:
        any_case = tomllib.loads(handle.read()).get("public", {}).get("any_case", [])
    count = failures = 0
    with tempfile.TemporaryDirectory() as root:
        os.makedirs(os.path.join(root, "config"))
        with open(os.path.join(root, CONFIG), "w", encoding="utf-8") as handle:
            handle.write("[public]\nnames = %s\nany_case = %s\n" % (json.dumps(any_case), json.dumps(any_case)))
        for index, name in enumerate(any_case):
            tree = os.path.join(root, "tree%d" % index)
            os.makedirs(os.path.join(tree, "docs"))
            planted = variants(name)
            for number, variant in enumerate(planted):
                with open(os.path.join(tree, "docs", "v%d.md" % number), "w", encoding="utf-8") as handle:
                    handle.write("Text before %s and after.\n" % variant)
            hit = {problem[0] for problem in check(root, root, tree)[1] if problem[2] == "private-file-name"}
            missed = [variant for number, variant in enumerate(planted) if "docs/v%d.md" % number not in hit]
            count += 1
            failures += bool(missed)
            print("self-test %s: private name %d: %d variant(s)%s" % ("FAIL" if missed else "ok", index, len(planted),
                                                                    ", missed " + ", ".join(missed) if missed else ""))
    return count, failures


def main(argv):
    if argv[1:] == ["--self-test"]:
        return self_test()
    args, repo, tree = argv[1:], REPO, None
    while args:
        if args[0] == "--repo" and len(args) > 1:
            repo, args = os.path.abspath(args[1]), args[2:]
        elif tree is None and not args[0].startswith("-"):
            tree, args = args[0], args[1:]
        else:
            print(__doc__.split("\n\n")[2])
            return 2
    return report(*check(REPO, repo, tree))


if __name__ == "__main__":
    sys.exit(main(sys.argv))
