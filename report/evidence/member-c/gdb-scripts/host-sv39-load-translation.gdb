set pagination off
set confirm off
set auto-load safe-path /
file /home/mcz/riscv/qemu-4.1.1-lab2-debug-build/riscv64-softmmu/qemu-system-riscv64
set args -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000
break /home/mcz/riscv/qemu-4.1.1-lab2-debug-src/target/riscv/cpu_helper.c:252 if addr == 0xffffffffc0202998 && access_type == 0
run
printf "\n[SV39 page walk for kernel load] addr=%#lx access=%d mode=%d satp=%#lx base=%#lx levels=%d\n", addr, access_type, mmu_idx, env->satp, base, levels
printf "[root PTE lookup] i=%d ptshift=%d idx=%#lx pte_addr=%#lx\n", i, ptshift, idx, pte_addr
next
next
printf "[PTE value] pte=%#lx ppn=%#lx\n", pte, ppn
break /home/mcz/riscv/qemu-4.1.1-lab2-debug-src/target/riscv/cpu_helper.c:334 if addr == 0xffffffffc0202998 && access_type == 0
continue
next
next
printf "[translated PA page] physical=%#lx\n", *physical
printf "[VA offset] %#lx; final PA=%#lx\n", addr & 0xfff, *physical + (addr & 0xfff)
quit


