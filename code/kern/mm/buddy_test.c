#include <buddy_pmm.h>
#include <buddy_test.h>
#include <stdio.h>

#ifdef LAB2_BUDDY_TEST
#ifndef LAB2_BUDDY_NEGATIVE
#define LAB2_BUDDY_NEGATIVE 0
#endif

static struct Page *original_base, *aligned_base;
static size_t original_count, fixture_count;

/* Tests own the boot-time free interval exclusively. Rebuilding descriptors
 * cannot affect the kernel, metadata or OpenSBI: all are below original_base.
 * Every normal fixture must return all its allocations before the next reset.
 */
static void empty_fixture(void) {
    buddy_validate();
    assert(nr_free_pages() == fixture_count);
    buddy_pmm_manager.init();
    for (size_t i = 0; i < original_count; i++) {
        original_base[i].flags = 0;
        SetPageReserved(original_base + i);
        original_base[i].property = 0;
        set_page_ref(original_base + i, 0);
        list_init(&original_base[i].page_link);
    }
    fixture_count = 0;
}

static void add_range(struct Page *base, size_t n) {
    assert(base >= original_base && n > 0);
    assert(base + n <= original_base + original_count);
    buddy_pmm_manager.init_memmap(base, n);
    fixture_count += n;
    assert(nr_free_pages() == fixture_count);
    buddy_validate();
}

static void fixture(struct Page *base, size_t n) {
    empty_fixture();
    add_range(base, n);
}

static size_t capacity(size_t n) {
    size_t result = 1;
    while (result < n) result <<= 1;
    return result;
}

static void disjoint(struct Page *a, struct Page *b) {
    assert(a + a->property <= b || b + b->property <= a);
}

static void payload(struct Page *page, uint64_t tag, bool write) {
    for (size_t i = 0; i < page->property; i++) {
        volatile uint64_t *lo = (volatile uint64_t *)
            (page2pa(page + i) + va_pa_offset);
        volatile uint64_t *hi = (volatile uint64_t *)
            ((uintptr_t)lo + PGSIZE - sizeof(uint64_t));
        if (write) {
            *lo = tag ^ i;
            *hi = ~tag ^ i;
        } else {
            assert(*lo == (tag ^ i) && *hi == (~tag ^ i));
        }
    }
}

static struct Page *take(size_t request) {
    size_t before = nr_free_pages(), actual = capacity(request);
    struct Page *p = alloc_pages(request);
    assert(p != NULL && p->property == actual);
    assert(nr_free_pages() == before - actual);
    assert(!PageReserved(p) && !PageProperty(p) && page_ref(p) == 0);
    assert((page2ppn(p) & (actual - 1)) == 0);
    assert(p >= original_base && p + actual <= original_base + original_count);
    return p;
}

static void give(struct Page *p, size_t n) {
    size_t before = nr_free_pages(), actual = p->property;
    free_pages(p, n);
    assert(nr_free_pages() == before + actual);
}

static void basic(void) {
    fixture(aligned_base, 64);
    size_t requests[] = {1, 2, 4, 8, 3, 5, 6};
    struct Page *live[7];
    for (size_t i = 0; i < 7; i++) {
        live[i] = take(requests[i]);
        for (size_t j = 0; j < i; j++) disjoint(live[i], live[j]);
        payload(live[i], 0x100 + i, 1);
    }
    for (size_t i = 0; i < 7; i++) {
        payload(live[i], 0x100 + i, 0);
        give(live[i], requests[i]);
    }
    assert(nr_free_pages() == 64);
    struct Page *p = take(3);
    give(p, 4);                  /* The explicit actual-capacity free contract. */
    buddy_validate();
    cprintf("[Buddy] basic, 3/5/6 rounding, payload, full recovery: PASS\n");
}

static void large_blocks(void) {
    size_t before = nr_free_pages();
    size_t request = npage - nbase >= (1UL << 15) ? 8193 : 4097;
    struct Page *small = take(513), *large = take(request);
    disjoint(small, large);
    buddy_validate();
    give(large, request);
    give(small, 513);
    assert(nr_free_pages() == before);
    buddy_validate();
    cprintf("[Buddy] real-arena high-order allocation and recovery: PASS\n");
}

static void split_and_merge(void) {
    fixture(aligned_base, 64);
    struct Page *p = take(1), *right = take(32);
    assert(p == aligned_base && right == aligned_base + 32);
    give(p, 1);
    give(right, 32);
    p = take(64);
    assert(p == aligned_base);
    give(p, 64);

    fixture(aligned_base, 16);
    struct Page *left = take(8);
    right = take(8);
    give(right, 8);
    assert(alloc_pages(16) == NULL && nr_free_pages() == 8);
    give(left, 8);
    p = take(16);
    assert(p == aligned_base);
    give(p, 16);
    buddy_validate();
    cprintf("[Buddy] high-order split, occupied buddy, recursive merge: PASS\n");
}

static void non_buddies(void) {
    fixture(aligned_base, 4);
    struct Page *p[4];
    for (size_t i = 0; i < 4; i++) {
        p[i] = take(1);
        assert(p[i] == aligned_base + i);
    }
    give(p[1], 1);
    give(p[2], 1);               /* Adjacent pages 1 and 2 are NOT buddies. */
    assert(nr_free_pages() == 2 && alloc_pages(2) == NULL);
    buddy_validate();
    give(p[0], 1);
    struct Page *pair = take(2);
    assert(pair == aligned_base);
    give(pair, 2);
    give(p[3], 1);
    pair = take(4);
    assert(pair == aligned_base);
    give(pair, 4);
    cprintf("[Buddy] adjacent non-buddies never merge: PASS\n");
}

static void exhaust(struct Page *base, size_t n) {
    fixture(base, n);
    struct Page *p[64];
    assert(n <= 64);
    assert(alloc_pages(n + 1) == NULL && nr_free_pages() == n);
    assert(alloc_pages(~(size_t)0) == NULL && nr_free_pages() == n);
    for (size_t i = 0; i < n; i++) {
        p[i] = take(1);
        assert(p[i] >= base && p[i] < base + n);
        for (size_t j = 0; j < i; j++) disjoint(p[i], p[j]);
    }
    assert(nr_free_pages() == 0 && alloc_pages(1) == NULL);
    for (size_t i = n; i > 0; i--) give(p[i - 1], 1);
    assert(nr_free_pages() == n);
    buddy_validate();
}

static void irregular(void) {
    exhaust(aligned_base, 64);
    struct Page *p = take(64);
    assert(p == aligned_base);
    give(p, 64);
    exhaust(aligned_base + 3, 13);
    exhaust(original_base + original_count - 1, 1);

    empty_fixture();
    add_range(aligned_base, 4);
    add_range(aligned_base + 8, 4);
    assert(alloc_pages(8) == NULL && nr_free_pages() == 8);
    add_range(aligned_base + 4, 4);
    p = take(8);                 /* Now the missing middle range is registered. */
    assert(p == aligned_base);
    give(p, 8);
    buddy_validate();
    cprintf("[Buddy] exhaustion, irregular range, last page, holes: PASS\n");
}

static uint32_t random_next(uint32_t *seed) {
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

static void stress(void) {
    fixture(aligned_base, 128);
    struct Page *live[32] = {NULL};
    size_t requests[32] = {0};
    uint32_t seed = 0x4e4b5532;
    size_t expected = 128;
    for (size_t step = 0; step < 2000; step++) {
        size_t slot = random_next(&seed) % 32;
        uint64_t tag = 0x12340000 + slot;
        if (live[slot]) {
            payload(live[slot], tag, 0);
            expected += live[slot]->property;
            give(live[slot], requests[slot]);
            live[slot] = NULL;
        } else {
            requests[slot] = random_next(&seed) % 12 + 1;
            struct Page *p = alloc_pages(requests[slot]);
            if (p) {
                assert(p->property == capacity(requests[slot]));
                expected -= p->property;
                for (size_t j = 0; j < 32; j++) if (live[j]) disjoint(p, live[j]);
                payload(p, tag, 1);
                live[slot] = p;
            }
        }
        assert(nr_free_pages() == expected);
        if ((step % 32) == 0) buddy_validate();
    }
    for (size_t i = 0; i < 32; i++) if (live[i]) {
        payload(live[i], 0x12340000 + i, 0);
        give(live[i], requests[i]);
    }
    assert(nr_free_pages() == 128);
    struct Page *whole = take(128);
    assert(whole == aligned_base);
    give(whole, 128);
    buddy_validate();
    cprintf("[Buddy] 2000 deterministic mixed operations, payload integrity: PASS\n");
}

static void negative(void) {
#if LAB2_BUDDY_NEGATIVE != 0
    fixture(aligned_base, 16);
    struct Page *p = take(3);
    cprintf("[Buddy negative %d] BEGIN\n", LAB2_BUDDY_NEGATIVE);
#if LAB2_BUDDY_NEGATIVE == 1
    give(p, 3);
    free_pages(p, 3);            /* Duplicate free. */
#elif LAB2_BUDDY_NEGATIVE == 2
    free_pages(p + 1, 1);        /* Interior pointer / partial free. */
#elif LAB2_BUDDY_NEGATIVE == 3
    free_pages(p, 2);            /* Wrong size. */
#elif LAB2_BUDDY_NEGATIVE == 4
    free_pages(pages, 1);        /* OpenSBI descriptor remains reserved. */
#elif LAB2_BUDDY_NEGATIVE == 5
    free_pages((struct Page *)((uintptr_t)p + 1), 3); /* Misaligned descriptor. */
#elif LAB2_BUDDY_NEGATIVE == 6
    buddy_pmm_manager.init_memmap(p + 3, 1); /* Re-register a page Buddy already owns. */
#else
#error Unknown LAB2_BUDDY_NEGATIVE
#endif
    panic("Buddy negative test unexpectedly returned");
#endif
}

void buddy_test(void) {
    assert(pmm_manager == &buddy_pmm_manager);
    size_t count = npage - nbase, first = 0;
    while (first < count && PageReserved(pages + first)) first++;
    assert(first < count);
    original_base = pages + first;
    original_count = count - first;
    fixture_count = nr_free_pages();
    assert(fixture_count == original_count); /* Fresh, single free interval. */
    for (size_t i = first; i < count; i++) assert(!PageReserved(pages + i));
    aligned_base = pa2page(ROUNDUP(page2pa(original_base), 256 * PGSIZE));
    assert(aligned_base + 256 <= original_base + original_count);
    large_blocks();
    basic();
    split_and_merge();
    non_buddies();
    irregular();
    stress();
    fixture(original_base, original_count);
    assert(nr_free_pages() == original_count);
    buddy_validate();
    cprintf("[Buddy] original boot-time free interval restored: PASS (%lu pages)\n",
            original_count);
    cprintf("[Buddy] ALL NORMAL TESTS PASS\n");
    negative();
}
#endif
