#!/usr/bin/env bash
# Member B (2413507) process-creation test runner.
#
# Usage (from anywhere):
#   bash code/tools/member-b/tests/run.sh [evidence-dir]
#
# Environment overrides (optional, never changes global config):
#   QEMU=/path/to/qemu-system-riscv64   (default: first on PATH; expect 4.1.1)
#   GCCPREFIX=/path/to/riscv64-unknown-elf-
#
# Phase 1: member-B test kernel (real proc.c + tests, fault injection via ld --wrap).
# Phase 2: the official grading test path: copy, tools/inject.py, build, QEMU,
#          same checks as tools/grade.sh, raw serial log kept.
# Exit status is non-zero if any phase fails. A QEMU timeout is expected only
# AFTER the final lines are printed (init_main returns into the framework's
# do_exit panic stub, then the kernel monitor waits for input).
set -u

TESTS="$(cd "$(dirname "$0")" && pwd)"
CODE="$(cd "$TESTS/../../.." && pwd)"
EVID="${1:-$CODE/tools/member-b/evidence}"
QEMU="${QEMU:-qemu-system-riscv64}"
MAKEARGS=()
[ -n "${GCCPREFIX:-}" ] && MAKEARGS+=("GCCPREFIX=$GCCPREFIX")
mkdir -p "$EVID"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/lab4-member-b.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

status=0
{
    echo "date: $(date -Iseconds)"
    echo "git: $(git -C "$CODE" rev-parse HEAD 2>/dev/null) $(git -C "$CODE" status --short -- kern | wc -l) dirty kern files"
    "${GCCPREFIX:-riscv64-unknown-elf-}gcc" --version | head -1
    "$QEMU" --version | head -1
} > "$EVID/versions.txt"
cat "$EVID/versions.txt"

copy_tree() {
    mkdir -p "$1"
    tar -C "$CODE" --exclude=./obj --exclude=./bin --exclude=./.qemu.out -cf - . | tar -C "$1" -xf -
}

run_qemu() { # <dir> <log>
    timeout 20 "$QEMU" -machine virt -nographic -bios default \
        -device loader,file="$1/bin/ucore.img",addr=0x80200000 \
        -monitor null -no-reboot < /dev/null > "$2" 2>&1
    echo "qemu exit status: $? (124 = stopped by timeout)" >> "$2"
}

# ---------------- phase 1 ----------------
echo "== phase 1: member-B test kernel"
copy_tree "$WORK/b"
python3 "$TESTS/prepare.py" "$WORK/b" "$TESTS"
if ! make -C "$WORK/b" "${MAKEARGS[@]}" > "$EVID/member-b-build.log" 2>&1; then
    echo "BUILD FAILED (see $EVID/member-b-build.log)"; status=1
else
    run_qemu "$WORK/b" "$EVID/member-b-qemu.log"
    summary="$(grep -a '\[member-b\] SUMMARY' "$EVID/member-b-qemu.log")"
    fails="$(grep -ac '\[member-b\] FAIL' "$EVID/member-b-qemu.log")"
    passes="$(grep -ac '\[member-b\] PASS' "$EVID/member-b-qemu.log")"
    echo "checks: pass=$passes fail=$fails; ${summary:-SUMMARY MISSING}"
    grep -a '\[member-b\] FAIL' "$EVID/member-b-qemu.log"
    if [ -z "$summary" ] || [ "$fails" -ne 0 ] || \
       ! grep -aq 'this initproc, pid = 1, name = "init"' "$EVID/member-b-qemu.log"; then
        status=1
    fi
fi

# ---------------- phase 2 ----------------
echo "== phase 2: official injected tests (tools/inject.py, unmodified)"
copy_tree "$WORK/g"
python3 "$CODE/tools/inject.py" "$WORK/g"
if ! make -C "$WORK/g" "${MAKEARGS[@]}" > "$EVID/official-build.log" 2>&1; then
    echo "BUILD FAILED (see $EVID/official-build.log)"; status=1
else
    run_qemu "$WORK/g" "$EVID/official-qemu.log"
    for line in '[Test PMM] PASS' '[Test Process Create] PASS' '[Test Schedule] PASS' \
                'this initproc, pid = 1, name = "init"' 'To U: "Hello world!!".' \
                'To U: "en.., Bye, Bye. :)"'; do
        if grep -aqF "$line" "$EVID/official-qemu.log"; then
            echo "  found:   $line"
        else
            echo "  MISSING: $line"; status=1
        fi
    done
fi

[ $status -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $status
