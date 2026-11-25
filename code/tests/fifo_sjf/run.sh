#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
make --no-print-directory all
set +e
./build/fifo_sjf_test 2>&1 | tee results.txt
codes=("${PIPESTATUS[@]}")
set -e
status=${codes[0]}
if ((status == 0 && codes[1] != 0)); then status=${codes[1]}; fi
printf 'EXIT_STATUS=%d\n' "$status" | tee -a results.txt
exit "$status"
