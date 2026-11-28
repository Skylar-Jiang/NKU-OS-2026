#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>
#include <clock.h>

//lab5 请补充你在lab5中的代码
#include <error.h>

_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t),
               "syscall registers and pointers must retain RV64 width");

static int
sys_exit(uint64_t arg[])
{
    return do_exit((int)arg[0]);
}

static int
sys_fork(uint64_t arg[])
{
    (void)arg;
    struct trapframe *tf = current->tf;
    return do_fork(0, tf->gpr.sp, tf);
}

static int
sys_wait(uint64_t arg[])
{
    /* do_wait validates and copies out the user-supplied status address. */
    return do_wait((int)arg[0], (int *)(uintptr_t)arg[1]);
}

static int
sys_exec(uint64_t arg[])
{
    return do_execve((const char *)(uintptr_t)arg[0], (size_t)arg[1],
                     (unsigned char *)(uintptr_t)arg[2], (size_t)arg[3]);
}

static int
sys_yield(uint64_t arg[])
{
    (void)arg;
    return do_yield();
}

static int
sys_kill(uint64_t arg[])
{
    return do_kill((int)arg[0]);
}

static int
sys_getpid(uint64_t arg[])
{
    (void)arg;
    return current->pid;
}

static int
sys_putc(uint64_t arg[])
{
    cputchar((int)arg[0]);
    return 0;
}

static int
sys_pgdir(uint64_t arg[])
{
    (void)arg;
    print_pgdir();
    return 0;
}


static int sys_gettime(uint64_t arg[]){
    return (int)ticks*10;
}
static int sys_lab6_set_priority(uint64_t arg[]){
    uint64_t priority = (uint64_t)arg[0];
    lab6_set_priority(priority);
    return 0;
}
static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
    [SYS_gettime]           sys_gettime,
    [SYS_lab6_set_priority]  sys_lab6_set_priority,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

/* syscall - dispatch the system call recorded in current->tf.
 * Course ABI: a0 = number / return value, a1..a5 = arguments. The valid range
 * comes from the table itself (it ends at SYS_lab6_set_priority = 255), and
 * holes in the table are rejected. */
void
syscall(void)
{
    assert(current != NULL && current->tf != NULL);
    struct trapframe *tf = current->tf;
    uint64_t number = tf->gpr.a0;
    int result = -E_INVAL;
    if (number < NUM_SYSCALLS && syscalls[number] != NULL)
    {
        uint64_t arg[5] = {
            tf->gpr.a1, tf->gpr.a2, tf->gpr.a3, tf->gpr.a4, tf->gpr.a5,
        };
        result = syscalls[number](arg);
    }
    else
    {
        print_trapframe(tf);
        cprintf("undefined syscall %d, pid = %d, name = %s.\n",
                (int)number, current->pid, current->name);
    }
    /* Sign-extend int errors into the full RV64 return-value register. */
    tf->gpr.a0 = (uintptr_t)(intptr_t)result;
}
