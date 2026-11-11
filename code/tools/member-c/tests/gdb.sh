#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
tool=/home/mcz/riscv/sifive-2020.12.8-verified/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14/bin
qemu=/home/mcz/riscv/qemu-4.1.1/bin/qemu-system-riscv64
"$qemu" -machine virt -m 128M -nographic -bios none -kernel evidence/c-runtime-s.elf -S -gdb tcp:127.0.0.1:12444 > evidence/gdb-qemu.log 2>&1 &
qemu_pid=$!
trap 'kill "$qemu_pid" 2>/dev/null || true' EXIT
timeout 25 "$tool/riscv64-unknown-elf-gdb" -q -batch evidence/c-runtime-s.elf -x tests/switch.gdb > evidence/gdb-switch.log 2>&1
cat evidence/gdb-switch.log
