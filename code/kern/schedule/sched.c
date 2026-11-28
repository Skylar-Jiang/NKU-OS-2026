#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <assert.h>
#include <default_sched.h>
#include <sched_select.h>
#include "sched_bench.h"

// the list of timer
static list_entry_t timer_list;

static struct sched_class *sched_class;

static struct run_queue *rq;

static inline void
sched_class_enqueue(struct proc_struct *proc)
{
    if (proc != idleproc)
    {
        bench_enqueue(proc);
        sched_class->enqueue(rq, proc);
    }
}

static inline void
sched_class_dequeue(struct proc_struct *proc)
{
    sched_class->dequeue(rq, proc);
}

static inline struct proc_struct *
sched_class_pick_next(void)
{
    return sched_class->pick_next(rq);
}

void sched_class_proc_tick(struct proc_struct *proc)
{
    if (proc != idleproc)
    {
        bench_tick(proc);
        sched_class->proc_tick(rq, proc);
    }
    else
    {
        proc->need_resched = 1;
    }
}

static struct run_queue __rq;

void sched_init(void)
{
    list_init(&timer_list);

    sched_class = &LAB6_SCHED_CLASS;

    rq = &__rq;
    rq->max_time_slice = MAX_TIME_SLICE;
    sched_class->init(rq);

    cprintf("sched class: %s\n", sched_class->name);
}

void wakeup_proc(struct proc_struct *proc)
{
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE)
        {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current)
            {
                sched_class_enqueue(proc);
            }
        }
        else
        {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

//lab4 inherited scheduling framework; LAB6 2411788
void schedule(void)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    struct proc_struct *prev = current;
#if LAB6_BENCH
    uint64_t decision_start = bench_now();
#endif
    bench_leave(prev);
    prev->need_resched = 0;
    if (prev->state == PROC_RUNNABLE) sched_class_enqueue(prev);
    struct proc_struct *next = sched_class_pick_next();
    if (next != NULL) sched_class_dequeue(next);
    else next = idleproc;
    next->runs++;
    bench_enter(next);
#if LAB6_BENCH
    bench_decisions++;
    bench_switches += next != prev;
    bench_decision_time += bench_now()-decision_start;
#endif
    bench_dump(prev);
    if (next != prev) proc_run(next);
    local_intr_restore(intr_flag);
}
