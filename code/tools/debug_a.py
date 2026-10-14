#!/usr/bin/env python3
"""Reproduce guest GDB translation evidence and Page Init fault injection.

This uses RISC-V GDB and a real QEMU kernel, not host-QEMU source debugging.
Fault cases deliberately change the parsed DTB globals through GDB; their
results are injection tests, not claims about real malformed hardware DTBs.
"""

import argparse
import os
from pathlib import Path
import shlex
import shutil
import socket
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]


def run_case(qemu, gdb, name, commands, logs):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        port = listener.getsockname()[1]
    image = ROOT / "bin/page-init/0/ucore.img"
    args = [qemu, "-machine", "virt", "-m", "128M", "-nographic",
            "-bios", "default", "-device",
            f"loader,file={image},addr=0x80200000", "-S",
            "-gdb", f"tcp:127.0.0.1:{port}"]
    with tempfile.TemporaryDirectory(prefix="lab2-guest-gdb-") as directory:
        script = Path(directory) / "commands.gdb"
        full = ("set pagination off\nset confirm off\nset tcp connect-timeout 5\n"
                f"file {ROOT / 'bin/page-init/0/kernel'}\n"
                f"target remote 127.0.0.1:{port}\n" + commands)
        script.write_text(full)
        (logs / f"{name}.gdb").write_text(full)
        with (logs / f"{name}-serial.log").open("w") as serial:
            serial.write("$ " + shlex.join(args) + "\n")
            serial.flush()
            proc = subprocess.Popen(args, cwd=ROOT, stdout=serial,
                                    stderr=subprocess.STDOUT)
            try:
                # QEMU starts its listening socket before executing the guest.
                try:
                    result = subprocess.run([gdb, "--batch", "-nx", "-x", str(script)],
                                            cwd=ROOT, stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT, text=True, timeout=20)
                except subprocess.TimeoutExpired as exc:
                    partial = exc.stdout or b""
                    if isinstance(partial, bytes):
                        partial = partial.decode(errors="replace")
                    result = subprocess.CompletedProcess(
                        exc.cmd, 124, partial + "\nGDB_TIMEOUT=20 seconds\n")
                time.sleep(0.1)
            finally:
                if proc.poll() is None:
                    proc.terminate()
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
        (logs / f"{name}.gdb").write_text(full)
        (logs / f"{name}.log").write_text(result.stdout + f"\nGDB_EXIT={result.returncode}\n")
        if result.returncode:
            raise RuntimeError(f"{name}: {result.stdout}")
        return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default=os.environ.get("LAB2_QEMU", str(
        Path.home() / "opt/qemu-4.1.1/bin/qemu-system-riscv64")))
    parser.add_argument("--gdb", default=shutil.which("riscv64-unknown-elf-gdb"))
    parser.add_argument("--logs", type=Path, default=ROOT.parent / "report/evidence")
    args = parser.parse_args()
    if not args.gdb:
        parser.error("RISC-V GDB is required")
    args.logs.mkdir(parents=True, exist_ok=True)
    subprocess.run(["make", "-j2", "LAB2_PROFILE=page-init"], cwd=ROOT, check=True)
    translation = '''hbreak page_init_test
continue
printf "GUEST_PC=0x%llx SATP=0x%llx OFFSET=0x%llx\\n", $pc, $satp, va_pa_offset
set $guest_pc = (unsigned long long)$pc
set $guest_satp = (unsigned long long)$satp
set $root_pa = ($satp & 0xfffffffffff) << 12
set $pte = *(unsigned long long *)((char *)&boot_page_table_sv39 + 511 * 8)
set $translated = (($pte >> 10) << 12) + ($guest_pc & 0x3fffffff)
printf "ROOT_PA=0x%llx VPN2=%llu LEAF_PTE=0x%llx\\n", $root_pa, (($guest_pc >> 30) & 511), $pte
printf "TRANSLATED_PA=0x%llx OFFSET_PA=0x%llx\\n", $translated, $guest_pc - va_pa_offset
if $translated != $guest_pc - va_pa_offset
  quit 1
end
if ($guest_satp >> 60) != 8 || ($pte & 0xf) != 0xf
  quit 1
end
x/4i $pc
x/8bx $pc
eval "monitor xp /8bx 0x%llx", $translated
printf "PAGE_COUNT=%llu NBASE=%llu FREE=%u\\n", npage, nbase, free_area.nr_free
printf "[GDB guest] Sv39 level-2 leaf translation: PASS\\n"
detach
'''
    output = run_case(args.qemu, args.gdb, "gdb-guest-translation", translation, args.logs)
    assert "[GDB guest] Sv39 level-2 leaf translation: PASS" in output
    print("guest Sv39 translation: PASS", flush=True)

    prefix = "hbreak pmm_init\ncontinue\ndelete breakpoints\n"
    cases = {
        "zero-size": ("set variable memory_size = 0\n", "invalid DTB memory range"),
        "bad-base": ("set variable memory_base = 0\n", "invalid DTB memory range"),
        "overflow": ("set variable memory_size = 0xffffffffffffffff\n", "invalid DTB memory range"),
        "kernel-outside": ("set variable memory_size = 0x200000\n", "memory does not contain the kernel"),
        "metadata-outside": ("set variable memory_size = (((unsigned long long)&end - 0xffffffff40000000 + 4095) & ~4095) - 0x80000000 + 4096\n", "no room for page descriptors"),
    }
    for name, (mutation, expected) in cases.items():
        commands = prefix + mutation + '''hbreak __panic
continue
printf "PANIC_FORMAT=%s\\n", fmt
detach
'''
        output = run_case(args.qemu, args.gdb, "gdb-page-init-" + name, commands, args.logs)
        assert "PANIC_FORMAT=page_init: " + expected in output, name
        print(f"page-init injection {name}: EXPECTED PANIC OBSERVED", flush=True)

    tail = prefix + '''set variable memory_size = 0x4000000 - 17
hbreak page_init_test
continue
printf "UNALIGNED_SIZE=0x%llx NPAGE=%llu\\n", memory_size, npage
if npage != (0x80000000 + 0x4000000 - 17) / 4096
  quit 1
end
printf "[GDB page_init] incomplete tail page excluded: PASS\\n"
detach
'''
    output = run_case(args.qemu, args.gdb, "gdb-page-init-unaligned-tail", tail, args.logs)
    assert "[GDB page_init] incomplete tail page excluded: PASS" in output
    serial = (args.logs / "gdb-page-init-unaligned-tail-serial.log").read_text()
    assert "[A Page Init] layout, all reserved pages, roundtrip: PASS" in serial
    print("page-init injection incomplete tail page: PASS", flush=True)

    # Smallest whole-page reported range that fits its own descriptor array.
    # Actual backing RAM stays 128 MiB; only the parsed memory-size input changes.
    no_free = prefix + '''set $pages_pa = (((unsigned long long)&end - 0xffffffff40000000 + 4095) & ~4095)
set $delta = $pages_pa - 0x80000000
set $count = ($delta + (4096 - sizeof(struct Page)) - 1) / (4096 - sizeof(struct Page))
set variable memory_size = $count * 4096
hbreak default_check
continue
printf "NO_FREE_NPAGE=%llu NO_FREE_COUNT=%u\\n", npage, free_area.nr_free
if free_area.nr_free != 0
  quit 1
end
if free_area.free_list.next != &free_area.free_list || free_area.free_list.prev != &free_area.free_list
  quit 1
end
set $i = 0
while $i < npage - nbase
  if (pages[$i].flags & 1) == 0
    quit 1
  end
  set $i = $i + 1
end
printf "[GDB page_init] no full free page: PASS (stopped before allocator self-test)\\n"
disconnect
'''
    output = run_case(args.qemu, args.gdb, "gdb-page-init-no-free", no_free, args.logs)
    assert "[GDB page_init] no full free page: PASS" in output
    print("page-init injection no full free page: PASS", flush=True)


if __name__ == "__main__":
    main()
