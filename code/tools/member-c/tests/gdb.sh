#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
build=${LAB5_C_BUILD:-"$PWD/build"}
qemu=${QEMU:-/home/mcz/riscv/qemu-4.1.1/bin/qemu-system-riscv64}
gdb=/home/mcz/riscv/sifive-2020.12.8-verified/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14/bin/riscv64-unknown-elf-gdb
"$qemu" -machine virt -m 128M -nographic -bios none -kernel "$build/runtime.elf" -S -gdb tcp:127.0.0.1:12445 > evidence/gdb-qemu.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
timeout 30 "$gdb" -q -batch "$build/runtime.elf" -x tests/cow.gdb > evidence/cow-gdb.log 2>&1
cat evidence/cow-gdb.log
