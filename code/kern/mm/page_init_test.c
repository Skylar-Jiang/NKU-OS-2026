#include <dtb.h>
#include <page_init_test.h>
#include <pmm.h>
#include <stdio.h>

#ifdef LAB2_PAGE_INIT_TEST
#include <test_lab2.h>

void page_init_test(void) {
    extern char end[];
    uint64_t reported_end = get_memory_base() + get_memory_size();
    uintptr_t managed_end = reported_end < PHYSICAL_MEMORY_END
                                ? reported_end : PHYSICAL_MEMORY_END;
    managed_end = ROUNDDOWN(managed_end, PGSIZE);
    assert(va_pa_offset == PHYSICAL_MEMORY_OFFSET);
    assert(npage == PPN(managed_end) && npage > nbase);
    assert((uintptr_t)pages == ROUNDUP((uintptr_t)end, PGSIZE));
    size_t count = npage - nbase;
    uintptr_t first_free = ROUNDUP(PADDR((uintptr_t)(pages + count)), PGSIZE);
    size_t expected_free = first_free < managed_end
                               ? (managed_end - first_free) / PGSIZE : 0;
    assert(nr_free_pages() == expected_free);
    for (size_t i = 0; i < count; i++) {
        uintptr_t pa = (nbase + i) * PGSIZE;
        assert(page2pa(pages + i) == pa);
        assert(pa2page(pa) == pages + i);
        assert(PageReserved(pages + i) == (pa < first_free));
    }
    if (reported_end <= PHYSICAL_MEMORY_END) {
        /* Run the teacher's actual, unmodified function on supported RAM. */
        test_page_init();
    } else {
        cprintf("[A Page Init] official raw-end check skipped: RAM is capped\n");
    }
    cprintf("[A Page Init] layout, all reserved pages, roundtrip: PASS (%lu free)\n",
            expected_free);
}
#endif
