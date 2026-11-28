#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
case "${1:-all}" in
 host) bash tests/fifo_sjf/run.sh; bash tests/rr_cfs/run_host.sh; bash tests/stride/run_host.sh; bash tests/stride/mutation.sh;;
 grade) bash tests/stride/build.sh;;
 qemu) bash tests/benchmark/run_process.sh; bash tests/benchmark/run_qemu.sh; python3 tests/benchmark/analyze.py;;
 all) bash tests/run_all.sh host; bash tests/run_all.sh grade; bash tests/run_all.sh qemu;;
 *) echo 'usage: tests/run_all.sh [host|grade|qemu|all]' >&2; exit 2;;
esac
