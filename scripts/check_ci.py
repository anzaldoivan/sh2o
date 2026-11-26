#!/usr/bin/env python3
"""Check the GitHub workflow jobs against the CI job rules.

Copyright (c) 2026 sh2o Contributors. MIT License.

Usage: python3 scripts/check_ci.py [--self-test | --names]

The rules are in CONTRIBUTING.md, section "CI jobs". The script checks rules
1, 2, 3, 6 and 9 on .github/workflows/*.yml, the Makefile, the list of
required check names in config/sh2o-publish.toml (when the file exists) and
the pin files in scripts/. It prints one line per problem, as
path:line: rule N: text, and exits 1 if it finds a problem. It reads only a
small part of YAML; for other YAML it prints the line and exits 2.
--names prints the name of each job row (one row per matrix combination).
--self-test checks a small fixture, then changes the fixture once per rule
and makes sure that each change gives a problem for that rule.
"""
import glob
import itertools
import os
import re
import shlex
import sys
import tomllib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = "config/sh2o-publish.toml"
SELF = "scripts/check_ci.py"
SOURCE = "(CONTRIBUTING.md, CI jobs)"

# The run steps that are not make calls and that a job may have.
SETUP = (
    "git submodule update --init --checkout --depth 1 external/psyz",
    'docker pull "$SH2_IMAGE"',
)
# These environment variables change only the result of these goals.
ENV_READ_BY = {"UBSAN_OPTIONS": {"sanitize"}, "ASAN_OPTIONS": {"sanitize"}}

MATRIX_EXPR = re.compile(r"\$\{\{\s*matrix\.([A-Za-z0-9_-]+)\s*\}\}")
ENV_EXPR = re.compile(r"\$\{\{\s*env\.([A-Za-z_]\w*)\s*\}\}")
SHELL_VAR = re.compile(r"\$\{([A-Za-z_]\w*)\}|\$([A-Za-z_]\w*)")
DIGEST = re.compile(r"@sha256:[0-9a-f]{64}$")
CACHE_KEY = re.compile(r"^[A-Za-z0-9._-]+-((?:sha256:)?(?:[0-9a-f]{64}|[0-9a-f]{40}))$")
MAKE_VAR = re.compile(r"^[A-Za-z_]\w*=")
MAKE_DEFAULT = re.compile(r"^([A-Za-z_]\w*)\s*\?=\s*(.*?)\s*$")
MAKE_IMAGE = re.compile(r"^(\w*IMAGE\w*)\s*[?:+]?=\s*(.*?)\s*$")
MAKE_RULE = re.compile(r"^([A-Za-z0-9_.-]+)\s*:(?!=)\s*(.*?)\s*$")


# ---------------------------------------------------------------- YAML reader

class YamlError(Exception):
    pass


class Map(dict):
    """A YAML mapping. line is the line of its first key; at[key] is the line of each key."""

    def __init__(self, line):
        super().__init__()
        self.line = line
        self.at = {}


class Seq(list):
    """A YAML sequence. line is the line of its first item; at[i] is the line of each item."""

    def __init__(self, line):
        super().__init__()
        self.line = line
        self.at = []


KEY = re.compile(r"""^([A-Za-z0-9_][^:#]*?|"[^"]*"|'[^']*')\s*:(?:\s+(.*))?$""")


def strip_comment(text):
    """Remove a comment: a # at the start or after a space, outside quotes."""
    quote = None
    index = 0
    while index < len(text):
        char = text[index]
        if quote == "'":
            if char == "'":
                if text[index + 1:index + 2] == "'":
                    index += 1
                else:
                    quote = None
        elif quote == '"':
            if char == "\\":
                index += 1
            elif char == '"':
                quote = None
        elif char in "'\"" and (index == 0 or text[index - 1] in " [,"):
            quote = char
        elif char == "#" and (index == 0 or text[index - 1] == " "):
            return text[:index]
        index += 1
    return text


def is_item(content):
    return content == "-" or content.startswith("- ")


class Reader:
    """Reads block mappings, block sequences, flow sequences, scalars and | blocks."""

    def __init__(self, path, text):
        self.path = path
        self.raw = text.split("\n")
        self.over = {}

    def fail(self, index, message):
        raise YamlError("%s:%d: unsupported YAML: %s" % (self.path, index + 1, message))

    def info(self, index):
        if index in self.over:
            return self.over[index]
        line = self.raw[index]
        body = line.lstrip(" ")
        content = strip_comment(body).rstrip()
        if "\t" in content or body.startswith("\t"):
            self.fail(index, "tab")
        if not content:
            return None
        return len(line) - len(body), content

    def skip(self, index):
        while index < len(self.raw) and self.info(index) is None:
            index += 1
        return index

    def read(self):
        for index, line in enumerate(self.raw):
            if re.match(r"^(---|\.\.\.)(\s|$)", line) or line.startswith("%"):
                self.fail(index, "document marker or directive")
        index = self.skip(0)
        if index >= len(self.raw):
            return Map(1)
        indent, content = self.info(index)
        if indent != 0 or is_item(content):
            self.fail(index, "the top level is not a mapping")
        doc, index = self.mapping(index, 0)
        index = self.skip(index)
        if index < len(self.raw):
            self.fail(index, "text after the top-level mapping")
        return doc

    def block(self, index, indent):
        content = self.info(index)[1]
        if is_item(content):
            return self.sequence(index, indent)
        return self.mapping(index, indent)

    def mapping(self, index, indent):
        result = Map(index + 1)
        while True:
            index = self.skip(index)
            if index >= len(self.raw):
                break
            here, content = self.info(index)
            if here < indent:
                break
            if here > indent:
                self.fail(index, "unexpected indentation")
            if is_item(content):
                self.fail(index, "a list item where a key is expected")
            if content.startswith("?"):
                self.fail(index, "complex key")
            match = KEY.match(content)
            if not match:
                self.fail(index, "expected 'key: value'")
            key = match.group(1)
            if key[0] in "'\"":
                key = key[1:-1]
            if key in result:
                self.fail(index, "key '%s' twice" % key)
            result.at[key] = index + 1
            result[key], index = self.value(index, indent, match.group(2) or "", True)
        return result, index

    def sequence(self, index, indent):
        result = Seq(index + 1)
        while True:
            index = self.skip(index)
            if index >= len(self.raw):
                break
            here, content = self.info(index)
            if here < indent or (here == indent and not is_item(content)):
                break
            if here > indent:
                self.fail(index, "unexpected indentation")
            rest = content[1:].lstrip(" ")
            result.at.append(index + 1)
            if is_item(rest):
                self.fail(index, "a list item in a list item on one line")
            if rest and not rest.startswith("?") and KEY.match(rest):
                inner = indent + len(content) - len(rest)
                self.over[index] = (inner, rest)
                item, index = self.mapping(index, inner)
            else:
                item, index = self.value(index, indent, rest, False)
            result.append(item)
        return result, index

    def value(self, index, indent, rest, in_mapping):
        if rest == "":
            following = self.skip(index + 1)
            if following < len(self.raw):
                here, content = self.info(following)
                if here > indent:
                    return self.block(following, here)
                if here == indent and in_mapping and is_item(content):
                    return self.sequence(following, here)
            return None, index + 1
        if rest[0] == "|":
            if rest not in ("|", "|-", "|+"):
                self.fail(index, "block indicator '%s'" % rest)
            return self.literal(index, indent, rest)
        if rest == "{}":
            result, index = Map(index + 1), index + 1
        elif rest[0] == "[":
            result, index = self.flow(index, rest), index + 1
        else:
            result, index = self.scalar(index, rest), index + 1
        following = self.skip(index)
        if following < len(self.raw) and self.info(following)[0] > indent:
            self.fail(following, "a value on more than one line")
        return result, index

    def literal(self, index, indent, rest):
        end = index + 1
        while end < len(self.raw):
            line = self.raw[end]
            if line.strip() and len(line) - len(line.lstrip(" ")) <= indent:
                break
            end += 1
        lines = self.raw[index + 1:end]
        while lines and not lines[-1].strip():
            lines.pop()
        # The caller continues after the block.
        after = index + 1 + len(lines)
        if not lines:
            return "", after
        first = next(line for line in lines if line.strip())
        margin = len(first) - len(first.lstrip(" "))
        text = "\n".join(line[margin:] for line in lines)
        return (text if rest == "|-" else text + "\n"), after

    def flow(self, index, rest):
        if not rest.endswith("]"):
            self.fail(index, "a flow list that does not end on its line")
        inner = rest[1:-1]
        if re.search(r"[\[\]{}]", inner):
            self.fail(index, "a nested flow collection")
        result = Seq(index + 1)
        parts, part, quote = [], "", None
        for char in inner:
            if quote:
                quote = None if char == quote else quote
            elif char in "'\"":
                quote = char
            elif char == ",":
                parts.append(part)
                part = ""
                continue
            part += char
        parts.append(part)
        for part in parts:
            if part.strip():
                result.append(self.scalar(index, part.strip()))
                result.at.append(index + 1)
        return result

    def scalar(self, index, text):
        if text[0] == '"':
            out, position = "", 1
            while position < len(text) and text[position] != '"':
                if text[position] == "\\" and position + 1 < len(text):
                    position += 1
                    out += {"n": "\n", "t": "\t"}.get(text[position], text[position])
                else:
                    out += text[position]
                position += 1
            if position != len(text) - 1:
                self.fail(index, "text after a quoted value")
            return out
        if text[0] == "'":
            out, position = "", 1
            while position < len(text):
                if text[position] == "'":
                    if text[position + 1:position + 2] == "'":
                        out += "'"
                        position += 2
                        continue
                    break
                out += text[position]
                position += 1
            if position != len(text) - 1:
                self.fail(index, "text after a quoted value")
            return out
        if text[0] in "&*!>{%@`":
            self.fail(index, "'%s'" % text[0])
        if text in ("~", "null"):
            return None
        return text


def read_yaml(path, text):
    return Reader(path, text).read()


# ---------------------------------------------------------------- job rows

class Row:
    """One job row: a job, or one matrix combination of a job."""

    def __init__(self, path, doc, job_id, job, combo, name):
        self.path, self.doc, self.job_id, self.job = path, doc, job_id, job
        self.combo, self.name = combo, name
        self.line = doc["jobs"].at[job_id]
        self.steps = []


def text(value):
    return "" if value is None else str(value)


def env_of(node, combo):
    env = node.get("env") if isinstance(node, Map) else None
    if not isinstance(env, Map):
        return {}
    return {name: MATRIX_EXPR.sub(lambda m: combo.get(m.group(1), ""), text(value)) for name, value in env.items()}


def scalar_of(path, line, value):
    if isinstance(value, (Map, Seq)):
        raise YamlError("%s:%d: unsupported YAML: a matrix value that is not a scalar" % (path, line))
    return text(value)


def expand(path, job):
    """Return the matrix combinations of a job: [{}] when it has no matrix."""
    strategy = job.get("strategy")
    matrix = strategy.get("matrix") if isinstance(strategy, Map) else None
    if matrix is None:
        return [{}], False
    if not isinstance(matrix, Map):
        raise YamlError("%s:%d: unsupported YAML: a matrix that is not a mapping" % (path, strategy.at["matrix"]))
    keys = [key for key in matrix if key not in ("include", "exclude")]
    lists = []
    for key in keys:
        if not isinstance(matrix[key], Seq):
            raise YamlError("%s:%d: unsupported YAML: matrix.%s is not a list" % (path, matrix.at[key], key))
        lists.append([scalar_of(path, matrix.at[key], value) for value in matrix[key]])
    combos = [dict(zip(keys, values)) for values in itertools.product(*lists)] if keys else []
    for row in matrix.get("exclude") or []:
        pairs = {key: scalar_of(path, row.line, value) for key, value in row.items()}
        combos = [combo for combo in combos if any(combo.get(key) != value for key, value in pairs.items())]
    added = []
    for row in matrix.get("include") or []:
        pairs = {key: scalar_of(path, row.line, value) for key, value in row.items()}
        joined = False
        # A row extends each original combination whose original values it keeps.
        for combo in combos:
            if all(combo[key] == value for key, value in pairs.items() if key in keys):
                combo.update(pairs)
                joined = True
        if not joined:
            added.append(dict(pairs))
    return combos + added, True


def make_rows(docs):
    rows, findings = [], []
    for path, doc in docs.items():
        jobs = doc.get("jobs")
        if not isinstance(jobs, Map):
            continue
        for job_id, job in jobs.items():
            if not isinstance(job, Map):
                continue
            combos, is_matrix = expand(path, job)
            for combo in combos:
                raw = job.get("name")
                name = text(raw) if raw is not None else job_id
                if is_matrix and not MATRIX_EXPR.search(name):
                    findings.append((path, jobs.at[job_id], 3, "job %s has a matrix, but its name has no "
                                     "matrix value, so the row names are not fixed by the file" % job_id))
                    name = "%s (%s)" % (name, ", ".join(combo.values()))
                name = MATRIX_EXPR.sub(lambda m: combo.get(m.group(1), ""), name)
                row = Row(path, doc, job_id, job, combo, name)
                base = env_of(doc, combo)
                base.update(env_of(job, combo))
                for index, step in enumerate(job.get("steps") or []):
                    if not isinstance(step, Map):
                        continue
                    env = dict(base)
                    env.update(env_of(step, combo))
                    row.steps.append((step, job["steps"].at[index], env))
                rows.append(row)
    return rows, findings


# ---------------------------------------------------------------- commands

def resolve(value, env):
    value = ENV_EXPR.sub(lambda m: env.get(m.group(1), m.group(0)), value)
    return SHELL_VAR.sub(lambda m: env.get(m.group(1) or m.group(2), m.group(0)), value)


def is_make(command):
    words = command.split()
    return bool(words) and words[0] == "make"


def parse_make(command):
    """Return (goals, [(name, value)], problems, tee file or None) of a make command."""
    command = command.strip()
    tee = None
    match = re.match(r"^(.*?)\s*\|\s*tee\s+(\S+)\s*$", command, re.S)
    if match:
        command, tee = match.group(1), match.group(2)
    problems = []
    if re.search(r"[|;&<>`\n]|\$\(", command):
        problems.append("the make step has shell operators")
    try:
        words = shlex.split(command)
    except ValueError:
        problems.append("the make step has a quote that does not close")
        words = command.split()
    goals, variables = [], []
    for word in words[1:]:
        if word.startswith("-"):
            problems.append("the make step has the option %s" % word)
        elif MAKE_VAR.match(word):
            variables.append(tuple(word.split("=", 1)))
        else:
            goals.append(word)
    if len(goals) != 1:
        problems.append("the make step has %d goals, not one" % len(goals))
    return goals, variables, problems, tee


def run_key(goals, pairs, defaults):
    goal = goals[0] if len(goals) == 1 else " ".join(goals)
    kept = sorted("%s=%s" % (name, value) for name, value in pairs.items() if defaults.get(name) != value)
    return goal, tuple(kept)


def show(key):
    return " ".join(("make", key[0]) + key[1])


# ---------------------------------------------------------------- rules

def rule1(rows):
    findings = []
    for row in rows:
        makes = 0
        for step, line, env in row.steps:
            if "run" not in step:
                continue
            line = step.at["run"]
            command = MATRIX_EXPR.sub(lambda m: row.combo.get(m.group(1), ""), text(step["run"])).strip()
            if is_make(command):
                makes += 1
                goals, variables, problems, tee = parse_make(command)
                if tee and text(step.get("shell")) != "bash":
                    problems.append("the make output goes to tee, but the step has no 'shell: bash', "
                                    "so a make failure does not stop the job")
                for problem in problems:
                    findings.append((row.path, line, 1, problem))
            elif command not in SETUP:
                findings.append((row.path, line, 1, "the run step '%s' is not a make call and is not "
                                 "in the setup list" % command.split("\n")[0]))
        if makes != 1:
            findings.append((row.path, row.line, 1, "job row '%s' has %d make steps, not one" % (row.name, makes)))
    return findings


def makefile_defaults(makefile):
    defaults = {}
    for line in makefile.split("\n"):
        match = MAKE_DEFAULT.match(line)
        if match:
            defaults[match.group(1)] = match.group(2)
    return defaults


def makefile_runs(makefile, defaults):
    """Return [(key, line)] of the ci recipe, or None when there is no ci rule."""
    lines = makefile.split("\n")
    rules = {}
    for number, line in enumerate(lines, 1):
        match = MAKE_RULE.match(line)
        if match and match.group(1) not in rules:
            rules[match.group(1)] = (number, match.group(2).split())
    if "ci" not in rules:
        return None
    ci_line, prerequisites = rules["ci"]
    runs = []
    for name in prerequisites:
        line, inner = rules.get(name, (ci_line, []))
        for goal in inner or [name]:
            runs.append(((goal, ()), line))
    for number in range(ci_line, len(lines)):
        line = lines[number]
        if not line.startswith("\t"):
            break
        match = re.match(r"^\$\(MAKE\)\s+(.*)$", line.strip())
        if match:
            goals, variables, _, _ = parse_make("make " + match.group(1))
            runs.append((run_key(goals, dict(variables), defaults), number + 1))
    return runs


def rule2(rows, makefile):
    defaults = makefile_defaults(makefile)
    flow = []
    for row in rows:
        for step, line, env in row.steps:
            if "run" not in step:
                continue
            command = MATRIX_EXPR.sub(lambda m: row.combo.get(m.group(1), ""), text(step["run"])).strip()
            if not is_make(command):
                continue
            goals, variables, _, _ = parse_make(command)
            goal = goals[0] if len(goals) == 1 else " ".join(goals)
            pairs = {name: value for name, value in env.items()
                     if name not in ENV_READ_BY or goal in ENV_READ_BY[name]}
            for name, value in variables:
                pairs[name] = resolve(value, env)
            flow.append((run_key(goals, pairs, defaults), row.path, step.at["run"], row.name))
    local = makefile_runs(makefile, defaults)
    if local is None:
        return [("Makefile", 1, 2, "the Makefile has no ci rule")]
    findings = []
    for key in sorted({entry[0] for entry in flow} | {entry[0] for entry in local}):
        in_flow = [entry for entry in flow if entry[0] == key]
        in_local = [entry for entry in local if entry[0] == key]
        for _, path, line, name in in_flow[len(in_local):]:
            findings.append((path, line, 2, "'%s' runs in job row '%s', but not in the Makefile ci recipe"
                             % (show(key), name)))
        for _, line in in_local[len(in_flow):]:
            findings.append(("Makefile", line, 2, "'%s' runs in the Makefile ci recipe, but not in a workflow"
                             % show(key)))
    return findings


def rule3(rows, checks, check_lines):
    findings = []
    names = {}
    for row in rows:
        names.setdefault(row.name, []).append(row)
    for name, same in names.items():
        for row in same[1:]:
            findings.append((row.path, row.line, 3, "the row name '%s' is not unique" % name))
    for name in checks:
        count = len(names.get(name, []))
        if count != 1:
            findings.append((CONFIG, check_lines.get(name, 1), 3, "the required check '%s' is the name of %d job "
                             "rows, not one" % (name, count)))
    return findings


def image_problem(path, line, what, ref):
    if DIGEST.search(ref):
        return []
    return [(path, line, 6, "%s '%s' is not given by @sha256:<digest>" % (what, ref))]


def rule6(docs, rows, makefile):
    findings = []
    for path, doc in docs.items():
        if "permissions" not in doc:
            findings.append((path, 1, 6, "the workflow has no top-level permissions"))
        else:
            permissions = doc["permissions"]
            line = doc.at["permissions"]
            if isinstance(permissions, Map):
                for scope, value in permissions.items():
                    if text(value) not in ("read", "none"):
                        findings.append((path, permissions.at[scope], 6, "permission %s is %s, not read or none"
                                         % (scope, text(value))))
            elif text(permissions) != "read-all":
                findings.append((path, line, 6, "the top-level permissions are '%s', not read" % text(permissions)))
        findings += env_images(path, doc)
        jobs = doc.get("jobs") if isinstance(doc.get("jobs"), Map) else Map(1)
        for job_id, job in jobs.items():
            if not isinstance(job, Map):
                continue
            line = jobs.at[job_id]
            if "timeout-minutes" not in job:
                findings.append((path, line, 6, "job %s has no timeout-minutes" % job_id))
            permissions = job.get("permissions")
            if isinstance(permissions, Map):
                for scope, value in permissions.items():
                    if text(value) == "write":
                        findings.append((path, permissions.at[scope], 6, "job %s gives write permission %s"
                                         % (job_id, scope)))
            elif text(permissions) == "write-all":
                findings.append((path, job.at["permissions"], 6, "job %s gives write-all" % job_id))
            container = job.get("container")
            if isinstance(container, Map) and "image" in container:
                findings += image_problem(path, container.at["image"], "the container image", text(container["image"]))
            elif container is not None and not isinstance(container, Map):
                findings += image_problem(path, job.at["container"], "the container image", text(container))
            services = job.get("services")
            if isinstance(services, Map):
                for service, body in services.items():
                    if isinstance(body, Map) and "image" in body:
                        findings += image_problem(path, body.at["image"], "the image of service %s" % service,
                                                  text(body["image"]))
            findings += env_images(path, job)
            for step in job.get("steps") or []:
                if isinstance(step, Map):
                    findings += env_images(path, step)
    for row in rows:
        for step, line, env in row.steps:
            command = MATRIX_EXPR.sub(lambda m: row.combo.get(m.group(1), ""), text(step.get("run"))).strip()
            words = command.split()
            if words[:2] == ["docker", "pull"] and len(words) > 2:
                ref = resolve(words[2].strip("'\""), env)
                findings += image_problem(row.path, step.at["run"], "the pulled image", ref)
    for number, line in enumerate(makefile.split("\n"), 1):
        match = MAKE_IMAGE.match(line)
        if match:
            findings += image_problem("Makefile", number, "the image %s" % match.group(1), match.group(2))
    return findings


def env_images(path, node):
    env = node.get("env")
    findings = []
    if isinstance(env, Map):
        for name, value in env.items():
            if "IMAGE" in name:
                findings += image_problem(path, env.at[name], "the image %s" % name, text(value))
    return findings


def rule9(docs, rows, pin_sources):
    findings = []
    psyq_jobs = set()
    for row in rows:
        for step, line, env in row.steps:
            command = MATRIX_EXPR.sub(lambda m: row.combo.get(m.group(1), ""), text(step.get("run"))).strip()
            if is_make(command) and "psyq47" in parse_make(command)[0]:
                psyq_jobs.add((row.path, row.job_id))
    for path, doc in docs.items():
        jobs = doc.get("jobs") if isinstance(doc.get("jobs"), Map) else Map(1)
        for job_id, job in jobs.items():
            if not isinstance(job, Map):
                continue
            for index, step in enumerate(job.get("steps") or []):
                if not isinstance(step, Map) or not text(step.get("uses")).startswith("actions/cache"):
                    continue
                line = job["steps"].at[index]
                options = step.get("with") if isinstance(step.get("with"), Map) else Map(line)
                key = options.get("key")
                match = CACHE_KEY.match(key) if isinstance(key, str) else None
                if not match:
                    findings.append((path, options.at.get("key", line), 9, "the cache key '%s' is not "
                                     "<name>-<pin>" % text(key)))
                elif not any(match.group(1) in source for source in pin_sources.values()):
                    findings.append((path, options.at["key"], 9, "the cache key pin %s is not in a pin file"
                                     % match.group(1)))
                if "restore-keys" in options:
                    findings.append((path, options.at["restore-keys"], 9, "the cache step has restore-keys"))
                for entry in text(options.get("path")).split("\n"):
                    entry = entry.strip()
                    if entry == "build" or entry.startswith(("build/", "./build/")):
                        findings.append((path, options.at["path"], 9, "the cache path %s is under build/" % entry))
                if "psyq47" in os.path.basename(path) or (path, job_id) in psyq_jobs:
                    findings.append((path, line, 9, "the PsyQ 4.7 archive job must not use a cache"))
    return findings


def check_all(workflows, makefile, checks, pin_sources, check_lines=None):
    """Return (findings, row names, notes). findings: sorted (path, line, rule, text)."""
    docs = {path: read_yaml(path, body) for path, body in sorted(workflows.items())}
    rows, findings = make_rows(docs)
    notes = []
    if checks is None:
        # Without the list, only the list part is skipped; the names must still be unique.
        notes.append("rule 3: skipped: no list of required check names")
    findings += rule3(rows, checks or [], check_lines or {})
    findings += rule1(rows)
    findings += rule2(rows, makefile)
    findings += rule6(docs, rows, makefile)
    findings += rule9(docs, rows, pin_sources)
    unique = sorted(set(findings), key=lambda item: (item[2], item[0], item[1], item[3]))
    return unique, [row.name for row in rows], notes


def show_finding(finding):
    path, line, rule, message = finding
    return "%s:%d: rule %d: %s %s" % (path, line, rule, message, SOURCE)


# ---------------------------------------------------------------- self-test

PIN = "0123456789abcdef0123456789abcdef01234567"
IMAGE = "example/sdk@sha256:" + "ab" * 32

FIXTURE_MAKEFILE = """\
PYTHON ?= python3
MODE ?= plain
SH2_IMAGE ?= %s
TOOL_PIN ?= %s

check: style lint

ci: check
\t$(MAKE) test CC=gcc
\t$(MAKE) test CC=clang
\t$(MAKE) test CC=gcc MODE=fast
\t$(MAKE) cross
""" % (IMAGE, PIN)

FIXTURE_CHECKS = """\
# Checks that need no compiler.
name: checks
on:
  push:
permissions:
  contents: read
jobs:
  style:
    runs-on: ubuntu-24.04
    timeout-minutes: 5
    steps:
      - uses: actions/checkout@v4
      - run: make style
  lint:
    name: lint the scripts
    runs-on: ubuntu-24.04
    timeout-minutes: 5
    steps:
      - uses: actions/checkout@v4
      - name: Keep the tool
        uses: actions/cache@v4
        with:
          key: tool-%s
          path: |
            .tools/
      - run: make lint  # the lint goal
""" % PIN

FIXTURE_HOST = """\
name: host
on: [push, pull_request]
permissions: read-all
jobs:
  host:
    name: "${{ matrix.mode }} test (${{ matrix.cc }})"
    runs-on: ubuntu-24.04
    timeout-minutes: 10
    strategy:
      fail-fast: false
      matrix:
        cc: [gcc, clang]
        mode: ['plain']
        include:
          - cc: gcc
            mode: fast
          - cc: clang
            label: c
    env:
      UBSAN_OPTIONS: halt_on_error=1
    steps:
      - uses: actions/checkout@v4
      - run: make test CC=${{ matrix.cc }} MODE=${{ matrix.mode }}
"""

FIXTURE_CROSS = """\
name: cross
on:
  push:
permissions:
  contents: read
env:
  SH2_IMAGE: '%s'
jobs:
  cross:
    runs-on: ubuntu-24.04
    timeout-minutes: 20
    steps:
      - uses: actions/checkout@v4
      - run: docker pull "$SH2_IMAGE"
      - name: make cross
        shell: bash
        run: make cross SH2_IMAGE="${{ env.SH2_IMAGE }}" | tee cross.txt
""" % IMAGE

FIXTURE_NAMES = ["cross", "style", "lint the scripts", "plain test (gcc)", "plain test (clang)", "fast test (gcc)"]


def fixture():
    return {
        "workflows": {".github/workflows/checks.yml": FIXTURE_CHECKS,
                      ".github/workflows/host.yml": FIXTURE_HOST,
                      ".github/workflows/cross.yml": FIXTURE_CROSS},
        "makefile": FIXTURE_MAKEFILE,
        "checks": list(FIXTURE_NAMES),
        "pins": {"Makefile": FIXTURE_MAKEFILE},
    }


def plant(data, where, old, new):
    if where == "makefile":
        assert old in data["makefile"]
        data["makefile"] = data["makefile"].replace(old, new, 1)
    else:
        assert old in data["workflows"][where]
        data["workflows"][where] = data["workflows"][where].replace(old, new, 1)
    return data


def plants():
    checks = ".github/workflows/checks.yml"
    cross = ".github/workflows/cross.yml"
    yield 1, "tee without 'shell: bash'", plant(fixture(), cross, "        shell: bash\n", "")
    yield 2, "one ci line less in the Makefile", plant(fixture(), "makefile", "\t$(MAKE) cross\n", "")
    data = fixture()
    data["checks"].append("nightly")
    yield 3, "a required check that no job row has", data
    yield 6, "a job without timeout-minutes", plant(fixture(), checks, "    timeout-minutes: 5\n    steps", "    steps")
    yield 9, "a cache step with restore-keys", plant(fixture(), checks, "          path: |",
                                                     "          restore-keys: tool-\n          path: |")


def self_test():
    bad = 0
    data = fixture()
    findings, names, _ = check_all(data["workflows"], data["makefile"], data["checks"], data["pins"])
    if findings or sorted(names) != sorted(FIXTURE_NAMES):
        print("check_ci: self-test, the fixture without a change FAILS:")
        for finding in findings:
            print("  " + show_finding(finding))
        if sorted(names) != sorted(FIXTURE_NAMES):
            print("  row names %s" % names)
        return 1
    print("check_ci: self-test, the fixture without a change passes")
    for rule, what, data in plants():
        findings, _, _ = check_all(data["workflows"], data["makefile"], data["checks"], data["pins"])
        if any(finding[2] == rule for finding in findings):
            print("check_ci: self-test, rule %d: %s: caught" % (rule, what))
        else:
            print("check_ci: self-test, rule %d: %s: NOT caught" % (rule, what))
            bad += 1
    return 1 if bad else 0


# ---------------------------------------------------------------- main

def read(path):
    with open(os.path.join(REPO, path), encoding="utf-8", errors="replace") as handle:
        return handle.read()


def main(argv):
    try:
        if "--self-test" in argv[1:]:
            return self_test()
        workflows = {}
        for full in sorted(glob.glob(os.path.join(REPO, ".github", "workflows", "*.y*ml"))):
            relative = os.path.relpath(full, REPO).replace(os.sep, "/")
            workflows[relative] = read(relative)
        makefile = read("Makefile")
        checks, check_lines = None, {}
        if os.path.exists(os.path.join(REPO, CONFIG)):
            config = read(CONFIG)
            checks = tomllib.loads(config).get("public", {}).get("checks")
            for number, line in enumerate(config.split("\n"), 1):
                for name in checks or []:
                    if '"%s"' % name in line:
                        check_lines.setdefault(name, number)
        pins = {"Makefile": makefile}
        for full in sorted(glob.glob(os.path.join(REPO, "scripts", "*"))):
            relative = os.path.relpath(full, REPO).replace(os.sep, "/")
            if os.path.isfile(full) and relative != SELF:
                pins[relative] = read(relative)
        findings, names, notes = check_all(workflows, makefile, checks, pins, check_lines)
    except YamlError as error:
        print(error)
        return 2
    if "--names" in argv[1:]:
        for name in names:
            print(name)
        return 0
    for note in notes:
        print(note)
    for finding in findings:
        print(show_finding(finding))
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
