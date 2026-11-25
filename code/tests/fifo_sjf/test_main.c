#include <defs.h>
#include <list.h>
#include <proc.h>
#include <sched.h>
#include "host_io.h"

extern struct sched_class fifo_sched_class;
extern struct sched_class sjf_sched_class;

/* Fixtures replace alloc_proc only; all scheduler operations call real code. */
static void
init_proc(struct proc_struct *proc, int pid)
{
    *proc = (struct proc_struct){0};
    proc->pid = pid;
    proc->state = PROC_RUNNABLE;
    list_init(&(proc->run_link));
}

static const char *
proc_name(struct proc_struct *proc)
{
    if (proc == NULL) return "NULL";
    switch (proc->pid)
    {
    case 'A': return "A";
    case 'B': return "B";
    case 'C': return "C";
    default: return "?";
    }
}

/* Observe list order without sorting or predicting which process should win. */
static void
check_order(const char *name, struct run_queue *rq, const char *expected)
{
    char actual[9];
    unsigned int n = 0;
    list_entry_t *le = list_next(&(rq->run_list));
    while (le != &(rq->run_list) && n < sizeof(actual) - 1)
    {
        actual[n++] = le2proc(le, run_link)->pid;
        le = list_next(le);
    }
    actual[n] = '\0';
    check_text(name, expected, actual);
}

static void
ticks(struct sched_class *sc, struct run_queue *rq,
      struct proc_struct *proc, int count)
{
    for (int i = 0; i < count; i++) sc->proc_tick(rq, proc);
}

static int
check_class(struct sched_class *sc, const char *name)
{
    int ready = sc->init && sc->enqueue && sc->dequeue &&
                sc->pick_next && sc->proc_tick;
    check_text("class name", name, sc->name ? sc->name : "NULL");
    check_value("five class callbacks", 1, ready);
    return ready;
}

static void
test_fifo(void)
{
    struct sched_class *sc = &fifo_sched_class;
    struct run_queue rq = {0};
    struct proc_struct p[3];
    char order[4] = {0};
    if (!check_class(sc, "FIFO")) return;
    rq.max_time_slice = 17;
    rq.proc_num = 99;
    rq.lab6_run_pool = &(p[0].lab6_run_pool);
    sc->init(&rq);
    check_value("FIFO init count", 0, rq.proc_num);
    check_value("FIFO init empty list", 1, list_empty(&(rq.run_list)));
    check_value("FIFO init unused heap", 1, rq.lab6_run_pool == NULL);
    check_value("FIFO init preserves max_time_slice", 17, rq.max_time_slice);
    check_text("FIFO empty pick", "NULL", proc_name(sc->pick_next(&rq)));

    for (int i = 0; i < 3; i++)
    {
        init_proc(&p[i], 'A' + i);
        sc->enqueue(&rq, &p[i]);
        check_value("FIFO enqueue count", i + 1, rq.proc_num);
        check_value("FIFO enqueue owner", 1, p[i].rq == &rq);
    }
    check_order("FIFO arrival order", &rq, "ABC");
    check_text("FIFO repeated pick 1", "A", proc_name(sc->pick_next(&rq)));
    check_text("FIFO repeated pick 2", "A", proc_name(sc->pick_next(&rq)));
    check_value("FIFO pick leaves count", 3, rq.proc_num);
    check_order("FIFO pick leaves order", &rq, "ABC");
    for (int i = 0; i < 3; i++)
    {
        struct proc_struct *next = sc->pick_next(&rq);
        order[i] = next ? next->pid : '?';
        if (next == NULL) break;
        sc->dequeue(&rq, next);
        check_value("FIFO dequeue count", 2 - i, rq.proc_num);
        check_value("FIFO dequeue resets node", 1, list_empty(&(next->run_link)));
        check_value("FIFO dequeue keeps last owner", 1, next->rq == &rq);
    }
    check_text("FIFO dispatch sequence", "ABC", order);
    check_text("FIFO drained pick", "NULL", proc_name(sc->pick_next(&rq)));

    p[0].time_slice = 1;
    p[0].need_resched = 0;
    ticks(sc, &rq, &p[0], 20);
    check_value("FIFO 20 ticks do not consume slice", 1, p[0].time_slice);
    check_value("FIFO 20 ticks do not preempt", 0, p[0].need_resched);
    p[0].need_resched = 1;
    ticks(sc, &rq, &p[0], 20);
    check_value("FIFO preserves pending reschedule", 1, p[0].need_resched);

    for (int i = 0; i < 3; i++) sc->enqueue(&rq, &p[i]);
    sc->dequeue(&rq, &p[1]);
    check_order("FIFO remove middle", &rq, "AC");
    sc->enqueue(&rq, &p[1]);
    check_order("FIFO re-enqueue at tail", &rq, "ACB");
    for (int i = 0; i < 3; i++) sc->dequeue(&rq, &p[i]);
    check_value("FIFO final count", 0, rq.proc_num);
}

static void
test_sjf(void)
{
    struct sched_class *sc = &sjf_sched_class;
    struct run_queue rq = {0};
    struct proc_struct p[3];
    char order[4] = {0};
    if (!check_class(sc, "SJF")) return;
    rq.max_time_slice = 17;
    rq.proc_num = 99;
    rq.lab6_run_pool = &(p[0].lab6_run_pool);
    sc->init(&rq);
    check_value("SJF init count", 0, rq.proc_num);
    check_value("SJF init empty list", 1, list_empty(&(rq.run_list)));
    check_value("SJF init unused heap", 1, rq.lab6_run_pool == NULL);
    check_value("SJF init preserves max_time_slice", 17, rq.max_time_slice);
    check_text("SJF empty pick", "NULL", proc_name(sc->pick_next(&rq)));

    for (int i = 0; i < 3; i++)
    {
        init_proc(&p[i], 'A' + i);
        sc->enqueue(&rq, &p[i]);
        check_value("SJF initial estimate", 5, p[i].lab6_stride);
        check_value("SJF initial elapsed ticks", 0, p[i].time_slice);
        check_value("SJF enqueue count", i + 1, rq.proc_num);
        check_value("SJF enqueue owner", 1, p[i].rq == &rq);
    }
    for (int i = 0; i < 3; i++) sc->dequeue(&rq, &p[i]);

    /* A seeded prediction models earlier history; zero ticks must preserve it. */
    p[0].lab6_stride = 8;
    p[1].lab6_stride = 2;
    p[2].lab6_stride = 5;
    for (int i = 0; i < 3; i++) sc->enqueue(&rq, &p[i]);
    check_text("SJF repeated shortest pick 1", "B", proc_name(sc->pick_next(&rq)));
    check_text("SJF repeated shortest pick 2", "B", proc_name(sc->pick_next(&rq)));
    check_value("SJF pick leaves count", 3, rq.proc_num);
    check_order("SJF pick does not sort queue", &rq, "ABC");
    for (int i = 0; i < 3; i++)
    {
        struct proc_struct *next = sc->pick_next(&rq);
        const int expected[] = {2, 5, 8};
        order[i] = next ? next->pid : '?';
        if (next == NULL) break;
        check_value("SJF selected estimate", expected[i], next->lab6_stride);
        sc->dequeue(&rq, next);
        check_value("SJF dequeue count", 2 - i, rq.proc_num);
        check_value("SJF dequeue resets node", 1, list_empty(&(next->run_link)));
        check_value("SJF dequeue keeps estimate", expected[i], next->lab6_stride);
        check_value("SJF dequeue keeps last owner", 1, next->rq == &rq);
    }
    check_text("SJF 8,2,5 dispatch sequence", "BCA", order);
    check_text("SJF drained pick", "NULL", proc_name(sc->pick_next(&rq)));

    for (int i = 0; i < 3; i++) p[i].lab6_stride = 3;
    sc->enqueue(&rq, &p[2]);
    sc->enqueue(&rq, &p[0]);
    sc->enqueue(&rq, &p[1]);
    for (int i = 0; i < 3; i++)
    {
        struct proc_struct *next = sc->pick_next(&rq);
        order[i] = next ? next->pid : '?';
        if (next == NULL) break;
        sc->dequeue(&rq, next);
    }
    check_text("SJF equal estimates follow arrival (not PID)", "CAB", order);

    init_proc(&p[0], 'A');
    sc->enqueue(&rq, &p[0]);
    sc->dequeue(&rq, &p[0]);
    ticks(sc, &rq, &p[0], 3);
    check_value("SJF three actual ticks", 3, p[0].time_slice);
    check_value("SJF ticks do not preempt", 0, p[0].need_resched);
    check_value("SJF ticks do not update prediction early", 5, p[0].lab6_stride);
    sc->enqueue(&rq, &p[0]);
    check_value("SJF average (5+3)/2", 4, p[0].lab6_stride);
    check_value("SJF average resets elapsed", 0, p[0].time_slice);
    sc->dequeue(&rq, &p[0]);
    sc->enqueue(&rq, &p[0]);
    check_value("SJF zero-tick burst preserves estimate", 4, p[0].lab6_stride);
    sc->dequeue(&rq, &p[0]);
    ticks(sc, &rq, &p[0], 1);
    sc->enqueue(&rq, &p[0]);
    check_value("SJF odd sum rounds down (4+1)/2", 2, p[0].lab6_stride);
    sc->dequeue(&rq, &p[0]);

    p[0].lab6_stride = 1;
    ticks(sc, &rq, &p[0], 1);
    sc->enqueue(&rq, &p[0]);
    check_value("SJF minimum prediction stays positive", 1, p[0].lab6_stride);
    sc->dequeue(&rq, &p[0]);
    ticks(sc, &rq, &p[0], 20);
    check_value("SJF 20 ticks accumulate", 20, p[0].time_slice);
    check_value("SJF 20 ticks do not preempt", 0, p[0].need_resched);
    p[0].need_resched = 1;
    ticks(sc, &rq, &p[0], 2);
    check_value("SJF pending-reschedule ticks still accumulate", 22, p[0].time_slice);
    check_value("SJF preserves pending reschedule", 1, p[0].need_resched);

    /* Seed near the limit, then exercise real tick callbacks across it. */
    p[0].time_slice = 0x7FFFFFFE;
    ticks(sc, &rq, &p[0], 1);
    check_value("SJF tick reaches INT_MAX", 0x7FFFFFFF, p[0].time_slice);
    ticks(sc, &rq, &p[0], 3);
    check_value("SJF tick saturates at INT_MAX", 0x7FFFFFFF, p[0].time_slice);
    check_value("SJF saturated tick preserves reschedule", 1, p[0].need_resched);
    /* Full uint32_t history also checks widening before the average addition. */
    p[0].lab6_stride = 0xFFFFFFFFU;
    sc->enqueue(&rq, &p[0]);
    check_value("SJF widened average at field bounds", 3221225471LL, p[0].lab6_stride);
    check_value("SJF bounded average resets elapsed", 0, p[0].time_slice);
    sc->dequeue(&rq, &p[0]);
    check_value("SJF final count", 0, rq.proc_num);
    check_value("SJF final empty list", 1, list_empty(&(rq.run_list)));
}

int main(void)
{
    test_fifo();
    test_sjf();
    return finish_tests();
}
