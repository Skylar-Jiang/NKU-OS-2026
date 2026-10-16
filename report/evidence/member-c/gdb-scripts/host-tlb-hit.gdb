set pagination off
set confirm off
set auto-load safe-path /
file /home/mcz/riscv/qemu-4.1.1-lab2-debug-build/riscv64-softmmu/qemu-system-riscv64
set args -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000
break /home/mcz/riscv/qemu-4.1.1-lab2-debug-src/accel/tcg/cputlb.c:1030 if addr >= 0xffffffffc0200000 && addr < 0xffffffffc0201000
run
printf "\n[main TLB lookup #1 before tlb_hit] addr=%#lx addr_code=%#lx\n", addr, entry->addr_code
next
printf "[after condition #1] "
x/i $pc
continue
printf "\n[main TLB lookup #2 before tlb_hit] addr=%#lx addr_code=%#lx\n", addr, entry->addr_code
next
printf "[after condition #2] "
x/i $pc
bt 4
quit


