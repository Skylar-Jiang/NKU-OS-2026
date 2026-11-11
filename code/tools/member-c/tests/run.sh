#!/bin/bash
set -eu
export PATH="/home/mcz/riscv/sifive-2020.12.8-verified/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14/bin:$PATH"
cd "$(dirname "$0")/.."
qemu=/home/mcz/riscv/qemu-4.1.1/bin/qemu-system-riscv64
mkdir -p evidence/objects
includes='-I../../libs -I../../kern/process -I../../kern/mm -I../../kern/trap -I../../kern/sync -I../../kern/driver -I../../kern/schedule -I../../kern/debug'
flags='-march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany -O2 -g -ffreestanding -fno-builtin -fno-stack-protector -ffunction-sections -fdata-sections -msmall-data-limit=0'
# The older course GCC uses rv64imac (CSR/fence are part of its ISA revision).
flags=${flags/rv64imac_zicsr_zifencei/rv64imac}
for src in ../../kern/process/proc.c ../../kern/process/switch.S ../../kern/schedule/sched.c ../../kern/driver/intr.c ../../libs/string.c tests/start.S tests/register_probe.S tests/schedule_runtime.c; do
 riscv64-unknown-elf-gcc $flags $includes -c "$src" -o "evidence/objects/$(basename "$src").o"
done
riscv64-unknown-elf-gcc $flags -nostdlib -Wl,--gc-sections -T tests/test.ld evidence/objects/*.o -o evidence/c-runtime.elf
riscv64-unknown-elf-objdump -d evidence/c-runtime.elf > evidence/c-runtime.disasm
"$qemu" --version > evidence/qemu-version.txt
timeout 20 "$qemu" -machine virt -m 128M -nographic -bios none -kernel evidence/c-runtime.elf > evidence/c-runtime.log 2>&1
cat evidence/c-runtime.log
grep -q 'ALL C ISOLATED TESTS PASS' evidence/c-runtime.log
riscv64-unknown-elf-gcc $flags $includes -DTEST_S_MODE -c tests/schedule_runtime.c -o evidence/objects/schedule_runtime.c.o
riscv64-unknown-elf-gcc $flags -nostdlib -Wl,--gc-sections -T tests/test.ld evidence/objects/*.o -o evidence/c-runtime-s.elf
timeout 20 "$qemu" -machine virt -m 128M -nographic -bios none -kernel evidence/c-runtime-s.elf > evidence/c-runtime-s.log 2>&1
cat evidence/c-runtime-s.log
grep -q 'ALL C ISOLATED TESTS PASS' evidence/c-runtime-s.log
set +e
make -C ../.. -j4 > evidence/build.log 2>&1
echo "Full original lab4 build exit: $? (A/B and inherited functions still missing)" >> evidence/build.log
