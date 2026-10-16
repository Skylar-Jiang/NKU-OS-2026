set pagination off
set confirm off
set auto-load safe-path /
file /home/mcz/riscv/qemu-4.1.1-lab2-debug-build/riscv64-softmmu/qemu-system-riscv64
set args -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000
break /home/mcz/riscv/qemu-4.1.1-lab2-debug-src/target/riscv/cpu_helper.c:459 if mmu_idx == 3 && access_type == 0
run
printf "\n[BARE M-mode translation result] VA=%#lx PA=%#lx ret=%d mmu_idx=%d access_type=%d\n", address, pa, ret, mmu_idx, access_type
quit


