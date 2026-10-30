#!/bin/sh
# Run with WSL/Linux from any directory; all injected tests use isolated copies.
set -eu
cd "$(dirname "$0")/.."
GCCPREFIX=${GCCPREFIX:-riscv64-unknown-elf-}
QEMU=${QEMU:-qemu-system-riscv64}
GDB=${GDB:-${GCCPREFIX}gdb}
export GCCPREFIX QEMU GDB
TOOLBIN=$(dirname "$(command -v "${GCCPREFIX}gcc")")
export PATH="$TOOLBIN:$PATH"
LAB3_SOURCE="$PWD" LAB3_TOOLBIN="$TOOLBIN" LAB3_QEMU="$QEMU" \
    python3 tools/member_a/tests/run_validation.py
sh tools/member_b/run_checks.sh
python3 tools/member_c/run_checks.py
make grade GCCPREFIX="$GCCPREFIX" QEMU="$QEMU"
echo LAB3_INTEGRATION_ALL_PASS
