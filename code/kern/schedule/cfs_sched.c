#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <sched.h>

/* Challenge 2 (2413507): a simplified Completely Fair Scheduler.
 *
 * This is a teaching-size version of the classic (pre-EEVDF) Linux CFS for
 * the single-CPU uCore framework, not a port of the Linux scheduler.
 *
 * Model
 *   - Every process has a virtual runtime cfs_vruntime. Each timer tick the
 *     running process is charged
 *         delta_vruntime = CFS_TICK_VRUNTIME / weight
 *     so a heavier process ages more slowly and receives proportionally more
 *     CPU: over a long run, CPU share_i ~= weight_i / sum(weight).
 *   - weight comes from lab6_priority (the value set by lab6_set_priority),
 *     clamped to [1, CFS_WEIGHT_MAX]. There is no nice-value table:
 *     priority 1 (the default) is the reference weight.
 *   - pick_next returns the ready process with the smallest vruntime.
 *   - min_vruntime is a monotonic per-queue baseline used to place new and
 *     waking processes, so they cannot exploit an old/zero vruntime.
 *
 * Units: one tick = one timer interrupt (10 ms with the default timebase).
 * cfs_sum_exec and cfs_slice_exec count ticks; vruntime counts
 * CFS_TICK_VRUNTIME units per weight-1 tick.
 *
 * Data structure: an unsorted list in arrival order (rq->run_list). pick_next
 * and the preemption check scan it, O(n); enqueue/dequeue are O(1). Linux
 * uses a red-black tree (O(log n) insert, cached leftmost); for the handful
 * of processes in uCore the list is simpler and exactly as fair. */

/* ---- parameters ---- */
#define CFS_WEIGHT_MAX 1024u            // weights are clamped to [1, 1024]
#define CFS_TICK_VRUNTIME (1ull << 20)  // vruntime of one tick at weight 1
#define CFS_SCHED_LATENCY 10u           // ticks: target period to run everyone once
#define CFS_MIN_GRANULARITY 2u          // ticks: minimum run before vruntime preemption
#define CFS_SLEEPER_CREDIT ((uint64_t)(CFS_SCHED_LATENCY / 2) * CFS_TICK_VRUNTIME)
#define CFS_MAX_RQ 4                    // run queues with live CFS state

/* Numeric design
 *   - delta_vruntime >= CFS_TICK_VRUNTIME / CFS_WEIGHT_MAX = 1024, so no
 *     weight ever gets a zero increment (no permanent "free" CPU); the
 *     truncation error is below 1 / 1024 of a tick.
 *   - At 2^20 per tick a 64-bit vruntime needs ~5.5e3 years at 100 Hz to
 *     wrap; comparisons still use the signed difference (as Linux does), so
 *     ordering stays correct across a wrap as long as live values are within
 *     2^63 of each other.
 *   - cfs_sum_exec is 64-bit; cfs_slice_exec saturates instead of wrapping. */

static inline int64_t
vrt_diff(uint64_t a, uint64_t b)
{
    return (int64_t)(a - b);
}

static inline uint64_t
cfs_weight(const struct proc_struct *proc)
{
    uint32_t prio = proc->lab6_priority;
    if (prio == 0)
    {
        prio = 1; // a zero priority is treated as the minimum weight
    }
    return prio > CFS_WEIGHT_MAX ? CFS_WEIGHT_MAX : prio;
}

static inline uint64_t
cfs_delta_vruntime(uint64_t ticks, uint64_t weight)
{
    return ticks * CFS_TICK_VRUNTIME / weight;
}

/* ---- per-queue state ----
 * struct run_queue (sched.h, member C) has no CFS field, so the
 * min_vruntime baseline is kept here, keyed by the run_queue pointer. The
 * kernel has one live run queue; the extra slots let tests use private
 * queues without disturbing it. CFS_init (re)claims and resets a slot. */
struct cfs_rq_state
{
    struct run_queue *rq;
    uint64_t min_vruntime;
};

static struct cfs_rq_state cfs_rq_states[CFS_MAX_RQ];
static unsigned int cfs_rq_victim;

static struct cfs_rq_state *
cfs_state(struct run_queue *rq)
{
    for (int i = 0; i < CFS_MAX_RQ; i++)
    {
        if (cfs_rq_states[i].rq == rq)
        {
            return &cfs_rq_states[i];
        }
    }
    panic("CFS: run queue %p was not initialized by CFS_init", rq);
}

static struct proc_struct *
cfs_leftmost(struct run_queue *rq)
{
    struct proc_struct *best = NULL;
    list_entry_t *le = &(rq->run_list);
    while ((le = list_next(le)) != &(rq->run_list))
    {
        struct proc_struct *proc = le2proc(le, run_link);
        // Strict "<": among equal vruntimes the earliest arrival wins.
        if (best == NULL || vrt_diff(proc->cfs_vruntime, best->cfs_vruntime) < 0)
        {
            best = proc;
        }
    }
    return best;
}

static uint64_t
cfs_queue_weight(struct run_queue *rq)
{
    uint64_t sum = 0;
    list_entry_t *le = &(rq->run_list);
    while ((le = list_next(le)) != &(rq->run_list))
    {
        sum += cfs_weight(le2proc(le, run_link));
    }
    return sum;
}

/* min_vruntime = max(min_vruntime, min(curr, leftmost)): it never goes back,
 * and never passes the process that is furthest behind. */
static void
cfs_update_min_vruntime(struct cfs_rq_state *st, struct run_queue *rq,
                        struct proc_struct *curr)
{
    uint64_t vmin = curr->cfs_vruntime;
    struct proc_struct *left = cfs_leftmost(rq);
    if (left != NULL && vrt_diff(left->cfs_vruntime, vmin) < 0)
    {
        vmin = left->cfs_vruntime;
    }
    if (vrt_diff(vmin, st->min_vruntime) > 0)
    {
        st->min_vruntime = vmin;
    }
}

/* Ideal slice of curr in ticks: its weight share of the scheduling period,
 * where the period stretches to nr * min_granularity when many processes
 * are ready (as in Linux __sched_period). At least one tick. */
static uint64_t
cfs_ideal_slice(struct run_queue *rq, struct proc_struct *curr)
{
    uint64_t nr = rq->proc_num + 1; // ready processes plus curr
    uint64_t period = CFS_SCHED_LATENCY;
    if (nr > CFS_SCHED_LATENCY / CFS_MIN_GRANULARITY)
    {
        period = nr * CFS_MIN_GRANULARITY;
    }
    uint64_t w = cfs_weight(curr);
    uint64_t slice = period * w / (cfs_queue_weight(rq) + w);
    return slice ? slice : 1;
}

/* ---- sched_class interface ---- */

static void
CFS_init(struct run_queue *rq)
{
    list_init(&(rq->run_list));
    rq->proc_num = 0;
    rq->lab6_run_pool = NULL; // the skew heap is not used by CFS
    if (rq->max_time_slice <= 0)
    {
        rq->max_time_slice = MAX_TIME_SLICE; // kept valid for the framework
    }

    struct cfs_rq_state *st = NULL;
    for (int i = 0; i < CFS_MAX_RQ && st == NULL; i++)
    {
        if (cfs_rq_states[i].rq == rq)
        {
            st = &cfs_rq_states[i];
        }
    }
    for (int i = 0; i < CFS_MAX_RQ && st == NULL; i++)
    {
        if (cfs_rq_states[i].rq == NULL)
        {
            st = &cfs_rq_states[i];
        }
    }
    if (st == NULL)
    {
        st = &cfs_rq_states[cfs_rq_victim];
        cfs_rq_victim = (cfs_rq_victim + 1) % CFS_MAX_RQ;
    }
    st->rq = rq;
    st->min_vruntime = 0;
}

static void
CFS_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
    assert(list_empty(&(proc->run_link)));
    struct cfs_rq_state *st = cfs_state(rq);

    if (!(proc->cfs_flags & CFS_F_PLACED))
    {
        // New process: start at the current baseline, not at 0, so it does
        // not out-run everyone who has been competing for a long time.
        proc->cfs_vruntime = st->min_vruntime;
        proc->cfs_flags |= CFS_F_PLACED;
    }
    else
    {
        // Re-enqueue keeps the history. A sleeper that fell far behind gets
        // at most CFS_SLEEPER_CREDIT (half a latency period at weight 1) of
        // catch-up instead of an unbounded burst. For a preempted process
        // the vruntime is already >= this bound, so this is a no-op.
        uint64_t floor = st->min_vruntime - CFS_SLEEPER_CREDIT;
        if (vrt_diff(proc->cfs_vruntime, floor) < 0)
        {
            proc->cfs_vruntime = floor;
        }
    }

    // Arrival order is kept; pick_next breaks vruntime ties by it.
    list_add_before(&(rq->run_list), &(proc->run_link));
    proc->rq = rq;
    rq->proc_num++;
}

static void
CFS_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    assert(rq->proc_num > 0);
    list_del_init(&(proc->run_link));
    rq->proc_num--;
    // schedule() dequeues exactly the process it is about to run: start a
    // new slice. vruntime and cfs_sum_exec are history and are kept.
    proc->cfs_slice_exec = 0;
}

static struct proc_struct *
CFS_pick_next(struct run_queue *rq)
{
    // Smallest vruntime, earliest arrival on ties; NULL lets the framework
    // fall back to idleproc. Selection does not dequeue.
    return cfs_leftmost(rq);
}

static void
CFS_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    struct cfs_rq_state *st = cfs_state(rq);

    // Charge exactly one tick, at the weight valid right now (a priority
    // change only affects ticks charged after it).
    proc->cfs_sum_exec++;
    if (proc->cfs_slice_exec < 0xFFFFFFFFu)
    {
        proc->cfs_slice_exec++;
    }
    proc->cfs_vruntime += cfs_delta_vruntime(1, cfs_weight(proc));
    proc->cfs_flags |= CFS_F_PLACED;
    cfs_update_min_vruntime(st, rq, proc);

    // Nobody else is ready: keep running, no pointless reschedule.
    if (rq->proc_num == 0)
    {
        return;
    }

    // Classic check_preempt_tick: preempt when the ideal slice is used up,
    // or, after a minimum granularity, when curr is ahead of the leftmost
    // ready process by more than its slice worth of vruntime.
    uint64_t ideal = cfs_ideal_slice(rq, proc);
    if (proc->cfs_slice_exec >= ideal)
    {
        proc->need_resched = 1;
        return;
    }
    if (proc->cfs_slice_exec < CFS_MIN_GRANULARITY)
    {
        return;
    }
    struct proc_struct *left = cfs_leftmost(rq);
    int64_t lead = vrt_diff(proc->cfs_vruntime, left->cfs_vruntime);
    if (lead > 0 && (uint64_t)lead > cfs_delta_vruntime(ideal, cfs_weight(proc)))
    {
        proc->need_resched = 1;
    }
    // need_resched is only ever set here; schedule() clears it.
}

struct sched_class cfs_sched_class = {
    .name = "CFS_scheduler",
    .init = CFS_init,
    .enqueue = CFS_enqueue,
    .dequeue = CFS_dequeue,
    .pick_next = CFS_pick_next,
    .proc_tick = CFS_proc_tick,
};
