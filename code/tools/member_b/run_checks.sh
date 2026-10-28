#!/bin/sh
# Member B checks: build, normal run, DEBUG_GRADE run, GDB timer flow and
# isolated dispatch. Run from code/:  sh tools/member_b/run_checks.sh
# Logs go to tools/member_b/evidence/. The normal kernel build is unchanged.
#
# QEMU defaults to the local 4.1.1 build; override with QEMU=...
# DEFS=-Dsbadaddr=stval only works around binutils >= 2.38 rejecting the
# old CSR name in trapentry.S (member A's file); it changes no behaviour.
set -u
cd "$(dirname "$0")/../.."

QEMU=${QEMU:-$HOME/src/qemu-4.1.1/build-debug/riscv64-softmmu/qemu-system-riscv64}
GDB=${GDB:-riscv64-unknown-elf-gdb}
DEFS=${DEFS:--Dsbadaddr=stval}
OUT=tools/member_b/evidence
GDBDIR=tools/member_b/gdb
PORT=${PORT:-26100}
mkdir -p "$OUT"
fail=0

result() { if [ "$1" = 0 ]; then echo "PASS  $2"; else echo "FAIL  $2"; fail=1; fi; }

build() {
    make clean >/dev/null 2>&1
    make QEMU="$QEMU" DEFS="$1" >"$OUT/$2" 2>&1
}

run_gdb() {  # $1 = script name, $2 = log prefix
    sed "s/@PORT@/$PORT/" "$GDBDIR/$1.gdb" >"$OUT/.$1.gdb"
    "$QEMU" -machine virt -nographic -bios default \
        -device loader,file=bin/ucore.img,addr=0x80200000 \
        -monitor null -serial "file:$OUT/$2-serial.log" \
        -S -gdb tcp::"$PORT" >/dev/null 2>&1 &
    qpid=$!
    sleep 1
    timeout 120 "$GDB" -q -nx -batch bin/kernel -x "$OUT/.$1.gdb" >"$OUT/$2-gdb.log" 2>&1
    rc=$?
    kill $qpid 2>/dev/null; wait $qpid 2>/dev/null
    rm -f "$OUT/.$1.gdb"
    return $rc
}

"$QEMU" --version | head -1 >"$OUT/versions.log"
"$GDB" --version | head -1 >>"$OUT/versions.log"
riscv64-unknown-elf-gcc --version | head -1 >>"$OUT/versions.log"

# 1. normal build and run: 10 x "100 ticks", then sbi_shutdown()
build "$DEFS" build.log
result $? "build"
timeout 30 make qemu QEMU="$QEMU" DEFS="$DEFS" </dev/null >"$OUT/qemu-normal.log" 2>&1
rc=$?
n=$(grep -c '^100 ticks' "$OUT/qemu-normal.log")
[ $rc = 0 ] && [ "$n" = 10 ] && grep -q 'System shutdown after 10 prints.' "$OUT/qemu-normal.log"
result $? "normal run: $n x '100 ticks', QEMU exited by itself (rc=$rc)"

# 2. GDB: real timer interrupts
run_gdb timer_flow timer-flow
grep -q GDB_TIMER_FLOW_ALL_PASS "$OUT/timer-flow-gdb.log"
result $? "gdb timer flow ($(grep -c '^PASS:' "$OUT/timer-flow-gdb.log") assertions)"

# 3. GDB: isolated dispatch of the other interrupt codes
run_gdb dispatch dispatch
grep -q GDB_DISPATCH_ALL_PASS "$OUT/dispatch-gdb.log"
result $? "gdb isolated dispatch ($(grep -c '^PASS:' "$OUT/dispatch-gdb.log") assertions)"
s="$OUT/dispatch-serial.log"
expect='User software interrupt
Supervisor software interrupt
Hypervisor software interrupt
Machine software interrupt
User timer interrupt
Hypervisor timer interrupt
Machine timer interrupt
User external interrupt
Supervisor external interrupt
Hypervisor external interrupt
Machine external interrupt'
[ "$(tr -d '\r' <"$s" | grep ' interrupt$')" = "$expect" ]
result $? "dispatch labels match IRQ_* codes 0-4, 6-11"
[ "$(grep -c '^trapframe at' "$s")" = 2 ] && grep -q 'cause    0x0000000c' "$s" && grep -q 'cause    0x00000010' "$s"
result $? "unknown codes 12 and 16 print the trapframe"

# 4. DEBUG_GRADE: first "100 ticks" is followed by End of Test. and panic
build "$DEFS -DDEBUG_GRADE" build-debug-grade.log
timeout 5 make qemu QEMU="$QEMU" DEFS="$DEFS -DDEBUG_GRADE" </dev/null 2>&1 \
    | head -c 4000 >"$OUT/qemu-debug-grade.log"
grep -A2 '^100 ticks' "$OUT/qemu-debug-grade.log" | grep -q 'End of Test.' \
    && grep -q 'EOT: kernel seems ok.' "$OUT/qemu-debug-grade.log" \
    && [ "$(grep -c '^100 ticks' "$OUT/qemu-debug-grade.log")" = 1 ]
result $? "DEBUG_GRADE: panic after the first '100 ticks'"

make clean >/dev/null 2>&1
exit $fail
