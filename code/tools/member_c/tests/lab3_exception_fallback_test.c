#include <assert.h>
#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>

#ifdef LAB3_EXCEPTION_FALLBACK_TEST

/* exception_handler is intentionally not exported by trap.h. */
void exception_handler(struct trapframe *tf);

#define FALLBACK_TEST_EPC ((uintptr_t)0xffffffffc0204000ULL)

/*
 * These cases call the existing handler with a synthetic trapframe instead of
 * executing non-recoverable hardware faults. The begin/end markers bracket
 * print_trapframe() output, and the assertion checks that the fallback path
 * leaves epc untouched.
 */
static void
test_fallback_exception(const char *name, uintptr_t cause, uintptr_t badvaddr) {
    struct trapframe tf = {0};
    uintptr_t epc_before = FALLBACK_TEST_EPC;

    tf.cause = cause;
    tf.epc = epc_before;
    tf.badvaddr = badvaddr;

    cprintf("[LAB3-FALLBACK-TEST] %s begin: expect trapframe output\n", name);
    exception_handler(&tf);
    cprintf("[LAB3-FALLBACK-TEST] %s epc before=0x%016llx after=0x%016llx\n",
            name, (unsigned long long)epc_before, (unsigned long long)tf.epc);
    assert(tf.epc == epc_before);
    cprintf("[LAB3-FALLBACK-TEST] %s PASS epc unchanged\n", name);
}

void
lab3_exception_fallback_test(void) {
    cprintf("[LAB3-FALLBACK-TEST] begin\n");

    test_fallback_exception("fetch-fault", CAUSE_FAULT_FETCH,
                            (uintptr_t)0x0000000040000000ULL);
    test_fallback_exception("load-fault", CAUSE_FAULT_LOAD,
                            (uintptr_t)0x000000008ffff000ULL);

    cprintf("[LAB3-FALLBACK-TEST] all cases passed\n");
}

#endif /* LAB3_EXCEPTION_FALLBACK_TEST */
