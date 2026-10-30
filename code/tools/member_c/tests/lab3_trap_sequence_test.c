#include <defs.h>
#include <stdio.h>

#ifdef LAB3_TRAP_SEQUENCE_TEST

#define SEQUENCE_T0_VALUE ((uintptr_t)0x13579bdfULL)
#define SEQUENCE_T1_VALUE ((uintptr_t)0x02468aceULL)
#define SEQUENCE_A0_VALUE ((uintptr_t)0x11223344ULL)

static volatile uintptr_t sequence_t0_after;
static volatile uintptr_t sequence_t1_after;
static volatile uintptr_t sequence_a0_after;

/*
 * Save every register this helper changes, execute three adjacent trap
 * instructions, then capture the values restored by RESTORE_ALL before
 * returning to C. The stack adjustment preserves the RISC-V 16-byte ABI
 * alignment and no function call is inserted between the three exceptions.
 */
static void __attribute__((naked, noinline))
trap_sequence_asm(void) {
    asm volatile(
        "addi sp, sp, -32\n"
        "sd t0, 0(sp)\n"
        "sd t1, 8(sp)\n"
        "sd t2, 16(sp)\n"
        "sd a0, 24(sp)\n"
        "li t0, 0x13579bdf\n"
        "li t1, 0x02468ace\n"
        "li a0, 0x11223344\n"
        ".option push\n"
        ".option norvc\n"
        "ebreak\n"
        ".option rvc\n"
        "c.ebreak\n"
        ".option norvc\n"
        "ebreak\n"
        ".option pop\n"
        "la t2, sequence_t0_after\n"
        "sd t0, 0(t2)\n"
        "la t2, sequence_t1_after\n"
        "sd t1, 0(t2)\n"
        "la t2, sequence_a0_after\n"
        "sd a0, 0(t2)\n"
        "ld a0, 24(sp)\n"
        "ld t2, 16(sp)\n"
        "ld t1, 8(sp)\n"
        "ld t0, 0(sp)\n"
        "addi sp, sp, 32\n"
        "ret\n");
}

static bool
sequence_registers_restored(void) {
    return sequence_t0_after == SEQUENCE_T0_VALUE &&
           sequence_t1_after == SEQUENCE_T1_VALUE &&
           sequence_a0_after == SEQUENCE_A0_VALUE;
}

void
lab3_trap_sequence_test(void) {
    bool registers_ok;

    sequence_t0_after = 0;
    sequence_t1_after = 0;
    sequence_a0_after = 0;

    cprintf("[LAB3-TRAP-SEQUENCE-TEST] CASE1 begin: ebreak, c.ebreak, ebreak\n");
    trap_sequence_asm();
    cprintf("[LAB3-TRAP-SEQUENCE-TEST] CASE1 PASS three exceptions returned\n");

    registers_ok = sequence_registers_restored();
    cprintf("[LAB3-TRAP-SEQUENCE-TEST] registers t0=0x%016llx t1=0x%016llx a0=0x%016llx\n",
            (unsigned long long)sequence_t0_after,
            (unsigned long long)sequence_t1_after,
            (unsigned long long)sequence_a0_after);
    if (registers_ok) {
        cprintf("[LAB3-TRAP-SEQUENCE-TEST] CASE2 PASS registers restored\n");
    } else {
        cprintf("[LAB3-TRAP-SEQUENCE-TEST] CASE2 FAIL registers changed\n");
    }

    cprintf("[LAB3-TRAP-SEQUENCE-TEST] CASE3 PASS execution continued after sequence\n");
}

#endif /* LAB3_TRAP_SEQUENCE_TEST */
