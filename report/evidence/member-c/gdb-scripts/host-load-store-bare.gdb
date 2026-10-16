set pagination off
set confirm off
set auto-load safe-path /
file /home/mcz/riscv/qemu-4.1.1-lab2-debug-build/riscv64-softmmu/qemu-system-riscv64
set args -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000
break riscv_cpu_tlb_fill if access_type == 0 && mmu_idx == 3
commands 1
silent
printf "\n[BARE M-mode LOAD] va=%#lx mmu_idx=%d access_type=%d\n", address, mmu_idx, access_type
bt 3
disable 1
continue
end
break riscv_cpu_tlb_fill if access_type == 1 && mmu_idx == 3
commands 2
silent
printf "\n[BARE M-mode STORE] va=%#lx mmu_idx=%d access_type=%d\n", address, mmu_idx, access_type
bt 3
disable 2
continue
end
break riscv_cpu_tlb_fill if access_type == 0 && mmu_idx == 1
commands 3
silent
printf "\n[SV39 S-mode LOAD] va=%#lx mmu_idx=%d access_type=%d\n", address, mmu_idx, access_type
bt 3
disable 3
quit
end
break riscv_cpu_tlb_fill if access_type == 1 && mmu_idx == 1
commands 4
silent
printf "\n[SV39 S-mode STORE] va=%#lx mmu_idx=%d access_type=%d\n", address, mmu_idx, access_type
bt 3
disable 4
continue
end
run





