/* Member B (2413507) user-level lifecycle test under fork/COW.
 * Copied into user/ of a temporary tree by tools/member-b/run.sh; it is not
 * part of the official program set. */
#include <stdio.h>
#include <ulib.h>
#include <unistd.h>

#define PG 4096
static volatile char data[3 * PG] = {1};   /* .data, shared COW after fork */
static volatile char bss[2 * PG];          /* .bss */

static int
sys_exec_raw(const char *name, long len, const void *binary, long size)
{
    register long a0 asm("a0") = SYS_exec;
    register long a1 asm("a1") = (long)name;
    register long a2 asm("a2") = len;
    register long a3 asm("a3") = (long)binary;
    register long a4 asm("a4") = size;
    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a3), "r"(a4)
                 : "memory");
    return (int)a0;
}

static void
check(int cond, const char *what)
{
    if (!cond) {
        panic("b_cowlife FAIL: %s", what);
    }
    cprintf("b_cowlife ok: %s\n", what);
}

int
main(void) {
    int pid, code, i;
    data[0] = 1;
    data[PG] = 2;
    bss[0] = 3;

    /* 1. Child writes after fork; parent's view stays private. The exit
     *    code is written by the kernel into `code` on the parent's stack,
     *    a page still COW-shared with the child at that moment. */
    if ((pid = fork()) == 0) {
        check(data[0] == 1 && data[PG] == 2 && bss[0] == 3, "child sees parent data");
        data[0] = 9;
        bss[0] = 9;
        exit(0x11);
    }
    data[PG] = 5;
    code = 0;
    check(waitpid(pid, &code) == 0 && code == 0x11, "wait collects exit code into COW stack page");
    check(data[0] == 1 && data[PG] == 5 && bss[0] == 3, "parent data isolated from child writes");
    check(waitpid(pid, &code) != 0, "second wait on reaped child fails");
    check(waitpid(pid, (int *)0x10) != 0, "wait with unmapped status pointer fails");

    /* 2. Bad status pointer does not consume the child; a later wait does. */
    if ((pid = fork()) == 0) {
        exit(0x22);
    }
    for (i = 0; i < 5; i++) yield();
    check(waitpid(pid, (int *)0xC0000000) != 0, "wait into kernel address refused");
    check(waitpid(pid, &code) == 0 && code == 0x22, "child still reapable after refused wait");

    /* 3. Several children, wait(any) reaps each exactly once. */
    for (i = 0; i < 5; i++) {
        if ((pid = fork()) == 0) {
            data[2 * PG] = (char)i;
            exit(0x30 + i);
        }
    }
    int seen = 0;
    for (i = 0; i < 5; i++) {
        code = 0;
        check(waitpid(0, &code) == 0 && code >= 0x30 && code < 0x35 &&
              !(seen & (1 << (code - 0x30))), "wait(any) reaps one distinct child");
        seen |= 1 << (code - 0x30);
    }
    check(waitpid(0, &code) != 0, "wait(any) with no children fails");

    /* 4. Parent exits first: grand-child is re-parented to initproc and
     *    still owns valid, private COW pages after its parent's mm is gone. */
    if ((pid = fork()) == 0) {
        if (fork() == 0) {
            for (i = 0; i < 20; i++) yield();
            check(data[0] == 1 && data[PG] == 5, "orphan still reads shared pages");
            data[PG] = 7;
            check(data[PG] == 7, "orphan writes after parent exit");
            exit(0);
        }
        exit(0x44);
    }
    check(waitpid(pid, &code) == 0 && code == 0x44, "middle process exits before its child");

    /* 5. Kill a child sleeping in wait(). */
    if ((pid = fork()) == 0) {
        if (fork() == 0) {
            for (i = 0; i < 40; i++) yield();
            exit(0);
        }
        wait();
        exit(0x55);                      /* not reached: killed while waiting */
    }
    for (i = 0; i < 5; i++) yield();
    check(kill(pid) == 0, "kill sleeping waiter");
    check(waitpid(pid, &code) == 0 && code == -9, "killed waiter exits with -E_KILLED");

    /* 6. Failed exec from a user process keeps its image. */
    check(sys_exec_raw("bad", 3, (const void *)bss, 64) < 0, "exec of non-ELF fails");
    check(sys_exec_raw("bad", 3, (const void *)0xC0200000, 4096) < 0, "exec of kernel address refused");
    check(data[PG] == 5 && bss[0] == 3, "image intact after failed exec");

    cprintf("b_cowlife pass.\n");
    return 0;
}
