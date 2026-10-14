#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>
#ifdef LAB2_BUDDY_TEST
#include <buddy_pmm.h>
#include <buddy_test.h>
#endif
#ifdef LAB2_PAGE_INIT_TEST
#include <page_init_test.h>
#endif

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#ifdef LAB2_BUDDY_TEST
    pmm_manager = &buddy_pmm_manager;
#elif defined(LAB2_PAGE_INIT_TEST)
    pmm_manager = &default_pmm_manager;
#else
    /* Team integration switches only this standard-build choice to Best-Fit
     * after its implementation arrives; the A-only profiles stay independent.
     */
    pmm_manager = &default_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 YOUR CODE
static void page_init(void) {
    extern char end[];
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();

    /* The boot mapping is fixed by entry.S, independently of the DTB base. */
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    if (mem_size == 0 || mem_base < DRAM_BASE ||
        mem_base > KERNEL_BEGIN_PADDR || mem_size > ~(uint64_t)0 - mem_base) {
        panic("page_init: invalid DTB memory range");
    }
    uint64_t reported_end = mem_base + mem_size;
    uintptr_t managed_end = reported_end < PHYSICAL_MEMORY_END
                                ? reported_end : PHYSICAL_MEMORY_END;
    managed_end = ROUNDDOWN(managed_end, PGSIZE);
    uintptr_t kernel_end = PADDR((uintptr_t)end);
    if (kernel_end < KERNEL_BEGIN_PADDR || kernel_end >= managed_end) {
        panic("page_init: memory does not contain the kernel");
    }

    /* npage is the exclusive physical PPN bound, not the descriptor count. */
    npage = PPN(managed_end);
    size_t count = npage - nbase;
    uintptr_t pages_pa = ROUNDUP(kernel_end, PGSIZE);
    if (pages_pa > managed_end ||
        count > (managed_end - pages_pa) / sizeof(struct Page)) {
        panic("page_init: no room for page descriptors");
    }
    pages = (struct Page *)(pages_pa + va_pa_offset);
    for (size_t i = 0; i < count; i++) {
        pages[i].flags = 0;
        SetPageReserved(&pages[i]);
        pages[i].property = 0;
        set_page_ref(&pages[i], 0);
        list_init(&pages[i].page_link);
    }

    uintptr_t descriptor_end = pages_pa + count * sizeof(struct Page);
    uintptr_t free_start = ROUNDUP(descriptor_end, PGSIZE);
    cprintf("PMM: DTB [0x%016lx, 0x%016lx), managed end 0x%016lx\n",
            mem_base, reported_end, managed_end);
    cprintf("PMM: descriptors [0x%016lx, 0x%016lx), free [0x%016lx, 0x%016lx)\n",
            pages_pa, descriptor_end, free_start, managed_end);
    /* Leave the empty manager intact when no complete free page remains. */
    if (free_start < managed_end) {
        init_memmap(pa2page(free_start), (managed_end - free_start) / PGSIZE);
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
#ifdef LAB2_PAGE_INIT_TEST
    page_init_test();
#endif
#ifdef LAB2_BUDDY_TEST
    buddy_test();
#endif
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
