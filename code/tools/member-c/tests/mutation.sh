#!/bin/bash
set -eu
export PATH="/home/mcz/riscv/sifive-2020.12.8-verified/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14/bin:$PATH"
cd "$(dirname "$0")/.."
build=${LAB5_C_BUILD:-"$PWD/build"}
qemu=${QEMU:-/home/mcz/riscv/qemu-4.1.1/bin/qemu-system-riscv64}
# Deterministic same-class isolation bug, not a Linux Dirty COW exploit.
python3 - "$build" <<'PY'
from pathlib import Path
import sys
s=Path('../../kern/mm/vmm.c').read_text()
before='''        if (*pte & PTE_COW) {
            uintptr_t checked = va < start ? start : va;
            if (do_cow_fault(mm, checked) != 0) return 0;
        }
        if (!(*pte & PTE_W)) return 0;'''
assert before in s
s=s.replace(before,'        /* Deliberate test mutant: write shared frame through kernel alias. */')
Path(sys.argv[1]+'/vmm-mutant.c').write_text(s)
PY
includes='-I../../libs -I../../kern/process -I../../kern/mm -I../../kern/trap -I../../kern/sync -I../../kern/driver -I../../kern/schedule -I../../kern/debug'
flags='-march=rv64imac -mabi=lp64 -mcmodel=medany -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -ffunction-sections -fdata-sections -msmall-data-limit=0'
# The integrated proc.c supplies B's real alloc_proc; old C-only baselines do not.
if grep -q '^alloc_proc(void)' ../../kern/process/proc.c; then
 flags="$flags -DMEMBER_C_PRODUCTION_ALLOC_PROC"
fi
riscv64-unknown-elf-gcc $flags $includes -c "$build/vmm-mutant.c" -o "$build/vmm-mutant.o"
# Skip the earlier OOM-preflight check only in this targeted mutant run so
# the parent/child data isolation assertion itself is reached.
riscv64-unknown-elf-gcc $flags $includes -DTEST_COPYOUT_MUTANT -c tests/runtime.c -o "$build/runtime-mutant.o"
riscv64-unknown-elf-gcc $flags -nostdlib -Wl,--gc-sections -T tests/test.ld "$build/pmm.c.o" "$build/vmm-mutant.o" "$build/intr.c.o" "$build/string.c.o" "$build/hash.c.o" "$build/printfmt.c.o" "$build/runtime-mutant.o" "$build/start.S.o" "$build/trapentry.S.o" -o "$build/mutant.elf"
set +e
timeout 25 "$qemu" -machine virt -m 128M -nographic -bios none -kernel "$build/mutant.elf" > evidence/mutation.log 2>&1
status=$?
set -e
# QEMU 4.1.1 may exit 0 for the test finisher's failure value too.
# Require the exact failed invariant, not a process-status guess.
test "$status" -ne 124
grep -q '^FAIL copy_to_user breaks COW through physical alias' evidence/mutation.log
if grep -q 'ALL MEMBER C ISOLATED TESTS PASS' evidence/mutation.log; then exit 1; fi
echo 'MUTATION DETECTED: kernel alias bypass violates parent/child isolation'
