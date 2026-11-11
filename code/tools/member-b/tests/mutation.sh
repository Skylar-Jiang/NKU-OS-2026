#!/usr/bin/env bash
# Mutation check for the member-B tests: inject one known bug at a time into a
# throw-away copy of code/, run phase 1 of run.sh, and require that it FAILS.
# Usage: bash code/tools/member-b/tests/mutation.sh [workdir]
# The real source tree is never modified. Each mutant uses a fresh mktemp dir.
set -u
TESTS="$(cd "$(dirname "$0")" && pwd)"
CODE="$(cd "$TESTS/../../.." && pwd)"
BASE="${1:-${TMPDIR:-/tmp}}"
killed=0; survived=0

mutate() { # <name> <old> <new>
    local d; d="$(mktemp -d "$BASE/lab4-b-mutant-$1.XXXXXX")"
    mkdir -p "$d/code"
    tar -C "$CODE" --exclude=./obj --exclude=./bin -cf - . | tar -C "$d/code" -xf -
    python3 - "$d/code/kern/process/proc.c" "$2" "$3" <<'EOF'
import sys
p, o, n = sys.argv[1:4]
s = open(p).read()
assert s.count(o) == 1, "mutation anchor not unique: %r" % o
open(p, "w").write(s.replace(o, n))
EOF
    bash "$d/code/tools/member-b/tests/run.sh" "$d/ev" > "$d/run.out" 2>&1
    local checks; checks="$(grep -E '^checks' "$d/run.out")"
    if grep -q 'fail=0; \[member-b\] SUMMARY' "$d/run.out"; then
        echo "SURVIVED $1: $checks"; survived=$((survived + 1))
    else
        echo "killed   $1: ${checks:-build/run failure}"
        grep -a '\] FAIL' "$d/ev/member-b-qemu.log" 2>/dev/null | head -2 | sed 's/ (.*//; s/^/           /'
        killed=$((killed + 1))
    fi
}

mutate pcb-no-pid-init     $'    proc->pid = -1;\n' ''
mutate rollback-no-kfree   $'bad_fork_cleanup_proc:\n    kfree(proc);' $'bad_fork_cleanup_proc:\n    ;'
mutate rollback-no-kstack  $'bad_fork_cleanup_kstack:\n    put_kstack(proc);' $'bad_fork_cleanup_kstack:\n    ;'
mutate kthread-null-sp     '(esp == 0) ? (uintptr_t)proc->tf : esp' 'esp'
mutate child-a0-kept       $'    proc->tf->gpr.a0 = 0;\n' ''
mutate status-keeps-sie    ' & ~SSTATUS_SIE' ''
mutate copy_mm-fake-ok     $'    return -E_INVAL;\n}' $'    return 0;\n}'
mutate nr_process-no-inc   $'        nr_process++;\n        wakeup_proc' $'        wakeup_proc'
mutate idle-in-proc_list   $'    nr_process++;\n\n    current = idleproc;' $'    nr_process++;\n    list_add(&proc_list, &idleproc->list_link);\n    current = idleproc;'

echo "mutants killed=$killed survived=$survived"
[ "$survived" -eq 0 ]
