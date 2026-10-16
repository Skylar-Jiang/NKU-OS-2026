"""Run real guest GDB checks; terminate only the QEMU child created here."""
from pathlib import Path
import subprocess,time,sys
root=Path(__file__).resolve().parents[3]
code=root/'code';logs=root/'report/evidence/member-b/vscode-final';logs.mkdir(parents=True,exist_ok=True)
mode=sys.argv[1]
commands=['set pagination off','set confirm off','set print pretty off','file bin/kernel','target remote 127.0.0.1:1249']
if mode=='best-fit':
 commands += ['break best_fit_alloc_pages','continue','info args','bt 6','list best_fit_alloc_pages','disable 1','detach']
else:
 commands += ['break slub_alloc if size == 1025','continue','info args','finish','print/x $a0','disable 1','break slub_alloc if size == 8','continue','info args','bt 4','finish','print/x $a0','disable 2','break kern/mm/slub_test.c:135','continue','info locals','detach']
commands+=['quit']
cmdfile=logs/(mode+'.gdb');cmdfile.write_text('\n'.join(commands)+'\n')
qemu='/home/lenovo/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64'
with (logs/(mode+'-serial.log')).open('wb') as serial:
 p=subprocess.Popen([qemu,'-machine','virt','-nographic','-bios','default','-device','loader,file=bin/ucore.img,addr=0x80200000','-S','-gdb','tcp:127.0.0.1:1249'],cwd=code,stdin=subprocess.DEVNULL,stdout=serial,stderr=subprocess.STDOUT)
 try:
  time.sleep(.8)
  result=subprocess.run(['/home/lenovo/riscv-elf-toolchains/bin/riscv64-unknown-elf-gdb','-q','-batch','-x',str(cmdfile)],cwd=code,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
  output=result.stdout.decode(errors='replace');print(output,end='');print('GDB_EXIT='+str(result.returncode));(logs/(mode+'-gdb.log')).write_text(output+'\nGDB_EXIT='+str(result.returncode)+'\n')
 finally:
  p.terminate()
  try:p.wait(timeout=3)
  except subprocess.TimeoutExpired:p.kill();p.wait()
