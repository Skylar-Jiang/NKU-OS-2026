# Member B: real timer interrupts on QEMU 4.1.1.
# Checks init CSRs, scause decoding, the trap path, epc and ticks.
# Run through tools/member_b/run_checks.sh (it substitutes @PORT@).
set pagination off
set confirm off
set architecture riscv:rv64
target remote :@PORT@

# usage: set $ok = (expr) then b_ok "label"
define b_ok
  if $ok
    echo PASS: $arg0\n
  else
    echo FAIL: $arg0\n
    quit 1
  end
end

hbreak kern_init
continue
delete breakpoints
echo \n=== 1. kern_init entry (before idt_init) ===\n
p/x $stvec
p/x $sie
p/x $sstatus

hbreak pmm_init
continue
delete breakpoints
echo \n=== 2. after idt_init (stopped at pmm_init) ===\n
p/x $stvec
info symbol $stvec
set $ok = ($stvec == (unsigned long)&__alltraps)
b_ok "stvec = &__alltraps"
set $ok = (($stvec & 3) == 0)
b_ok "stvec MODE = Direct (low 2 bits 0)"
set $ok = ($sscratch == 0)
b_ok "sscratch = 0 (trap from kernel)"

hbreak clock_init
continue
delete breakpoints
echo \n=== 3. before clock_init ===\n
p/x $sie
p/x $sstatus
set $ok = (($sie & 0x20) == 0)
b_ok "sie.STIE = 0 before clock_init"
set $ok = (($sstatus & 0x2) == 0)
b_ok "sstatus.SIE = 0 before intr_enable"

hbreak intr_enable
continue
delete breakpoints
echo \n=== 4. after clock_init, before intr_enable ===\n
p/x $sie
p/x $sstatus
p ticks
set $ok = (($sie & 0x20) != 0)
b_ok "clock_init set sie.STIE (bit 5)"
set $ok = (($sstatus & 0x2) == 0)
b_ok "sstatus.SIE still 0 before intr_enable"
set $ok = (ticks == 0)
b_ok "ticks = 0 after clock_init"
set $b_status_before = $sstatus
finish
echo \n=== 5. after intr_enable returns ===\n
p/x $sstatus
set $ok = (($sstatus & 0x2) != 0)
b_ok "intr_enable set sstatus.SIE"
set $ok = (($sstatus & ~0x2) == ($b_status_before & ~0x2))
b_ok "intr_enable changed no other sstatus bit"

echo \n=== 6. first timer interrupt: hardware entry ===\n
hbreak __alltraps
continue
delete breakpoints
info registers pc sstatus sepc scause
set $b_epc = $sepc
info symbol $sepc
set $ok = (((unsigned long)$scause >> 63) == 1)
b_ok "scause bit 63 = 1 (interrupt)"
set $ok = (((unsigned long)$scause & 0x7fffffffffffffff) == 5)
b_ok "scause code = 5 (IRQ_S_TIMER)"
set $ok = (($sstatus & 0x2) == 0)
b_ok "hardware cleared sstatus.SIE on trap entry"
set $ok = (($sstatus & 0x20) != 0)
b_ok "sstatus.SPIE = 1 (SIE was 1 before trap)"
set $ok = (($sstatus & 0x100) != 0)
b_ok "sstatus.SPP = 1 (trap came from S-mode)"
set $ok = ($sepc == (unsigned long)kern_init + 0x44)
b_ok "sepc = idle loop in kern_init"

hbreak trap
continue
delete breakpoints
echo \n=== 7. trap(tf) ===\n
set $b_tf = (struct trapframe *)$a0
p/x $b_tf->cause
p/x $b_tf->epc
set $ok = ($b_tf->cause == (unsigned long)$scause)
b_ok "tf->cause = scause"
set $ok = ($b_tf->epc == $b_epc)
b_ok "tf->epc = sepc"

hbreak interrupt_handler
continue
delete breakpoints
echo \n=== 8. interrupt_handler(tf) ===\n
bt 2
set $ok = ((struct trapframe *)$a0 == $b_tf)
b_ok "same trapframe passed to interrupt_handler"
set $ok = (ticks == 0)
b_ok "ticks = 0 before first handler"

hbreak __trapret
continue
delete breakpoints
echo \n=== 9. __trapret (handler returned) ===\n
p ticks
p/x $b_tf->epc
set $ok = (ticks == 1)
b_ok "ticks incremented exactly once"
set $ok = ($b_tf->epc == $b_epc)
b_ok "timer handler did not modify tf->epc"

hbreak *$b_epc
continue
delete breakpoints
echo \n=== 10. after sret ===\n
info registers pc sstatus
set $ok = ($pc == $b_epc)
b_ok "sret resumed at the interrupted instruction"
set $ok = (($sstatus & 0x2) != 0)
b_ok "sret restored sstatus.SIE = 1"

echo \n=== 11. consecutive interrupts ===\n
set $b_i = 2
while $b_i <= 6
  hbreak __trapret
  continue
  delete breakpoints
  printf "interrupt #%d: ticks=%d scause=0x%lx sepc=0x%lx\n", $b_i, ticks, $scause, $sepc
  set $ok = (ticks == $b_i)
  b_ok "ticks follows interrupt count"
  set $ok = ((unsigned long)$scause == 0x8000000000000005)
  b_ok "still a supervisor timer interrupt"
  set $b_i = $b_i + 1
end

echo \n=== 12. 100th interrupt prints "100 ticks" ===\n
hbreak print_ticks
continue
delete breakpoints
p ticks
bt 3
set $ok = (ticks == 100)
b_ok "print_ticks reached at ticks = 100"

echo GDB_TIMER_FLOW_ALL_PASS\n
kill
quit
