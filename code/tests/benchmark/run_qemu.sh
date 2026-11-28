#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source tests/stride/env.sh
out="$PWD/tests/results/qemu"
mkdir -p "$out"
tmp=$(mktemp -d /tmp/lab6-c-benchmark.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
# Only temporary build copies are edited; no production A/B file is patched.
tar -cf - Makefile kern libs user tools | tar -xf - -C "$tmp"
cp tests/benchmark/schedbench.c "$tmp/user/"
repeats=${REPEATS:-3}
declare -A names=([rr]=RR_scheduler [stride]=stride_scheduler [fifo]=FIFO [sjf]=SJF [cfs]=CFS_scheduler)
for policy in ${POLICIES:-rr stride fifo sjf cfs}; do
 for scenario in ${SCENARIOS:-0 1 2 3}; do
  touch "$tmp/user/schedbench.c"
  (cd "$tmp" && make -s build-schedbench SCHED="$policy" BENCH=1 "DEFS=-DBENCH_CASE=$scenario") > "$out/${policy}_${scenario}.build.txt" 2>&1
  for round in $(seq 1 "$repeats"); do
   log="$out/${policy}_${scenario}_${round}.txt"
   set +e
   timeout 45 "$QEMU" -machine virt -nographic -bios default -icount shift=3,align=off,sleep=off \
      -device loader,file="$tmp/bin/ucore.img",addr=0x80200000 > "$log" 2>&1
   status=$?
   set -e
   if [ "$status" = 124 ]; then echo "FAIL timeout $policy case=$scenario round=$round"; exit 1; fi
   grep -q "sched class: ${names[$policy]}" "$log"
   grep -q "schedbench pass. case=$scenario" "$log"
   grep -q 'init check memory pass.' "$log"
   test "$(grep -c '^BJOB_DONE ' "$log")" = 4
   if grep -q 'user panic' "$log"; then exit 1; fi
   echo "PASS $policy case=$scenario round=$round"
  done
 done
 # Official real user priority workload; no observer, same boot policy.
 (cd "$tmp" && make -s build-priority SCHED="$policy" BENCH=0) > "$out/${policy}_priority.build.txt" 2>&1
 timeout 45 "$QEMU" -machine virt -nographic -bios default \
    -device loader,file="$tmp/bin/ucore.img",addr=0x80200000 > "$out/${policy}_priority.txt" 2>&1
 grep -q 'main: wait pids over' "$out/${policy}_priority.txt"
 grep -q 'init check memory pass.' "$out/${policy}_priority.txt"
 echo "PASS $policy priority"
done
