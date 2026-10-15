from pathlib import Path
import os,signal,subprocess,sys,time,select
code=Path(__file__).resolve().parents[3]/'code'
p=subprocess.Popen(['make','qemu'],cwd=code,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
data=bytearray();markers=[b'memory management: best_fit_pmm_manager',b'check_alloc_page() succeeded!',b'slub_check() succeeded!']
try:
 deadline=time.monotonic()+30
 while time.monotonic()<deadline:
  if select.select([p.stdout],[],[],.2)[0]:
   chunk=os.read(p.stdout.fileno(),65536)
   if not chunk:break
   data.extend(chunk);sys.stdout.buffer.write(chunk);sys.stdout.buffer.flush()
   if all(m in data for m in markers):break
finally:
 try:os.killpg(p.pid,signal.SIGINT)
 except ProcessLookupError:pass
 try:tail,_=p.communicate(timeout=3)
 except subprocess.TimeoutExpired:
  os.killpg(p.pid,signal.SIGKILL);tail,_=p.communicate()
 data.extend(tail);sys.stdout.buffer.write(tail);sys.stdout.buffer.flush()
ok=all(m in data for m in markers) and b'kernel panic' not in data
print('\nQEMU_MARKERS='+('PASS' if ok else 'FAIL')+'; STOP=controlled SIGINT; MAKE_EXIT='+str(p.returncode))
sys.exit(0 if ok else 1)
