/* Standalone RV64 harness, NOT the complete Lab4 boot/grade test.
 * Link the actual C implementation; supply static PCBs/stacks instead of B's
 * allocator. Optional S-mode uses private identity mappings, not A's mappings. */
#include <proc.h>
#include <sched.h>
#include <riscv.h>
#include <sync.h>
#include <assert.h>
static struct proc_struct idle, worker[3];
static unsigned char stacks[3][8192] __attribute__((aligned(16)));
static unsigned long roots[3][512] __attribute__((aligned(4096)));
static volatile int order[16], used;
static void say(const char *s) { while (*s) *(volatile char *)0x10000000 = *s++; }
static void finish(int ok) __attribute__((noreturn));
static void finish(int ok) { *(volatile unsigned *)0x100000 = ok ? 0x5555 : 0x3333; for (;;) {} }
void __panic(const char *file, int line, const char *fmt, ...) {
    say("FAIL assertion\n"); finish(0);
}
static void check(int ok, const char *name) {
    if (!ok) { say("FAIL "); say(name); say("\n"); finish(0); }
    say("PASS "); say(name); say("\n");
}
static void body(void) {
    int id = current->pid;
    assert(!(read_csr(sstatus) & SSTATUS_SIE));
    assert(read_csr(satp) == (((uintptr_t)8 << 60) | (current->pgdir >> 12)));
    for (int i=0; i<3; ++i) {
        order[used++] = id;
        schedule();
    }
    current->state = PROC_SLEEPING;
    schedule();
    panic("sleeping thread resumed");
}
void context_ping(void) { proc_run(&worker[0]); }
extern int register_probe(void);
static void reset_workers(void) {
    list_init(&proc_list);
    used=0;
    for (int i=0;i<3;++i) {
        worker[i]=(struct proc_struct){0};
        worker[i].pid=i+1;
        worker[i].state=PROC_RUNNABLE;
        worker[i].pgdir=(uintptr_t)roots[i];
        worker[i].context.ra=(uintptr_t)body;
        worker[i].context.sp=(uintptr_t)&stacks[i][8192];
        list_add_before(&proc_list,&worker[i].list_link);
    }
}
static void idle_probe(void) {
    check(current==&worker[0],"cpu_idle observes need_resched and schedules worker");
#ifdef TEST_S_MODE
    say("ALL C ISOLATED TESTS PASS (S-mode identity mappings; full Lab4 pending)\n");
#else
    say("ALL C ISOLATED TESTS PASS (M-mode harness; full Lab4 pending)\n");
#endif
    finish(1);
}
void test_main(void) {
    idleproc=current=&idle;
    idle.state=PROC_RUNNABLE;
    idle.pgdir=(uintptr_t)roots[0];
    list_init(&proc_list);
    current->need_resched=1;
    schedule();
    check(current==&idle && !idle.need_resched && idle.runs==1,"empty list uses idle");
    reset_workers();
    worker[0].state=PROC_SLEEPING;
    worker[1].state=PROC_ZOMBIE;
    worker[2].state=PROC_UNINIT;
    schedule();
    check(current==&idle,"skip all non-runnable states");
    worker[0].state=PROC_RUNNABLE;
    current=&worker[0];
    int runs=current->runs;
    schedule();
    check(current==&worker[0] && current->runs==runs+1,"only current runnable selected on wrap");
    current=&idle;
    reset_workers();
    clear_csr(sstatus,SSTATUS_SIE);
    schedule();
    check(used==9 && current==&idle,"three threads yield and sleep back to idle");
    for(int i=0;i<9;++i) assert(order[i]==i%3+1);
    check(1,"round robin ordering across list head");
    check(!(read_csr(sstatus)&SSTATUS_SIE),"disabled interrupt state preserved");
    reset_workers();
    set_csr(sstatus,SSTATUS_SIE);
    check(register_probe()==0,"s0-s11 and stack return across real context switches");
    check(read_csr(sstatus)&SSTATUS_SIE,"enabled interrupt state restored on resume");
    bool outer,inner;
    local_intr_save(outer); local_intr_save(inner);
    local_intr_restore(inner);
    check(!(read_csr(sstatus)&SSTATUS_SIE),"nested restore keeps outer critical section disabled");
    local_intr_restore(outer);
    check(read_csr(sstatus)&SSTATUS_SIE,"outer restore re-enables interrupts");
    uintptr_t old=read_csr(satp);
    proc_run(current);
    check(read_csr(satp)==old && current==&idle,"proc_run current is a no-op");
    reset_workers();
    worker[1].state=worker[2].state=PROC_SLEEPING;
    worker[0].context.ra=(uintptr_t)idle_probe;
    current->need_resched=1;
    cpu_idle();
}

void test_boot(void) {
#ifdef TEST_S_MODE
    for (int i=0;i<3;++i) {
        roots[i][0]=0xcf; /* MMIO identity 0..1 GiB, supervisor RWXAD. */
        roots[i][2]=0x200000cf; /* RAM identity 0x80000000..0xbfffffff. */
    }
    write_csr(pmpaddr0, (uintptr_t)-1);
    write_csr(pmpcfg0, 0x0f); /* TOR, R/W/X over the physical address range. */
    write_csr(satp, ((uintptr_t)8<<60) | ((uintptr_t)roots[0]>>12));
    asm volatile("sfence.vma" ::: "memory");
    uintptr_t status=read_csr(mstatus);
    status=(status & ~(uintptr_t)0x1800) | 0x800; /* mret target = S. */
    write_csr(mstatus,status);
    write_csr(mepc,(uintptr_t)test_main);
    asm volatile("mret" ::: "memory");
    __builtin_unreachable();
#else
    test_main();
#endif
}
