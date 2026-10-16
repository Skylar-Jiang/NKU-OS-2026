set pagination off
set confirm off
set auto-load safe-path /
file /home/mcz/riscv/qemu-4.1.1-lab2-debug-build/riscv64-softmmu/qemu-system-riscv64
set args -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000
break /home/mcz/riscv/qemu-4.1.1-lab2-debug-src/accel/tcg/cputlb.c:994 if cmp == page
run
printf "\n[REAL VICTIM TLB HIT] page=%#lx vidx=%lu index=%lu mmu_idx=%lu cmp=%#lx\n", page, vidx, index, mmu_idx, cmp
bt 4
next
printf "[after victim entry match/swap path] "
x/i $pc
quit


