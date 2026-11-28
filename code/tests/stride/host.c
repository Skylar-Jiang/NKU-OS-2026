#include <defs.h>
#include <string.h>
#include <proc.h>
#include "../fifo_sjf/host_io.h"
#include "../rr_cfs/host/host_guard.h"
#include "../../kern/schedule/default_sched_stride.c"
static struct proc_struct p[8];
static struct run_queue rq;
static void init(int n) {
    memset(&rq,0,sizeof(rq)); stride_init(&rq);
    memset(p,0,sizeof(p));
    for(int i=0;i<n;i++){p[i].pid=i+1;p[i].runs=1;p[i].lab6_priority=1;list_init(&p[i].run_link);}
}
static int heap_ok(skew_heap_entry_t *h,skew_heap_entry_t *parent) {
    if(!h)return 1;
    if(h->parent!=parent)return 0;
    if(h->left&&proc_stride_comp_f(h,h->left)>0)return 0;
    if(h->right&&proc_stride_comp_f(h,h->right)>0)return 0;
    return heap_ok(h->left,h)&&heap_ok(h->right,h);
}
static void bad_duplicate(void *unused){(void)unused;init(1);stride_enqueue(&rq,p);stride_enqueue(&rq,p);}
static void bad_remove(void *unused){(void)unused;init(1);stride_dequeue(&rq,p);}
int main(void) {
    init(3);
    check_value("empty returns NULL",1,stride_pick_next(&rq)==NULL);
    check_value("default quantum",MAX_TIME_SLICE,rq.max_time_slice);
    p[0].time_slice=-1;stride_enqueue(&rq,p);
    check_value("negative quantum refilled",MAX_TIME_SLICE,p[0].time_slice);
    check_value("queue assignment",1,p[0].rq==&rq);
    stride_dequeue(&rq,p);
    check_value("last remove empties heap",1,!rq.lab6_run_pool&&rq.proc_num==0);
    p[0].time_slice=2;stride_enqueue(&rq,p);stride_dequeue(&rq,p);
    check_value("partial quantum preserved",2,p[0].time_slice);
    p[0].time_slice=1;stride_proc_tick(&rq,p);
    check_value("tick expires",0,p[0].time_slice);
    check_value("tick requests reschedule",1,p[0].need_resched);
    stride_proc_tick(&rq,p);check_value("tick never goes negative",0,p[0].time_slice);
    init(3);p[0].lab6_stride=30;p[1].lab6_stride=10;p[2].lab6_stride=20;
    for(int i=0;i<3;i++)stride_enqueue(&rq,p+i);
    struct proc_struct *q=stride_pick_next(&rq);
    check_value("minimum pass selected",2,q->pid);stride_dequeue(&rq,q);
    check_value("charge is one BIG_STRIDE at weight1",10+BIG_STRIDE,q->lab6_stride);
    check_value("heap links after root removal",1,heap_ok(rq.lab6_run_pool,NULL));
    stride_dequeue(&rq,p);check_value("arbitrary node removal",1,rq.proc_num==1&&heap_ok(rq.lab6_run_pool,NULL));
    stride_dequeue(&rq,p+2);check_value("all removed count zero",0,rq.proc_num);
    init(2);rq.stride_floor=0xffffff00U;p[0].lab6_stride=0xfffffff0U;p[1].lab6_stride=0x10;
    stride_enqueue(&rq,p);stride_enqueue(&rq,p+1);q=stride_pick_next(&rq);
    check_value("wrap ordering selects older pass",1,q->pid);stride_dequeue(&rq,q);
    check_value("pass addition wraps intentionally",(uint32_t)(0xfffffff0U+BIG_STRIDE),q->lab6_stride);
    init(1);p[0].lab6_priority=0;stride_enqueue(&rq,p);q=stride_pick_next(&rq);stride_dequeue(&rq,q);
    check_value("zero priority uses weight1",BIG_STRIDE,q->lab6_stride);
    init(1);p[0].lab6_priority=~0U;stride_enqueue(&rq,p);q=stride_pick_next(&rq);stride_dequeue(&rq,q);
    check_value("huge priority still advances",1,q->lab6_stride);
    init(1);rq.stride_floor=1000;p[0].lab6_stride=1;stride_enqueue(&rq,p);
    check_value("sleeper stale credit clamped",1000,p[0].lab6_stride);stride_dequeue(&rq,p);
    p[0].runs=0;p[0].lab6_stride=0x90000000U;stride_enqueue(&rq,p);
    check_value("new task starts at current floor",1000,p[0].lab6_stride);stride_dequeue(&rq,p);
    init(1);stride_enqueue(&rq,p);q=stride_pick_next(&rq);stride_dequeue(&rq,q);
    q->runs++;q->lab6_priority=5;stride_enqueue(&rq,q);
    check_value("priority set in first quantum reconciles charge",BIG_STRIDE/5,q->lab6_stride);
    stride_dequeue(&rq,q);
    init(3);int counts[3]={0};p[1].lab6_priority=2;p[2].lab6_priority=4;
    for(int i=0;i<3;i++)stride_enqueue(&rq,p+i);
    int valid=1;
    for(int i=0;i<7000;i++){q=stride_pick_next(&rq);stride_dequeue(&rq,q);counts[q-p]++;q->runs++;stride_enqueue(&rq,q);valid&=heap_ok(rq.lab6_run_pool,NULL)&&rq.proc_num==3;}
    check_value("7000 dispatches preserve heap and count",1,valid);
    for(int i=0;i<3;i++)check_value("weighted share within two quanta",1,counts[i]>=(1000<<i)-2&&counts[i]<=(1000<<i)+2);
    check_value("duplicate enqueue assertion",2,run_isolated(bad_duplicate,NULL));
    check_value("nonmember dequeue assertion",2,run_isolated(bad_remove,NULL));
    return finish_tests();
}
