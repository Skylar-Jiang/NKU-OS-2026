#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source tests/stride/env.sh
mkdir -p tests/results
make clean > tests/results/clean.txt 2>&1
make -j4 SCHED=stride > tests/results/build.txt 2>&1
make grade SCHED=stride > tests/results/grade.txt 2>&1
