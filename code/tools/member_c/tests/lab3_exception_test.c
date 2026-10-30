#include <defs.h>
#include <stdio.h>

#ifdef LAB3_EXCEPTION_TEST

/*
 * These instructions execute in S-mode after idt_init().  Reaching the PASS
 * marker proves that trapentry.S restored the saved context and that
 * exception_handler() advanced sepc over the faulting instruction.
 */
static void __attribute__((noinline))
test_breakpoint_32(void) {
    asm volatile(
        ".option push\n"
        ".option norvc\n"
        "ebreak\n"
        ".option pop\n"
        :
        :
        : "memory");
}

static void __attribute__((noinline))
test_breakpoint_16(void) {
    asm volatile(
        ".option push\n"
        ".option rvc\n"
        "c.ebreak\n"
        ".option pop\n"
        :
        :
        : "memory");
}

static void __attribute__((noinline))
test_illegal_16(void) {
    asm volatile(
        ".option push\n"
        ".option rvc\n"
        /* 0x0000 is a reserved 16-bit compressed encoding. */
        ".2byte 0x0000\n"
        ".option pop\n"
        :
        :
        : "memory");
}

static void __attribute__((noinline))
test_illegal_32(void) {
    asm volatile(
        ".option push\n"
        ".option norvc\n"
        /* RV64 reserves SLLIW encodings with shamt[5] set. */
        ".4byte 0x0200101b\n" /* slliw x0, x0, 32 */
        ".option pop\n"
        :
        :
        : "memory");
}

void lab3_exception_test(void) {
    cprintf("[LAB3-EXCEPTION-TEST] begin\n");

    cprintf("[LAB3-EXCEPTION-TEST] CASE1 breakpoint-32 begin\n");
    test_breakpoint_32();
    cprintf("[LAB3-EXCEPTION-TEST] CASE1 PASS epc+4\n");

    cprintf("[LAB3-EXCEPTION-TEST] CASE2 breakpoint-16 begin\n");
    test_breakpoint_16();
    cprintf("[LAB3-EXCEPTION-TEST] CASE2 PASS epc+2\n");

    cprintf("[LAB3-EXCEPTION-TEST] CASE3 illegal-16 begin\n");
    test_illegal_16();
    cprintf("[LAB3-EXCEPTION-TEST] CASE3 PASS epc+2\n");

    cprintf("[LAB3-EXCEPTION-TEST] CASE4 illegal-32 begin\n");
    test_illegal_32();
    cprintf("[LAB3-EXCEPTION-TEST] CASE4 PASS epc+4\n");

    cprintf("[LAB3-EXCEPTION-TEST] all cases passed\n");
}

#endif /* LAB3_EXCEPTION_TEST */
