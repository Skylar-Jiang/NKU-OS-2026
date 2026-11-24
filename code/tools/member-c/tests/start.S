.section .text.entry
.globl _start
_start:
    la sp, boot_test_stack_top
    la t0, machine_fail
    csrw mtvec,t0
    call test_boot
1:  j 1b
.align 2
machine_fail:
    li t0,0x10000000
    li t1,70
    sb t1,0(t0)
    li t1,10
    sb t1,0(t0)
    li t0,0x100000
    li t1,0x3333
    sw t1,0(t0)
    j 1b
.section .bss
.balign 16
boot_test_stack: .space 16384
boot_test_stack_top:
