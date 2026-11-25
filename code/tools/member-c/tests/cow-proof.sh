#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
build=${LAB5_C_BUILD:-"$PWD/build"}
qemu=${QEMU:-/home/mcz/riscv/qemu-4.1.1/bin/qemu-system-riscv64}
gdb=/home/mcz/riscv/sifive-2020.12.8-verified/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14/bin/riscv64-unknown-elf-gdb
"$qemu" -machine virt -m 128M -nographic -bios none -kernel "$build/runtime.elf" -S -gdb tcp:127.0.0.1:12446 > evidence/cow-proof-qemu.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
timeout 30 "$gdb" -q -batch "$build/runtime.elf" -x tests/cow-proof.gdb > evidence/cow-proof-session.log 2>&1
cat evidence/cow-proof-session.log
grep -Fq 'Same physical page    = 1' evidence/cow-proof-before.txt
grep -Fq 'Physical page refs    = 2' evidence/cow-proof-before.txt
grep -Fq 'EPC unchanged (retry) = 1' evidence/cow-proof-after.txt
grep -Fq 'Different pages       = 1' evidence/cow-proof-after.txt
grep -Fq 'Parent unchanged      = 1' evidence/cow-proof-isolation.txt
grep -Fq 'Child store committed = 1' evidence/cow-proof-isolation.txt
grep -Fq 'Data isolation        = 1' evidence/cow-proof-isolation.txt
