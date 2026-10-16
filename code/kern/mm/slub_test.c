#include <assert.h>
#include <pmm.h>
#include <slub.h>
#include <stdio.h>

#define SLUB_TEST_ALIGN 16

static size_t
slub_test_class_size(size_t size) {
    static const size_t classes[] = {16, 32, 64, 128, 256, 512, 1024};
    size_t i;
    for (i = 0; i < sizeof(classes) / sizeof(classes[0]); i ++) {
        if (size <= classes[i]) {
            return classes[i];
        }
    }
    return 0;
}

static void
slub_test_sizes(void) {
    static const size_t sizes[] = {8, 24, 32, 64, 100, 128, 512, 513, 1024};
    void *objects[sizeof(sizes) / sizeof(sizes[0])];
    size_t i, j;

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i ++) {
        objects[i] = slub_alloc(sizes[i]);
        assert(objects[i] != NULL);
        assert(((uintptr_t)objects[i] % SLUB_TEST_ALIGN) == 0);
    }

    // 同时分配的对象必须没有重叠。
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i ++) {
        uintptr_t begin_i = (uintptr_t)objects[i];
        uintptr_t end_i = begin_i + slub_test_class_size(sizes[i]);
        for (j = i + 1; j < sizeof(sizes) / sizeof(sizes[0]); j ++) {
            uintptr_t begin_j = (uintptr_t)objects[j];
            uintptr_t end_j = begin_j + slub_test_class_size(sizes[j]);
            assert(end_i <= begin_j || end_j <= begin_i);
        }
    }

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i ++) {
        slub_free(objects[i]);
    }
}

static void
slub_test_reuse(void) {
    void *a = slub_alloc(32);
    void *b = slub_alloc(32);
    void *c = slub_alloc(32);
    void *d = slub_alloc(32);
    assert(a != NULL && b != NULL && c != NULL && d != NULL);
    assert(a != b && a != c && a != d && b != c && b != d && c != d);

    // 交替释放两个对象后，空闲槽应以链表顺序被再次使用。
    slub_free(b);
    slub_free(d);
    void *reused_d = slub_alloc(32);
    void *reused_b = slub_alloc(32);
    assert(reused_d == d && reused_b == b);

    slub_free(a);
    slub_free(c);
    slub_free(reused_d);
    slub_free(reused_b);
}

static void
slub_test_page_reclaim(void) {
    size_t before = nr_free_pages();
    void *obj = slub_alloc(1024);
    assert(obj != NULL);
    assert(nr_free_pages() + 1 == before);

    slub_free(obj);
    assert(nr_free_pages() == before);
}

#define SLUB_TEST_MAX_1024_OBJECTS_PER_PAGE (PGSIZE / 1024)

static void
slub_test_memory_exhaustion(void) {
    size_t before = nr_free_pages();
    size_t page_count = 0, object_count = 0;
    size_t available_pages;
    struct Page *held_pages = NULL;
    struct Page *page;
    void *held_objects = NULL;
    int pages_exhausted, reused_slot = 0, objects_exhausted = 0, pages_restored;

    // 先留下一个已有 SLUB 页，验证物理页耗尽后仍可复用其中的槽位。
    void *object = slub_alloc(1024);
    assert(object != NULL);
    *(void **)object = held_objects;
    held_objects = object;
    object_count ++;
    available_pages = nr_free_pages();

    // 有上限地占用其余全部空闲物理页。
    while (page_count < available_pages && (page = alloc_page()) != NULL) {
        page->page_link.next = held_pages == NULL ? NULL : &(held_pages->page_link);
        held_pages = page;
        page_count ++;
    }
    pages_exhausted = page_count == available_pages && nr_free_pages() == 0;

    // 先复用已有槽位；超过一页的最大槽位数后必须分配失败。
    while (object_count <= SLUB_TEST_MAX_1024_OBJECTS_PER_PAGE) {
        object = slub_alloc(1024);
        if (object == NULL) {
            objects_exhausted = 1;
            break;
        }
        reused_slot = 1;
        *(void **)object = held_objects;
        held_objects = object;
        object_count ++;
    }

    // 先释放对象，再归还临时占用的物理页。
    while (held_objects != NULL) {
        void *next = *(void **)held_objects;
        slub_free(held_objects);
        held_objects = next;
    }
    while (held_pages != NULL) {
        list_entry_t *next = held_pages->page_link.next;
        free_page(held_pages);
        held_pages = next == NULL ? NULL : le2page(next, page_link);
    }
    pages_restored = nr_free_pages() == before;

    assert(pages_exhausted);
    assert(reused_slot);
    assert(objects_exhausted);
    assert(pages_restored);
}

void
slub_test(void) {
    size_t before = nr_free_pages();

    assert(slub_alloc(0) == NULL);
    assert(slub_alloc(1025) == NULL);
    slub_test_sizes();
    slub_test_reuse();
    slub_test_page_reclaim();
    slub_test_memory_exhaustion();
    assert(nr_free_pages() == before);

    cprintf("slub_check() succeeded!\n");
}
