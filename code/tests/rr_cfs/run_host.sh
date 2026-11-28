#!/usr/bin/env bash
# Host-side RR/CFS unit tests: builds host/ and saves results/host.txt.
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p results
make --no-print-directory -C host all >/dev/null
set +e
./host/build/rr_cfs_test 2>&1 | tee results/host.txt
codes=("${PIPESTATUS[@]}")
set -e
status=${codes[0]}
if ((status == 0 && codes[1] != 0)); then status=${codes[1]}; fi
printf 'EXIT_STATUS=%d\n' "$status" | tee -a results/host.txt
exit "$status"
