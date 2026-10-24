#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

//lab2 challenge SLUB interface 2412096
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *obj);
void slub_test(void);

#endif /* !__KERN_MM_SLUB_H__ */
