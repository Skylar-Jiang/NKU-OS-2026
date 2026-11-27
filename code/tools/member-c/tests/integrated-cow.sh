#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
export PATH="/home/mcz/riscv/sifive-2020.12.8-verified/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14/bin:$PATH"
mkdir -p tools/member-c/build/native-cow
# Only our six known capture markers, never another session's files.
if [ "${COW_NATIVE_PAUSE:-0}" = 1 ]; then
 for n in 1 2 3; do rm -f "tools/member-c/build/native-cow/stage$n.ready" "tools/member-c/build/native-cow/stage$n.next"; done
fi
make build-cowproof > tools/member-c/evidence/integrated-cow-build.log 2>&1
# GDB script expects this test's page-aligned .data symbol.
test "$(riscv64-unknown-elf-nm obj/__user_cowproof.out | awk '$3 == "cow_data" {print $1}')" = 0000000000801000
qemu=/home/mcz/riscv/qemu-4.1.1/bin/qemu-system-riscv64
gdb=/home/mcz/riscv/sifive-2020.12.8-verified/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14/bin/riscv64-unknown-elf-gdb
"$qemu" -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000 -S -gdb tcp:127.0.0.1:12447 > tools/member-c/evidence/integrated-cow-qemu.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
"$gdb" -q -batch bin/kernel -x tools/member-c/tests/integrated-cow.gdb 2>&1 | tee tools/member-c/evidence/integrated-cow-gdb.log
sleep 2
grep -q 'SPP=0' tools/member-c/evidence/integrated-cow-gdb.log
grep -q 'Shared physical page=1; refs=2' tools/member-c/evidence/integrated-cow-gdb.log
grep -q 'Different physical pages=1' tools/member-c/evidence/integrated-cow-gdb.log
grep -q 'EPC unchanged=1' tools/member-c/evidence/integrated-cow-gdb.log
grep -q 'cowproof pass.' tools/member-c/evidence/integrated-cow-qemu.log
grep -q 'init check memory pass.' tools/member-c/evidence/integrated-cow-qemu.log
echo 'Integrated user COW demonstration PASS'
