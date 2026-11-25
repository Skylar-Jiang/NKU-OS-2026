#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <sched.h>

static void
FIFO_init(struct run_queue *rq)
{
    list_init(&(rq->run_list));
    rq->proc_num = 0;
    rq->lab6_run_pool = NULL;
}

static void
FIFO_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
    assert(list_empty(&(proc->run_link)));
    /* Append at the tail so earlier arrivals keep their positions. */
    list_add_before(&(rq->run_list), &(proc->run_link));
    proc->rq = rq;
    rq->proc_num++;
}

static void
FIFO_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    assert(rq->proc_num > 0);
    list_del_init(&(proc->run_link));
    rq->proc_num--;
    /* Keep proc->rq as the last assigned queue; run_link tracks membership. */
}

static struct proc_struct *
FIFO_pick_next(struct run_queue *rq)
{
    if (list_empty(&(rq->run_list)))
    {
        return NULL;
    }
    /* Selection does not remove the process from the run queue. */
    return le2proc(list_next(&(rq->run_list)), run_link);
}

static void
FIFO_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    /* FIFO is non-preemptive: preserve time_slice and need_resched. */
    (void)rq;
    (void)proc;
}

struct sched_class fifo_sched_class = {
    .name = "FIFO",
    .init = FIFO_init,
    .enqueue = FIFO_enqueue,
    .dequeue = FIFO_dequeue,
    .pick_next = FIFO_pick_next,
    .proc_tick = FIFO_proc_tick,
};
