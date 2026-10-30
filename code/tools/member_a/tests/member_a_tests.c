#include <assert.h>
#include <best_fit_pmm.h>
#include <dtb.h>
#include <intr.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include <../sync/sync.h>

/* Isolated test-build code. Never linked into the submitted default kernel. */
static struct Page arena[64];
static unsigned char used[64];
static const struct pmm_manager *const manager = &best_fit_pmm_manager;
extern void lab3_a_context_probe(uintptr_t *result);

static void reset_arena(void) {
    manager->init();
    memset(arena, 0, sizeof(arena));
    for (size_t i = 0; i < 64; ++i) {
        SetPageReserved(&arena[i]);
        arena[i].property = 99;
        set_page_ref(&arena[i], 7);
    }
}

static void check_blocks(const unsigned *heads, const unsigned *lengths,
                         size_t blocks, size_t total) {
    assert(manager->nr_free_pages() == total);
    size_t seen = 0, sum = 0;
    list_entry_t *sentinel = NULL, *node = NULL;
    if (blocks) {
        sentinel = list_prev(&arena[heads[0]].page_link);
        node = list_next(sentinel);
        assert(list_prev(sentinel) == &arena[heads[blocks - 1]].page_link);
    }
    for (size_t i = 0; i < 64; ++i) {
        if (!PageReserved(&arena[i])) {
            assert(page_ref(&arena[i]) == 0);
        }
        if (PageProperty(&arena[i])) {
            assert(seen < blocks);
            assert(heads[seen] == i && lengths[seen] == arena[i].property);
            assert(node == &arena[i].page_link);
            assert(list_prev(list_next(node)) == node);
            assert(list_next(list_prev(node)) == node);
            node = list_next(node);
            sum += arena[i].property;
            ++seen;
        }
    }
    assert(seen == blocks && sum == total);
    if (blocks) {
        assert(node == sentinel);
    }
}

static void check_shadow(void) {
    unsigned heads[64], lengths[64];
    size_t blocks = 0, total = 0;
    for (size_t i = 0; i < 64;) {
        assert(!PageReserved(&arena[i]) && page_ref(&arena[i]) == 0);
        if (used[i]) {
            assert(!PageProperty(&arena[i]) && arena[i].property == 0);
            ++i;
            continue;
        }
        size_t start = i;
        while (i < 64 && !used[i]) {
            if (i != start) {
                assert(!PageProperty(&arena[i]) && arena[i].property == 0);
            }
            ++i;
        }
        heads[blocks] = start;
        lengths[blocks++] = i - start;
        total += i - start;
    }
    check_blocks(heads, lengths, blocks, total);
}

static unsigned next_random(unsigned *state) {
    unsigned x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

void lab3_a_allocator_tests(void) {
    reset_arena();
    assert(manager->alloc_pages(1) == NULL);
    manager->init_memmap(arena + 40, 4);
    manager->init_memmap(arena + 8, 8);
    manager->init_memmap(arena + 24, 6);
    manager->init_memmap(arena + 56, 4);
    unsigned heads[] = {8, 24, 40, 56}, lengths[] = {8, 6, 4, 4};
    check_blocks(heads, lengths, 4, 22);
    assert(manager->alloc_pages(23) == NULL);
    assert(manager->alloc_pages(9) == NULL);
    struct Page *p = manager->alloc_pages(4);
    assert(p == arena + 40); /* exact match, equal-size lower-address tie */
    manager->free_pages(p, 4);
    p = manager->alloc_pages(5);
    assert(p == arena + 24 && !PageProperty(p) && p->property == 0);
    assert(PageProperty(arena + 29) && arena[29].property == 1);
    manager->free_pages(p, 5);
    check_blocks(heads, lengths, 4, 22);
    cprintf("A_ALLOC: initialization/order/exact/tie/split/fragmentation PASS\n");

    reset_arena();
    manager->init_memmap(arena, 20);
    assert(manager->alloc_pages(20) == arena);
    assert(manager->nr_free_pages() == 0 && manager->alloc_pages(1) == NULL);
    manager->free_pages(arena + 4, 4);
    manager->free_pages(arena + 12, 4);
    manager->free_pages(arena + 8, 4); /* both neighbours */
    unsigned h[] = {4}, len[] = {12};
    check_blocks(h, len, 1, 12);
    manager->free_pages(arena, 4); /* right neighbour only */
    h[0] = 0; len[0] = 16;
    check_blocks(h, len, 1, 16);
    manager->free_pages(arena + 16, 4); /* left neighbour only */
    len[0] = 20;
    check_blocks(h, len, 1, 20);
    assert(manager->alloc_pages(20) == arena);
    manager->free_pages(arena, 20); /* insert into empty list */
    check_blocks(h, len, 1, 20);
    cprintf("A_ALLOC: empty/no-neighbour/left/right/both coalescing PASS\n");

    for (unsigned seed = 1; seed <= 3; ++seed) {
        reset_arena();
        manager->init_memmap(arena, 64);
        memset(used, 0, sizeof(used));
        struct { unsigned start, length; } live[64];
        size_t live_count = 0;
        unsigned state = seed * 0x9e3779b9U;
        for (unsigned step = 0; step < 1000; ++step) {
            if (live_count && (next_random(&state) & 1)) {
                size_t k = next_random(&state) % live_count;
                unsigned start = live[k].start, n = live[k].length;
                manager->free_pages(arena + start, n);
                memset(used + start, 0, n);
                live[k] = live[--live_count];
            } else {
                unsigned request = 1 + next_random(&state) % 12;
                int best = -1;
                unsigned best_length = 65;
                for (unsigned i = 0; i < 64;) {
                    if (used[i]) { ++i; continue; }
                    unsigned start = i;
                    while (i < 64 && !used[i]) { ++i; }
                    unsigned n = i - start;
                    if (n >= request && n < best_length) {
                        best = start;
                        best_length = n;
                    }
                }
                p = manager->alloc_pages(request);
                assert(p == (best < 0 ? NULL : arena + best));
                if (p) {
                    live[live_count].start = best;
                    live[live_count++].length = request;
                    memset(used + best, 1, request);
                }
            }
            check_shadow();
        }
        while (live_count) {
            --live_count;
            manager->free_pages(arena + live[live_count].start, live[live_count].length);
            memset(used + live[live_count].start, 0, live[live_count].length);
        }
        check_shadow();
        assert(manager->nr_free_pages() == 64);
    }
    cprintf("A_ALLOC: 3000 deterministic operations vs bitmap oracle PASS\n");
    /* pmm_init() subsequently resets this isolated allocator state. */
}

static void check_page_init(void) {
    extern char end[];
    uintptr_t reported_end = get_memory_base() + get_memory_size();
    uintptr_t managed_end = reported_end < PHYSICAL_MEMORY_END
                              ? reported_end : PHYSICAL_MEMORY_END;
    managed_end = ROUNDDOWN(managed_end, PGSIZE);
    assert(npage * PGSIZE == managed_end);
    assert((uintptr_t)pages % PGSIZE == 0);
    assert(PADDR(pages) >= PADDR((uintptr_t)end));
    size_t count = npage - nbase;
    uintptr_t free_start = ROUNDUP(PADDR(pages) + count * sizeof(struct Page), PGSIZE);
    assert(nr_free_pages() == (managed_end - free_start) / PGSIZE);
    for (size_t i = 0; i < count; ++i) {
        assert(PageReserved(pages + i) == (page2pa(pages + i) < free_start));
    }
    cprintf("A_PAGE_INIT: managed_end=0x%016lx free_start=0x%016lx pages=%lu PASS\n",
            managed_end, free_start, nr_free_pages());
}

static void check_irq_wrappers(void) {
    uintptr_t before = read_csr(sstatus);
    bool flag;
    intr_disable();
    if (1) local_intr_save(flag); else assert(0);
    assert(flag == 0 && !(read_csr(sstatus) & SSTATUS_SIE));
    local_intr_restore(flag);
    assert(!(read_csr(sstatus) & SSTATUS_SIE));
    intr_enable();
    local_intr_save(flag);
    assert(flag == 1 && !(read_csr(sstatus) & SSTATUS_SIE));
    local_intr_restore(flag);
    assert(read_csr(sstatus) & SSTATUS_SIE);
    size_t count = nr_free_pages();
    struct Page *p = alloc_page();
    assert(p && (read_csr(sstatus) & SSTATUS_SIE));
    free_page(p);
    assert(nr_free_pages() == count && (read_csr(sstatus) & SSTATUS_SIE));
    intr_disable();
    p = alloc_page();
    assert(p && !(read_csr(sstatus) & SSTATUS_SIE));
    free_page(p);
    assert(nr_free_pages() == count && !(read_csr(sstatus) & SSTATUS_SIE));
    if (before & SSTATUS_SIE) { intr_enable(); }
    cprintf("A_IRQ: do-while macro and alloc/free preserve prior SIE=0/1 PASS\n");
}

__attribute__((noinline, noreturn)) void lab3_a_test_done(void) {
    cprintf("LAB3_A_ALL_TESTS_PASS\n");
    sbi_shutdown();
    while (1) { }
}

void lab3_a_after_pmm(void) {
    check_page_init();
    check_irq_wrappers();
    uintptr_t result[35];
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        lab3_a_context_probe(result);
        for (unsigned i = 0; i < 32; ++i) {
            uintptr_t expected = i == 0 ? 0 : i == 2 ? result[32] : 0x100 + i;
            assert(result[i] == expected);
        }
        assert((result[33] & SSTATUS_SIE) == (result[34] & SSTATUS_SIE));
        assert((result[33] & SSTATUS_SPP) == 0);
        /* QEMU 4.1.1 clears SPIE on sret; newer implementations set it. */
        cprintf("A_CONTEXT: 32 GPR slots/original SP/sret status round %u PASS\n", repeat + 1);
    }
    lab3_a_test_done();
}
