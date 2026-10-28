# Member B: isolated dispatch test (simulated, not real device interrupts).
# QEMU virt only raises the S-mode timer in this lab, so the other
# branches are reached by GDB inferior calls of trap() with a synthetic
# trapframe. sstatus.SIE is still 0 at intr_enable, so no real interrupt
# interleaves. Output goes to the serial log checked by run_checks.sh.
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

hbreak intr_enable
continue
delete breakpoints
set $ok = (($sstatus & 0x2) == 0)
b_ok "sstatus.SIE = 0 during the isolated calls"

# keep the fake frame at the bottom of the 8 KB boot stack, far below the
# frames GDB's inferior calls push under the current sp
set $tf = (struct trapframe *)&bootstack
set $ok = ((unsigned long)$sp - (unsigned long)$tf > 4096)
b_ok "fake trapframe is >4 KB below sp"
set $ticks0 = ticks

define b_dispatch
  call (void)memset($tf, 0, sizeof(struct trapframe))
  set $tf->epc = 0x1234
  set $tf->cause = $arg0
  printf "call trap(tf) with tf->cause = 0x%lx\n", $tf->cause
  call (void)trap($tf)
  set $ok = ($tf->epc == 0x1234)
  b_ok "tf->epc unchanged"
end

# interrupt bit set: codes 0-4 and 6-11 print a label, 12 and 16 are unknown
b_dispatch 0x8000000000000000
b_dispatch 0x8000000000000001
b_dispatch 0x8000000000000002
b_dispatch 0x8000000000000003
b_dispatch 0x8000000000000004
b_dispatch 0x8000000000000006
b_dispatch 0x8000000000000007
b_dispatch 0x8000000000000008
b_dispatch 0x8000000000000009
b_dispatch 0x800000000000000a
b_dispatch 0x800000000000000b
b_dispatch 0x800000000000000c
b_dispatch 0x8000000000000010
# interrupt bit clear: code 1 is an instruction access fault and must go to
# exception_handler (member C), which prints nothing for this cause
b_dispatch 0x0000000000000001

set $ok = (ticks == $ticks0)
b_ok "no isolated call touched ticks"
echo GDB_DISPATCH_ALL_PASS\n
kill
quit
