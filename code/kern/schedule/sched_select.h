#ifndef __KERN_SCHED_SELECT_H__
#define __KERN_SCHED_SELECT_H__
/* LAB6 2411788: boot-time policy only. Never reinterpret a live queue. */
#ifndef LAB6_SCHED
#define LAB6_SCHED 2
#endif
#if LAB6_SCHED == 1
#define LAB6_SCHED_CLASS default_sched_class
#elif LAB6_SCHED == 2
#define LAB6_SCHED_CLASS stride_sched_class
#elif LAB6_SCHED == 3
#define LAB6_SCHED_CLASS fifo_sched_class
#elif LAB6_SCHED == 4
#define LAB6_SCHED_CLASS sjf_sched_class
#elif LAB6_SCHED == 5
#define LAB6_SCHED_CLASS cfs_sched_class
#else
#error Unsupported LAB6_SCHED
#endif
#endif
