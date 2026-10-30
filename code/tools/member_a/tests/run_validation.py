#!/usr/bin/env python3
"""Build and exercise real RISC-V code in QEMU; GDB checks use hardware stops."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time

PACKAGE_OUT = Path(__file__).resolve().parents[1]
ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SOURCE = PACKAGE_OUT / 'code' if (PACKAGE_OUT / 'code/Makefile').is_file() else ROOT if (ROOT / 'Makefile').is_file() else ROOT / 'work/course-code-download/extracted/lab3_new'
SOURCE = Path(os.environ.get('LAB3_SOURCE', str(DEFAULT_SOURCE))).resolve()
# When this verifier is stored inside code/tools/member_a, keep its build
# copies outside the source tree to prevent recursive copytree traversal.
OUT_BASE = Path(os.environ.get('LAB3_VERIFY_ROOT', str(SOURCE.parent.parent / 'lab3-verification'))) / 'member_a'
OUT = OUT_BASE / ('run-' + str(time.time_ns()))
TESTS = Path(__file__).resolve().parent
EVIDENCE = Path(os.environ.get('LAB3_A_EVIDENCE', str(PACKAGE_OUT / 'evidence')))
STAGE = OUT / 'validation'
TOOLBIN = Path(os.environ.get('LAB3_TOOLBIN', str(Path(shutil.which('riscv64-unknown-elf-gcc') or 'riscv64-unknown-elf-gcc').parent)))
QEMU = os.environ.get('LAB3_QEMU', shutil.which('qemu-system-riscv64') or 'qemu-system-riscv64')
GDB = str(TOOLBIN / 'riscv64-unknown-elf-gdb')
ENV = dict(os.environ, PATH=str(TOOLBIN) + os.pathsep + os.environ['PATH'])
REGS = ['zero', 'ra', 'sp', 'gp', 'tp', 't0', 't1', 't2', 's0', 's1',
        'a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 's2', 's3',
        's4', 's5', 's6', 's7', 's8', 's9', 's10', 's11', 't3', 't4', 't5', 't6']
RESULTS = {}


def run(args, cwd, log, timeout=60):
    print('RUN', log, flush=True)
    result = subprocess.run(args, cwd=cwd, env=ENV, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout, text=True)
    (EVIDENCE / log).write_text('$ ' + ' '.join(map(str, args)) + '\n' + result.stdout)
    return result


def must(condition, message):
    if not condition:
        raise AssertionError(message)


def gdb_check(commands, condition, label):
    commands += [f'if !({condition})', f'  echo FAIL: {label}\\n', '  quit 1', 'end']


def gdb_script(port, context):
    cmds = ['set pagination off', 'set confirm off', 'set print pretty on',
            'set remotetimeout 10', 'set architecture riscv:rv64', f'target remote :{port}']
    if context:
        cmds += ['hbreak *lab3_a_probe_trap', 'continue', 'delete breakpoints',
                 'echo === BEFORE EXPLICIT 32-BIT EBREAK ===\\n', 'x/i $pc']
        gdb_check(cmds, '(unsigned long long)$sp % 16 == 0', 'pre-trap stack alignment')
        cmds += ['set $a_before_status = $sstatus']
    cmds += ['hbreak *__alltraps', 'continue', 'delete breakpoints',
             'echo === HARDWARE TRAP ENTRY ===\\n', 'info registers pc sp sstatus sepc scause',
             'set $a_entry_status = $sstatus', 'set $a_epc = $sepc', 'set $a_cause = $scause']
    for i, reg in enumerate(REGS):
        cmds.append(f'set $a_old_{i} = ${reg}')
    gdb_check(cmds, '$a_cause == 3' if context else '$a_cause == 0x8000000000000005', 'expected scause')
    cmds += ['hbreak *trap', 'continue', 'delete breakpoints', 'set $a_frame = $a0',
             'echo === C TRAP FRAME ===\\n', 'p/x $a_frame', 'p sizeof(struct trapframe)',
             'p/x *(struct trapframe *)$a_frame']
    gdb_check(cmds, '$a_frame == $sp', 'a0 points to current trapframe')
    gdb_check(cmds, '$a_old_2 - $sp == 288', 'exactly 288 bytes allocated')
    gdb_check(cmds, '(unsigned long long)$sp % 16 == 0', 'C ABI stack alignment')
    for i in range(32):
        gdb_check(cmds, f'*(unsigned long long *)($a_frame + {i * 8}) == $a_old_{i}', f'saved GPR slot {i}')
    for i, expr in ((32, '$a_entry_status'), (33, '$a_epc'), (35, '$a_cause')):
        gdb_check(cmds, f'*(unsigned long long *)($a_frame + {i * 8}) == {expr}', f'saved CSR slot {i}')
    cmds += ['hbreak *__trapret', 'continue', 'delete breakpoints',
             'echo === BEFORE RESTORE_ALL ===\\n', 'info registers pc sp sstatus sepc',
             'p/x ((struct trapframe *)$a_frame)->epc']
    return_pc = '(unsigned long long)&lab3_a_probe_after' if context else '$a_epc'
    gdb_check(cmds, f'((struct trapframe *)$a_frame)->epc == {return_pc}', 'handler return EPC')
    if context:
        cmds.append('echo Poison all 30 writable GPRs except SP before RESTORE_ALL.\\n')
        for i, reg in enumerate(REGS):
            if i not in (0, 2):
                cmds.append(f'set ${reg} = {0xDEAD0000 + i}')
    cmds += [f'hbreak *{return_pc}', 'continue', 'delete breakpoints',
             'echo === AFTER SRET ===\\n', 'info registers pc sp sstatus sepc']
    for i, reg in enumerate(REGS):
        gdb_check(cmds, f'${reg} == $a_old_{i}', f'restored GPR {reg}')
    # QEMU v4.1.1 target/riscv/op_helper.c:96 explicitly clears SPIE.
    version = subprocess.check_output([QEMU, '--version'], text=True).splitlines()[0]
    spie = 0 if 'version 4.1.1' in version else 0x20
    gdb_check(cmds, f'($sstatus & 0x20) == {spie}', 'version-specific SPIE after sret')
    gdb_check(cmds, '($sstatus & 0x100) == 0', 'sret clears SPP')
    if context:
        gdb_check(cmds, '($sstatus & 2) == ($a_before_status & 2)', 'restored prior SIE')
        cmds += ['hbreak *lab3_a_test_done', 'continue', 'delete breakpoints']
    else:
        gdb_check(cmds, '$sstatus & 2', 'timer return restores SIE=1')
    cmds += [f'echo GDB_{"CONTEXT" if context else "TIMER"}_ALL_PASS\\n', 'detach', 'quit']
    return '\n'.join(cmds) + '\n'


def debug_run(tree, context):
    name = 'context' if context else 'timer'
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    script = EVIDENCE / f'{name}.gdb'
    script.write_text(gdb_script(port, context))
    log_path = EVIDENCE / f'qemu-{name}.log'
    with log_path.open('w') as output:
        qemu = subprocess.Popen([QEMU, '-machine', 'virt', '-m', '128M', '-nographic',
                                 '-bios', 'default', '-device', 'loader,file=bin/ucore.img,addr=0x80200000',
                                 '-S', '-gdb', f'tcp:127.0.0.1:{port}'],
                                cwd=tree, env=ENV, stdin=subprocess.DEVNULL,
                                stdout=output, stderr=subprocess.STDOUT)
        try:
            time.sleep(0.5)
            result = run([GDB, '-q', '-nx', '-batch', 'bin/kernel', '-x', str(script)],
                         tree, f'gdb-{name}.log', timeout=40)
            must(result.returncode == 0, f'GDB {name} failed: {result.stdout[-1300:]}')
            must(f'GDB_{name.upper()}_ALL_PASS' in result.stdout, f'GDB {name} did not finish')
            qemu.wait(timeout=25)
            must(qemu.returncode == 0, f'QEMU {name} exit {qemu.returncode}')
        finally:
            if qemu.poll() is None:
                qemu.terminate()
                try:
                    qemu.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    qemu.kill()
                    qemu.wait()
    log = log_path.read_text()
    must('kernel panic' not in log, f'{name} kernel panic')
    if context:
        must('LAB3_A_ALL_TESTS_PASS' in log, 'isolated kernel tests did not finish')
    else:
        must(sum(line.strip() == '100 ticks' for line in log.splitlines()) == 10, 'expected exactly 10 timer reports')
        must('System shutdown after 10 prints.' in log, 'timer did not shut down')
    RESULTS[f'gdb_{name}'] = 'PASS'


def main():
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    # In an exported package, snapshot the current input before any build or
    # test injection. In the original workspace, retain the supplied baseline.
    if not (OUT / 'baseline').exists():
        shutil.copytree(SOURCE, OUT / 'baseline', ignore=shutil.ignore_patterns(
            'bin', 'obj', '.git', '.gdb.in', '.qemu.out', '*.log'))
    env = run([str(TOOLBIN / 'riscv64-unknown-elf-gcc'), '--version'], SOURCE, 'compiler.log')
    must(env.returncode == 0, 'compiler unavailable')
    run([QEMU, '--version'], SOURCE, 'qemu-version.log')
    clean = run(['make', 'clean'], SOURCE, 'clean.log')
    must(clean.returncode == 0, 'clean failed')
    build = run(['make', '-j4', f'QEMU={QEMU}'], SOURCE, 'build-default.log')
    must(build.returncode == 0, 'default build failed')
    RESULTS['default_build'] = 'PASS'
    debug_run(SOURCE, False)
    grade = run(['make', 'grade', f'QEMU={QEMU}'], SOURCE, 'make-grade.log', timeout=65)
    RESULTS['provided_make_grade'] = 'PASS' if grade.returncode == 0 and 'Total Score: 30/30' in grade.stdout else 'FAIL'
    print(grade.stdout[-1500:], flush=True)

    if STAGE.exists():
        must(STAGE.resolve() == (OUT / 'validation').resolve(), 'unsafe staging path')
        shutil.rmtree(STAGE)
    shutil.copytree(SOURCE, STAGE, ignore=shutil.ignore_patterns('bin', 'obj', '.git', '.gdb.in', '.qemu.out', '*.log'))
    for name in ('member_a_tests.c', 'member_a_context.S'):
        shutil.copy2(TESTS / name, STAGE / 'kern/debug' / name)
    init = STAGE / 'kern/init/init.c'
    text = init.read_text()
    text = text.replace('int kern_init(void) {',
                        'extern void lab3_a_allocator_tests(void);\nextern void lab3_a_after_pmm(void);\nint kern_init(void) {')
    needle = '    pmm_init();  // init physical memory management'
    must(text.count(needle) == 1, 'unexpected init.c; no injection performed')
    text = text.replace(needle, '    lab3_a_allocator_tests();\n' + needle + '\n    lab3_a_after_pmm();')
    init.write_text(text)
    build = run(['make', '-j4', f'QEMU={QEMU}'], STAGE, 'build-isolated.log')
    must(build.returncode == 0, 'isolated test build failed')
    RESULTS['isolated_build'] = 'PASS'
    debug_run(STAGE, True)
    for memory in ('64M', '256M'):
        result = run([QEMU, '-machine', 'virt', '-m', memory, '-nographic', '-bios', 'default',
                      '-device', 'loader,file=bin/ucore.img,addr=0x80200000'],
                     STAGE, f'memory-{memory}.log', timeout=25)
        must(result.returncode == 0 and 'LAB3_A_ALL_TESTS_PASS' in result.stdout and 'kernel panic' not in result.stdout,
             f'memory {memory} test failed')
        RESULTS[f'memory_{memory}'] = 'PASS'
    # Default tree and official tests must retain their original shared files.
    baseline = OUT / 'baseline'
    for relative in ['kern/trap/trap.c', 'kern/init/init.c', 'kern/driver/clock.c', 'tools/grade.sh']:
        must((SOURCE / relative).read_text() == (baseline / relative).read_text(), f'out-of-scope logic modification {relative}')
    for relative in ['kern/trap/trapentry.S', 'kern/mm/best_fit_pmm.c']:
        current = (SOURCE / relative).read_text()
        original = (baseline / relative).read_text()
        if relative.endswith('trapentry.S'):
            must(current.split('    .macro SAVE_ALL')[1].split('    .endm')[0] ==
                 original.split('    .macro SAVE_ALL')[1].split('    .endm')[0], 'SAVE_ALL was changed')
        else:
            a = current.split('static void\nbasic_check(void)')[1].split('//这个结构体')[0]
            b = original.split('static void\nbasic_check(void)')[1].split('//这个结构体')[0]
            must(a == b, 'provided allocator checks were changed')
    RESULTS['scope_and_framework_integrity'] = 'PASS'
    RESULTS['source_sha256'] = {str(p.relative_to(SOURCE)): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in SOURCE.rglob('*') if p.is_file() and p.suffix in ('.c', '.S', '.h')}
    (EVIDENCE / 'results.json').write_text(json.dumps(RESULTS, indent=2) + '\n')
    print(json.dumps({k: v for k, v in RESULTS.items() if k != 'source_sha256'}, indent=2), flush=True)
    if RESULTS['provided_make_grade'] != 'PASS':
        raise SystemExit('Provided make grade failed; see evidence/make-grade.log (other checks completed).')


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        RESULTS['error'] = str(exc)
        (EVIDENCE / 'results.json').write_text(json.dumps(RESULTS, indent=2) + '\n')
        raise
