#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

//lab6 function2 2413507

/* Round Robin scheduling class.
 *
 * Ready processes live on rq->run_list in arrival order: enqueue appends at
 * the tail, pick_next returns the head, and the framework (schedule()) then
 * dequeues the chosen process before running it. Each tick of the running
 * process consumes one unit of time_slice; when it reaches 0 need_resched is
 * set and the existing trap()/cpu_idle() path performs the switch.
 *
 * Queue-membership convention (shared with FIFO/SJF): run_link is the source
 * of truth. A queued process has a non-empty run_link; dequeue restores it to
 * a self-linked node. proc->rq keeps the last queue the process joined.
 * idleproc is never enqueued: sched_class_enqueue() filters it out and
 * sched_class_proc_tick() handles its ticks without calling RR. */

static void
RR_init(struct run_queue *rq)
{
    list_init(&(rq->run_list));
    rq->proc_num = 0;
    // Keep a slice length configured by sched_init(); a zeroed queue (or an
    // invalid value) gets the default MAX_TIME_SLICE.
    if (rq->max_time_slice <= 0)
    {
        rq->max_time_slice = MAX_TIME_SLICE;
    }
    // RR does not use the skew heap.
    rq->lab6_run_pool = NULL;
}

static void
RR_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
    // A process already on a run queue would corrupt the list if re-added.
    assert(list_empty(&(proc->run_link)));
    // An exhausted (0), corrupt (< 0) or oversized slice is refilled; a
    // valid remaining slice (e.g. after a wakeup) is kept.
    if (proc->time_slice <= 0 || proc->time_slice > rq->max_time_slice)
    {
        proc->time_slice = rq->max_time_slice;
    }
    // Insert before the head sentinel, i.e. at the tail of the queue.
    list_add_before(&(rq->run_list), &(proc->run_link));
    proc->rq = rq;
    rq->proc_num++;
}

static void
RR_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
    // The process must actually be queued on this run queue.
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    assert(rq->proc_num > 0);
    // list_del_init leaves run_link self-linked, so a repeated dequeue is
    // caught by the assertion above and a later enqueue is legal.
    list_del_init(&(proc->run_link));
    rq->proc_num--;
    // time_slice is kept: the slice is consumed by ticks, not by dequeue.
}

static struct proc_struct *
RR_pick_next(struct run_queue *rq)
{
    // Only select; schedule() dequeues the returned process itself.
    list_entry_t *le = list_next(&(rq->run_list));
    if (le == &(rq->run_list))
    {
        return NULL;
    }
    return le2proc(le, run_link);
}

static void
RR_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    (void)rq;
    if (proc->time_slice > 0)
    {
        proc->time_slice--;
    }
    if (proc->time_slice <= 0)
    {
        // Never go negative; keep requesting a reschedule until it happens.
        // An already-set need_resched is never cleared here.
        proc->time_slice = 0;
        proc->need_resched = 1;
    }
}

struct sched_class default_sched_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .proc_tick = RR_proc_tick,
};
