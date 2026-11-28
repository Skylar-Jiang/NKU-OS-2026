#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p tests/stride/build tests/results
flags="-std=gnu99 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"
inc="-Ilibs -Ikern/process -Ikern/schedule -Ikern/trap -Ikern/mm -Ikern/debug -D__riscv_xlen=64 -fno-builtin"
gcc $flags $inc -c tests/stride/host.c -o tests/stride/build/host.o
gcc $flags -c tests/fifo_sjf/host_io.c -o tests/stride/build/io.o
gcc $flags -c tests/rr_cfs/host/host_guard.c -o tests/stride/build/guard.o
gcc $flags -no-pie tests/stride/build/{host,io,guard}.o -o tests/stride/build/test
tests/stride/build/test 2>&1 | tee tests/results/stride-host.txt
