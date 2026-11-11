set pagination off
set confirm off
set arch riscv:rv64
target remote localhost:12507
break proc_run
break forkret
break kernel_thread_entry
break init_main
continue
echo \n== proc_run(next): idle -> init\n
printf "current pid=%d name=%s  next pid=%d name=%s\n", current->pid, current->name, proc->pid, proc->name
printf "next->pgdir=0x%lx next->kstack=0x%lx next->tf=%p\n", proc->pgdir, proc->kstack, proc->tf
printf "next->context.ra=0x%lx (forkret=%p)  next->context.sp=0x%lx\n", proc->context.ra, forkret, proc->context.sp
continue
echo \n== forkret (after switch_to)\n
printf "current pid=%d  sp=0x%lx  ra=0x%lx  satp=0x%lx\n", current->pid, (long)$sp, (long)$ra, $satp
printf "tf->epc=0x%lx tf->status=0x%lx tf->gpr.sp=0x%lx tf->gpr.s0=0x%lx tf->gpr.s1=0x%lx\n", current->tf->epc, current->tf->status, current->tf->gpr.sp, current->tf->gpr.s0, current->tf->gpr.s1
continue
echo \n== kernel_thread_entry (after __trapret/sret)\n
printf "pc=0x%lx sp=0x%lx sp%%16=%ld s0=0x%lx (init_main=%p) s1=%s\n", $pc, (long)$sp, (long)$sp % 16, $s0, init_main, (char *)$s1
printf "sstatus=0x%lx SIE=%ld SPIE=%ld SPP=%ld\n", $sstatus, ($sstatus >> 1) & 1, ($sstatus >> 5) & 1, ($sstatus >> 8) & 1
continue
echo \n== init_main\n
printf "current pid=%d name=%s arg=%s sp=0x%lx kstack=[0x%lx,0x%lx)\n", current->pid, current->name, (char *)arg, (long)$sp, current->kstack, current->kstack + 8192
kill
quit
