#!/usr/bin/env python3
"""Run C's original tests on isolated copies of the integrated kernel."""
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1]
EVIDENCE = HERE / 'evidence'
WORK = Path(os.environ.get('LAB3_VERIFY_ROOT', str(SOURCE.parent.parent / 'lab3-verification'))) / 'member_c'
PREFIX = os.environ.get('GCCPREFIX', 'riscv64-unknown-elf-')
QEMU = os.environ.get('QEMU', 'qemu-system-riscv64')
GDB = os.environ.get('GDB', PREFIX + 'gdb')
RESULTS = {}

def must(ok, message):
    if not ok:
        raise AssertionError(message)

def run(args, cwd, name, timeout=90):
    result = subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=timeout)
    (EVIDENCE/name).write_text('$ ' + ' '.join(map(str,args)) + '\n' + result.stdout)
    must(result.returncode == 0, f'{name}: exit {result.returncode}')
    return result.stdout

def prepare(name, macro, function, filename):
    stage = Path(tempfile.mkdtemp(prefix=name+'-', dir=WORK))
    shutil.copytree(SOURCE, stage/'code', ignore=shutil.ignore_patterns(
        'bin','obj','.git','.vscode','.gdb.in','.qemu.out','*.log','__pycache__'))
    code = stage/'code'
    shutil.copy2(HERE/'tests'/filename, code/'kern/init'/filename)
    init = code/'kern/init/init.c'
    text = init.read_text()
    needle = '    pmm_init();  // init physical memory management'
    must(text.count(needle)==1, 'unexpected initialization entry; no injection')
    text = text.replace('int kern_init(void) {',
                        f'#ifdef {macro}\nvoid {function}(void);\n#endif\n\nint kern_init(void) {{',1)
    text = text.replace(needle, needle+f'\n\n#ifdef {macro}\n    {function}();\n#endif',1)
    init.write_text(text)
    run(['make','-j4',f'GCCPREFIX={PREFIX}',f'DEFS=-D{macro}'],code,name+'-build.log')
    symbols = run([PREFIX+'nm','bin/kernel'],code,name+'-symbols.log')
    must(re.search(r'\bT\s+'+function+r'\b', symbols), 'test macro did not enable entry')
    return code

def normal_run(code, name):
    output = run([QEMU,'-machine','virt','-nographic','-bios','default',
                  '-device','loader,file=bin/ucore.img,addr=0x80200000'],code,name+'.log',35)
    must('kernel panic' not in output and 'FAIL' not in output, name+' kernel failure')
    must(sum(x.strip()=='100 ticks' for x in output.splitlines())==10, name+' clock count')
    must('System shutdown after 10 prints.' in output, name+' normal shutdown')
    return output

def gdb_exceptions(code):
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    updates=[i+1 for i,line in enumerate((code/'kern/trap/trap.c').read_text().splitlines())
             if 'tf->epc += trap_instruction_length(tf->epc)' in line]
    must(len(updates)==2,'expected both exception EPC update statements')
    # Use only native GDB commands: this toolchain's GDB has no Python support.
    script = ('set pagination off\nset confirm off\nset remotetimeout 10\n'
              'set architecture riscv:rv64\ntarget remote :%d\n' % port)
    expected=[(3,4,0x00100073),(3,2,0x9002),(2,2,0x0000),(2,4,0x0200101b)]
    for number,(cause,length,encoding) in enumerate(expected,1):
        raw_type='unsigned int' if length==4 else 'unsigned short'
        script+=f'''hbreak *exception_handler
continue
delete breakpoints
set $c_frame = (struct trapframe *)$a0
set $c_before = $c_frame->epc
set $c_cause = $c_frame->cause
set $c_encoding = *({raw_type} *)$c_before
printf "CASE{number} ENTRY scause=0x%lx cause=0x%lx sepc=0x%lx original_epc=0x%lx encoding=0x%lx\\n", $scause, $c_cause, $sepc, $c_before, $c_encoding
if $c_cause != {cause} || $scause != {cause} || (unsigned long)$sepc != $c_before || $c_encoding != {encoding}
  echo FAIL: unexpected exception or instruction encoding\\n
  quit 1
end
hbreak kern/trap/trap.c:{updates[0]}
hbreak kern/trap/trap.c:{updates[1]}
continue
delete breakpoints
if $c_frame->epc != $c_before
  echo FAIL: EPC modified before recorded update stop\\n
  quit 1
end
set $c_steps = 0
while $c_frame->epc == $c_before && $c_steps < 128
  stepi
  set $c_steps = $c_steps + 1
end
set $c_after = $c_frame->epc
set $c_delta = $c_after - $c_before
printf "CASE{number} original_epc=0x%lx modified_epc=0x%lx delta=0x%lx steps=%d\\n", $c_before, $c_after, $c_delta, $c_steps
if $c_delta != {length}
  echo FAIL: incorrect EPC change\\n
  quit 1
end
echo RESULT: PASS CASE{number}\\n
'''
    script+='echo GDB_EXCEPTION_ALL_PASS\\n\ndetach\nquit\n'
    path=EVIDENCE/'exceptions.gdb';path.write_text(script)
    serial=EVIDENCE/'exceptions-qemu.log'
    with serial.open('w') as stream:
        qemu=subprocess.Popen([QEMU,'-machine','virt','-nographic','-bios','default',
            '-device','loader,file=bin/ucore.img,addr=0x80200000','-S','-gdb',f'tcp::{port}'],
            cwd=code,stdout=stream,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL)
        try:
            time.sleep(0.4)
            output=run([GDB,'-q','-nx','-batch','bin/kernel','-x',str(path)],code,'exceptions-gdb.log',80)
            must('GDB_EXCEPTION_ALL_PASS' in output,'GDB exception checks incomplete')
            qemu.wait(timeout=35)
            must(qemu.returncode==0,'exception QEMU did not shut down normally')
        finally:
            if qemu.poll() is None:
                qemu.terminate()
                try:qemu.wait(timeout=3)
                except subprocess.TimeoutExpired:qemu.kill();qemu.wait()
    output=serial.read_text()
    must('kernel panic' not in output,'exception test kernel panic')
    must(output.count('CASE1 PASS')==1 and output.count('CASE2 PASS')==1 and
         output.count('CASE3 PASS')==1 and output.count('CASE4 PASS')==1,'not all four exceptions returned')
    must(sum(x.strip()=='100 ticks' for x in output.splitlines())==10,'post-exception clock count')
    for number in range(1,5):
        RESULTS[f'exception_{number}']='PASS'

def main():
    EVIDENCE.mkdir(parents=True,exist_ok=True);WORK.mkdir(parents=True,exist_ok=True)
    code=prepare('exceptions','LAB3_EXCEPTION_TEST','lab3_exception_test','lab3_exception_test.c')
    gdb_exceptions(code)
    code=prepare('fallback','LAB3_EXCEPTION_FALLBACK_TEST','lab3_exception_fallback_test','lab3_exception_fallback_test.c')
    output=normal_run(code,'fallback-qemu')
    must(output.count('trapframe at ')==2,'fallback did not print both trapframes')
    must('fetch-fault PASS epc unchanged' in output and 'load-fault PASS epc unchanged' in output,'fallback EPC changed')
    RESULTS['fallback_fetch_load']='PASS'
    code=prepare('sequence','LAB3_TRAP_SEQUENCE_TEST','lab3_trap_sequence_test','lab3_trap_sequence_test.c')
    output=normal_run(code,'sequence-qemu')
    addresses=[int(x,16) for x in re.findall(r'Bad instruction address: 0x([0-9a-f]+)',output)]
    must(len(addresses)==3 and addresses[1]-addresses[0]==4 and addresses[2]-addresses[1]==2,'sequence instruction layout')
    for marker in ['CASE1 PASS','CASE2 PASS','CASE3 PASS']:
        must(marker in output,'missing '+marker)
    must('t0=0x0000000013579bdf t1=0x0000000002468ace a0=0x0000000011223344' in output,'register mismatch')
    RESULTS['consecutive_exceptions']='PASS';RESULTS['register_restore']='PASS'
    print('MEMBER_C_ALL_PASS')
    print(json.dumps(RESULTS,indent=2))
    (EVIDENCE/'results.json').write_text(json.dumps(RESULTS,indent=2)+'\n')

if __name__=='__main__':
    main()
