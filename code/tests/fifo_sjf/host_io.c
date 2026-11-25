#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "host_io.h"

static unsigned int checks, failures;

void check_value(const char *name, long long expected, long long actual)
{
    int pass = expected == actual;
    checks++;
    failures += !pass;
    printf("[%s] %s | expected=%lld actual=%lld\n",
           pass ? "PASS" : "FAIL", name, expected, actual);
}

void check_text(const char *name, const char *expected, const char *actual)
{
    int pass = strcmp(expected, actual) == 0;
    checks++;
    failures += !pass;
    printf("[%s] %s | expected=%s actual=%s\n",
           pass ? "PASS" : "FAIL", name, expected, actual);
}

int finish_tests(void)
{
    printf("SUMMARY: checks=%u passed=%u failed=%u\n",
           checks, checks - failures, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* The schedulers only need the kernel assertion exit, not a running kernel. */
void __attribute__((noreturn))
__panic(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "[FAIL] kernel assertion at %s:%d | ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(2);
}
