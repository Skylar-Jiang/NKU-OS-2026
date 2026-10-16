# Host GDB on the --enable-debug QEMU 4.1.1: single-step the instruction-fetch TLB miss
# for kern_init (VA 0xffffffffc02000d6), the Sv39 walk, the TLB fill, then the later TLB hit.
set pagination off
set confirm off
set width 0
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
set suppress-cli-notifications on
source trace.py
break get_page_addr_code if (addr & ~0xfffUL) == 0xffffffffc0200000UL && env->priv == 1
set $lookup = $bpnum
run
printf "\n===== [A] get_page_addr_code: first lookup of the kern_init page (expected TLB miss) =====\n"
bt 8
set scheduler-locking step
break riscv_cpu_tlb_fill
set $b1 = $bpnum
break get_physical_address
set $b2 = $bpnum
break tlb_set_page_with_attrs
set $b3 = $bpnum
trace-fn 800
delete $b1 $b2 $b3
set scheduler-locking off
continue
printf "\n===== [B] get_page_addr_code: next lookup of the same page (expected TLB hit) =====\n"
bt 6
set scheduler-locking step
trace-fn 200
set scheduler-locking off
delete
continue
