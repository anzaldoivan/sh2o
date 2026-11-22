#!/bin/sh
# Cross build of the library for the SH-2, run inside a Yaul toolchain image.
# Copyright (c) 2026 sh2o Contributors. MIT License.
#
# Usage: sh scripts/sh2_cross.sh   (from the repository root; make cross runs it)
#
# The script builds src/*.c two times: native (SH2O_REQUIRE_SH2) and portable
# (SH2O_FORCE_PORTABLE). For each build it checks that every object is
# big-endian SH-2 code and that the library needs no C library function.
# Reports go to build/sh2-<mode>/.
# SH2O_COUNT=ON (environment; make cross SH2O_COUNT=ON): both builds with
# -DSH2O_COUNT=1 (sh2o/count.h) into build/sh2-count-<mode>/, the same checks,
# and no static check (the specs pin the OFF objects).
#
# Then it runs each family's static check, tests/sh2/<f>_static.txt: it builds
# the spec's probe source once per level and checks the object's code. Reports
# go to build/sh2-static/<f>/<level without the dash>/. A failure names the
# family, level, function and rule; any failure fails the script at the end.
#
# Spec file: one directive per line; '#' starts a comment; blank lines are
# ignored; names are C names (the script adds the ELF '_').
#   source <path>                 probe source (required)
#   flags <cflags...>             added to the cross flags
#   levels <-Ox...>               optimisation levels, one build each (default -O2)
#   undefined none                the object has no undefined symbol
#   undefined <name...>           each undefined symbol is one of these
#   undefined exactly <name...>   the undefined symbols are exactly these
#   globals <prefix> <n>          the object defines exactly n global symbols, each
#                                 with a name that starts with <prefix>
#   functions <name...>           the defined text symbols are exactly these
#   calls none                    no jsr, bsr, bsrf or jmp in any function
#   count <function|*> <mnemonic> <n>  exactly n of that mnemonic in that function
#                                 ('*': in the whole object)
#   forbid <function|*> <ERE>     no instruction ("mnemonic operands", then objdump's
#                                 "! ..." comment, e.g. a literal-pool value) matches
#
# Plants: after a spec passes, its first count line with n+1 (build/sh2-static/<f>/plant.txt)
# and its first 'undefined exactly' line without its last name
# (build/sh2-static/<f>/plant-undefined.txt) must each fail on the last level's reports,
# which shows the check can fail; a spec with neither line has no plant.
set -eu

if [ -n "${YAUL_INSTALL_ROOT:-}" ]; then
    PATH="$YAUL_INSTALL_ROOT/bin:$PATH"
fi
if [ -d /opt/sh2/linux/sh2eb-elf/bin ]; then
    PATH="/opt/sh2/linux/sh2eb-elf/bin:$PATH"
fi
PREFIX="${YAUL_ARCH_SH_PREFIX:-sh2eb-elf}-"
CC="${PREFIX}gcc"
"$CC" --version | head -n 1

# Fails the script unless every named object is a big-endian ELF.
check_big_endian() {
    for obj in "$@"; do
        "${PREFIX}readelf" -h "$obj" > "$obj.header.txt"
        if ! grep -q "big endian" "$obj.header.txt"; then
            echo "sh2_cross: $obj is not big-endian" >&2
            exit 1
        fi
    done
}

count="${SH2O_COUNT:-OFF}"
count_define=
if [ "$count" = ON ]; then
    count_define=-DSH2O_COUNT=1
fi

for mode in native portable; do
    if [ "$count" = ON ]; then
        out="build/sh2-count-$mode"
    else
        out="build/sh2-$mode"
    fi
    rm -rf "$out"
    mkdir -p "$out"
    if [ "$mode" = native ]; then
        define=-DSH2O_REQUIRE_SH2=1
    else
        define=-DSH2O_FORCE_PORTABLE=1
    fi
    for src in src/*.c; do
        "$CC" -m2 -mb -O2 -ffreestanding -std=c11 -Wall -Wextra -Wconversion -Wshadow -Werror \
            -ffunction-sections -fdata-sections "$define" $count_define -Iinclude -c "$src" \
            -o "$out/$(basename "${src%.c}").o"
    done
    "${PREFIX}ar" rcsD "$out/libsh2o.a" "$out"/*.o
    check_big_endian "$out"/*.o
    "${PREFIX}objdump" -d "$out/libsh2o.a" > "$out/disassembly.txt"
    "${PREFIX}nm" -u "$out/libsh2o.a" > "$out/undefined.txt"
    if grep -Eq "memcpy|memset|memmove|malloc|free|printf" "$out/undefined.txt"; then
        echo "sh2_cross: $mode: the library needs a C library function:" >&2
        cat "$out/undefined.txt" >&2
        exit 1
    fi
    echo "sh2_cross: $mode: ok ($(grep -c " U " "$out/undefined.txt" || true) undefined symbol(s), see $out/undefined.txt)"
    # Each family suite in tests/suite/ (a program on the SH-2 can compile it): it must build with the same
    # flags and need no C library function. It is not in the library.
    mkdir -p "$out/suite"
    for src in tests/suite/*_test_suite.c; do
        [ -f "$src" ] || continue
        obj="$out/suite/$(basename "${src%.c}").o"
        "$CC" -m2 -mb -O2 -ffreestanding -std=c11 -Wall -Wextra -Wconversion -Wshadow -Werror \
            -ffunction-sections -fdata-sections "$define" -Iinclude -Itests/suite -c "$src" -o "$obj"
        check_big_endian "$obj"
        "${PREFIX}nm" -u "$obj" > "$obj.undefined.txt"
        if grep -Eq "memcpy|memset|memmove|malloc|free|printf" "$obj.undefined.txt"; then
            echo "sh2_cross: $mode: $src needs a C library function:" >&2
            cat "$obj.undefined.txt" >&2
            exit 1
        fi
        echo "sh2_cross: $mode: $src: ok"
    done
done

if [ "$count" = ON ]; then
    echo "sh2_cross: SH2O_COUNT=ON: no static check (the specs pin the objects built without the counters)"
    exit 0
fi

failed=0

# spec_fail <rule>: reports a failed rule of family $f at level $level.
spec_fail() {
    echo "sh2_cross: $f: $level: $*" >&2
    failed=1
    spec_failed=1
}

# Prints the spec's directive lines with comments and blank lines removed.
spec_lines() {
    sed -e 's/#.*//' -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' "$1" | grep -v '^$' || true
}

# Checks the directives of spec $spec against the reports in $dir.
spec_check() {
    spec_lines "$spec" | while IFS= read -r line; do
        set -- $line
        case "$1" in
            undefined)
                if [ "$2" = none ]; then
                    if [ -s "$dir/undefined.txt" ]; then
                        spec_fail "*: undefined none: $(tr '\n' ' ' < "$dir/undefined.txt")"
                    fi
                elif [ "$2" = exactly ]; then
                    shift 2
                    for name in "$@"; do echo "_$name"; done | sort > "$dir/undefined.want"
                    awk '{ print $NF }' "$dir/undefined.txt" | sort > "$dir/undefined.got"
                    if [ "$(cat "$dir/undefined.want")" != "$(cat "$dir/undefined.got")" ]; then
                        spec_fail "*: undefined exactly $*: found $(tr '\n' ' ' < "$dir/undefined.got")"
                    fi
                else
                    shift
                    for name in "$@"; do echo "_$name"; done > "$dir/undefined.want"
                    awk '{ print $NF }' "$dir/undefined.txt" | grep -vxF -f "$dir/undefined.want" > "$dir/undefined.extra" || true
                    if [ -s "$dir/undefined.extra" ]; then
                        spec_fail "*: undefined $*: also $(tr '\n' ' ' < "$dir/undefined.extra")"
                    fi
                fi
                ;;
            globals)
                n=$(wc -l < "$dir/globals.txt" | tr -d ' ')
                grep -v "^_$2" "$dir/globals.txt" > "$dir/globals.other" || true
                if [ -s "$dir/globals.other" ]; then
                    spec_fail "*: globals $2: also $(tr '\n' ' ' < "$dir/globals.other")"
                fi
                if [ "$n" != "$3" ]; then
                    spec_fail "*: globals $2 $3: found $n"
                fi
                ;;
            functions)
                shift
                for name in "$@"; do echo "_$name"; done | sort > "$dir/functions.want"
                if [ "$(cat "$dir/functions.want")" != "$(cat "$dir/functions.txt")" ]; then
                    spec_fail "*: functions: defined $(tr '\n' ' ' < "$dir/functions.txt")"
                fi
                ;;
            calls)
                awk -F'\t' '$2 ~ /^(jsr|bsr|bsrf|jmp)( |$)/ { sub(/^_/, "", $1); print $1 ": " $2 }' \
                    "$dir/instructions.txt" |
                    while IFS= read -r hit; do spec_fail "${hit%%:*}: calls none: ${hit#*: }"; done
                ;;
            count)
                if [ "$2" != "*" ] && ! grep -q "^_$2	" "$dir/instructions.txt"; then
                    spec_fail "$2: count $3: no such function"
                    continue
                fi
                n=$(awk -F'\t' -v fn="_$2" -v m="$3" '(fn == "_*" || $1 == fn) { split($2, w, " "); if (w[1] == m) n++ }
                    END { print n + 0 }' "$dir/instructions.txt")
                if [ "$n" != "$4" ]; then
                    spec_fail "$2: count $3 $4: found $n"
                fi
                ;;
            forbid)
                ere=$(printf '%s\n' "$line" | sed -e 's/^forbid[[:space:]]*[^[:space:]]*[[:space:]]*//')
                if [ "$2" = "*" ]; then
                    cut -f2- "$dir/instructions.txt" > "$dir/forbid.in"
                    awk -F'\t' '{ print $1 }' "$dir/instructions.txt" > "$dir/forbid.fn"
                else
                    if ! grep -q "^_$2	" "$dir/instructions.txt"; then
                        spec_fail "$2: forbid $ere: no such function"
                        continue
                    fi
                    grep "^_$2	" "$dir/instructions.txt" | cut -f2- > "$dir/forbid.in"
                fi
                grep -nE -- "$ere" "$dir/forbid.in" > "$dir/forbid.hits" || true
                while IFS= read -r hit; do
                    if [ "$2" = "*" ]; then
                        fn=$(sed -n "${hit%%:*}p" "$dir/forbid.fn")
                    else
                        fn="_$2"
                    fi
                    spec_fail "${fn#_}: forbid $ere: ${hit#*:}"
                done < "$dir/forbid.hits"
                ;;
        esac
    done
}

mkdir -p build/sh2-static
for spec in tests/sh2/*_static.txt; do
    [ -f "$spec" ] || continue
    # No globbing from here: a spec line may hold '*'.
    set -f
    f=$(basename "$spec" _static.txt)
    level=spec
    spec_failed=0
    source=
    flags=
    levels=-O2
    while IFS= read -r line; do
        [ -n "$line" ] || continue
        set -- $line
        case "$1" in
            source) source=${2:-} ;;
            flags) shift; flags="$*" ;;
            levels) shift; levels="$*" ;;
            undefined)
                [ "$#" -ge 2 ] || spec_fail "*: undefined needs 'none' or names: $line"
                [ "$2" != exactly ] || [ "$#" -ge 3 ] || spec_fail "*: undefined exactly needs names: $line"
                ;;
            globals)
                case "$#:${3:-x}" in
                    3:*[!0-9]* | 3:) spec_fail "*: globals needs a number: $line" ;;
                    3:*) ;;
                    *) spec_fail "*: globals needs <prefix> <n>: $line" ;;
                esac
                ;;
            calls)
                [ "${2:-}" = none ] || spec_fail "*: $1 takes 'none': $line" ;;
            functions | forbid)
                [ "$#" -ge 2 ] || spec_fail "*: $1 needs an argument: $line" ;;
            count)
                case "$#:${4:-x}" in
                    4:*[!0-9]* | 4:) spec_fail "*: count needs a number: $line" ;;
                    4:*) ;;
                    *) spec_fail "*: count needs <function> <mnemonic> <n>: $line" ;;
                esac
                ;;
            *) spec_fail "*: unknown directive: $line" ;;
        esac
    done <<SPEC
$(spec_lines "$spec")
SPEC
    if [ -z "$source" ] || [ ! -f "$source" ]; then
        spec_fail "*: source: missing or not a file: '$source'"
    fi
    [ "$spec_failed" -eq 0 ] || continue
    for level in $levels; do
        dir="build/sh2-static/$f/${level#-}"
        rm -rf "$dir"
        mkdir -p "$dir"
        # $flags is split into words on purpose.
        if ! "$CC" -m2 -mb "$level" -ffreestanding -std=c11 -Wall -Wextra -Wconversion -Wshadow -Werror \
            -ffunction-sections -fdata-sections $flags -Iinclude -c "$source" -o "$dir/probe.o"; then
            spec_fail "*: source: $source does not build"
            continue
        fi
        check_big_endian "$dir/probe.o"
        "${PREFIX}objdump" -d "$dir/probe.o" > "$dir/disassembly.txt"
        "${PREFIX}nm" -u "$dir/probe.o" > "$dir/undefined.txt"
        "${PREFIX}nm" --defined-only "$dir/probe.o" | awk '$2 == "T" || $2 == "t" { print $3 }' |
            sort > "$dir/functions.txt"
        "${PREFIX}nm" -g --defined-only "$dir/probe.o" | awk '{ print $3 }' | sort > "$dir/globals.txt"
        # One line per instruction: function TAB "mnemonic operands".
        awk -F'\t' '/^[0-9a-f]+ <.*>:$/ { fn = $0; sub(/^[0-9a-f]+ </, "", fn); sub(/>:$/, "", fn); next }
            /^ *[0-9a-f]+:\t/ && NF >= 3 { ins = $3; if (NF >= 4 && $4 != "") ins = ins " " $4
                if (NF >= 5 && $5 != "") ins = ins " " $5; print fn "\t" ins }' \
            "$dir/disassembly.txt" > "$dir/instructions.txt"
        spec_check > "$dir/failures.txt" 2>&1 || true
        if [ -s "$dir/failures.txt" ]; then
            cat "$dir/failures.txt" >&2
            failed=1
            spec_failed=1
        else
            echo "sh2_cross: $f: $level: static check ok ($dir)"
        fi
    done
    # Plant: the spec's first count, raised by one, must fail on the last level's reports.
    [ "$spec_failed" -eq 0 ] || continue
    real_spec=$spec
    plant=$(spec_lines "$real_spec" | awk '$1 == "count" { print; exit }')
    if [ -n "$plant" ]; then
        spec="build/sh2-static/$f/plant.txt"
        spec_lines "$real_spec" | awk -v p="$plant" '$0 == p && !done { $4 = $4 + 1; done = 1 } { print }' > "$spec"
        spec_check > "$dir/plant.failures.txt" 2>&1 || true
        if grep -q ": count " "$dir/plant.failures.txt"; then
            echo "sh2_cross: $f: plant: planted count found"
        else
            echo "sh2_cross: $f: plant: the planted count passed ($spec)" >&2
            failed=1
        fi
        spec=$real_spec
    fi
    # Plant: the spec's first 'undefined exactly' line without its last name (one undefined name more than it
    # allows) must fail on the last level's reports.
    plant=$(spec_lines "$real_spec" | awk '$1 == "undefined" && $2 == "exactly" { print; exit }')
    if [ -n "$plant" ]; then
        spec="build/sh2-static/$f/plant-undefined.txt"
        spec_lines "$real_spec" | awk -v p="$plant" '$0 == p && !done { NF = NF - 1; done = 1 } { print }' > "$spec"
        spec_check > "$dir/plant-undefined.failures.txt" 2>&1 || true
        if grep -q ": undefined exactly" "$dir/plant-undefined.failures.txt"; then
            echo "sh2_cross: $f: plant: planted undefined name found"
        else
            echo "sh2_cross: $f: plant: the planted undefined name passed ($spec)" >&2
            failed=1
        fi
        spec=$real_spec
    fi
done
if [ "$failed" -ne 0 ]; then
    exit 1
fi
