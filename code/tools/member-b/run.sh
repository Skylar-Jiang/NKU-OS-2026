#!/bin/sh
# Member B (2413507) tests. Run from code/:  sh tools/member-b/run.sh [log]
# Copies the current tree to a temporary directory, injects test_lab5_b.c at
# the entry of init_main (same function-boundary rule as tools/inject.py),
# adds b_cowlife as a user program, builds it as the TEST program and runs
# QEMU. The source tree is not modified.
set -e
here=$(cd "$(dirname "$0")" && pwd)
code=$(cd "$here/../.." && pwd)
tmp=${LAB5_B_TMP:-/tmp/lab5_b_test}
log=${1:-$tmp.log}

rm -rf "$tmp"
mkdir -p "$tmp"
tar -C "$code" --exclude=obj --exclude=bin -cf - . | tar -C "$tmp" -xf -
mkdir -p "$tmp/kern/test"
cp "$here/test_lab5_b.c" "$tmp/kern/test/"
cp "$here/b_cowlife.c" "$tmp/user/"

python3 - "$tmp" <<'EOF'
import re, sys
root = sys.argv[1]
mk = root + "/Makefile"
s = open(mk).read()
for var in ("KINCLUDE", "KSRCDIR"):
    m = re.search(r"(?m)^" + var + r"[ \t]*\+=.*(?:\n[ \t]+[^\n]*)*", s)
    s = s[:m.end()] + "\n" + var + "\t+= kern/test" + s[m.end():]
open(mk, "w").write(s)
p = root + "/kern/process/proc.c"
s = open(p).read()
m = re.search(r"(?m)^init_main\(void \*arg\)\n\{\n", s)
s = s[:m.end()] + "    extern void test_lab5_b_kernel(void);\n    test_lab5_b_kernel();\n" + s[m.end():]
open(p, "w").write(s)
EOF

cd "$tmp"
make build-b_cowlife > "$tmp.build" 2>&1 || { tail -20 "$tmp.build"; exit 1; }
timeout ${LAB5_B_TIMEOUT:-60} make qemu < /dev/null > "$log" 2>&1 || true
grep -a -E "^\[B\]|b_cowlife|all user-mode|init check memory|init_main: free|panic|assert" "$log" || true
fails=$(grep -a -c -E "^\[B\] FAIL|b_cowlife FAIL" "$log" || true)
if grep -a -q "b_cowlife pass." "$log" && grep -a -q "init check memory pass." "$log" && [ "$fails" = 0 ]; then
    echo "member-b tests: PASS (log: $log)"
else
    echo "member-b tests: FAIL (log: $log)"
    exit 1
fi
