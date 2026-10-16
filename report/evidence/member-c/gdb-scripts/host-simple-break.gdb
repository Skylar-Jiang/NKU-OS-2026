set pagination off
set confirm off
set auto-load safe-path /
file /home/mcz/riscv/qemu-4.1.1-lab2-debug-build/riscv64-softmmu/qemu-system-riscv64
set args -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000
break riscv_cpu_tlb_fill
run
bt 3
quit
