set pagination off
set confirm off
set width 0
set arch riscv:rv64
file bin/kernel
target remote 127.0.0.1:__PORT__
hbreak kern_init
continue
printf "\n[GUEST] kern_init: pc=0x%lx satp=0x%lx (MODE=%ld, root PPN=0x%lx)\n", $pc, $satp, (unsigned long)$satp >> 60, (unsigned long)$satp & 0xfffffffffff
x/2i $pc
x/4bx $pc
delete
detach
