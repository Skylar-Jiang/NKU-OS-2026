/* LAB6 2411788: same real user workload for all five boot-time policies. */
#include <ulib.h>
#include <stdio.h>
#ifndef BENCH_CASE
#define BENCH_CASE 0
#endif
#define N 4
static void unit(void) {
    /* Fixed instruction-count work, not a wall-time deadline: preemption
     * cannot make a waiting process finish its CPU work for free. */
    volatile unsigned int x = 17;
    for (unsigned int k=0;k<300000;k++) x=(x*1664525U+1013904223U)^k;
}
int main(void) {
    int pids[N];
    const int lengths[N]={12,3,6,1};
    cprintf("BENCH_BEGIN case=%d\n",BENCH_CASE);
    for(int i=0;i<N;i++) {
        pids[i]=fork(); assert(pids[i]>=0);
        if(pids[i]==0) {
            int units=BENCH_CASE==1 ? lengths[i] : (BENCH_CASE==2 ? 32 : 8);
            int weight=BENCH_CASE==2 ? (1<<i) : 1;
            lab6_setpriority(weight);
            cprintf("BJOB case=%d idx=%d pid=%d weight=%d units=%d\n",BENCH_CASE,i,getpid(),weight,units);
            if(BENCH_CASE==3) {
                /* Unequal CPU bursts separated by voluntary yields. This
                 * stresses prediction/response; it is not disk I/O. */
                for(int round=0;round<4;round++) {
                    for(int j=0;j<(i%2?1:4);j++)unit();
                    yield();
                }
            } else for(int j=0;j<units;j++)unit();
            cprintf("BJOB_DONE idx=%d pid=%d\n",i,getpid());
            exit(0);
        }
    }
    for(int i=0;i<N;i++){int status=-1;assert(waitpid(pids[i],&status)==0&&status==0);}
    cprintf("schedbench pass. case=%d\n",BENCH_CASE);
    return 0;
}
