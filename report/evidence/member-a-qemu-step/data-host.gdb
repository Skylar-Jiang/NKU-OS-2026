# Host GDB on the --enable-debug --enable-tcg-interpreter QEMU 4.1.1. With TCI every guest
# load/store calls the C helpers, so the data-TLB lookup in load_helper/store_helper is
# executed (and can be single-stepped) on every access, for both misses and hits.
set pagination off
set confirm off
set width 0
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
set suppress-cli-notifications on
source trace.py
# [C] kern_init: sd ra,8(sp) -> first access to the stack page (expected miss)
break store_helper if addr == 0xffffffffc0204ff8UL
set $s1 = $bpnum
run
printf "\n===== [C] store_helper: sd ra,8(sp) VA 0xffffffffc0204ff8, first access to the stack page (expected TLB miss) =====\n"
bt 4
delete $s1
set scheduler-locking step
break get_physical_address
set $g = $bpnum
trace-fn 400
delete $g
set scheduler-locking off
# [D] dtb_init: sd ra,40(sp) -> same stack page, already in the TLB (expected hit)
break store_helper if addr == 0xffffffffc0204fe8UL
set $s2 = $bpnum
continue
printf "\n===== [D] store_helper: sd ra,40(sp) VA 0xffffffffc0204fe8, same stack page (expected TLB hit) =====\n"
bt 4
delete $s2
set scheduler-locking step
trace-fn 200
set scheduler-locking off
# [E] dtb_init: ld a1,boot_hartid -> page 0xffffffffc0206000 already filled by memset's store (expected hit)
break load_helper if addr == 0xffffffffc0206000UL
set $l1 = $bpnum
continue
printf "\n===== [E] load_helper: ld a1 <- boot_hartid VA 0xffffffffc0206000, page filled earlier by a store (expected TLB hit) =====\n"
bt 4
delete $l1
set scheduler-locking step
trace-fn 200
set scheduler-locking off
continue
