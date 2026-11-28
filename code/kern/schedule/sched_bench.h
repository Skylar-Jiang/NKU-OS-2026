#ifndef LAB6_BENCH
#define LAB6_BENCH 0
#endif
#if LAB6_BENCH
#include <riscv.h>
/* LAB6 2411788: optional observer. No policy, PCB or syscall changes.
 * rdtime units on QEMU virt = 10 MHz. Dispatch intervals include time in
 * the process's kernel services; decision overhead excludes switch.S/TLB. */
struct bench_record {
    uint64_t arrival, first, end, ready, run_start, waiting, running;
    unsigned int dispatches, ticks;
};
static struct bench_record bench_records[MAX_PID];
static uint64_t bench_decision_time, bench_decisions, bench_switches;
static int bench_max_pid, bench_shared_snapshot;
static uint64_t bench_now(void) { return read_csr(time); }
static struct bench_record *bench_rec(struct proc_struct *p) {
    return p && p->pid > 0 && p->pid < MAX_PID ? &bench_records[p->pid] : NULL;
}
static void bench_enqueue(struct proc_struct *p) {
    struct bench_record *r = bench_rec(p); if (!r) return;
    r->ready = bench_now(); if (!r->arrival) r->arrival = r->ready;
    if (p->pid > bench_max_pid) bench_max_pid = p->pid;
}
static void bench_leave(struct proc_struct *p) {
    struct bench_record *r = bench_rec(p); if (!r) return;
    uint64_t now = bench_now();
    if (r->run_start) r->running += now-r->run_start;
    r->run_start = 0;
    if (p->state == PROC_ZOMBIE) r->end = now;
}
static void bench_enter(struct proc_struct *p) {
    struct bench_record *r = bench_rec(p); if (!r) return;
    uint64_t now = bench_now();
    if (r->ready) r->waiting += now-r->ready;
    if (!r->first) r->first = now;
    r->ready = 0; r->run_start = now; r->dispatches++;
}
static void bench_tick(struct proc_struct *p) {
    struct bench_record *r = bench_rec(p); if (r) r->ticks++;
}
static void bench_dump(struct proc_struct *p) {
    struct bench_record *r = bench_rec(p);
    if (!r || p->pid < 2 || p->state != PROC_ZOMBIE) return;
    if (!bench_shared_snapshot && p->parent && p->parent->pid == 2) {
        bench_shared_snapshot = 1;
        for (int i=3; i<=bench_max_pid; i++) {
            struct proc_struct *task = find_proc(i);
            if (task && task->parent && task->parent->pid == 2) {
                struct bench_record *sample = &bench_records[i];
                cprintf("KSHARE pid=%d weight=%u run=%llu ticks=%u\n",
                    i, task->lab6_priority, sample->running, sample->ticks);
            }
        }
    }
    cprintf("KSTAT pid=%d parent=%d arrival=%llu first=%llu end=%llu run=%llu wait=%llu dispatches=%u ticks=%u decisions=%llu decision_time=%llu switches=%llu\n",
        p->pid,p->parent?p->parent->pid:-1,r->arrival,r->first,r->end,
        r->running,r->waiting,r->dispatches,r->ticks,
        bench_decisions,bench_decision_time,bench_switches);
}
#else
#define bench_enqueue(p) ((void)0)
#define bench_leave(p) ((void)0)
#define bench_enter(p) ((void)0)
#define bench_tick(p) ((void)0)
#define bench_dump(p) ((void)0)
#endif
