#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 - <<'PY'
from pathlib import Path
b=Path('tests/stride/build')
s=Path('kern/schedule/default_sched_stride.c').read_text()
start=s.index('    if (rq->stride_running == p && p->runs > 0)')
end=s.index('    /* New tasks start at virtual time;',start)
s=s[:start]+s[end:]
(b/'stride-mutant.c').write_text(s)
s=Path('tests/stride/host.c').read_text().replace('../../kern/schedule/default_sched_stride.c',(b/'stride-mutant.c').resolve().as_posix()).replace('../fifo_sjf/host_io.h',Path('tests/fifo_sjf/host_io.h').resolve().as_posix()).replace('../rr_cfs/host/host_guard.h',Path('tests/rr_cfs/host/host_guard.h').resolve().as_posix())
(b/'host-mutant.c').write_text(s)
PY
flags="-std=gnu99 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie"
inc="-Ilibs -Ikern/process -Ikern/schedule -Ikern/trap -Ikern/mm -Ikern/debug -D__riscv_xlen=64 -fno-builtin"
gcc $flags $inc -c tests/stride/build/host-mutant.c -o tests/stride/build/mutant.o
gcc $flags -no-pie tests/stride/build/{mutant,io,guard}.o -o tests/stride/build/mutant
set +e
tests/stride/build/mutant > tests/results/stride-mutation.txt 2>&1
status=$?
set -e
test "$status" = 1
grep -q '\[FAIL\] priority set in first quantum reconciles charge' tests/results/stride-mutation.txt
echo 'EXPECTED_MUTANT_DETECTED: first-quantum priority charge regression; production source unchanged' | tee -a tests/results/stride-mutation.txt
