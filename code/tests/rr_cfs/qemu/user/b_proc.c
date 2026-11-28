/* b_proc - process-management and syscall regression (member B, Lab6).
 * Every check prints "b_proc: ok <name>"; any failure is a user panic. */
#include <ulib.h>
#include <stdio.h>

#define E_BAD_PROC 2
#define E_INVAL 3
#define E_FAULT 6
#define E_KILLED 9

static volatile int shared = 7;

#define CHECK(name, cond)                                   \
    do {                                                    \
        if (!(cond)) panic("b_proc: FAIL %s\n", name);      \
        cprintf("b_proc: ok %s\n", name);                   \
    } while (0)

static void
spin_ms(unsigned int ms)
{
    unsigned int end = gettime_msec() + ms;
    while (gettime_msec() < end)
        ;
}

static int
fork_and_wait(void (*child)(void), int *code)
{
    int pid = fork();
    if (pid == 0)
    {
        child();
        exit(0);
    }
    if (pid < 0) return pid;
    *code = 12345;
    int r = waitpid(pid, code);
    return r;
}

static void child_read_null(void) { volatile int *p = (int *)0; shared = *p; }
static void child_write_kernel(void) { *(volatile int *)0xffffffffc0200000UL = 1; }
static void child_write_text(void) { *(volatile int *)(void *)&spin_ms = 1; }
static void child_exit_code(void) { exit(0x5a); }

int
main(void)
{
    int code, pid, me = getpid();
    cprintf("b_proc: start pid %d\n", me);
    CHECK("first user process is pid 2", me == 2);

    unsigned int t0 = gettime_msec();
    spin_ms(300);
    CHECK("gettime advances with timer ticks", gettime_msec() >= t0 + 300);

    CHECK("wait without children -> -E_BAD_PROC", wait() == -E_BAD_PROC);
    CHECK("waitpid of unknown pid -> -E_BAD_PROC", waitpid(4000, &code) == -E_BAD_PROC);
    CHECK("kill of unknown pid -> -E_INVAL", kill(4000) == -E_INVAL);

    pid = fork();
    if (pid == 0)
    {
        if (getpid() == me) panic("b_proc: FAIL child has parent pid\n");
        shared = 99; /* private copy: must not leak into the parent */
        exit(0x33);
    }
    CHECK("fork returns child pid > parent", pid > me);
    CHECK("waitpid(child) returns 0", waitpid(pid, &code) == 0);
    CHECK("exit code delivered (0x33)", code == 0x33);
    CHECK("child write stays private (no COW leak)", shared == 7);
    CHECK("reaped child cannot be waited again", waitpid(pid, &code) == -E_BAD_PROC);

    pid = fork();
    if (pid == 0) { spin_ms(200); exit(1); }
    CHECK("wait with kernel status pointer -> -E_INVAL",
          waitpid(pid, (int *)0xffffffffc0200000UL) == -E_INVAL);
    CHECK("child survives bad wait and is reaped later", waitpid(pid, &code) == 0 && code == 1);

    pid = fork();
    if (pid == 0) { for (;;) ; } /* never yields: only the timer can preempt it */
    yield();
    spin_ms(50);
    CHECK("kill running CPU-bound child -> 0", kill(pid) == 0);
    CHECK("second kill -> -E_KILLED", kill(pid) == -E_KILLED);
    CHECK("killed child reaped with -E_KILLED", waitpid(pid, &code) == 0 && code == -E_KILLED);

    CHECK("user read of NULL kills only the child",
          fork_and_wait(child_read_null, &code) == 0 && code == -E_FAULT);
    CHECK("user write to kernel memory kills only the child",
          fork_and_wait(child_write_kernel, &code) == 0 && code == -E_FAULT);
    CHECK("user write to read-only text kills only the child",
          fork_and_wait(child_write_text, &code) == 0 && code == -E_FAULT);
    CHECK("explicit exit(0x5a) code", fork_and_wait(child_exit_code, &code) == 0 && code == 0x5a);

    int i, ok = 1;
    for (i = 0; i < 32 && ok; i++)
    {
        pid = fork();
        if (pid == 0) exit(i);
        ok = pid > 0 && waitpid(pid, &code) == 0 && code == i;
    }
    CHECK("32 fork/exit/wait cycles", ok);

    lab6_setpriority(3);
    yield();
    CHECK("setpriority and yield return to the caller", 1);

    cprintf("b_proc: all checks passed\n");
    return 0;
}
