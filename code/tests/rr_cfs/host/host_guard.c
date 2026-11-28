#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "host_guard.h"

int run_isolated(void (*fn)(void *), void *arg)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0)
    {
        /* Silence the expected assertion message of a negative test. */
        if (freopen("/dev/null", "w", stderr) == NULL) _exit(3);
        fn(arg);
        _exit(0);
    }
    int status;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}

void note(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void fmt_name(char *buf, unsigned long size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, size, fmt, ap);
    va_end(ap);
}
