#!/usr/bin/env bash
# In-kernel RR/CFS tests for member B (Lab6).
#
#   bash run_qemu.sh [rr|cfs|all] [gdb]
#
# For each scheduler a TEMPORARY copy of code/ is built (the repository tree
# is never modified):
#   * tests/rr_cfs/qemu/user/*.c are added as extra user programs;
#   * tests/rr_cfs/qemu/kernel/b_kcheck.c is appended to the copy's proc.c and
#     called at the end of proc_init() (alloc_proc is static);
#   * "cfs" switches the copy's sched_init() to cfs_sched_class.
# Then priority (official), b_proc and b_spin are run in QEMU with a timeout
# and judged on their real output. Logs go to results/qemu/*.txt.
#
# LAB6_OVERLAY=/path/script  optional hook run as "script <tmpdir>" before the
#   build, e.g. to supply a teammate's not-yet-merged module.
# "gdb" additionally counts timer ticks vs. scheduler-class ticks in GDB.
set -uo pipefail
HERE=$(cd -- "$(dirname -- "$0")" && pwd)
CODE=$(cd -- "$HERE/../.." && pwd)
OUT="$HERE/results/qemu"
QEMU=${QEMU:-qemu-system-riscv64}
GDB=${GDB:-riscv64-unknown-elf-gdb}
TIMEOUT=${TIMEOUT:-60}
mkdir -p "$OUT"

which_sched=${1:-all}
want_gdb=${2:-}
fails=0
pass() { printf '[PASS] %s\n' "$*"; }
fail() { printf '[FAIL] %s\n' "$*"; fails=$((fails + 1)); }
check() { if eval "$2"; then pass "$1"; else fail "$1"; fi; }

prepare() { # prepare <tmp> <rr|cfs>
    local tmp=$1 sched=$2
    (cd "$CODE" && tar --exclude=./obj --exclude=./bin --exclude=./tests -cf - .) | tar -xf - -C "$tmp"
    cp "$HERE"/qemu/user/*.c "$tmp/user/"
    python3 - "$tmp/kern/process/proc.c" "$HERE/qemu/kernel/b_kcheck.c" <<'PY' || return 1
import re, sys
path, check = sys.argv[1], sys.argv[2]
s = open(path).read()
m = re.search(r'(?m)^void proc_init\(void\)\s*\{', s)
if not m:
    sys.exit("proc_init not found")
depth, i = 0, m.end() - 1
while True:
    depth += {'{': 1, '}': -1}.get(s[i], 0)
    if depth == 0:
        break
    i += 1
s = s[:i] + "    b_kcheck_alloc_proc();\n" + s[i:]
s = s[:m.start()] + open(check).read() + "\n" + s[m.start():]
open(path, 'w').write(s)
PY
    if [ "$sched" = cfs ]; then
        local f="$tmp/kern/schedule/sched.c"
        grep -q 'sched_class = &default_sched_class;' "$f" || {
            echo "cannot find the sched_class selection in sched.c; adapt run_qemu.sh to member C's selector"; return 1; }
        sed -i 's/sched_class = &default_sched_class;/extern struct sched_class cfs_sched_class; sched_class = \&cfs_sched_class;/' "$f"
    fi
    if [ -n "${LAB6_OVERLAY:-}" ]; then
        "$LAB6_OVERLAY" "$tmp" || return 1
    fi
    grep -q '^void schedule(void)' "$tmp/kern/schedule/sched.c" || {
        echo "schedule() is missing from sched.c (member C); provide it or set LAB6_OVERLAY"; return 1; }
}

run_prog() { # run_prog <tmp> <prog> <log>
    (cd "$1" && make -s "build-$2" >/dev/null 2>"$3.build") || { cat "$3.build"; return 1; }
    rm -f "$3.build"
    (cd "$1" && timeout "$TIMEOUT" "$QEMU" -machine virt -nographic -bios default \
        -device loader,file=bin/ucore.img,addr=0x80200000 </dev/null >"$3" 2>&1)
    return 0
}

common_checks() { # common_checks <tag> <log>
    local tag=$1 log=$2
    check "$tag: PCB pre-Lab6 fields"        "grep -q 'b_kcheck: pre-Lab6 PCB fields ok' '$log'"
    check "$tag: PCB six Lab6 fields"        "grep -q 'b_kcheck: six Lab6 scheduling fields ok' '$log'"
    check "$tag: PCB CFS fields"             "grep -q 'b_kcheck: CFS fields ok' '$log'"
    check "$tag: all user processes quit"    "grep -q 'all user-mode processes have quit.' '$log'"
    check "$tag: init check memory pass"     "grep -q 'init check memory pass.' '$log'"
    check "$tag: only the final initproc-exit panic" \
        "[ \$(grep -c 'kernel panic at' '$log') -eq 1 ] && grep -A1 'kernel panic at' '$log' | grep -q 'initproc exit.'"
    check "$tag: no user panic"              "! grep -q 'user panic' '$log'"
    check "$tag: no Lab3 '100 ticks'/EOT"    "! grep -qE '^100 ticks|End of Test|EOT' '$log'"
}

# ratio_ok <ratio_x100> <expected_x100>: within +/-20 % (fixed before running:
# QEMU TCG timing jitter, 10 ms gettime granularity and fork offsets).
ratio_ok() { awk -v r="$1" -v e="$2" 'BEGIN { exit !(r >= e * 0.8 && r <= e * 1.2) }'; }

test_sched() { # test_sched <rr|cfs>
    local tmp; tmp=$(mktemp -d)
    test_sched_in "$tmp" "$1"
    rm -rf "$tmp"
}

test_sched_in() { # test_sched_in <tmp> <rr|cfs>
    local tmp=$1 sched=$2 name
    [ "$sched" = rr ] && name=RR_scheduler || name=CFS_scheduler
    echo "== $sched: building temporary copy in $tmp"
    prepare "$tmp" "$sched" || { fail "$sched: prepare"; return; }

    local log="$OUT/${sched}_priority.txt"
    run_prog "$tmp" priority "$log" || { fail "$sched: build priority"; return; }
    check "$sched: sched class is $name" "grep -q 'sched class: $name' '$log'"
    common_checks "$sched priority" "$log"
    for p in 'kernel_execve: pid = 2, name = "priority".' 'main: fork ok,now need to wait pids.' \
             'set priority to 5' 'set priority to 1' 'main: wait pids over'; do
        check "$sched priority: '$p'" "grep -qF '$p' '$log'"
    done
    local accs; accs=$(grep -oE 'main: pid [0-9]+, acc [0-9]+' "$log" | awk '{print $5}' | tr '\n' ' ')
    echo "    accs: $accs | $(grep 'sched result' "$log")"
    local i=1 a1; a1=$(echo "$accs" | awk '{print $1}')
    for a in $accs; do
        local r=$((a * 100 / a1)) e
        [ "$sched" = rr ] && e=100 || e=$((i * 100))
        check "$sched priority: child prio $i acc ratio ${r}% ~ ${e}%" "ratio_ok $r $e"
        i=$((i + 1))
    done

    log="$OUT/${sched}_b_proc.txt"
    run_prog "$tmp" b_proc "$log" || { fail "$sched: build b_proc"; return; }
    common_checks "$sched b_proc" "$log"
    check "$sched b_proc: all checks passed" "grep -q 'b_proc: all checks passed' '$log'"
    echo "    b_proc: $(grep -c 'b_proc: ok' "$log") checks ok"

    log="$OUT/${sched}_b_spin.txt"
    run_prog "$tmp" b_spin "$log" || { fail "$sched: build b_spin"; return; }
    common_checks "$sched b_spin" "$log"
    check "$sched b_spin: done" "grep -q 'b_spin: done' '$log'"
    local late; late=$(grep -oE 'start [0-9]+ ms' "$log" | awk '$2 >= 1500' | wc -l)
    check "$sched b_spin: non-yielding children all start < 1500 ms (timer preemption)" "[ $late -eq 0 ] && [ \$(grep -c 'start .* ms' '$log') -eq 6 ]"
    local eq w; eq=$(grep 'b_spin: equal acc' "$log"); w=$(grep 'b_spin: weighted acc' "$log")
    echo "    $eq"; echo "    $w"
    set -- $(echo "$eq" | awk '{print $9, $10}')
    check "$sched b_spin equal: ratio $1% ~ 100%" "ratio_ok $1 100"
    check "$sched b_spin equal: ratio $2% ~ 100%" "ratio_ok $2 100"
    set -- $(echo "$w" | awk '{print $9, $10}')
    if [ "$sched" = rr ]; then
        check "rr b_spin weighted 1:2:4 ignored by RR: $1% ~ 100%" "ratio_ok $1 100"
        check "rr b_spin weighted 1:2:4 ignored by RR: $2% ~ 100%" "ratio_ok $2 100"
    else
        check "cfs b_spin weighted 1:2:4: $1% ~ 200%" "ratio_ok $1 200"
        check "cfs b_spin weighted 1:2:4: $2% ~ 400%" "ratio_ok $2 400"
    fi

    if [ "$want_gdb" = gdb ]; then
        gdb_ticks "$tmp" "$sched"
    fi
}

# GDB: count sched_class_proc_tick and the class tick until the first
# do_exit of the priority run, then compare with the kernel's ticks counter.
gdb_ticks() {
    local tmp=$1 sched=$2 fn log="$OUT/${sched}_gdb_ticks.txt"
    [ "$sched" = rr ] && fn=RR_proc_tick || fn=CFS_proc_tick
    (cd "$tmp" && make -s build-priority >/dev/null 2>&1)
    (cd "$tmp" && "$QEMU" -machine virt -nographic -bios default \
        -device loader,file=bin/ucore.img,addr=0x80200000 -S -gdb tcp::1235 </dev/null >"$OUT/${sched}_gdb_qemu.txt" 2>&1 &
     echo $! >"$tmp/qemu.pid")
    sleep 1
    cat >"$tmp/ticks.gdb" <<EOF
set pagination off
set confirm off
target remote :1235
set \$all = 0
set \$cls = 0
break sched_class_proc_tick
commands
silent
set \$all = \$all + 1
continue
end
break $fn
commands
silent
set \$cls = \$cls + 1
continue
end
break do_exit
commands
silent
printf "GDB_TICKS ticks=%d sched_class_proc_tick=%d $fn=%d idle_ticks=%d\n", ticks, \$all, \$cls, \$all - \$cls
kill
quit
end
continue
EOF
    (cd "$tmp" && timeout 300 "$GDB" -q -batch -x ticks.gdb bin/kernel >"$log" 2>&1)
    kill "$(cat "$tmp/qemu.pid")" 2>/dev/null; wait 2>/dev/null
    local line; line=$(grep GDB_TICKS "$log")
    echo "    $line"
    set -- $(echo "$line" | sed -E 's/[^0-9 ]+=//g; s/GDB_TICKS//')
    check "$sched gdb: one sched_class_proc_tick per timer tick ($2 == $1)" "[ -n '$line' ] && [ '$1' = '$2' ]"
    check "$sched gdb: class tick for every non-idle tick ($3 + $4 == $2)" "[ -n '$line' ] && [ \$(( $3 + $4 )) -eq $2 ] && [ $3 -gt 100 ]"
}

echo "QEMU: $($QEMU --version | head -1)"
case "$which_sched" in
rr | cfs) test_sched "$which_sched" ;;
all) test_sched rr; test_sched cfs ;;
*) echo "usage: $0 [rr|cfs|all] [gdb]"; exit 2 ;;
esac
echo "QEMU SUMMARY: failed=$fails"
[ "$fails" -eq 0 ]
