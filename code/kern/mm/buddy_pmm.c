#include <buddy_pmm.h>
#include <stdio.h>

#define BUDDY_MANAGED (1UL << 2)
#define BUDDY_ALLOCATED (1UL << 3)
#define BUDDY_REQUEST_SHIFT 32

static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t free_count;

static size_t block_pages(unsigned order) {
    return (size_t)1 << order;
}

/* Validate integer addresses before doing descriptor pointer arithmetic. */
static size_t descriptor_index(const struct Page *page) {
    uintptr_t addr = (uintptr_t)page, start = (uintptr_t)pages;
    if (pages == NULL || npage <= nbase || addr < start ||
        (addr - start) % sizeof(struct Page) != 0 ||
        (addr - start) / sizeof(struct Page) >= npage - nbase) {
        panic("buddy: invalid page descriptor");
    }
    return (addr - start) / sizeof(struct Page);
}

static void insert_block(struct Page *base, unsigned order) {
    size_t ppn = page2ppn(base);
    while (order < BUDDY_MAX_ORDER) {
        size_t capacity = block_pages(order);
        size_t other_ppn = ppn ^ capacity;
        if (other_ppn < nbase || other_ppn >= npage ||
            capacity > npage - other_ppn) {
            break;
        }
        struct Page *other = &pages[other_ppn - nbase];
        if (!(other->flags & BUDDY_MANAGED) || !PageProperty(other) ||
            other->property != capacity) {
            break;
        }
        list_del_init(&other->page_link);
        ClearPageProperty(other);
        other->property = 0;
        if (other_ppn < ppn) {
            base->property = 0;
            base = other;
            ppn = other_ppn;
        }
        order++;
    }
    base->flags = BUDDY_MANAGED;
    SetPageProperty(base);
    base->property = block_pages(order);
    list_add(&free_lists[order], &base->page_link);
}

static void buddy_init(void) {
    static_assert((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE ==
                  (1UL << BUDDY_MAX_ORDER));
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    free_count = 0;
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    size_t index = descriptor_index(base);
    assert(n > 0 && n <= npage - nbase - index);
    /* Validate the entire range before mutating it, including overlap checks.
     * Like default_init_memmap, Reserved here means not yet handed to a PMM;
     * page_init is responsible for choosing only genuinely available pages.
     */
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
        assert(!((base + i)->flags & BUDDY_MANAGED));
    }
    for (size_t i = 0; i < n; i++) {
        base[i].flags = BUDDY_MANAGED;
        base[i].property = 0;
        set_page_ref(base + i, 0);
        list_init(&base[i].page_link);
    }
    free_count += n;
    while (n != 0) {
        size_t ppn = page2ppn(base);
        unsigned order = BUDDY_MAX_ORDER;
        while (block_pages(order) > n || (ppn & (block_pages(order) - 1))) {
            order--;
        }
        size_t capacity = block_pages(order);
        insert_block(base, order);
        base += capacity;
        n -= capacity;
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > block_pages(BUDDY_MAX_ORDER) || n > free_count) {
        return NULL;
    }
    unsigned wanted = 0;
    while (block_pages(wanted) < n) {
        wanted++;
    }
    unsigned order = wanted;
    while (order <= BUDDY_MAX_ORDER && list_empty(&free_lists[order])) {
        order++;
    }
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }
    struct Page *base = le2page(list_next(&free_lists[order]), page_link);
    list_del_init(&base->page_link);
    ClearPageProperty(base);
    base->property = 0;
    while (order > wanted) {
        order--;
        struct Page *right = base + block_pages(order);
        right->flags = BUDDY_MANAGED;
        SetPageProperty(right);
        right->property = block_pages(order);
        list_add(&free_lists[order], &right->page_link);
    }
    size_t capacity = block_pages(wanted);
    base->flags = BUDDY_MANAGED | BUDDY_ALLOCATED |
                  ((uint64_t)n << BUDDY_REQUEST_SHIFT);
    base->property = capacity;
    free_count -= capacity;
    return base;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    size_t index = descriptor_index(base);
    if (!(base->flags & BUDDY_MANAGED) || PageReserved(base) ||
        !(base->flags & BUDDY_ALLOCATED) || PageProperty(base)) {
        panic("buddy: free requires an allocated block head");
    }
    size_t capacity = base->property;
    size_t request = base->flags >> BUDDY_REQUEST_SHIFT;
    if (n == 0 || (n != request && n != capacity)) {
        panic("buddy: free size must match request or capacity");
    }
    assert(capacity > 0 && capacity <= block_pages(BUDDY_MAX_ORDER));
    assert((capacity & (capacity - 1)) == 0);
    assert(capacity <= npage - nbase - index);
    assert((page2ppn(base) & (capacity - 1)) == 0);
    unsigned order = 0;
    while (block_pages(order) < capacity) {
        order++;
    }
    base->flags = BUDDY_MANAGED;
    base->property = 0;
    free_count += capacity;
    insert_block(base, order);
}

static size_t buddy_nr_free_pages(void) {
    return free_count;
}

void buddy_validate(void) {
    size_t total = 0;
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_entry_t *head = &free_lists[order];
        size_t visits = 0, capacity = block_pages(order);
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
            assert(++visits <= npage - nbase);
            assert(le->next->prev == le && le->prev->next == le);
            struct Page *base = le2page(le, page_link);
            size_t index = descriptor_index(base), ppn = page2ppn(base);
            assert(PageProperty(base) && base->property == capacity);
            assert(base->flags == (BUDDY_MANAGED | (1UL << PG_property)));
            assert((ppn & (capacity - 1)) == 0);
            assert(capacity <= npage - nbase - index);
            for (size_t i = 1; i < capacity; i++) {
                assert(base[i].flags == BUDDY_MANAGED);
                assert(base[i].property == 0);
            }
            if (order < BUDDY_MAX_ORDER) {
                size_t other_ppn = ppn ^ capacity;
                if (other_ppn >= nbase && other_ppn < npage) {
                    struct Page *other = &pages[other_ppn - nbase];
                    assert(!PageProperty(other) || other->property != capacity);
                }
            }
            total += capacity;
        }
    }
    assert(total == free_count);
    size_t managed = 0, allocated = 0;
    for (size_t i = 0; i < npage - nbase; i++) {
        struct Page *base = pages + i;
        if (!(base->flags & BUDDY_MANAGED)) continue;
        managed++;
        assert(!PageReserved(base));
        if (base->flags & BUDDY_ALLOCATED) {
            size_t capacity = base->property;
            size_t request = base->flags >> BUDDY_REQUEST_SHIFT;
            assert(!PageProperty(base) && request > 0 && request <= capacity);
            assert(capacity <= block_pages(BUDDY_MAX_ORDER));
            assert((capacity & (capacity - 1)) == 0);
            assert(request > capacity / 2 || capacity == 1);
            assert(capacity <= npage - nbase - i);
            assert((page2ppn(base) & (capacity - 1)) == 0);
            for (size_t j = 1; j < capacity; j++) {
                assert(base[j].flags == BUDDY_MANAGED && base[j].property == 0);
            }
            allocated += capacity;
        }
    }
    assert(managed == total + allocated);
}

static void buddy_check(void) {
    buddy_validate();
    cprintf("buddy_check: free-list invariants PASS (%lu pages)\n", free_count);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
