/*
 * Member B (2413507) process-creation tests.
 *
 * prepare.py appends `#include` of this file to the END of a temporary copy of
 * the real kern/process/proc.c, so every check below calls the production
 * static functions (alloc_proc, copy_thread, copy_mm, setup_kstack, ...) and
 * reads the production globals (nr_process, hash_list, ...). Nothing here is
 * compiled into the real kernel.
 *
 * Fault injection uses the linker's --wrap for kmalloc / kfree / alloc_pages
 * (added only to the temporary Makefile). The wrappers forward to the real
 * allocators unless a test arms a one-shot failure.
 */

#include <riscv.h>

void *__real_kmalloc(size_t n);
void __real_kfree(void *objp);
struct Page *__real_alloc_pages(size_t n);

static int mb_fail_kmalloc;      /* fail the next N kmalloc calls     */
static int mb_fail_alloc_pages;  /* fail the next N alloc_pages calls */
static int mb_live_objs;         /* kmalloc successes minus kfree calls */

void *__wrap_kmalloc(size_t n)
{
    if (mb_fail_kmalloc > 0)
    {
        mb_fail_kmalloc--;
        return NULL;
    }
    void *p = __real_kmalloc(n);
    if (p != NULL)
    {
        mb_live_objs++;
    }
    return p;
}

void __wrap_kfree(void *objp)
{
    mb_live_objs--;
    __real_kfree(objp);
}

struct Page *__wrap_alloc_pages(size_t n)
{
    if (mb_fail_alloc_pages > 0)
    {
        mb_fail_alloc_pages--;
        return NULL;
    }
    return __real_alloc_pages(n);
}

/* ------------------------------------------------------------------ */

static int mb_pass, mb_fail;

#define MB_CHECK(cond, name)                                                \
    do {                                                                    \
        if (cond) {                                                         \
            mb_pass++;                                                      \
            cprintf("[member-b] PASS %s\n", name);                          \
        } else {                                                            \
            mb_fail++;                                                      \
            cprintf("[member-b] FAIL %s (%s:%d: %s)\n", name, __FILE__,     \
                    __LINE__, #cond);                                       \
        }                                                                   \
    } while (0)

static int mb_list_count(void)
{
    int n = 0;
    list_entry_t *le = &proc_list;
    while ((le = list_next(le)) != &proc_list)
    {
        n++;
    }
    return n;
}

static bool mb_in_proc_list(struct proc_struct *p)
{
    list_entry_t *le = &proc_list;
    while ((le = list_next(le)) != &proc_list)
    {
        if (le2proc(le, list_link) == p)
        {
            return 1;
        }
    }
    return 0;
}

static bool mb_bytes_zero(const void *p, size_t n)
{
    const char *c = p;
    size_t i;
    for (i = 0; i < n; i++)
    {
        if (c[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static bool mb_node_self_linked(list_entry_t *le)
{
    return le->next == le && le->prev == le;
}

/* Undo a successful do_fork for a thread that never ran (test cleanup). */
static void mb_destroy_unrun(struct proc_struct *p)
{
    bool flag;
    local_intr_save(flag);
    list_del(&p->list_link);
    list_del(&p->hash_link);
    nr_process--;
    local_intr_restore(flag);
    put_kstack(p);
    kfree(p);
}

/* Snapshot of everything a failed do_fork must leave unchanged. */
struct mb_snapshot
{
    int nr_process;
    int list_count;
    size_t free_pages;
    int live_objs;
};

static void mb_take(struct mb_snapshot *s)
{
    s->nr_process = nr_process;
    s->list_count = mb_list_count();
    s->free_pages = nr_free_pages();
    s->live_objs = mb_live_objs;
}

static bool mb_same(const struct mb_snapshot *s)
{
    return s->nr_process == nr_process && s->list_count == mb_list_count() &&
           s->free_pages == nr_free_pages() && s->live_objs == mb_live_objs;
}

static bool mb_frame_ok(struct proc_struct *p, uintptr_t want_sp)
{
    uintptr_t top = p->kstack + KSTACKSIZE;
    return p->tf == (struct trapframe *)(top - sizeof(struct trapframe)) &&
           ((uintptr_t)p->tf & 15) == 0 &&
           p->context.ra == (uintptr_t)forkret &&
           p->context.sp == (uintptr_t)p->tf &&
           p->tf->gpr.sp == want_sp && p->tf->gpr.a0 == 0;
}

/* ---------------- runtime probe thread ---------------- */

static volatile int mb_probe_ran;
static volatile uintptr_t mb_probe_arg, mb_probe_sp, mb_probe_satp;
static volatile uintptr_t mb_probe_sstatus;
static volatile int mb_probe_pid, mb_probe_runs;
static struct proc_struct *mb_probe_proc;

#define MB_PROBE_MAGIC 0x2413507UL

static int mb_probe(void *arg)
{
    uintptr_t sp;
    asm volatile("mv %0, sp" : "=r"(sp));
    mb_probe_sp = sp;
    mb_probe_arg = (uintptr_t)arg;
    mb_probe_satp = read_csr(satp);
    mb_probe_sstatus = read_csr(sstatus);
    mb_probe_pid = current->pid;
    mb_probe_runs = current->runs;
    mb_probe_ran = 1;
    /* do_exit is still the framework panic stub: never return. */
    for (;;)
    {
        current->need_resched = 1;
        schedule();
    }
    return 0;
}

static int mb_dummy(void *arg)
{
    return 0;
}

/* ---------------- tests run at the end of proc_init ---------------- */

void member_b_test_after_proc_init(void)
{
    struct mb_snapshot snap;
    struct proc_struct *p;
    int ret;

    cprintf("[member-b] begin proc_init checks\n");

    /* alloc_proc: every field in a defined blank state */
    p = alloc_proc();
    MB_CHECK(p != NULL, "alloc_proc returns a PCB");
    if (p != NULL)
    {
        MB_CHECK(p->state == PROC_UNINIT && p->pid == -1, "alloc_proc state/pid");
        MB_CHECK(p->runs == 0 && p->kstack == 0 && p->need_resched == 0 &&
                     p->flags == 0,
                 "alloc_proc runs/kstack/need_resched/flags zero");
        MB_CHECK(p->parent == NULL && p->mm == NULL && p->tf == NULL,
                 "alloc_proc parent/mm/tf NULL");
        MB_CHECK(mb_bytes_zero(&p->context, sizeof(p->context)),
                 "alloc_proc context zeroed");
        MB_CHECK(mb_bytes_zero(p->name, sizeof(p->name)), "alloc_proc name zeroed");
        MB_CHECK(p->pgdir == boot_pgdir_pa && (p->pgdir & (PGSIZE - 1)) == 0 &&
                     p->pgdir != (uintptr_t)boot_pgdir_va,
                 "alloc_proc pgdir is page-aligned boot_pgdir_pa");
        MB_CHECK(mb_node_self_linked(&p->list_link) &&
                     mb_node_self_linked(&p->hash_link),
                 "alloc_proc list nodes self-linked");
        struct proc_struct *q = alloc_proc();
        MB_CHECK(q != NULL && q != p, "alloc_proc returns independent PCBs");
        if (q != NULL)
        {
            kfree(q);
        }
        kfree(p);
    }

    /* alloc_proc: allocation failure */
    mb_take(&snap);
    mb_fail_kmalloc = 1;
    p = alloc_proc();
    mb_fail_kmalloc = 0;
    MB_CHECK(p == NULL, "alloc_proc returns NULL when kmalloc fails");
    MB_CHECK(mb_same(&snap), "alloc_proc failure leaves global state unchanged");

    /* idleproc */
    MB_CHECK(current == idleproc && idleproc != NULL, "current is idleproc");
    MB_CHECK(idleproc->pid == 0 && idleproc->state == PROC_RUNNABLE,
             "idle pid 0 runnable");
    MB_CHECK(idleproc->kstack == (uintptr_t)bootstack, "idle uses bootstack");
    MB_CHECK(idleproc->need_resched == 1, "idle need_resched set");
    MB_CHECK(idleproc->parent == NULL && idleproc->mm == NULL &&
                 idleproc->tf == NULL && idleproc->runs == 0 &&
                 idleproc->context.ra == 0,
             "idle parent/mm/tf/runs/context untouched");
    MB_CHECK(strcmp(idleproc->name, "idle") == 0, "idle name");
    MB_CHECK(!mb_in_proc_list(idleproc) && find_proc(0) == NULL,
             "idle not in proc_list or hash");

    /* initproc */
    MB_CHECK(initproc != NULL && initproc->pid == 1, "init pid 1");
    MB_CHECK(strcmp(initproc->name, "init") == 0, "init name");
    MB_CHECK(initproc->state == PROC_RUNNABLE && initproc->parent == idleproc &&
                 initproc->mm == NULL,
             "init runnable, parent idle, mm NULL");
    MB_CHECK(initproc->pgdir == idleproc->pgdir && initproc->pgdir == boot_pgdir_pa,
             "init shares kernel root page table");
    MB_CHECK(mb_in_proc_list(initproc) && find_proc(1) == initproc,
             "init in proc_list and hash");
    MB_CHECK(nr_process == 2 && mb_list_count() == 1, "nr_process == 2 after proc_init");
    MB_CHECK(initproc->kstack != 0 && initproc->kstack != (uintptr_t)bootstack &&
                 (initproc->kstack & (PGSIZE - 1)) == 0,
             "init has its own page-aligned kernel stack");
    MB_CHECK(mb_frame_ok(initproc, (uintptr_t)initproc->tf),
             "init trapframe at stack top, context ra=forkret sp=tf");
    MB_CHECK(initproc->tf->epc == (uintptr_t)kernel_thread_entry &&
                 initproc->tf->gpr.s0 == (uintptr_t)init_main &&
                 strcmp((const char *)initproc->tf->gpr.s1, "Hello world!!") == 0,
             "init trapframe epc/s0/s1 match kernel_thread_entry");
    MB_CHECK((initproc->tf->status & SSTATUS_SPP) &&
                 (initproc->tf->status & SSTATUS_SPIE) &&
                 !(initproc->tf->status & SSTATUS_SIE),
             "init sstatus SPP=1 SPIE=1 SIE=0");

    /* consecutive creation: unique PIDs, separate stacks, registration */
    struct proc_struct *t[3];
    int pids[3], i, j;
    int nr_before = nr_process;
    for (i = 0; i < 3; i++)
    {
        pids[i] = kernel_thread(mb_dummy, (void *)(uintptr_t)i, 0);
        t[i] = find_proc(pids[i]);
    }
    MB_CHECK(pids[0] > 1 && pids[1] > 1 && pids[2] > 1, "three threads created");
    MB_CHECK(pids[0] != pids[1] && pids[1] != pids[2] && pids[0] != pids[2],
             "PIDs unique");
    MB_CHECK(pids[1] == pids[0] + 1 && pids[2] == pids[1] + 1, "PIDs sequential");
    MB_CHECK(nr_process == nr_before + 3, "nr_process counts new threads");
    bool all_ok = 1, stacks_ok = 1;
    for (i = 0; i < 3; i++)
    {
        all_ok = all_ok && t[i] != NULL && t[i]->pid == pids[i] &&
                 t[i]->state == PROC_RUNNABLE && t[i]->parent == current &&
                 t[i]->mm == NULL && mb_in_proc_list(t[i]) &&
                 mb_frame_ok(t[i], (uintptr_t)t[i]->tf) &&
                 t[i]->tf->gpr.s1 == (uintptr_t)i;
        for (j = 0; j < 3 && t[i] != NULL; j++)
        {
            if (i != j && t[j] != NULL &&
                t[i]->kstack < t[j]->kstack + KSTACKSIZE &&
                t[j]->kstack < t[i]->kstack + KSTACKSIZE)
            {
                stacks_ok = 0;
            }
        }
        if (t[i] != NULL && (initproc->kstack < t[i]->kstack + KSTACKSIZE &&
                             t[i]->kstack < initproc->kstack + KSTACKSIZE))
        {
            stacks_ok = 0;
        }
    }
    MB_CHECK(all_ok, "new threads registered with parent/frame/state");
    MB_CHECK(stacks_ok, "kernel stacks do not overlap");
    for (i = 0; i < 3; i++)
    {
        if (t[i] != NULL)
        {
            mb_destroy_unrun(t[i]);
        }
    }
    MB_CHECK(nr_process == nr_before && find_proc(pids[0]) == NULL,
             "test cleanup restored process table");

    /* process limit */
    mb_take(&snap);
    int saved_nr = nr_process;
    nr_process = MAX_PROCESS;
    snap.nr_process = MAX_PROCESS;
    ret = kernel_thread(mb_dummy, NULL, 0);
    MB_CHECK(ret == -E_NO_FREE_PROC, "do_fork returns -E_NO_FREE_PROC at limit");
    MB_CHECK(mb_same(&snap), "limit failure leaves no resources");
    nr_process = saved_nr;

    /* PCB allocation failure */
    mb_take(&snap);
    mb_fail_kmalloc = 1;
    ret = kernel_thread(mb_dummy, NULL, 0);
    mb_fail_kmalloc = 0;
    MB_CHECK(ret == -E_NO_MEM, "do_fork returns -E_NO_MEM when PCB alloc fails");
    MB_CHECK(mb_same(&snap), "PCB failure leaves no resources");

    /* kernel stack allocation failure */
    mb_take(&snap);
    mb_fail_alloc_pages = 1;
    ret = kernel_thread(mb_dummy, NULL, 0);
    mb_fail_alloc_pages = 0;
    MB_CHECK(ret == -E_NO_MEM, "do_fork returns -E_NO_MEM when kstack alloc fails");
    MB_CHECK(mb_same(&snap), "kstack failure frees the PCB");

    /* copy_mm: unsupported user mm */
    MB_CHECK(copy_mm(CLONE_VM, initproc) == 0 && initproc->mm == NULL,
             "copy_mm accepts kernel thread (mm NULL)");
    mb_take(&snap);
    struct mm_struct *saved_mm = current->mm;
    current->mm = (struct mm_struct *)0xdead0000UL; /* never dereferenced */
    ret = kernel_thread(mb_dummy, NULL, 0);
    current->mm = saved_mm;
    MB_CHECK(ret == -E_INVAL, "copy_mm rejects non-NULL user mm");
    MB_CHECK(mb_same(&snap), "copy_mm failure frees kstack and PCB");

    /* PID not consumed by failed forks */
    int next = kernel_thread(mb_dummy, NULL, 0);
    MB_CHECK(next == pids[2] + 1, "failed forks consumed no PID");
    if (next > 0)
    {
        mb_destroy_unrun(find_proc(next));
    }

    /* copy_thread with a caller-supplied stack */
    p = alloc_proc();
    if (p != NULL && setup_kstack(p) == 0)
    {
        struct trapframe in;
        memset(&in, 0x5a, sizeof(in));
        copy_thread(p, 0x80001230UL, &in);
        MB_CHECK(mb_frame_ok(p, 0x80001230UL), "copy_thread keeps nonzero esp, a0=0");
        MB_CHECK(p->tf->epc == in.epc && p->tf->gpr.s0 == in.gpr.s0,
                 "copy_thread copies the rest of the frame");
        put_kstack(p);
        kfree(p);
    }
    else
    {
        MB_CHECK(0, "copy_thread setup");
    }

    /* runtime probe for first-run checks (scheduled later) */
    int probe = kernel_thread(mb_probe, (void *)MB_PROBE_MAGIC, 0);
    mb_probe_proc = find_proc(probe);
    MB_CHECK(probe > 0 && mb_probe_proc != NULL, "probe thread created");

    /* official test expects these idle fields to be untouched */
    MB_CHECK(idleproc->runs == 0 && idleproc->tf == NULL &&
                 idleproc->context.ra == 0 && current == idleproc,
             "idle still pristine after tests");

    cprintf("[member-b] end proc_init checks pass=%d fail=%d\n", mb_pass, mb_fail);
}

/* ---------------- tests run at the start of init_main ---------------- */

void member_b_test_in_init_main(void)
{
    uintptr_t sp;
    int i;
    asm volatile("mv %0, sp" : "=r"(sp));

    cprintf("[member-b] begin first-run checks\n");
    MB_CHECK(current == initproc && current->pid == 1, "init_main runs as initproc");
    MB_CHECK(current->runs >= 1, "initproc runs counted");
    MB_CHECK(sp > initproc->kstack && sp <= initproc->kstack + KSTACKSIZE &&
                 (sp & 15) == 0,
             "init_main sp inside own kstack, 16-byte aligned");

    /* Create a thread while SIE=1: the saved status must still clear SIE, or
       RESTORE_ALL would re-enable interrupts before the GPRs are restored. */
    bool sie_on = (read_csr(sstatus) & SSTATUS_SIE) != 0;
    MB_CHECK(sie_on, "init_main runs with interrupts enabled");
    int k = kernel_thread(mb_dummy, NULL, 0);
    struct proc_struct *kp = find_proc(k);
    MB_CHECK(kp != NULL && kp->parent == initproc, "thread created from init has parent init");
    if (kp != NULL)
    {
        MB_CHECK(!(kp->tf->status & SSTATUS_SIE) && (kp->tf->status & SSTATUS_SPIE) &&
                     (kp->tf->status & SSTATUS_SPP),
                 "kernel_thread clears SIE even when caller has SIE=1");
        mb_destroy_unrun(kp);
    }

    for (i = 0; i < 16 && !mb_probe_ran; i++)
    {
        current->need_resched = 1;
        schedule();
    }
    MB_CHECK(mb_probe_ran, "probe thread executed");
    if (mb_probe_ran)
    {
        uintptr_t lo = mb_probe_proc->kstack;
        MB_CHECK(mb_probe_arg == MB_PROBE_MAGIC, "probe received fn(arg) argument");
        MB_CHECK(mb_probe_sp > lo && mb_probe_sp <= lo + KSTACKSIZE &&
                     (mb_probe_sp & 15) == 0,
                 "probe sp inside own kstack, 16-byte aligned");
        MB_CHECK(mb_probe_pid == mb_probe_proc->pid && mb_probe_runs >= 1,
                 "probe current/runs consistent");
        MB_CHECK(mb_probe_satp == (((uintptr_t)8 << 60) | (boot_pgdir_pa >> RISCV_PGSHIFT)),
                 "probe satp is Sv39 kernel root");
        MB_CHECK(mb_probe_sstatus & SSTATUS_SIE, "probe runs with interrupts enabled (SPIE->SIE)");
    }
    cprintf("[member-b] SUMMARY pass=%d fail=%d\n", mb_pass, mb_fail);
}
