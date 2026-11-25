#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <sched.h>

#define SJF_MAX_TICKS ((int)(~0U >> 1))

/* SJF and Stride run in separate boots. alloc_proc must initialize both
 * time_slice (elapsed ticks) and lab6_stride (prediction) to zero.
 * Re-enqueue ends the previous sample; a sub-tick burst keeps its prediction.
 */
static void
SJF_update_burst(struct proc_struct *proc)
{
    if (proc->lab6_stride == 0)
    {
        proc->lab6_stride = MAX_TIME_SLICE;
    }
    else if (proc->time_slice > 0)
    {
        /* alpha = 0.5, rounded down. Widen before adding to avoid overflow;
         * two positive integer inputs keep the resulting prediction positive.
         */
        proc->lab6_stride = (uint32_t)(((uint64_t)proc->lab6_stride +
                                      (uint32_t)proc->time_slice) / 2);
    }
    proc->time_slice = 0;
}

static void
SJF_init(struct run_queue *rq)
{
    list_init(&(rq->run_list));
    rq->proc_num = 0;
    rq->lab6_run_pool = NULL;
}

static void
SJF_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
    assert(list_empty(&(proc->run_link)));
    SJF_update_burst(proc);
    /* Keep arrival order in the list; pick_next resolves equal predictions. */
    list_add_before(&(rq->run_list), &(proc->run_link));
    proc->rq = rq;
    rq->proc_num++;
}

static void
SJF_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    assert(rq->proc_num > 0);
    list_del_init(&(proc->run_link));
    rq->proc_num--;
    /* Keep the burst fields and the last assigned queue, as in FIFO. */
}

static struct proc_struct *
SJF_pick_next(struct run_queue *rq)
{
    struct proc_struct *best = NULL;
    list_entry_t *le = &(rq->run_list);
    while ((le = list_next(le)) != &(rq->run_list))
    {
        struct proc_struct *proc = le2proc(le, run_link);
        /* Strict comparison preserves FIFO order for equal predictions. */
        if (best == NULL || proc->lab6_stride < best->lab6_stride)
        {
            best = proc;
        }
    }
    return best;
}

static void
SJF_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    (void)rq;
    /* Saturate instead of wrapping; never change need_resched here. */
    if (proc->time_slice < SJF_MAX_TICKS)
    {
        proc->time_slice++;
    }
}

struct sched_class sjf_sched_class = {
    .name = "SJF",
    .init = SJF_init,
    .enqueue = SJF_enqueue,
    .dequeue = SJF_dequeue,
    .pick_next = SJF_pick_next,
    .proc_tick = SJF_proc_tick,
};
