#!/usr/bin/env python3
"""Turn a temporary copy of code/ into the member-B test kernel.

Changes are made only inside the given copy:
  * kern/process/proc.c: call member_b_test_after_proc_init() at the end of
    proc_init, member_b_test_in_init_main() at the start of init_main, and
    #include the test source at the end of the file (so tests reach the real
    static functions).
  * Makefile: link with --wrap for kmalloc/kfree/alloc_pages (fault injection).
The production code of proc.c is not edited.
"""

import os
import re
import sys


def die(msg):
    print("[member-b prepare] ERROR: " + msg)
    raise SystemExit(1)


def body_bounds(text, name):
    pat = re.compile(r"(?m)^(?:[A-Za-z_][^\n;]*?)?\b" + re.escape(name) +
                     r"\s*\([^;{}]*\)\s*\{")
    ms = list(pat.finditer(text))
    if len(ms) != 1:
        die("function not found exactly once: " + name)
    start = text.index("{", ms[0].start())
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return start, i
    die("unbalanced braces in " + name)


if len(sys.argv) != 3:
    die("usage: prepare.py <temp code copy> <tests dir>")
root, tests = map(os.path.abspath, sys.argv[1:])

proc = os.path.join(root, "kern", "process", "proc.c")
with open(proc, encoding="utf-8") as f:
    text = f.read()
if "member_b_test" in text:
    die("proc.c already prepared")

_, end = body_bounds(text, "proc_init")
text = (text[:end] +
        "    { void member_b_test_after_proc_init(void); "
        "member_b_test_after_proc_init(); }\n" + text[end:])
start, _ = body_bounds(text, "init_main")
text = (text[:start + 1] +
        "\n    { void member_b_test_in_init_main(void); "
        "member_b_test_in_init_main(); }" + text[start + 1:])
text += '\n#include "%s"\n' % os.path.join(tests, "member_b_tests.inc")
with open(proc, "w", encoding="utf-8") as f:
    f.write(text)

mk = os.path.join(root, "Makefile")
with open(mk, encoding="utf-8") as f:
    make = f.read()
anchor = "LDFLAGS\t+= -nostdlib --gc-sections --print-gc-sections\n"
if anchor not in make:
    die("LDFLAGS anchor not found in Makefile")
make = make.replace(anchor, anchor +
                    "LDFLAGS\t+= --wrap=kmalloc --wrap=kfree --wrap=alloc_pages\n")
with open(mk, "w", encoding="utf-8") as f:
    f.write(make)
print("member-b test kernel prepared in " + root)
