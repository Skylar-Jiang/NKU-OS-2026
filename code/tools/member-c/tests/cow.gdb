set pagination off
set confirm off
target remote 127.0.0.1:12445
break trap
continue
set $frame=(struct trapframe *)$a0
set $saved_epc=$frame->epc
printf "Real store-page-fault entry\n"
p $frame->cause
p/x $frame->tval
p/x $saved_epc
x/i $saved_epc
set $l1=(unsigned long long *)(((fault_mm->pgdir[0] >> 10) << 12)+va_pa_offset)
set $l0=(unsigned long long *)((($l1[4] >> 10) << 12)+va_pa_offset)
set $oldpa=($l0[0] >> 10) << 12
printf "Before repair: COW bit and W bit\n"
p/x $l0[0] & 0x104
tbreak __trapret
continue
printf "After repair: unchanged EPC, different physical page, writable non-COW\n"
p $frame->epc == $saved_epc
p (($l0[0] >> 10) << 12) != $oldpa
p/x $l0[0] & 0x104
detach
quit
