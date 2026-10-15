#!/usr/bin/env bash
set -o pipefail
export PATH=/home/lenovo/riscv-elf-toolchains/bin:$PATH
export QEMU=/home/lenovo/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
cd /mnt/d/NKU-OS-2026-lab2-integration/code || exit
logs=../report/evidence/member-b/vscode-final
mkdir -p "$logs"
case "$1" in
 build) { make clean && make; } 2>&1 | tee "$logs/build.log"; rc=${PIPESTATUS[0]}; printf '\nBUILD_EXIT=%s\n' "$rc" | tee -a "$logs/build.log" ;;
 grade) make grade 2>&1 | tee "$logs/grade.log"; rc=${PIPESTATUS[0]}; printf '\nGRADE_EXIT=%s\n' "$rc" | tee -a "$logs/grade.log" ;;
 qemu) make > "$logs/rebuild.log" 2>&1 || exit; python3 ../report/member-b/tools/run_qemu.py 2>&1 | tee "$logs/qemu.log"; rc=${PIPESTATUS[0]}; printf '\nQEMU_CHECK_EXIT=%s\n' "$rc" | tee -a "$logs/qemu.log" ;;
 check-a) python3 tools/check_a.py --qemu "$QEMU" --logs "$logs/check-a" 2>&1 | tee "$logs/check-a.log"; rc=${PIPESTATUS[0]}; printf '\nCHECK_A_EXIT=%s\n' "$rc" | tee -a "$logs/check-a.log" ;;
esac
