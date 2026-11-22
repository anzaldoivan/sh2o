#!/bin/sh
# SH-2 model run of every family: build in a Yaul toolchain image, run on the host.
# Copyright (c) 2026 sh2o Contributors. MIT License.
#
# Usage: sh scripts/sh2_model.sh elf    (in the Yaul image; make sh2-model runs it)
#        bash scripts/sh2_model.sh run  (on the host, after elf; SH2_IMAGE in the environment)
#
# Families: each benchmarks/<f>-model/bridge.c names a family <f>. A family
# also needs benchmarks/sh2-model/<f>_harness.c and tests/<f>_oracle.h; a
# missing file, or no family at all, is an error. A family with no oracle file
# is accepted only when its harness source has the --plant-known switch: its
# checks are the suite's digest and known answers.
#
# elf: for each mode (native, portable) the script builds src/*.c once with
# the flags of scripts/sh2_cross.sh into build/sh2-model/<mode>/lib/, and each
# bridge once. It links each family with SH2ZAM's model.ld into
# build/sh2-model/<mode>/<f>-model.elf: the bridge object first, then the
# library objects in name order. A family with tests/suite/sh2o_<f>_test_suite.c
# also links that suite (built with the same flags) after the bridge. Each ELF
# must have no undefined symbol.
#
# run: the script gets the pinned Yabause source (sparse, depth 1) into
# YABAUSE_SOURCE (default build/sh2-model/yabause) when it is absent, and
# refuses a source at another commit or with changes. For each family it
# builds the GPL harness benchmarks/sh2-model/<f>_harness.c with the host
# compiler into build/sh2-model/<f>_harness, runs it on the family's two ELFs,
# then runs it with --plant and requires a failure. A family with no oracle
# file runs with --plant-known 1 instead of --plant and must stop (exit 1) with
# "known answer" (output in <f>-plant-known.out and .err). A harness whose
# source has the --plant-digest switch also runs with it and must stop (exit 1)
# with "the digest is not the pin". A harness whose source has the
# --plant-count switch also runs with it and must stop (exit 1) with "the
# counts are not the pins". A harness whose source has the --plant-entry
# switch also runs with it and must stop (exit 1) with "differs from the
# command". Every family runs; any failure fails the
# script at the end. Only the Yabause fetch uses the network.
#
# SH2O_ZAM (environment, default OFF; make sh2-model SH2O_ZAM=ON): OFF runs
# the commands above. A bridge with the line "sh2_model-option: SH2O_ZAM"
# names a family that runs only with ON. With ON, the folder is
# build/sh2-model-zam (the Yabause source stays in build/sh2-model/yabause)
# and every family runs. For each mode, the elf step first builds SH2ZAM's
# external/SH2ZAM/saturn/src/*.c, unmodified, into <mode>/sh2zam/ and the
# archive <mode>/libsh2zam.a, with -DSH2Z_REQUIRE_SH2=1 (native) or
# -DSH2Z_FORCE_PORTABLE=1 (portable). The library, the bridges and the suites
# get the same flags as with OFF, plus -DSH2O_ZAM=1, SH2ZAM's include folder
# and the same SH2Z define. The link adds --gc-sections and the archive after
# the library objects.
set -eu

zam="${SH2O_ZAM:-OFF}"
if [ "$zam" != ON ] && [ "$zam" != OFF ]; then
    echo "sh2_model: SH2O_ZAM is '$zam', not ON or OFF" >&2
    exit 2
fi
if [ "$zam" = ON ]; then
    out=build/sh2-model-zam
else
    out=build/sh2-model
fi
model_commit=7e38821dbac265490f115e163c523a939acda759
model_ld=external/SH2ZAM/saturn/benchmarks/integer-model/model.ld
sh2zam_model=external/SH2ZAM/saturn/benchmarks/sh2-model
failed=0

# Sets $families to the family names, in name order, after checking each one's files.
find_families() {
    families=
    for bridge in benchmarks/*-model/bridge.c; do
        [ -f "$bridge" ] || continue
        f=$(basename "$(dirname "$bridge")")
        f=${f%-model}
        if [ "$zam" != ON ] && grep -q "sh2_model-option: SH2O_ZAM" "$bridge"; then
            continue
        fi
        for need in "benchmarks/sh2-model/${f}_harness.c" "tests/${f}_oracle.h"; do
            if [ "$need" = "tests/${f}_oracle.h" ] && [ ! -f "$need" ] && [ -f "benchmarks/sh2-model/${f}_harness.c" ] \
                && grep -q -e '--plant-known' "benchmarks/sh2-model/${f}_harness.c"; then
                continue
            fi
            if [ ! -f "$need" ]; then
                echo "sh2_model: $f: missing $need" >&2
                exit 1
            fi
        done
        families="$families $f"
    done
    if [ -z "$families" ]; then
        echo "sh2_model: no family: no benchmarks/*-model/bridge.c" >&2
        exit 1
    fi
}

fail() {
    echo "sh2_model: $*" >&2
    failed=1
}

elf() {
    if [ -n "${YAUL_INSTALL_ROOT:-}" ]; then
        PATH="$YAUL_INSTALL_ROOT/bin:$PATH"
    fi
    if [ -d /opt/sh2/linux/sh2eb-elf/bin ]; then
        PATH="/opt/sh2/linux/sh2eb-elf/bin:$PATH"
    fi
    PREFIX="${YAUL_ARCH_SH_PREFIX:-sh2eb-elf}-"
    CC="${PREFIX}gcc"
    "$CC" --version | head -n 1
    find_families
    for mode in native portable; do
        dir="$out/$mode"
        rm -rf "$dir"
        mkdir -p "$dir/lib"
        if [ "$mode" = native ]; then
            define=-DSH2O_REQUIRE_SH2=1
        else
            define=-DSH2O_FORCE_PORTABLE=1
        fi
        # With OFF, $zflags, $gc and $zlib are empty and add no argument.
        zflags=
        gc=
        zlib=
        if [ "$zam" = ON ]; then
            if [ "$mode" = native ]; then
                zdefine=-DSH2Z_REQUIRE_SH2=1
            else
                zdefine=-DSH2Z_FORCE_PORTABLE=1
            fi
            mkdir -p "$dir/sh2zam"
            for src in external/SH2ZAM/saturn/src/*.c; do
                "$CC" -m2 -mb -O2 -ffreestanding -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
                    -Iexternal/SH2ZAM/saturn/include "$zdefine" -c "$src" \
                    -o "$dir/sh2zam/$(basename "${src%.c}").o"
            done
            "${PREFIX}ar" rcsD "$dir/libsh2zam.a" "$dir"/sh2zam/*.o
            zflags="-DSH2O_ZAM=1 -Iexternal/SH2ZAM/saturn/include $zdefine"
            gc=-Wl,--gc-sections
            zlib="$dir/libsh2zam.a"
        fi
        for src in src/*.c; do
            # shellcheck disable=SC2086 # $zflags: words without spaces, or empty
            "$CC" -m2 -mb -O2 -ffreestanding -std=c11 -Wall -Wextra -Wconversion -Wshadow -Werror \
                -ffunction-sections -fdata-sections "$define" $zflags -Iinclude -c "$src" \
                -o "$dir/lib/$(basename "${src%.c}").o"
        done
        for f in $families; do
            if ! "$CC" -m2 -mb -O2 -ffreestanding -std=c11 -Wall -Wextra -Wconversion -Wshadow -Werror \
                -ffunction-sections -fdata-sections "$define" $zflags -Iinclude -Itests/suite \
                -c "benchmarks/$f-model/bridge.c" \
                -o "$dir/$f-bridge.o"; then
                fail "$f: $mode: the bridge does not build"
                continue
            fi
            # With ON, --gc-sections keeps each global symbol that the bridge defines (the harness reaches the
            # ELF through them) and what they refer to. With -e 0 there is no other root.
            keep=
            if [ "$zam" = ON ]; then
                keep=$("${PREFIX}nm" -g --defined-only "$dir/$f-bridge.o" | sed -n 's/^[0-9a-fA-F]* [A-Za-z] /-Wl,-u,/p')
                if [ -z "$keep" ]; then
                    fail "$f: $mode: the bridge defines no global symbol"
                    continue
                fi
            fi
            suite=
            if [ -f "tests/suite/sh2o_${f}_test_suite.c" ]; then
                suite="$dir/$f-suite.o"
                if ! "$CC" -m2 -mb -O2 -ffreestanding -std=c11 -Wall -Wextra -Wconversion -Wshadow -Werror \
                    -ffunction-sections -fdata-sections "$define" $zflags -Iinclude -Itests/suite \
                    -c "tests/suite/sh2o_${f}_test_suite.c" -o "$suite"; then
                    fail "$f: $mode: the suite does not build"
                    continue
                fi
            fi
            # Link order: bridge, the family's suite when it has one, then the library objects in
            # name order (the glob sorts), then libsh2zam.a with ON.
            # shellcheck disable=SC2086 # $suite, $gc, $keep, $zlib: empty or words without spaces
            if ! "$CC" -m2 -mb -nostdlib -nostartfiles -Wl,-T,"$model_ld" -Wl,-e,0 $gc $keep \
                -o "$dir/$f-model.elf" "$dir/$f-bridge.o" $suite "$dir"/lib/*.o $zlib -lgcc; then
                fail "$f: $mode: the ELF does not link"
                continue
            fi
            "${PREFIX}nm" "$dir/$f-model.elf" > "$dir/$f-model.sym"
            for root in $keep; do
                if ! grep -q " ${root#-Wl,-u,}\$" "$dir/$f-model.sym"; then
                    fail "$f: $mode: the ELF has no symbol ${root#-Wl,-u,}"
                fi
            done
            "${PREFIX}nm" -u "$dir/$f-model.elf" > "$dir/$f-undefined.txt"
            if [ -s "$dir/$f-undefined.txt" ]; then
                fail "$f: $mode: the ELF has undefined symbols:"
                cat "$dir/$f-undefined.txt" >&2
                continue
            fi
            echo "sh2_model: $f: $mode: $dir/$f-model.elf"
        done
    done
    if [ "$failed" -ne 0 ]; then
        exit 1
    fi
}

run() {
    : "${SH2_IMAGE:?sh2_model: set SH2_IMAGE to the Yaul image of the elf step}"
    # SH2ZAM's driver script in that folder states the Yabause pin; it must be ours.
    stated=$(sed -n 's/^model_commit=\([0-9a-f]*\)$/\1/p' "$sh2zam_model"/*.sh)
    if [ "$stated" != "$model_commit" ]; then
        echo "sh2_model: $sh2zam_model gives Yabause '$stated', this script gives $model_commit" >&2
        exit 1
    fi
    find_families
    for f in $families; do
        for mode in native portable; do
            test -f "$out/$mode/$f-model.elf" || { echo "sh2_model: $f: run the elf step first" >&2; exit 1; }
        done
    done
    core="${YABAUSE_SOURCE:-build/sh2-model/yabause}"
    if [ ! -d "$core/.git" ]; then
        mkdir -p "$core"
        git -C "$core" init -q
        git -C "$core" remote add origin https://github.com/Yabause/yabause.git
        git -C "$core" sparse-checkout set --cone yabause/src
        git -C "$core" fetch -q --depth 1 origin "$model_commit"
        git -C "$core" -c advice.detachedHead=false checkout -q --detach FETCH_HEAD
    fi
    head=$(git -C "$core" rev-parse HEAD)
    if [ "$head" != "$model_commit" ]; then
        echo "sh2_model: $core is at $head, not the pinned Yabause $model_commit" >&2
        exit 2
    fi
    if ! git -C "$core" diff --quiet --exit-code HEAD -- yabause/src; then
        echo "sh2_model: $core has changes in yabause/src" >&2
        exit 2
    fi
    sh2zam=$(git -C external/SH2ZAM rev-parse HEAD)
    for f in $families; do
        # The interpreter source refers to Saturn parts that the harness never
        # reaches; --gc-sections drops those references.
        if ! "${CC:-cc}" -O2 -fwrapv -fno-strict-aliasing -ffunction-sections -fdata-sections -DHAVE_STDINT_H \
            -I"$core/yabause/src" -Iinclude -Wl,--gc-sections "benchmarks/sh2-model/${f}_harness.c" -lm \
            -o "$out/${f}_harness"; then
            fail "$f: harness: benchmarks/sh2-model/${f}_harness.c does not build"
            continue
        fi
        native_sha=$(sha256sum "$out/native/$f-model.elf" | cut -d' ' -f1)
        portable_sha=$(sha256sum "$out/portable/$f-model.elf" | cut -d' ' -f1)
        set -- "$out/native/$f-model.elf" "$out/native/$f-model.sym" \
            "$out/portable/$f-model.elf" "$out/portable/$f-model.sym" \
            "$sh2zam" "$model_commit" "$SH2_IMAGE" "$native_sha" "$portable_sha"
        status=0
        "$out/${f}_harness" "$@" || status=$?
        if [ "$status" -ne 0 ]; then
            fail "$f: harness: $out/${f}_harness failed (exit $status)"
        fi
        status=0
        if [ ! -f "tests/${f}_oracle.h" ]; then
            "$out/${f}_harness" "$@" --plant-known 1 > "$out/$f-plant-known.out" 2> "$out/$f-plant-known.err" \
                || status=$?
            if [ "$status" -ne 1 ] || ! grep -q "known answer" "$out/$f-plant-known.err"; then
                fail "$f: plant-known: the planted known answer did not stop the run (exit $status)"
                continue
            fi
            echo "sh2_model: $f: planted known answer stops the run (exit $status): $(head -n 1 "$out/$f-plant-known.err")"
        else
            "$out/${f}_harness" "$@" --plant 0 > "$out/$f-plant.out" 2> "$out/$f-plant.err" || status=$?
            if [ "$status" -eq 0 ] || ! grep -q "first difference" "$out/$f-plant.err"; then
                fail "$f: plant: the planted difference was not found (exit $status)"
                continue
            fi
            echo "sh2_model: $f: planted difference found (exit $status): $(head -n 1 "$out/$f-plant.err")"
        fi
        if grep -q -e '--plant-digest' "benchmarks/sh2-model/${f}_harness.c"; then
            status=0
            "$out/${f}_harness" "$@" --plant-digest > "$out/$f-plant-digest.out" 2> "$out/$f-plant-digest.err" \
                || status=$?
            if [ "$status" -ne 1 ] || ! grep -q "the digest is not the pin" "$out/$f-plant-digest.err"; then
                fail "$f: plant-digest: the planted digest did not stop the run (exit $status)"
                continue
            fi
            echo "sh2_model: $f: planted digest stops the run (exit $status): $(head -n 1 "$out/$f-plant-digest.err")"
        fi
        if grep -q -e '--plant-count' "benchmarks/sh2-model/${f}_harness.c"; then
            status=0
            "$out/${f}_harness" "$@" --plant-count > "$out/$f-plant-count.out" 2> "$out/$f-plant-count.err" \
                || status=$?
            if [ "$status" -ne 1 ] || ! grep -q "the counts are not the pins" "$out/$f-plant-count.err"; then
                fail "$f: plant-count: the planted counts did not stop the run (exit $status)"
                continue
            fi
            echo "sh2_model: $f: planted counts stop the run (exit $status): $(head -n 1 "$out/$f-plant-count.err")"
        fi
        if grep -q -e '--plant-entry' "benchmarks/sh2-model/${f}_harness.c"; then
            status=0
            "$out/${f}_harness" "$@" --plant-entry > "$out/$f-plant-entry.out" 2> "$out/$f-plant-entry.err" \
                || status=$?
            if [ "$status" -ne 1 ] || ! grep -q "differs from the command" "$out/$f-plant-entry.err"; then
                fail "$f: plant-entry: the planted entry word did not stop the run (exit $status)"
                continue
            fi
            echo "sh2_model: $f: planted entry word stops the run (exit $status): $(head -n 1 "$out/$f-plant-entry.err")"
        fi
    done
    if [ "$failed" -ne 0 ]; then
        exit 1
    fi
}

case "${1:-}" in
    elf) elf ;;
    run) run ;;
    *) echo "usage: sh2_model.sh elf|run" >&2; exit 2 ;;
esac
