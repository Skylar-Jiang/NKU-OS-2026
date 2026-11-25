#ifndef FIFO_SJF_HOST_IO_H
#define FIFO_SJF_HOST_IO_H

/* Primitive types only: keep host libc types out of the kernel headers. */
void check_value(const char *name, long long expected, long long actual);
void check_text(const char *name, const char *expected, const char *actual);
int finish_tests(void);

#endif
