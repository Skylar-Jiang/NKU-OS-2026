#ifndef RR_CFS_HOST_GUARD_H
#define RR_CFS_HOST_GUARD_H

/* Run fn(arg) in a forked child and return its exit status: 0 when fn
 * returned normally, 2 when a kernel assert/panic fired (host_io's __panic),
 * -1 on a harness error. Lets negative tests trigger real assertions. */
int run_isolated(void (*fn)(void *), void *arg);
void note(const char *fmt, ...);
void fmt_name(char *buf, unsigned long size, const char *fmt, ...);

#endif
