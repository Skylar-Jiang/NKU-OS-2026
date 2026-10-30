#include <riscv.h>
    .section .text
    .align 2
    .globl lab3_a_context_probe
    .type lab3_a_context_probe, @function
lab3_a_context_probe:
    addi sp, sp, -416
    STORE a0, 256(sp)
    STORE sp, 264(sp)
    STORE ra, 272(sp)
    STORE gp, 280(sp)
    STORE tp, 288(sp)
    STORE s0, 296(sp)
    STORE s1, 304(sp)
    STORE s2, 312(sp)
    STORE s3, 320(sp)
    STORE s4, 328(sp)
    STORE s5, 336(sp)
    STORE s6, 344(sp)
    STORE s7, 352(sp)
    STORE s8, 360(sp)
    STORE s9, 368(sp)
    STORE s10, 376(sp)
    STORE s11, 384(sp)
    csrr t0, sstatus
    STORE t0, 400(sp)
    .irp reg,1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    li x\reg, 0x100 + \reg
    .endr
    .globl lab3_a_probe_trap
lab3_a_probe_trap:
    # Explicit 32-bit ebreak: no dependence on C's 16-bit Challenge 3 work.
    .4byte 0x00100073
    .globl lab3_a_probe_after
lab3_a_probe_after:
    .irp reg,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    STORE x\reg, \reg * REGBYTES(sp)
    .endr
    LOAD a0, 256(sp)
    mv t1, sp
    li t2, 32
1:
    LOAD t0, 0(t1)
    STORE t0, 0(a0)
    addi t1, t1, REGBYTES
    addi a0, a0, REGBYTES
    addi t2, t2, -1
    bnez t2, 1b
    LOAD t0, 264(sp)
    STORE t0, 0(a0)
    csrr t0, sstatus
    STORE t0, 8(a0)
    LOAD t0, 400(sp)
    STORE t0, 16(a0)
    LOAD ra, 272(sp)
    LOAD gp, 280(sp)
    LOAD tp, 288(sp)
    LOAD s0, 296(sp)
    LOAD s1, 304(sp)
    LOAD s2, 312(sp)
    LOAD s3, 320(sp)
    LOAD s4, 328(sp)
    LOAD s5, 336(sp)
    LOAD s6, 344(sp)
    LOAD s7, 352(sp)
    LOAD s8, 360(sp)
    LOAD s9, 368(sp)
    LOAD s10, 376(sp)
    LOAD s11, 384(sp)
    addi sp, sp, 416
    ret
    .size lab3_a_context_probe, .-lab3_a_context_probe
