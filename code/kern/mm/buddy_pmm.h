#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

/* 128 MiB framework ceiling, 4 KiB pages: orders 0 .. 15. */
#define BUDDY_MAX_ORDER 15

extern const struct pmm_manager buddy_pmm_manager;

/* Read-only consistency check, also used by the independent kernel tests. */
void buddy_validate(void);

/* Whole-block frees only: n is the original request or the recorded capacity.
 * Partial frees, interior pointers, wrong sizes and duplicate frees panic.
 * Page.property records capacity on allocated heads. Refcounts remain unused.
 * Flags bits 2/3 and bits 32..63 are private to this manager.
 */

#endif
