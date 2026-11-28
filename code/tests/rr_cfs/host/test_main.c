/* Host-side regression tests for the RR and CFS scheduling classes.
 *
 * The production files kern/schedule/default_sched.c and cfs_sched.c are
 * compiled unchanged and only reached through their struct sched_class
 * callbacks. The small "framework driver" below reproduces what
 * kern/schedule/sched.c does around a class (tick -> need_resched ->
 * enqueue current -> pick_next -> dequeue); it contains no scheduling
 * policy of its own. RISC-V traps and context switches are NOT covered here;
 * see ../qemu for the in-kernel tests. */
#include <defs.h>
#include <list.h>
#include <proc.h>
#include <sched.h>
#include "../../fifo_sjf/host_io.h"
#include "host_guard.h"

extern struct sched_class default_sched_class;
extern struct sched_class cfs_sched_class;

#define NPROC 8
#define TICK_VRT (1ull << 20) /* must equal CFS_TICK_VRUNTIME */
#define CFS_LATENCY 10        /* must equal CFS_SCHED_LATENCY */

/* Fixture replacing alloc_proc (static in proc.c): a zeroed PCB with the same
 * Lab6/CFS initial values that alloc_proc assigns. */
static void
init_proc(struct proc_struct *proc, int pid)
{
    *proc = (struct proc_struct){0};
    proc->pid = pid;
    proc->state = PROC_RUNNABLE;
    list_init(&(proc->run_link));
    proc->lab6_priority = 1;
}

static long long
list_count(struct run_queue *rq)
{
    long long n = 0;
    list_entry_t *le = &(rq->run_list);
    while ((le = list_next(le)) != &(rq->run_list)) n++;
    return n;
}

static void
check_order(const char *name, struct run_queue *rq, const char *expected)
{
    char actual[NPROC + 1];
    unsigned int n = 0;
    list_entry_t *le = list_next(&(rq->run_list));
    while (le != &(rq->run_list) && n < NPROC)
    {
        actual[n++] = (char)le2proc(le, run_link)->pid;
        le = list_next(le);
    }
    actual[n] = '\0';
    check_text(name, expected, actual);
}

static int
pid_or_zero(struct proc_struct *p)
{
    return p ? p->pid : 0;
}

/* ---- framework driver (mirrors sched.c / trap.c, no policy) ---- */
struct sim
{
    struct sched_class *sc;
    struct run_queue rq;
    struct proc_struct *curr;
    long long ran[NPROC];
    long long switches;
};

static void
sim_start(struct sim *s, struct sched_class *sc, int max_slice,
          struct proc_struct *p, int n)
{
    *s = (struct sim){0};
    s->sc = sc;
    s->rq.max_time_slice = max_slice;
    sc->init(&(s->rq));
    for (int i = 0; i < n; i++) sc->enqueue(&(s->rq), &p[i]);
    s->curr = sc->pick_next(&(s->rq));
    sc->dequeue(&(s->rq), s->curr);
}

/* schedule(): re-queue current if runnable, pick, dequeue. */
static void
sim_schedule(struct sim *s, int requeue)
{
    struct proc_struct *prev = s->curr;
    prev->need_resched = 0;
    if (requeue) s->sc->enqueue(&(s->rq), prev);
    struct proc_struct *next = s->sc->pick_next(&(s->rq));
    if (next != NULL) s->sc->dequeue(&(s->rq), next);
    s->curr = next;
    if (next != prev) s->switches++;
}

/* One timer interrupt followed by the trap-return reschedule check. */
static void
sim_tick(struct sim *s, struct proc_struct *base, char *trace)
{
    struct proc_struct *c = s->curr;
    if (trace) *trace = (char)c->pid;
    s->ran[c - base]++;
    s->sc->proc_tick(&(s->rq), c);
    if (c->need_resched) sim_schedule(s, 1);
}

static void
sim_run(struct sim *s, struct proc_struct *base, int ticks)
{
    for (int i = 0; i < ticks; i++) sim_tick(s, base, NULL);
}

/* ---- negative-test bodies (run in a forked child) ---- */
struct neg
{
    struct sched_class *sc;
    struct run_queue rq, other;
    struct proc_struct p;
    int mode;
};

static void
neg_body(void *arg)
{
    struct neg *n = arg;
    n->rq.max_time_slice = 0;
    n->sc->init(&(n->rq));
    n->other.max_time_slice = 0;
    n->sc->init(&(n->other));
    init_proc(&(n->p), 'X');
    switch (n->mode)
    {
    case 0: /* double enqueue */
        n->sc->enqueue(&(n->rq), &(n->p));
        n->sc->enqueue(&(n->rq), &(n->p));
        break;
    case 1: /* double dequeue */
        n->sc->enqueue(&(n->rq), &(n->p));
        n->sc->dequeue(&(n->rq), &(n->p));
        n->sc->dequeue(&(n->rq), &(n->p));
        break;
    case 2: /* dequeue from the wrong queue */
        n->sc->enqueue(&(n->rq), &(n->p));
        n->sc->dequeue(&(n->other), &(n->p));
        break;
    case 3: /* control: a legal sequence must not panic */
        n->sc->enqueue(&(n->rq), &(n->p));
        n->sc->dequeue(&(n->rq), &(n->p));
        n->sc->enqueue(&(n->rq), &(n->p));
        break;
    }
}

static void
check_guards(struct sched_class *sc, const char *tag)
{
    static const char *what[] = {"double enqueue asserts",
                                 "double dequeue asserts",
                                 "wrong-queue dequeue asserts",
                                 "legal enqueue/dequeue/enqueue ok"};
    char name[96];
    for (int mode = 0; mode < 4; mode++)
    {
        struct neg n = {.sc = sc, .mode = mode};
        fmt_name(name, sizeof(name), tag, what[mode]);
        check_value(name, mode == 3 ? 0 : 2, run_isolated(neg_body, &n));
    }
}

/* ===================================================================== */
/*                                  RR                                   */
/* ===================================================================== */
static void
test_rr(void)
{
    struct sched_class *sc = &default_sched_class;
    struct run_queue rq;
    struct proc_struct p[NPROC];
    note("\n== RR: class and init ==\n");
    check_text("RR name", "RR_scheduler", sc->name);
    check_value("RR five callbacks", 1, sc->init && sc->enqueue && sc->dequeue &&
                                        sc->pick_next && sc->proc_tick);

    /* 1. empty queue */
    rq = (struct run_queue){0};
    rq.proc_num = 99;
    rq.lab6_run_pool = &(p[0].lab6_run_pool);
    sc->init(&rq);
    check_value("RR1 zeroed rq: proc_num", 0, rq.proc_num);
    check_value("RR1 zeroed rq: empty list", 1, list_empty(&rq.run_list));
    check_value("RR1 zeroed rq: max_time_slice = MAX_TIME_SLICE", MAX_TIME_SLICE, rq.max_time_slice);
    check_value("RR1 zeroed rq: heap root NULL", 1, rq.lab6_run_pool == NULL);
    check_value("RR1 pick on empty -> NULL", 0, pid_or_zero(sc->pick_next(&rq)));
    rq.max_time_slice = 3;
    sc->init(&rq);
    check_value("RR1 configured max_time_slice 3 kept", 3, rq.max_time_slice);
    rq.max_time_slice = -4;
    sc->init(&rq);
    check_value("RR1 invalid max_time_slice -4 -> 5", MAX_TIME_SLICE, rq.max_time_slice);

    /* 2. single process */
    note("\n== RR: single process ==\n");
    rq = (struct run_queue){0};
    sc->init(&rq);
    init_proc(&p[0], 'A');
    sc->enqueue(&rq, &p[0]);
    check_value("RR2 enqueue: proc_num", 1, rq.proc_num);
    check_value("RR2 enqueue: proc->rq", 1, p[0].rq == &rq);
    check_value("RR2 enqueue: run_link linked", 0, list_empty(&p[0].run_link));
    check_value("RR2 pick -> A", 'A', pid_or_zero(sc->pick_next(&rq)));
    check_value("RR2 pick does not dequeue", 1, rq.proc_num);
    sc->dequeue(&rq, &p[0]);
    check_value("RR2 dequeue: proc_num", 0, rq.proc_num);
    check_value("RR2 dequeue: run_link self-linked", 1, list_empty(&p[0].run_link));
    check_value("RR2 dequeue: rq kept as last queue", 1, p[0].rq == &rq);
    check_value("RR2 pick after dequeue -> NULL", 0, pid_or_zero(sc->pick_next(&rq)));

    /* 3/4/10. order, counts, re-enqueue */
    note("\n== RR: ordering and counts ==\n");
    rq = (struct run_queue){0};
    sc->init(&rq);
    for (int i = 0; i < 3; i++) init_proc(&p[i], 'A' + i);
    for (int i = 0; i < 3; i++) sc->enqueue(&rq, &p[i]);
    check_order("RR3 FIFO order after A,B,C", &rq, "ABC");
    check_value("RR3 pick -> head A", 'A', pid_or_zero(sc->pick_next(&rq)));
    check_value("RR4 proc_num == list length (3)", list_count(&rq), rq.proc_num);
    sc->dequeue(&rq, &p[1]);
    check_order("RR4 remove middle B", &rq, "AC");
    check_value("RR4 proc_num == list length (2)", list_count(&rq), rq.proc_num);
    sc->dequeue(&rq, &p[0]);
    sc->enqueue(&rq, &p[0]);
    check_order("RR10 re-enqueued A goes to the tail", &rq, "CA");
    sc->enqueue(&rq, &p[1]);
    check_order("RR10 B re-enqueued behind", &rq, "CAB");
    check_value("RR4 proc_num == list length (3 again)", list_count(&rq), rq.proc_num);

    /* 5/6/7. time slice on enqueue */
    note("\n== RR: time slice on enqueue (max 5) ==\n");
    static const struct { int in, out; const char *name; } ts[] = {
        {0, 5, "RR5 slice 0 refilled to 5"},
        {3, 3, "RR6 valid remaining slice 3 kept"},
        {5, 5, "RR6 full slice 5 kept"},
        {1, 1, "RR6 valid remaining slice 1 kept"},
        {-2, 5, "RR7 negative slice -2 -> 5"},
        {9, 5, "RR7 oversized slice 9 -> 5"},
    };
    for (unsigned int i = 0; i < sizeof(ts) / sizeof(ts[0]); i++)
    {
        rq = (struct run_queue){0};
        sc->init(&rq);
        init_proc(&p[0], 'A');
        p[0].time_slice = ts[i].in;
        sc->enqueue(&rq, &p[0]);
        check_value(ts[i].name, ts[i].out, p[0].time_slice);
    }
    rq = (struct run_queue){0};
    rq.max_time_slice = 2;
    sc->init(&rq);
    init_proc(&p[0], 'A');
    p[0].time_slice = 4;
    sc->enqueue(&rq, &p[0]);
    check_value("RR7 slice 4 > configured max 2 -> 2", 2, p[0].time_slice);

    /* 8/9. tick */
    note("\n== RR: proc_tick ==\n");
    init_proc(&p[0], 'A');
    p[0].time_slice = 3;
    sc->proc_tick(&rq, &p[0]);
    check_value("RR8 tick 3 -> 2", 2, p[0].time_slice);
    check_value("RR8 tick 3 -> 2: need_resched stays 0", 0, p[0].need_resched);
    p[0].need_resched = 1;
    sc->proc_tick(&rq, &p[0]);
    check_value("RR8 existing need_resched not cleared", 1, p[0].need_resched);
    p[0].time_slice = 1;
    p[0].need_resched = 0;
    sc->proc_tick(&rq, &p[0]);
    check_value("RR8 tick 1 -> 0", 0, p[0].time_slice);
    check_value("RR8 tick 1 -> 0 sets need_resched", 1, p[0].need_resched);
    p[0].need_resched = 0;
    sc->proc_tick(&rq, &p[0]);
    check_value("RR9 tick at 0 stays 0 (no negative)", 0, p[0].time_slice);
    check_value("RR9 tick at 0 sets need_resched again", 1, p[0].need_resched);
    p[0].time_slice = -3;
    p[0].need_resched = 0;
    sc->proc_tick(&rq, &p[0]);
    check_value("RR9 corrupt slice -3 clamped to 0", 0, p[0].time_slice);
    check_value("RR9 corrupt slice -3 sets need_resched", 1, p[0].need_resched);
    check_value("RR8 tick does not touch the queue", 1, rq.proc_num);

    /* 10/12. rotation through the framework driver */
    note("\n== RR: rotation and max time slice ==\n");
    static const struct { int slice; const char *expect; } rot[] = {
        {5, "AAAAABBBBBCCCCCAAAAABBBBBCCCCC"},
        {2, "AABBCCAABBCCAABBCCAABBCCAABBCC"},
        {1, "ABCABCABCABCABCABCABCABCABCABC"},
    };
    for (unsigned int k = 0; k < sizeof(rot) / sizeof(rot[0]); k++)
    {
        struct sim s;
        char trace[31] = {0}, name[64];
        for (int i = 0; i < 3; i++) init_proc(&p[i], 'A' + i);
        sim_start(&s, sc, rot[k].slice, p, 3);
        for (int t = 0; t < 30; t++) sim_tick(&s, p, &trace[t]);
        fmt_name(name, sizeof(name), "RR10 30-tick trace, max slice %d", rot[k].slice);
        check_text(name, rot[k].expect, trace);
        fmt_name(name, sizeof(name), "RR12 switches in 30 ticks, max slice %d", rot[k].slice);
        check_value(name, 30 / rot[k].slice, s.switches);
        note("    max_time_slice=%d: ran A/B/C = %lld/%lld/%lld ticks, %lld switches\n",
             rot[k].slice, s.ran[0], s.ran[1], s.ran[2], s.switches);
    }

    /* 11. guards */
    note("\n== RR: misuse guards ==\n");
    check_guards(sc, "RR11 %s");
}

/* ===================================================================== */
/*                                  CFS                                  */
/* ===================================================================== */
static void
uninit_body(void *arg)
{
    struct run_queue rq = {0};
    struct proc_struct *p = arg;
    list_init(&rq.run_list);
    cfs_sched_class.enqueue(&rq, p);
}

static void
report_shares(const char *title, struct sim *s, struct proc_struct *p, int n,
              long long total, double tol)
{
    long long wsum = 0;
    for (int i = 0; i < n; i++) wsum += p[i].lab6_priority;
    note("    %s (%lld ticks, tolerance +/-%.3f)\n", title, total, tol);
    note("      proc weight  ticks  theory  actual\n");
    for (int i = 0; i < n; i++)
    {
        double theory = (double)p[i].lab6_priority / (double)wsum;
        double actual = (double)s->ran[i] / (double)total;
        note("      %c    %6u %6lld  %.4f  %.4f\n", p[i].pid,
             p[i].lab6_priority, s->ran[i], theory, actual);
        char name[96];
        fmt_name(name, sizeof(name), "%s: %c share within tolerance", title, p[i].pid);
        double err = actual - theory;
        if (err < 0) err = -err;
        check_value(name, 1, err <= tol);
    }
}

static void
test_cfs(void)
{
    struct sched_class *sc = &cfs_sched_class;
    struct run_queue rq;
    struct proc_struct p[NPROC];
    note("\n== CFS: class, init, empty/single ==\n");
    check_text("CFS name", "CFS_scheduler", sc->name);
    check_value("CFS five callbacks", 1, sc->init && sc->enqueue && sc->dequeue &&
                                         sc->pick_next && sc->proc_tick);
    rq = (struct run_queue){0};
    rq.proc_num = 7;
    sc->init(&rq);
    check_value("CFS1 init: proc_num", 0, rq.proc_num);
    check_value("CFS1 init: empty list", 1, list_empty(&rq.run_list));
    check_value("CFS1 init: max_time_slice valid", MAX_TIME_SLICE, rq.max_time_slice);
    check_value("CFS1 pick on empty -> NULL", 0, pid_or_zero(sc->pick_next(&rq)));
    init_proc(&p[0], 'A');
    sc->enqueue(&rq, &p[0]);
    check_value("CFS1 single: proc_num", 1, rq.proc_num);
    check_value("CFS1 single: rq set", 1, p[0].rq == &rq);
    check_value("CFS1 single: placed flag set", 1, (p[0].cfs_flags & CFS_F_PLACED) != 0);
    check_value("CFS1 single: pick -> A", 'A', pid_or_zero(sc->pick_next(&rq)));
    check_value("CFS1 single: pick does not dequeue", 1, rq.proc_num);
    sc->dequeue(&rq, &p[0]);
    check_value("CFS1 single: dequeue -> 0", 0, rq.proc_num);
    check_value("CFS1 single: run_link self-linked", 1, list_empty(&p[0].run_link));
    init_proc(&p[1], 'U');
    check_value("CFS1 uninitialized run queue is rejected", 2, run_isolated(uninit_body, &p[1]));

    /* 2/3. ties and minimum */
    note("\n== CFS: selection ==\n");
    rq = (struct run_queue){0};
    sc->init(&rq);
    for (int i = 0; i < 3; i++) init_proc(&p[i], 'A' + i);
    for (int i = 0; i < 3; i++) sc->enqueue(&rq, &p[i]);
    check_value("CFS2 equal vruntime: earliest arrival A", 'A', pid_or_zero(sc->pick_next(&rq)));
    check_value("CFS2 repeated pick is stable", 'A', pid_or_zero(sc->pick_next(&rq)));
    sc->dequeue(&rq, &p[0]);
    check_value("CFS2 after A leaves: B", 'B', pid_or_zero(sc->pick_next(&rq)));
    rq = (struct run_queue){0};
    sc->init(&rq);
    static const uint64_t v[3] = {300, 100, 200};
    for (int i = 0; i < 3; i++)
    {
        init_proc(&p[i], 'A' + i);
        p[i].cfs_flags = CFS_F_PLACED;
        p[i].cfs_vruntime = v[i];
        sc->enqueue(&rq, &p[i]);
    }
    check_value("CFS3 min vruntime (A300,B100,C200) -> B", 'B', pid_or_zero(sc->pick_next(&rq)));
    check_value("CFS3 history kept on enqueue (B = 100)", 100, (long long)p[1].cfs_vruntime);
    check_order("CFS3 list keeps arrival order", &rq, "ABC");

    /* 4/10. tick accounting and numeric edges */
    note("\n== CFS: vruntime accounting and numeric edges ==\n");
    static const struct { uint32_t prio; long long inc; const char *name; } inc[] = {
        {1, 1 << 20, "CFS4 weight 1: +2^20 per tick"},
        {2, 1 << 19, "CFS4 weight 2: +2^19 per tick"},
        {4, 1 << 18, "CFS4 weight 4: +2^18 per tick"},
        {3, (1 << 20) / 3, "CFS10 weight 3: truncated 349525"},
        {0, 1 << 20, "CFS10 priority 0 treated as weight 1"},
        {1024, 1024, "CFS10 weight 1024: +1024 (never 0)"},
        {5000, 1024, "CFS10 priority 5000 clamped to 1024"},
        {0xFFFFFFFFu, 1024, "CFS10 priority UINT32_MAX clamped to 1024"},
    };
    for (unsigned int i = 0; i < sizeof(inc) / sizeof(inc[0]); i++)
    {
        rq = (struct run_queue){0};
        sc->init(&rq);
        init_proc(&p[0], 'A');
        p[0].lab6_priority = inc[i].prio;
        sc->enqueue(&rq, &p[0]);
        sc->dequeue(&rq, &p[0]);
        uint64_t before = p[0].cfs_vruntime;
        sc->proc_tick(&rq, &p[0]);
        check_value(inc[i].name, inc[i].inc, (long long)(p[0].cfs_vruntime - before));
    }
    check_value("CFS4 one tick: cfs_sum_exec = 1", 1, (long long)p[0].cfs_sum_exec);
    check_value("CFS4 one tick: cfs_slice_exec = 1", 1, p[0].cfs_slice_exec);
    p[0].cfs_slice_exec = 0xFFFFFFFFu;
    sc->proc_tick(&rq, &p[0]);
    check_value("CFS10 cfs_slice_exec saturates at UINT32_MAX", 0xFFFFFFFFll, p[0].cfs_slice_exec);

    /* wrap-around: A just below 2^64, B just past the wrap (logically later) */
    rq = (struct run_queue){0};
    sc->init(&rq);
    init_proc(&p[0], 'A');
    init_proc(&p[1], 'B');
    p[0].cfs_flags = p[1].cfs_flags = CFS_F_PLACED;
    p[0].cfs_vruntime = ~0ull - 10;
    p[1].cfs_vruntime = 5;
    sc->enqueue(&rq, &p[1]);
    sc->enqueue(&rq, &p[0]);
    check_value("CFS10 wrap-safe compare: UINT64_MAX-10 before 5", 'A', pid_or_zero(sc->pick_next(&rq)));
    sc->dequeue(&rq, &p[0]);
    sc->proc_tick(&rq, &p[0]); /* A crosses the 2^64 boundary */
    check_value("CFS10 vruntime wrapped past 2^64", 1, p[0].cfs_vruntime < 1ull << 21);
    check_value("CFS10 after wrap B (5) still before A", 'B', pid_or_zero(sc->pick_next(&rq)));

    /* priority change keeps history */
    rq = (struct run_queue){0};
    sc->init(&rq);
    init_proc(&p[0], 'A');
    sc->enqueue(&rq, &p[0]);
    sc->dequeue(&rq, &p[0]);
    for (int i = 0; i < 10; i++) sc->proc_tick(&rq, &p[0]);
    uint64_t before = p[0].cfs_vruntime;
    p[0].lab6_priority = 2;
    sc->proc_tick(&rq, &p[0]);
    check_value("CFS11 priority 1->2: history not reset", 1, p[0].cfs_vruntime > before);
    check_value("CFS11 priority 1->2: next tick charged at weight 2", 1 << 19,
                (long long)(p[0].cfs_vruntime - before));

    /* 11/12. preemption */
    note("\n== CFS: preemption ==\n");
    rq = (struct run_queue){0};
    sc->init(&rq);
    init_proc(&p[0], 'A');
    sc->enqueue(&rq, &p[0]);
    sc->dequeue(&rq, &p[0]);
    long long resched = 0;
    for (int i = 0; i < 1000; i++)
    {
        sc->proc_tick(&rq, &p[0]);
        resched += p[0].need_resched;
    }
    check_value("CFS12 alone for 1000 ticks: need_resched never set", 0, resched);

    static const struct { uint32_t wa, wb; long long at; const char *name; } pre[] = {
        {1, 1, 5, "CFS11 equal weights: ideal slice 10*1/2 = 5 ticks"},
        {4, 1, 8, "CFS11 A weight 4 vs 1: ideal slice 10*4/5 = 8 ticks"},
        {1, 4, 2, "CFS11 A weight 1 vs 4: ideal 2 ticks"},
    };
    for (unsigned int k = 0; k < sizeof(pre) / sizeof(pre[0]); k++)
    {
        rq = (struct run_queue){0};
        sc->init(&rq);
        init_proc(&p[0], 'A');
        init_proc(&p[1], 'B');
        p[0].lab6_priority = pre[k].wa;
        p[1].lab6_priority = pre[k].wb;
        sc->enqueue(&rq, &p[0]);
        sc->enqueue(&rq, &p[1]);
        sc->dequeue(&rq, &p[0]);
        long long first = 0;
        for (int t = 1; t <= 20 && !first; t++)
        {
            sc->proc_tick(&rq, &p[0]);
            if (p[0].need_resched) first = t;
        }
        check_value(pre[k].name, pre[k].at, first);
    }

    /* vruntime-lead preemption respects the minimum granularity */
    rq = (struct run_queue){0};
    sc->init(&rq);
    init_proc(&p[0], 'A');
    init_proc(&p[1], 'B');
    sc->enqueue(&rq, &p[1]); /* B placed at min_vruntime 0 */
    p[0].cfs_flags = CFS_F_PLACED;
    p[0].cfs_vruntime = 100 * TICK_VRT; /* A far ahead of B */
    sc->proc_tick(&rq, &p[0]);
    check_value("CFS11 lead but slice 1 < min granularity 2: no resched", 0, p[0].need_resched);
    sc->proc_tick(&rq, &p[0]);
    check_value("CFS11 lead after 2 ticks: resched", 1, p[0].need_resched);

    /* 5/6. long-run fairness through the framework driver */
    note("\n== CFS: long-run CPU shares ==\n");
    static const struct { int n; uint32_t w[3]; int ticks; const char *title; } fair[] = {
        {3, {1, 1, 1}, 3000, "CFS5 equal weights 1:1:1"},
        {2, {1, 2, 0}, 3000, "CFS6 weights 1:2"},
        {3, {1, 2, 4}, 7000, "CFS6 weights 1:2:4"},
        {2, {1, 8, 0}, 9000, "CFS6 weights 1:8"},
    };
    for (unsigned int k = 0; k < sizeof(fair) / sizeof(fair[0]); k++)
    {
        struct sim s;
        for (int i = 0; i < fair[k].n; i++)
        {
            init_proc(&p[i], 'A' + i);
            p[i].lab6_priority = fair[k].w[i];
        }
        sim_start(&s, sc, MAX_TIME_SLICE, p, fair[k].n);
        sim_run(&s, p, fair[k].ticks);
        /* Tolerance fixed before running: one scheduling period (10 ticks)
         * per process of rounding at the end of the run, as a share. */
        report_shares(fair[k].title, &s, p, fair[k].n, fair[k].ticks,
                      (double)(CFS_LATENCY * fair[k].n) / fair[k].ticks);
        note("      switches: %lld\n", s.switches);
    }

    /* 7. a new process joining a long-running system */
    note("\n== CFS: new process and sleeper placement ==\n");
    {
        struct sim s;
        for (int i = 0; i < 2; i++) init_proc(&p[i], 'A' + i);
        sim_start(&s, sc, MAX_TIME_SLICE, p, 2);
        sim_run(&s, p, 1000);
        uint64_t vmin = p[0].cfs_vruntime < p[1].cfs_vruntime ? p[0].cfs_vruntime : p[1].cfs_vruntime;
        uint64_t vcur = s.curr->cfs_vruntime;
        init_proc(&p[2], 'C');
        sc->enqueue(&s.rq, &p[2]);
        check_value("CFS7 new proc not placed at 0", 1, p[2].cfs_vruntime > 0);
        check_value("CFS7 new proc placed at a baseline <= every live vruntime", 1,
                    p[2].cfs_vruntime <= vmin && p[2].cfs_vruntime <= vcur);
        check_value("CFS7 new proc within one latency of the others", 1,
                    vmin - p[2].cfs_vruntime <= (uint64_t)CFS_LATENCY * TICK_VRT);
        long long before_c = s.ran[2];
        sim_run(&s, p, 300);
        long long got = s.ran[2] - before_c;
        note("    C joined after 1000 ticks: %lld of next 300 ticks (fair 100)\n", got);
        check_value("CFS7 newcomer gets ~fair share (<= 100 + latency)", 1, got <= 100 + CFS_LATENCY);
        check_value("CFS7 newcomer is not starved (>= 100 - latency)", 1, got >= 100 - CFS_LATENCY);
    }

    /* 8. sleep and wake-up */
    {
        struct sim s;
        for (int i = 0; i < 3; i++) init_proc(&p[i], 'A' + i);
        sim_start(&s, sc, MAX_TIME_SLICE, p, 3);
        sim_run(&s, p, 300);
        /* C goes to sleep: when it is running it leaves without re-queue,
         * when it is queued it is taken off the queue (as do_wait would). */
        if (s.curr == &p[2]) sim_schedule(&s, 0);
        else sc->dequeue(&s.rq, &p[2]);
        uint64_t slept_at = p[2].cfs_vruntime;
        sim_run(&s, p, 3000);
        uint64_t vmin = p[0].cfs_vruntime < p[1].cfs_vruntime ? p[0].cfs_vruntime : p[1].cfs_vruntime;
        sc->enqueue(&s.rq, &p[2]); /* wakeup_proc */
        check_value("CFS8 sleeper vruntime raised on wake-up", 1, p[2].cfs_vruntime > slept_at);
        check_value("CFS8 sleeper credit bounded by latency/2", 1,
                    vmin - p[2].cfs_vruntime <= (uint64_t)(CFS_LATENCY / 2) * TICK_VRT + TICK_VRT);
        long long before_c = s.ran[2], burst = 0, run = 0;
        char last = 0;
        for (int t = 0; t < 90; t++)
        {
            char who;
            sim_tick(&s, p, &who);
            run = (who == 'C' && last == 'C') ? run + 1 : (who == 'C');
            if (run > burst) burst = run;
            last = who;
        }
        long long got = s.ran[2] - before_c;
        note("    C woke after sleeping 3000 ticks: %lld of next 90 ticks, longest burst %lld\n",
             got, burst);
        check_value("CFS8 woken sleeper share bounded (<= 30 + 5 + latency)", 1,
                    got <= 30 + CFS_LATENCY / 2 + CFS_LATENCY);
        check_value("CFS8 woken sleeper does not monopolize (burst <= 20)", 1, burst <= 20);
    }

    /* 9. dequeue / re-enqueue keeps history */
    {
        rq = (struct run_queue){0};
        sc->init(&rq);
        init_proc(&p[0], 'A');
        sc->enqueue(&rq, &p[0]);
        sc->dequeue(&rq, &p[0]);
        for (int i = 0; i < 7; i++) sc->proc_tick(&rq, &p[0]);
        uint64_t vr = p[0].cfs_vruntime, sum = p[0].cfs_sum_exec;
        sc->enqueue(&rq, &p[0]);
        sc->dequeue(&rq, &p[0]);
        check_value("CFS9 re-enqueue keeps vruntime", 1, p[0].cfs_vruntime == vr);
        check_value("CFS9 re-enqueue keeps cfs_sum_exec (7)", 7, (long long)sum);
        check_value("CFS9 dequeue starts a new slice", 0, p[0].cfs_slice_exec);
    }

    note("\n== CFS: misuse guards ==\n");
    check_guards(sc, "CFS %s");
}

int
main(void)
{
    test_rr();
    test_cfs();
    return finish_tests();
}
