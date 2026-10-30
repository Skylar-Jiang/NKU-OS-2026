#include <clock.h>
#include <defs.h>
#include <sbi.h>
#include <stdio.h>
#include <riscv.h>

volatile size_t ticks;

static inline uint64_t get_cycles(void) {
#if __riscv_xlen == 64
    uint64_t n;
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
#else
    uint32_t lo, hi, tmp;
    __asm__ __volatile__(
        "1:\n"
        "rdtimeh %0\n"
        "rdtime %1\n"
        "rdtimeh %2\n"
        "bne %0, %2, 1b"
        : "=&r"(hi), "=&r"(lo), "=&r"(tmp));
    return ((uint64_t)hi << 32) | lo;
#endif
}

// Hardcode timebase
static uint64_t timebase = 100000;

/* *
 * clock_init - enable the S-mode timer interrupt (sie.STIE) and ask OpenSBI
 * for the first timer event; QEMU's time CSR runs at 10MHz, so a timebase
 * of 100000 gives 100 interrupts per second.
 * */
/* LAB3 B 2413507: 初始化监督态时钟中断和首次事件。 */
void clock_init(void) {
    // enable timer interrupt in sie
    set_csr(sie, MIP_STIP);
    // divided by 500 when using Spike(2MHz)
    // divided by 100 when using QEMU(10MHz)
    // timebase = sbi_timebase() / 500;
    clock_set_next_event();

    // initialize time counter 'ticks' to zero
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
}

/* clock_set_next_event - request one timer interrupt timebase ticks from now.
 * get_cycles() reads the time CSR (rdtime), not the cycle CSR. */
void clock_set_next_event(void) { sbi_set_timer(get_cycles() + timebase); }
