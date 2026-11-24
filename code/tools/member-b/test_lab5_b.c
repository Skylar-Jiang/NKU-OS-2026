/* Member B (2413507) kernel-side tests for load_icode/do_execve/do_exit/
 * do_wait/do_kill. Injected by tools/member-b/run.sh at the entry of
 * init_main in a temporary copy of the tree; never built into the real
 * kernel. Covers paths the official user programs cannot reach: malformed
 * ELF images, allocation failure in the middle of loading, exec replacing a
 * live address space whose image lives in user memory, and the resulting
 * page accounting. */
#include <defs.h>
#include <proc.h>
#include <vmm.h>
#include <pmm.h>
#include <elf.h>
#include <error.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <riscv.h>
#include <unistd.h>
#include <sched.h>

extern unsigned char _binary_obj___user_hello_out_start[], _binary_obj___user_hello_out_size[];
extern unsigned char _binary_obj___user_testbss_out_start[], _binary_obj___user_testbss_out_size[];
extern unsigned char _binary_obj___user_exit_out_start[], _binary_obj___user_exit_out_size[];

#define HELLO_BIN _binary_obj___user_hello_out_start
#define HELLO_SIZE ((size_t)_binary_obj___user_hello_out_size)
#define BSS_BIN _binary_obj___user_testbss_out_start
#define BSS_SIZE ((size_t)_binary_obj___user_testbss_out_size)
#define EXIT_BIN _binary_obj___user_exit_out_start
#define EXIT_SIZE ((size_t)_binary_obj___user_exit_out_size)

static int b_pass, b_fail;

#define B_CHECK(cond, ...)                         \
    do {                                           \
        if (cond) {                                \
            b_pass++;                              \
            cprintf("[B] PASS ");                  \
        } else {                                   \
            b_fail++;                              \
            cprintf("[B] FAIL ");                  \
        }                                          \
        cprintf(__VA_ARGS__);                      \
        cprintf("\n");                             \
    } while (0)

static void get_ph(unsigned char *img, int i, struct proghdr *ph)
{
    struct elfhdr eh;
    memcpy(&eh, img, sizeof(eh));
    memcpy(ph, img + eh.e_phoff + i * sizeof(*ph), sizeof(*ph));
}

static void put_ph(unsigned char *img, int i, struct proghdr *ph)
{
    struct elfhdr eh;
    memcpy(&eh, img, sizeof(eh));
    memcpy(img + eh.e_phoff + i * sizeof(*ph), ph, sizeof(*ph));
}

/* The current kernel thread must be untouched after a failed exec. */
static bool image_untouched(void)
{
    return current->mm == NULL && current->pgdir == boot_pgdir_pa &&
           (read_csr(satp) & 0xFFFFFFFFFFFULL) == (boot_pgdir_pa >> PGSHIFT);
}

enum { M_MAGIC, M_TRUNC, M_PHENT, M_PHOFF, M_PHNUM, M_FILESZ, M_OFFSET,
       M_KVA, M_WRAP, M_ENTRY, M_STACK, M_OVERLAP, M_NOLOAD, M_COUNT };
static const char *mut_name[M_COUNT] = {
    "bad magic", "truncated header", "bad e_phentsize", "e_phoff past end",
    "program headers past end", "p_filesz > p_memsz", "p_offset overflow",
    "segment in kernel space", "segment address wraps", "entry outside text",
    "segment overlaps user stack", "segment covers earlier segment",
    "no PT_LOAD segment",
};

static size_t mutate(unsigned char *img, size_t size, int m)
{
    struct elfhdr eh;
    struct proghdr ph;
    memcpy(&eh, img, sizeof(eh));
    get_ph(img, 0, &ph);
    switch (m) {
    case M_MAGIC: eh.e_magic ^= 0x0000ff00; break;
    case M_TRUNC: return sizeof(struct elfhdr) - 1;
    case M_PHENT: eh.e_phentsize = sizeof(struct proghdr) + 8; break;
    case M_PHOFF: eh.e_phoff = size; break;
    case M_PHNUM: eh.e_phoff = size - sizeof(struct proghdr) / 2; break;
    case M_FILESZ: ph.p_filesz = ph.p_memsz + 1; break;
    case M_OFFSET: ph.p_offset = (uint64_t)-16; break;
    case M_KVA: ph.p_va = KERNBASE; break;
    case M_WRAP: ph.p_va = (uint64_t)-PGSIZE; ph.p_memsz = 2 * PGSIZE; break;
    case M_ENTRY: eh.e_entry = 0x10; break;
    case M_STACK: ph.p_va = USTACKTOP - PGSIZE; eh.e_entry = ph.p_va; break;
    case M_OVERLAP: {
        /* Second segment starts below the first and covers it. */
        struct proghdr ph1;
        get_ph(img, 1, &ph1);
        ph1.p_va = ph.p_va - PGSIZE;
        ph1.p_memsz = 3 * PGSIZE;
        put_ph(img, 1, &ph1);
        break;
    }
    case M_NOLOAD:
        for (int i = 0; i < eh.e_phnum; i++) {
            get_ph(img, i, &ph);
            ph.p_type = 0;
            put_ph(img, i, &ph);
        }
        get_ph(img, 0, &ph);
        break;
    }
    memcpy(img, &eh, sizeof(eh));
    if (m != M_NOLOAD)
        put_ph(img, 0, &ph);
    return size;
}

static void test_bad_elf(void)
{
    /* exit has two PT_LOAD segments (text, data). */
    size_t size = EXIT_SIZE;
    unsigned char *img = kmalloc(size);
    assert(img != NULL);
    /* Warm SLOB with one failing load so page counts below are exact. */
    memcpy(img, EXIT_BIN, size);
    mutate(img, size, M_ENTRY);
    do_execve("warm", 4, img, size);

    for (int m = 0; m < M_COUNT; m++) {
        memcpy(img, EXIT_BIN, size);
        size_t sz = mutate(img, size, m);
        size_t before = nr_free_pages();
        int ret = do_execve("bad", 3, img, sz);
        B_CHECK(ret < 0 && image_untouched() && nr_free_pages() == before,
                "malformed ELF (%s): ret=%d, image intact, pages %d->%d",
                mut_name[m], ret, (int)before, (int)nr_free_pages());
    }
    kfree(img);

    int ret = do_execve(NULL, 0, HELLO_BIN, HELLO_SIZE);
    B_CHECK(ret == -E_INVAL, "do_execve(NULL name) = %d", ret);
    ret = do_execve("x", 1, NULL, 100);
    B_CHECK(ret == -E_INVAL, "do_execve(NULL binary) = %d", ret);
    ret = do_execve("x", 1, HELLO_BIN, 0);
    B_CHECK(ret == -E_INVAL, "do_execve(size 0) = %d", ret);
}

/* Leave only `keep` free pages, try to load testbss (~1030 pages) and check
 * that every partially built resource is returned. */
static void test_oom(void)
{
    static const size_t keeps[] = {0, 1, 2, 3, 6, 20, 300};
    struct { struct Page *p; size_t n; } held[64];
    for (int k = 0; k < (int)(sizeof(keeps) / sizeof(keeps[0])); k++) {
        int nheld = 0;
        size_t want;
        while ((want = nr_free_pages()) > keeps[k] && nheld < 64) {
            want -= keeps[k];
            struct Page *p = NULL;
            while (want > 0 && (p = alloc_pages(want)) == NULL)
                want /= 2;
            if (p == NULL)
                break;
            held[nheld].p = p;
            held[nheld++].n = want;
        }
        size_t before = nr_free_pages();
        int ret = do_execve("testbss", 7, BSS_BIN, BSS_SIZE);
        size_t after = nr_free_pages();
        for (int i = 0; i < nheld; i++)
            free_pages(held[i].p, held[i].n);
        B_CHECK(ret == -E_NO_MEM && image_untouched() && after == before,
                "exec with %d free pages: ret=%d, image intact, pages %d->%d",
                (int)before, ret, (int)before, (int)after);
    }
}

static uintptr_t pte_of(uintptr_t va)
{
    pte_t *pte = get_pte(current->mm->pgdir, va, 0);
    return (pte == NULL) ? 0 : *pte;
}

static int exec_child(void *arg)
{
    struct elfhdr eh;
    struct proghdr text;
    memcpy(&eh, BSS_BIN, sizeof(eh));
    get_ph(BSS_BIN, 0, &text);

    /* Case 1: kernel thread without mm loads testbss. */
    int ret = do_execve("testbss-and-a-very-long-name", 28, BSS_BIN, BSS_SIZE);
    B_CHECK(ret == 0, "kernel thread exec testbss: ret=%d", ret);
    if (ret != 0)
        return -1;
    struct mm_struct *mm = current->mm;
    B_CHECK(mm != NULL && mm_count(mm) == 1 && current->pgdir == PADDR(mm->pgdir) &&
                (read_csr(satp) & 0xFFFFFFFFFFFULL) == (current->pgdir >> PGSHIFT),
            "new mm installed: mm_count=1, pgdir and satp match");
    B_CHECK(strcmp(current->name, "testbss-and-a-v") == 0 &&
                current->name[PROC_NAME_LEN] == '\0',
            "name truncated to PROC_NAME_LEN: \"%s\"", current->name);

    struct trapframe *tf = current->tf;
    B_CHECK(tf->epc == eh.e_entry && tf->gpr.sp == USTACKTOP &&
                !(tf->status & SSTATUS_SPP) && (tf->status & SSTATUS_SPIE) &&
                !(tf->status & SSTATUS_SIE),
            "trapframe: epc=0x%lx sp=0x%lx SPP=0 SPIE=1 SIE=0",
            (unsigned long)tf->epc, (unsigned long)tf->gpr.sp);

    struct vma_struct *tv = find_vma(mm, text.p_va);
    uintptr_t tpte = pte_of(text.p_va);
    B_CHECK(tv != NULL && (tv->vm_flags & (VM_READ | VM_EXEC)) == (VM_READ | VM_EXEC) &&
                !(tv->vm_flags & VM_WRITE) && (tpte & (PTE_R | PTE_X | PTE_U | PTE_V)) ==
                (PTE_R | PTE_X | PTE_U | PTE_V) && !(tpte & PTE_W),
            "text segment R-X in VMA and PTE (pte=0x%lx)", (unsigned long)tpte);
    B_CHECK(memcmp((void *)text.p_va, BSS_BIN + text.p_offset, text.p_filesz) == 0 &&
                *(uint8_t *)(ROUNDDOWN(text.p_va, PGSIZE)) == 0,
            "text bytes copied at unaligned va 0x%lx, page head zeroed",
            (unsigned long)text.p_va);

    struct proghdr bss;
    get_ph(BSS_BIN, 1, &bss);
    struct vma_struct *bv = find_vma(mm, bss.p_va);
    bool zero = 1;
    for (uintptr_t va = bss.p_va; va < bss.p_va + bss.p_memsz; va += PGSIZE / 4)
        zero = zero && *(uint32_t *)va == 0;
    zero = zero && *(uint32_t *)(bss.p_va + bss.p_memsz - 4) == 0;
    uintptr_t bpte = pte_of(bss.p_va + bss.p_memsz - 1);
    B_CHECK(bv != NULL && (bv->vm_flags & VM_WRITE) && !(bv->vm_flags & VM_EXEC) &&
                (bpte & PTE_W) && !(bpte & PTE_X) && zero,
            "BSS RW-, %d bytes zero through the last page", (int)bss.p_memsz);

    struct vma_struct *sv = find_vma(mm, USTACKTOP - 1);
    bool stack_ok = sv != NULL && (sv->vm_flags & VM_STACK) &&
                    sv->vm_start == USTACKTOP - USTACKSIZE;
    for (int i = 1; i <= 4; i++)
        stack_ok = stack_ok && (pte_of(USTACKTOP - i * PGSIZE) & (PTE_V | PTE_W | PTE_U)) ==
                                   (PTE_V | PTE_W | PTE_U);
    stack_ok = stack_ok && !(pte_of(USTACKTOP - 5 * PGSIZE) & PTE_V);
    B_CHECK(stack_ok, "user stack VMA [0x%lx,0x%lx), top 4 pages RW, below unmapped",
            (unsigned long)(USTACKTOP - USTACKSIZE), (unsigned long)USTACKTOP);

    /* Case 2: exec while an mm exists, image and name in the old user
     * memory (the testbss BSS): the old mm must stay readable until the new
     * one is built, then be destroyed. */
    unsigned char *uimg = (unsigned char *)bss.p_va + PGSIZE;
    char *uname = (char *)bss.p_va;
    memcpy(uimg, HELLO_BIN, HELLO_SIZE);
    strcpy(uname, "hello");
    size_t free_before = nr_free_pages();

    /* Kernel pointers are not user memory: rejected for a process with mm. */
    ret = do_execve("hello", 5, HELLO_BIN, HELLO_SIZE);
    B_CHECK(ret == -E_INVAL && current->mm == mm, "user exec with kernel pointer rejected: %d", ret);
    ret = do_execve((char *)0x10, 5, uimg, HELLO_SIZE);
    B_CHECK(ret == -E_INVAL && current->mm == mm, "user exec with unmapped name rejected: %d", ret);
    ret = do_execve(uname, 5, uimg, USTACKTOP);
    B_CHECK(ret == -E_INVAL && current->mm == mm, "user exec with oversized binary rejected: %d", ret);
    uimg[0] ^= 0xff;
    ret = do_execve(uname, 5, uimg, HELLO_SIZE);
    uimg[0] ^= 0xff;
    B_CHECK(ret == -E_INVAL_ELF && current->mm == mm && uimg[1] == 'E',
            "failed exec keeps the old image usable: %d", ret);
    B_CHECK(nr_free_pages() == free_before, "failed user exec leaks nothing (%d pages)",
            (int)nr_free_pages());

    ret = do_execve(uname, 5, uimg, HELLO_SIZE);
    B_CHECK(ret == 0 && current->mm != mm && mm_count(current->mm) == 1 &&
                strcmp(current->name, "hello") == 0,
            "exec from user memory replaces mm: ret=%d name=%s", ret, current->name);
    B_CHECK(nr_free_pages() > free_before + 1000,
            "old testbss mm released: free pages %d -> %d", (int)free_before,
            (int)nr_free_pages());
    return 0x5b; /* -> kernel_thread_entry -> do_exit(0x5b) */
}

static int sleeper(void *arg)
{
    /* Waits for a child that never exits; must leave via PF_EXITING. */
    int pid = kernel_thread(exec_child, NULL, 0);
    (void)pid;
    /* Kernel threads never return to user mode, where trap() would act
     * on PF_EXITING, so this one polls the flag itself. */
    while (!(current->flags & PF_EXITING)) {
        do_yield();
        schedule();
    }
    return -E_KILLED;
}

static int waiter(void *arg)
{
    int pid = kernel_thread(sleeper, NULL, 0);
    int code = 0;
    int ret = do_wait(pid, &code); /* sleeps; killed below */
    cprintf("waiter: do_wait returned %d (must not happen)\n", ret);
    return 0;
}

void test_lab5_b_kernel(void)
{
    cprintf("[B] member B kernel tests start (pid %d)\n", current->pid);
    size_t pages0 = nr_free_pages();
    test_bad_elf();
    test_oom();

    /* exec + do_exit + do_wait with exit-code propagation. */
    size_t pages1 = nr_free_pages();
    int pid = kernel_thread(exec_child, NULL, 0);
    int code = 0;
    int ret = do_wait(pid, &code);
    B_CHECK(ret == 0 && code == 0x5b, "do_wait(child) = %d, exit code 0x%x", ret, code);
    B_CHECK(find_proc(pid) == NULL && initproc->cptr == NULL,
            "reaped child removed from hash table and process tree");
    ret = do_wait(pid, &code);
    B_CHECK(ret == -E_BAD_PROC, "second do_wait on reaped pid = %d", ret);
    B_CHECK(nr_free_pages() == pages1, "exec/exit/wait cycle returns all pages (%d -> %d)",
            (int)pages1, (int)nr_free_pages());

    B_CHECK(do_wait(0, NULL) == -E_BAD_PROC, "do_wait(0) without children = -E_BAD_PROC");
    B_CHECK(do_wait(-1, NULL) == -E_BAD_PROC, "do_wait(-1) = -E_BAD_PROC");
    B_CHECK(do_kill(-1) == -E_INVAL && do_kill(0) == -E_INVAL && do_kill(MAX_PID) == -E_INVAL,
            "do_kill of invalid pids = -E_INVAL");
    B_CHECK(do_kill(1) == -E_INVAL, "do_kill(initproc) refused");

    /* kill while waiting: waiter sleeps in do_wait(WT_CHILD), is woken by
     * do_kill and exits with -E_KILLED; its child (sleeper) becomes an
     * orphan of initproc, is killed in turn, and its own exec'ing child is
     * re-parented and reaped as well. */
    pid = kernel_thread(waiter, NULL, 0);
    for (int i = 0; i < 4; i++) {
        do_yield();
        schedule();
    }
    struct proc_struct *w = find_proc(pid);
    B_CHECK(w != NULL && w->state == PROC_SLEEPING && w->wait_state == WT_CHILD,
            "waiter sleeps in do_wait");
    B_CHECK(do_kill(pid) == 0, "do_kill(waiter) = 0");
    B_CHECK(do_kill(pid) == -E_KILLED, "second do_kill(waiter) = -E_KILLED");
    ret = do_wait(pid, &code);
    B_CHECK(ret == 0 && code == -E_KILLED, "killed waiter reaped, exit code %d", code);
    int orphans = 0;
    for (struct proc_struct *p = initproc->cptr; p != NULL; p = p->optr) {
        orphans++;
        do_kill(p->pid);
    }
    B_CHECK(orphans >= 1, "%d orphan(s) re-parented to initproc", orphans);
    int reaped = 0;
    while (do_wait(0, &code) == 0)
        reaped++;
    B_CHECK(reaped >= 2 && initproc->cptr == NULL,
            "initproc reaped %d orphans/grand-children", reaped);
    B_CHECK(nr_free_pages() == pages1, "all pages returned after kill/orphan test (%d -> %d)",
            (int)pages1, (int)nr_free_pages());

    cprintf("[B] member B kernel tests: %d passed, %d failed (pages %d -> %d)\n",
            b_pass, b_fail, (int)pages0, (int)nr_free_pages());
}
