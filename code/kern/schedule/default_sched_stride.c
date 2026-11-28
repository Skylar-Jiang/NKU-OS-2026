#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

//lab6 function3 2411788
/* Keep live pass values within half the uint32 range. Unsigned subtraction
 * makes rollover intentional; a single stride is much smaller than 2^31.
 * Priorities above BIG_STRIDE saturate to a one-unit step (never zero). */
#define BIG_STRIDE (1U << 20)
static int pass_before(uint32_t a, uint32_t b) {
    return a != b && ((a - b) & 0x80000000U) != 0;
}
static int proc_stride_comp_f(void *a, void *b) {
    struct proc_struct *pa = le2proc(a, lab6_run_pool);
    struct proc_struct *pb = le2proc(b, lab6_run_pool);
    if (pa->lab6_stride == pb->lab6_stride)
        return pa->pid < pb->pid ? -1 : pa->pid != pb->pid;
    return pass_before(pa->lab6_stride, pb->lab6_stride) ? -1 : 1;
}
static uint32_t stride_step(struct proc_struct *p) {
    uint32_t weight = p->lab6_priority ? p->lab6_priority : 1;
    uint32_t step = BIG_STRIDE / weight;
    return step ? step : 1;
}
static void stride_init(struct run_queue *rq) {
    list_init(&rq->run_list);
    rq->lab6_run_pool = NULL;
    rq->proc_num = 0;
    rq->stride_floor = 0;
    rq->stride_running = NULL;
    rq->stride_charge = 0;
    if (rq->max_time_slice <= 0) rq->max_time_slice = MAX_TIME_SLICE;
}
static void stride_enqueue(struct run_queue *rq, struct proc_struct *p) {
    assert(p->lab6_run_pool.parent == NULL && rq->lab6_run_pool != &p->lab6_run_pool);
    /* A user can set its priority during the first quantum. Reconcile the
     * provisional pick-time charge using the completed quantum's weight,
     * otherwise all children pay weight-1 first and short priority tests
     * are heavily biased. The running process is outside the heap. */
    if (rq->stride_running == p && p->runs > 0) {
        p->lab6_stride -= rq->stride_charge;
        p->lab6_stride += stride_step(p);
        rq->stride_running = NULL;
    }
    /* New tasks start at virtual time; sleepers cannot hoard stale credit.
     * runs is maintained by schedule(), not an extra PCB field owned by B. */
    if (p->runs == 0 || pass_before(p->lab6_stride, rq->stride_floor))
        p->lab6_stride = rq->stride_floor;
    if (p->time_slice <= 0 || p->time_slice > rq->max_time_slice)
        p->time_slice = rq->max_time_slice;
    rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &p->lab6_run_pool, proc_stride_comp_f);
    rq->lab6_run_pool->parent = NULL;
    p->rq = rq;
    rq->proc_num++;
}
static void stride_dequeue(struct run_queue *rq, struct proc_struct *p) {
    assert(p->rq == rq && rq->proc_num > 0);
    assert(p->lab6_run_pool.parent != NULL || rq->lab6_run_pool == &p->lab6_run_pool);
    rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &p->lab6_run_pool, proc_stride_comp_f);
    if (rq->lab6_run_pool) rq->lab6_run_pool->parent = NULL;
    skew_heap_init(&p->lab6_run_pool);
    rq->proc_num--;
}
static struct proc_struct *stride_pick_next(struct run_queue *rq) {
    if (!rq->lab6_run_pool) return NULL;
    struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
    rq->stride_floor = p->lab6_stride;
    rq->stride_running = p;
    rq->stride_charge = stride_step(p);
    p->lab6_stride += rq->stride_charge;
    /* Contract: the framework must dequeue this root immediately. Its new
     * key is charged now; children are merged by dequeue, restoring order. */
    return p;
}
static void stride_proc_tick(struct run_queue *rq, struct proc_struct *p) {
    (void)rq;
    if (p->time_slice > 0) p->time_slice--;
    if (p->time_slice <= 0) {
        p->time_slice = 0;
        p->need_resched = 1;
    }
}
struct sched_class stride_sched_class = {
    .name = "stride_scheduler", .init = stride_init,
    .enqueue = stride_enqueue, .dequeue = stride_dequeue,
    .pick_next = stride_pick_next, .proc_tick = stride_proc_tick,
};
