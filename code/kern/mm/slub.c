#include <slub.h>
#include <pmm.h>
#include <mmu.h>
#include <memlayout.h>

//lab2 challenge SLUB 2412096
#define SLUB_ALIGN              16
#define SLUB_MAGIC              0x534C5542UL
#define SLUB_CACHE_COUNT        7

struct slub_cache;

struct slub_slab {
    struct slub_slab *next;
    struct slub_slab *prev;
    struct slub_cache *cache;
    struct Page *page;
    void *free_list;
    size_t capacity;
    size_t inuse;
    uint64_t magic;
};

struct slub_cache {
    size_t object_size;
    struct slub_slab *partial;
};

static const size_t slub_sizes[SLUB_CACHE_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024,
};
static struct slub_cache slub_caches[SLUB_CACHE_COUNT];
static int slub_ready;

static size_t
slub_data_offset(void) {
    return ROUNDUP(sizeof(struct slub_slab), SLUB_ALIGN);
}

static void *
slub_page_kva(struct Page *page) {
    // 物理页描述符转换为内核虚拟地址。
    return (void *)(page2pa(page) + va_pa_offset);
}

static struct slub_cache *
slub_find_cache(size_t size) {
    size_t i;
    for (i = 0; i < SLUB_CACHE_COUNT; i ++) {
        if (size <= slub_caches[i].object_size) {
            return &slub_caches[i];
        }
    }
    return NULL;
}

static void
slub_add_partial(struct slub_cache *cache, struct slub_slab *slab) {
    slab->prev = NULL;
    slab->next = cache->partial;
    if (cache->partial != NULL) {
        cache->partial->prev = slab;
    }
    cache->partial = slab;
}

static void
slub_remove_partial(struct slub_cache *cache, struct slub_slab *slab) {
    if (slab->prev != NULL) {
        slab->prev->next = slab->next;
    } else {
        cache->partial = slab->next;
    }
    if (slab->next != NULL) {
        slab->next->prev = slab->prev;
    }
    slab->next = slab->prev = NULL;
}

static struct slub_slab *
slub_new_slab(struct slub_cache *cache) {
    struct Page *page = alloc_page();
    if (page == NULL) {
        return NULL;
    }

    struct slub_slab *slab = (struct slub_slab *)slub_page_kva(page);
    size_t offset = slub_data_offset();
    size_t capacity = (PGSIZE - offset) / cache->object_size;
    if (capacity == 0) {
        free_page(page);
        return NULL;
    }

    slab->next = slab->prev = NULL;
    slab->cache = cache;
    slab->page = page;
    slab->free_list = NULL;
    slab->capacity = capacity;
    slab->inuse = 0;
    slab->magic = SLUB_MAGIC;

    // 每个空闲对象的开头保存下一空闲对象指针。
    uintptr_t slot = (uintptr_t)slab + offset;
    size_t i;
    for (i = 0; i < capacity; i ++) {
        *(void **)slot = slab->free_list;
        slab->free_list = (void *)slot;
        slot += cache->object_size;
    }
    slub_add_partial(cache, slab);
    return slab;
}

void
slub_init(void) {
    if (slub_ready) {
        return;
    }

    size_t i;
    for (i = 0; i < SLUB_CACHE_COUNT; i ++) {
        slub_caches[i].object_size = slub_sizes[i];
        slub_caches[i].partial = NULL;
    }
    slub_ready = 1;
}

void *
slub_alloc(size_t size) {
    if (!slub_ready || size == 0) {
        return NULL;
    }

    struct slub_cache *cache = slub_find_cache(size);
    if (cache == NULL) {
        return NULL;
    }

    struct slub_slab *slab = cache->partial;
    if (slab == NULL) {
        slab = slub_new_slab(cache);
        if (slab == NULL) {
            return NULL;
        }
    }

    void *obj = slab->free_list;
    slab->free_list = *(void **)obj;
    slab->inuse ++;
    if (slab->free_list == NULL) {
        slub_remove_partial(cache, slab);
    }
    return obj;
}

static int
slub_object_is_free(struct slub_slab *slab, void *obj) {
    void *entry = slab->free_list;
    while (entry != NULL) {
        if (entry == obj) {
            return 1;
        }
        entry = *(void **)entry;
    }
    return 0;
}

void
slub_free(void *obj) {
    if (obj == NULL || !slub_ready) {
        return;
    }

    struct slub_slab *slab =
        (struct slub_slab *)ROUNDDOWN((uintptr_t)obj, PGSIZE);
    if (slab->magic != SLUB_MAGIC || slab->cache == NULL) {
        return;
    }

    uintptr_t first = (uintptr_t)slab + slub_data_offset();
    uintptr_t address = (uintptr_t)obj;
    size_t bytes = slab->capacity * slab->cache->object_size;
    if (address < first || address >= first + bytes ||
        (address - first) % slab->cache->object_size != 0 ||
        slub_object_is_free(slab, obj)) {
        return;
    }

    if (slab->free_list == NULL) {
        slub_add_partial(slab->cache, slab);
    }
    *(void **)obj = slab->free_list;
    slab->free_list = obj;
    slab->inuse --;

    if (slab->inuse == 0) {
        struct Page *page = slab->page;
        slub_remove_partial(slab->cache, slab);
        slab->magic = 0;
        free_page(page);
    }
}
