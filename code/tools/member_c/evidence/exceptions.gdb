set pagination off
set confirm off
set remotetimeout 10
set architecture riscv:rv64
target remote :45887
hbreak *exception_handler
continue
delete breakpoints
set $c_frame = (struct trapframe *)$a0
set $c_before = $c_frame->epc
set $c_cause = $c_frame->cause
set $c_encoding = *(unsigned int *)$c_before
printf "CASE1 ENTRY scause=0x%lx cause=0x%lx sepc=0x%lx original_epc=0x%lx encoding=0x%lx\n", $scause, $c_cause, $sepc, $c_before, $c_encoding
if $c_cause != 3 || $scause != 3 || (unsigned long)$sepc != $c_before || $c_encoding != 1048691
  echo FAIL: unexpected exception or instruction encoding\n
  quit 1
end
hbreak kern/trap/trap.c:155
hbreak kern/trap/trap.c:166
continue
delete breakpoints
if $c_frame->epc != $c_before
  echo FAIL: EPC modified before recorded update stop\n
  quit 1
end
set $c_steps = 0
while $c_frame->epc == $c_before && $c_steps < 128
  stepi
  set $c_steps = $c_steps + 1
end
set $c_after = $c_frame->epc
set $c_delta = $c_after - $c_before
printf "CASE1 original_epc=0x%lx modified_epc=0x%lx delta=0x%lx steps=%d\n", $c_before, $c_after, $c_delta, $c_steps
if $c_delta != 4
  echo FAIL: incorrect EPC change\n
  quit 1
end
echo RESULT: PASS CASE1\n
hbreak *exception_handler
continue
delete breakpoints
set $c_frame = (struct trapframe *)$a0
set $c_before = $c_frame->epc
set $c_cause = $c_frame->cause
set $c_encoding = *(unsigned short *)$c_before
printf "CASE2 ENTRY scause=0x%lx cause=0x%lx sepc=0x%lx original_epc=0x%lx encoding=0x%lx\n", $scause, $c_cause, $sepc, $c_before, $c_encoding
if $c_cause != 3 || $scause != 3 || (unsigned long)$sepc != $c_before || $c_encoding != 36866
  echo FAIL: unexpected exception or instruction encoding\n
  quit 1
end
hbreak kern/trap/trap.c:155
hbreak kern/trap/trap.c:166
continue
delete breakpoints
if $c_frame->epc != $c_before
  echo FAIL: EPC modified before recorded update stop\n
  quit 1
end
set $c_steps = 0
while $c_frame->epc == $c_before && $c_steps < 128
  stepi
  set $c_steps = $c_steps + 1
end
set $c_after = $c_frame->epc
set $c_delta = $c_after - $c_before
printf "CASE2 original_epc=0x%lx modified_epc=0x%lx delta=0x%lx steps=%d\n", $c_before, $c_after, $c_delta, $c_steps
if $c_delta != 2
  echo FAIL: incorrect EPC change\n
  quit 1
end
echo RESULT: PASS CASE2\n
hbreak *exception_handler
continue
delete breakpoints
set $c_frame = (struct trapframe *)$a0
set $c_before = $c_frame->epc
set $c_cause = $c_frame->cause
set $c_encoding = *(unsigned short *)$c_before
printf "CASE3 ENTRY scause=0x%lx cause=0x%lx sepc=0x%lx original_epc=0x%lx encoding=0x%lx\n", $scause, $c_cause, $sepc, $c_before, $c_encoding
if $c_cause != 2 || $scause != 2 || (unsigned long)$sepc != $c_before || $c_encoding != 0
  echo FAIL: unexpected exception or instruction encoding\n
  quit 1
end
hbreak kern/trap/trap.c:155
hbreak kern/trap/trap.c:166
continue
delete breakpoints
if $c_frame->epc != $c_before
  echo FAIL: EPC modified before recorded update stop\n
  quit 1
end
set $c_steps = 0
while $c_frame->epc == $c_before && $c_steps < 128
  stepi
  set $c_steps = $c_steps + 1
end
set $c_after = $c_frame->epc
set $c_delta = $c_after - $c_before
printf "CASE3 original_epc=0x%lx modified_epc=0x%lx delta=0x%lx steps=%d\n", $c_before, $c_after, $c_delta, $c_steps
if $c_delta != 2
  echo FAIL: incorrect EPC change\n
  quit 1
end
echo RESULT: PASS CASE3\n
hbreak *exception_handler
continue
delete breakpoints
set $c_frame = (struct trapframe *)$a0
set $c_before = $c_frame->epc
set $c_cause = $c_frame->cause
set $c_encoding = *(unsigned int *)$c_before
printf "CASE4 ENTRY scause=0x%lx cause=0x%lx sepc=0x%lx original_epc=0x%lx encoding=0x%lx\n", $scause, $c_cause, $sepc, $c_before, $c_encoding
if $c_cause != 2 || $scause != 2 || (unsigned long)$sepc != $c_before || $c_encoding != 33558555
  echo FAIL: unexpected exception or instruction encoding\n
  quit 1
end
hbreak kern/trap/trap.c:155
hbreak kern/trap/trap.c:166
continue
delete breakpoints
if $c_frame->epc != $c_before
  echo FAIL: EPC modified before recorded update stop\n
  quit 1
end
set $c_steps = 0
while $c_frame->epc == $c_before && $c_steps < 128
  stepi
  set $c_steps = $c_steps + 1
end
set $c_after = $c_frame->epc
set $c_delta = $c_after - $c_before
printf "CASE4 original_epc=0x%lx modified_epc=0x%lx delta=0x%lx steps=%d\n", $c_before, $c_after, $c_delta, $c_steps
if $c_delta != 4
  echo FAIL: incorrect EPC change\n
  quit 1
end
echo RESULT: PASS CASE4\n
echo GDB_EXCEPTION_ALL_PASS\n
detach
quit
