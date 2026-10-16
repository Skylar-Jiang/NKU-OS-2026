# Guest GDB: riscv64-unknown-elf-gdb talks to QEMU's gdbstub and debugs uCore itself.
set pagination off
set confirm off
set width 0
set arch riscv:rv64
file bin/kernel
target remote 127.0.0.1:__PORT__
hbreak *0x80200000
continue
printf "\n[GUEST] kern_entry before paging: pc=0x%lx satp=0x%lx (MODE=%ld)\n", $pc, $satp, (unsigned long)$satp >> 60
x/4i $pc
delete
hbreak kern_init
continue
printf "\n[GUEST] kern_init after paging: pc=0x%lx satp=0x%lx (MODE=%ld) root PPN=0x%lx\n", $pc, $satp, (unsigned long)$satp >> 60, (unsigned long)$satp & 0xfffffffffff
x/2i $pc
printf "[GUEST] instruction bytes at VA pc:\n"
x/4bx $pc
delete
hbreak *0xffffffffc02000ec
continue
printf "\n[GUEST] store instruction: pc=0x%lx sp=0x%lx -> store VA=0x%lx\n", $pc, $sp, $sp + 8
x/1i $pc
stepi
printf "[GUEST] after stepi, value at VA 0x%lx = 0x%lx (ra=0x%lx)\n", $sp + 8, *(unsigned long *)($sp + 8), $ra
delete
detach
