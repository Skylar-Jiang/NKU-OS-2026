#!/usr/bin/env python3
"""Build and boot real A-only kernels; never edit or inject official tests.

These explicit profiles skip the unfinished Best-Fit translation unit. They
are independent validation, not a substitute for the team's standard grade.
"""

import argparse
import os
from pathlib import Path
import select
import shlex
import shutil
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]


def build(profile, negative, logs):
    args = ["make", "-j2", f"LAB2_PROFILE={profile}",
            f"LAB2_BUDDY_NEGATIVE={negative}"]
    result = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    path = logs / f"build-{profile}-{negative}.log"
    path.write_text("$ " + shlex.join(args) + "\n" + result.stdout +
                    f"\nBUILD_EXIT={result.returncode}\n")
    if result.returncode:
        raise RuntimeError(f"build failed: {path}\n{result.stdout}")
    return ROOT / "bin" / profile / str(negative) / "ucore.img"


def boot(qemu, image, memory, marker, path):
    args = [qemu, "-machine", "virt", "-m", memory, "-nographic",
            "-bios", "default", "-device",
            f"loader,file={image},addr=0x80200000"]
    proc = subprocess.Popen(args, cwd=ROOT, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    data = bytearray()
    deadline = time.monotonic() + 20
    try:
        while time.monotonic() < deadline:
            readable, _, _ = select.select([proc.stdout], [], [], 0.2)
            if readable:
                chunk = os.read(proc.stdout.fileno(), 65536)
                if not chunk:
                    break
                data.extend(chunk)
                if marker.encode() in data:
                    break
            if proc.poll() is not None:
                break
    finally:
        if proc.poll() is None:
            proc.terminate()    # Only the QEMU process created by this test.
        try:
            tail, _ = proc.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            tail, _ = proc.communicate()
        data.extend(tail)
    output = data.decode(errors="replace")
    path.write_text("$ " + shlex.join(args) + "\n" + output +
                    f"\nQEMU_EXIT={proc.returncode} (terminated after marker/deadline)\n")
    if marker not in output:
        raise RuntimeError(f"missing expected marker: {marker}\nSee {path}")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=["all", "page-init", "buddy"], default="all")
    parser.add_argument("--qemu", default=os.environ.get("LAB2_QEMU"))
    parser.add_argument("--logs", type=Path, default=ROOT.parent / "report/evidence")
    args = parser.parse_args()
    qemu = args.qemu
    if not qemu:
        course_qemu = Path.home() / "opt/qemu-4.1.1/bin/qemu-system-riscv64"
        qemu = str(course_qemu) if course_qemu.is_file() else shutil.which("qemu-system-riscv64")
    if not qemu:
        parser.error("set --qemu or LAB2_QEMU to a QEMU executable")
    args.logs.mkdir(parents=True, exist_ok=True)
    results = []
    if args.suite in ("all", "page-init"):
        image = build("page-init", 0, args.logs)
        # The bundled firmware exposes DTB=0x82200000 in the default boot;
        # use at least 64 MiB for this environment's RAM-size matrix.
        for memory in ("64M", "128M", "256M"):
            name = f"page-init-{memory}"
            output = boot(qemu, image, memory,
                          "[A Page Init] layout, all reserved pages, roundtrip: PASS",
                          args.logs / f"{name}.log")
            assert "kernel panic" not in output and ": FAIL" not in output, name
            if memory != "256M":
                assert "[Test 1] Page Init: PASS" in output, name
            results.append(name + ": PASS")
            print(results[-1], flush=True)
    if args.suite in ("all", "buddy"):
        image = build("buddy", 0, args.logs)
        for memory in ("64M", "128M", "256M"):
            name = f"buddy-{memory}"
            output = boot(qemu, image, memory, "[Buddy] ALL NORMAL TESTS PASS",
                          args.logs / f"{name}.log")
            assert "kernel panic" not in output and ": FAIL" not in output, name
            assert "original boot-time free interval restored: PASS" in output, name
            results.append(name + ": PASS")
            print(results[-1], flush=True)
        expected = {
            1: "buddy: free requires an allocated block head",
            2: "buddy: free requires an allocated block head",
            3: "buddy: free size must match request or capacity",
            4: "buddy: free requires an allocated block head",
            5: "buddy: invalid page descriptor",
            6: "assertion failed: PageReserved(base + i)",
        }
        for negative, message in expected.items():
            image = build("buddy", negative, args.logs)
            name = f"buddy-negative-{negative}"
            output = boot(qemu, image, "128M", message, args.logs / f"{name}.log")
            assert f"[Buddy negative {negative}] BEGIN" in output, name
            assert "kernel panic at kern/mm/buddy_pmm.c:" in output, name
            assert "unexpectedly returned" not in output, name
            results.append(name + ": EXPECTED PANIC OBSERVED")
            print(results[-1], flush=True)
    (args.logs / "a-check-summary.txt").write_text("\n".join(results) + "\n")
    print(f"Raw build/boot evidence: {args.logs}")


if __name__ == "__main__":
    main()
