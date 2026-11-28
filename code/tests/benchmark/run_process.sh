#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source tests/stride/env.sh
out="$PWD/tests/results/process"
mkdir -p "$out"
tmp=$(mktemp -d /tmp/lab6-c-process.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
tar -cf - Makefile kern libs user tools | tar -xf - -C "$tmp"
cp tests/rr_cfs/qemu/user/*.c "$tmp/user/"
# Reuse B's actual allocator/lifecycle assertions, only in a temporary tree.
python3 - "$tmp/kern/process/proc.c" tests/rr_cfs/qemu/kernel/b_kcheck.c <<'PY'
import re,sys
p=sys.argv[1];s=open(p).read();m=re.search(r'(?m)^void proc_init\(void\)\s*\{',s);assert m
i=m.end()-1;depth=0
while True:
 depth += {'{':1,'}':-1}.get(s[i],0)
 if depth==0:break
 i+=1
s=s[:i]+"    b_kcheck_alloc_proc();\n"+s[i:]
s=s[:m.start()]+open(sys.argv[2]).read()+"\n"+s[m.start():]
open(p,'w').write(s)
PY
for policy in rr stride cfs; do
 for prog in b_proc b_spin; do
  log="$out/${policy}_${prog}.txt"
  (cd "$tmp" && make -s "build-$prog" SCHED="$policy" BENCH=0) > "$out/${policy}_${prog}.build.txt" 2>&1
  timeout 45 "$QEMU" -machine virt -nographic -bios default -device loader,file="$tmp/bin/ucore.img",addr=0x80200000 > "$log" 2>&1
  grep -q 'b_kcheck: six Lab6 scheduling fields ok' "$log"
  grep -q 'init check memory pass.' "$log"
  if [ "$prog" = b_proc ]; then grep -q 'b_proc: all checks passed' "$log"; fi
  if grep -q 'user panic' "$log"; then exit 1; fi
  echo "PASS integrated $policy $prog"
 done
done
