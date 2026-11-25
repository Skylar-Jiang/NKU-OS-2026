#!/bin/bash
set -eu
export PATH="/home/mcz/riscv/sifive-2020.12.8-verified/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14/bin:$PATH"
cd "$(dirname "$0")/.."
qemu=${QEMU:-/home/mcz/riscv/qemu-4.1.1/bin/qemu-system-riscv64}
build=${LAB5_C_BUILD:-"$PWD/build"}
mkdir -p "$build" evidence
includes='-I../../libs -I../../kern/process -I../../kern/mm -I../../kern/trap -I../../kern/sync -I../../kern/driver -I../../kern/schedule -I../../kern/debug'
flags='-march=rv64imac -mabi=lp64 -mcmodel=medany -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -ffunction-sections -fdata-sections -msmall-data-limit=0'
# The integrated proc.c supplies B's real alloc_proc; old C-only baselines do not.
if grep -q '^alloc_proc(void)' ../../kern/process/proc.c; then
 flags="$flags -DMEMBER_C_PRODUCTION_ALLOC_PROC"
fi
for src in ../../kern/mm/pmm.c ../../kern/mm/vmm.c ../../kern/driver/intr.c ../../libs/string.c ../../libs/hash.c ../../libs/printfmt.c tests/runtime.c tests/start.S tests/trapentry.S; do
 riscv64-unknown-elf-gcc $flags $includes -c "$src" -o "$build/$(basename "$src").o"
done
# Compile the migrated allocator too; runtime intentionally substitutes allocation.
riscv64-unknown-elf-gcc $flags $includes -c ../../kern/mm/default_pmm.c -o "$build/default-check.o"
riscv64-unknown-elf-gcc $flags -nostdlib -Wl,--gc-sections -T tests/test.ld "$build/pmm.c.o" "$build/vmm.c.o" "$build/intr.c.o" "$build/string.c.o" "$build/hash.c.o" "$build/printfmt.c.o" "$build/runtime.c.o" "$build/start.S.o" "$build/trapentry.S.o" -o "$build/runtime.elf"
"$qemu" --version > evidence/qemu-version.txt
timeout 25 "$qemu" -machine virt -m 128M -nographic -bios none -kernel "$build/runtime.elf" > evidence/cow-runtime.log 2>&1
cat evidence/cow-runtime.log
grep -q 'ALL MEMBER C ISOLATED TESTS PASS' evidence/cow-runtime.log
# Separate baseline: no production-source edits needed to select eager fork.
riscv64-unknown-elf-gcc $flags $includes -DLAB5_COW=0 -c ../../kern/mm/vmm.c -o "$build/eager-vmm.o"
riscv64-unknown-elf-gcc $flags $includes -DLAB5_COW=0 -c tests/runtime.c -o "$build/eager-runtime.o"
riscv64-unknown-elf-gcc $flags -nostdlib -Wl,--gc-sections -T tests/test.ld "$build/pmm.c.o" "$build/eager-vmm.o" "$build/intr.c.o" "$build/string.c.o" "$build/hash.c.o" "$build/printfmt.c.o" "$build/eager-runtime.o" "$build/start.S.o" "$build/trapentry.S.o" -o "$build/eager.elf"
timeout 25 "$qemu" -machine virt -m 128M -nographic -bios none -kernel "$build/eager.elf" > evidence/eager-runtime.log 2>&1
cat evidence/eager-runtime.log
grep -q 'ALL MEMBER C EAGER TESTS PASS' evidence/eager-runtime.log
