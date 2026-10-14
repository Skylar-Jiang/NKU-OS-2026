#ifndef __KERN_MM_BUDDY_TEST_H__
#define __KERN_MM_BUDDY_TEST_H__

/* Boot-time only: call with a single entirely free managed interval and no
 * live allocations. Tests temporarily rebuild subsets of that same real
 * interval, then restore the complete interval. Never call on a running heap.
 */
void buddy_test(void);

#endif
