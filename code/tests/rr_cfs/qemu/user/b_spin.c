/* b_spin - CPU-bound children that never yield (member B, Lab6).
 * Phase 1: three equal-priority children. Phase 2: priorities 1, 2, 4.
 * Each child spins until SPIN_MS and exits with its loop count; the parent
 * prints start times (preemption evidence) and shares. Judged by run.sh. */
#include <ulib.h>
#include <stdio.h>

#define N 3
#define SPIN_MS 3000

static void
burn(void)
{
    volatile int j = 0;
    for (int i = 0; i < 200; i++) j = !j;
}

static void
phase(const char *name, const unsigned int prio[N])
{
    int pids[N], codes[N];
    unsigned int base = gettime_msec();
    for (int i = 0; i < N; i++)
    {
        if ((pids[i] = fork()) == 0)
        {
            lab6_setpriority(prio[i]);
            unsigned int start = gettime_msec() - base;
            cprintf("b_spin: %s child %d prio %d start %d ms\n", name, i, prio[i], start);
            int acc = 0;
            while (1)
            {
                burn();
                if (++acc % 2000 == 0 && gettime_msec() - base > SPIN_MS) exit(acc / 1000);
            }
        }
        if (pids[i] < 0) panic("b_spin: fork failed\n");
    }
    for (int i = 0; i < N; i++)
    {
        if (waitpid(pids[i], &codes[i]) != 0) panic("b_spin: wait failed\n");
    }
    cprintf("b_spin: %s acc(k) %d %d %d ratio_x100 100 %d %d\n", name,
            codes[0], codes[1], codes[2],
            codes[1] * 100 / codes[0], codes[2] * 100 / codes[0]);
}

int
main(void)
{
    static const unsigned int equal[N] = {1, 1, 1};
    static const unsigned int weighted[N] = {1, 2, 4};
    lab6_setpriority(8); /* the waiting parent does not compete */
    phase("equal", equal);
    phase("weighted", weighted);
    cprintf("b_spin: done\n");
    return 0;
}
