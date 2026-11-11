.section .text.entry
.globl _start
_start:
    la sp, test_stack_top
    call test_boot
1:  j 1b
.section .bss
.balign 16
test_stack: .space 16384
test_stack_top:
