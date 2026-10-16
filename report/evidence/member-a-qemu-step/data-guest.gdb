set pagination off
set confirm off
set width 0
set arch riscv:rv64
file bin/kernel
target remote 127.0.0.1:__PORT__
hbreak *0xffffffffc02000ec
continue
printf "\n[GUEST C] pc=0x%lx sp=0x%lx store VA=0x%lx\n", $pc, $sp, $sp + 8
x/1i $pc
delete
stepi
printf "[GUEST C] after stepi: *(VA 0x%lx) = 0x%lx, ra = 0x%lx\n", $sp + 8, *(unsigned long *)($sp + 8), $ra
hbreak *0xffffffffc020023a
continue
printf "\n[GUEST D] pc=0x%lx sp=0x%lx store VA=0x%lx\n", $pc, $sp, $sp + 40
x/1i $pc
delete
stepi
printf "[GUEST D] after stepi: *(VA 0x%lx) = 0x%lx, ra = 0x%lx\n", $sp + 40, *(unsigned long *)($sp + 40), $ra
hbreak *0xffffffffc0200246
continue
printf "\n[GUEST E] pc=0x%lx a1=0x%lx load VA=0x%lx\n", $pc, $a1, $a1 - 578
x/1i $pc
delete
stepi
printf "[GUEST E] after stepi: a1 = 0x%lx (boot_hartid = %ld)\n", $a1, boot_hartid
detach
