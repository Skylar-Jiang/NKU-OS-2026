#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

//lab6 function1-1 YOUR CODE update alloc_proc

// alloc_proc - alloc a proc_struct and initialize every field to a known state
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc == NULL)
    {
        return NULL;
    }
    // Zero first: runs, kstack, need_resched, parent, mm, context, tf,
    // flags and name all start as 0/NULL.
    memset(proc, 0, sizeof(struct proc_struct));
    proc->state = PROC_UNINIT;
    proc->pid = -1;
    // Kernel threads share the kernel root page table (physical address).
    proc->pgdir = boot_pgdir_pa;
    // Self-linked nodes: not on proc_list or any hash bucket yet.
    list_init(&(proc->list_link));
    list_init(&(proc->hash_link));
    // Lab5 fields: no exit status, not waiting, no process-tree links yet.
    proc->exit_code = 0;
    proc->wait_state = 0;
    proc->cptr = proc->yptr = proc->optr = NULL;
    return proc;
}

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}



// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}

// The trapframe sits at the top of a page-aligned kernel stack, so its size
// must keep the frame (and therefore the first-run sp) 16-byte aligned.
_Static_assert(sizeof(struct trapframe) % 16 == 0,
               "trapframe must keep the kernel stack 16-byte aligned");

// kernel_thread - create a kernel thread running fn(arg)
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    // kernel_thread_entry: a0 <- s1 (arg), then jalr s0 (fn).
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    // sret returns to S-mode (SPP=1) and enables interrupts there (SPIE=1);
    // SIE stays clear until sret actually happens.
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;
    // Kernel threads share the kernel address space.
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

/* release_mm - drop one reference to mm and destroy it with the last one.
 * mm_count counts the sharers of one address space (CLONE_VM). Ordinary fork
 * gives the child private page copies (dup_mmap/copy_range with share = 0),
 * so every user page is owned by exactly one mm. */
static void
release_mm(struct mm_struct *mm)
{
    if (mm != NULL && mm_count_dec(mm) == 0)
    {
        exit_mmap(mm);
        put_pgdir(mm);
        mm_destroy(mm);
    }
}

// copy_mm - share (CLONE_VM) or duplicate the parent's address space
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *old = current->mm;
    if (old == NULL)
    {
        // Kernel thread: keep the kernel page table set by alloc_proc.
        return 0;
    }
    struct mm_struct *mm = old;
    int ret = 0;
    lock_mm(old);
    if (!(clone_flags & CLONE_VM))
    {
        mm = mm_create();
        if (mm == NULL)
        {
            ret = -E_NO_MEM;
            goto out;
        }
        if ((ret = setup_pgdir(mm)) != 0)
        {
            mm_destroy(mm);
            goto out;
        }
        // Eager private copy of every mapped user page.
        if ((ret = dup_mmap(mm, old)) != 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
            goto out;
        }
    }
    mm_count_inc(mm);
    proc->mm = mm;
    proc->pgdir = PADDR(mm->pgdir);
out:
    unlock_mm(old);
    return ret;
}

// copy_thread - build the child's trapframe and first-run context
static void
copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf)
{
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;
    // The child sees fork() return 0.
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = stack ? stack :
        ((tf->status & SSTATUS_SPP) ? (uintptr_t)proc->tf : tf->gpr.sp);
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)proc->tf;
}

/* do_fork - parent process for a new child process
 *   1. alloc_proc, 2. setup_kstack, 3. copy_mm, 4. copy_thread,
 *   5. get_pid + hash_proc + set_links, 6. wakeup_proc, 7. return child pid.
 * Every failure releases what was allocated before it. */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }
    proc->parent = current;
    assert(current->wait_state == 0);
    if ((ret = setup_kstack(proc)) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    if ((ret = copy_mm(clone_flags, proc)) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc); // includes nr_process++
    }
    local_intr_restore(intr_flag);

    // The child enters the scheduler's run queue only once it is complete.
    wakeup_proc(proc);
    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

/* Number of user-stack pages populated eagerly by load_icode. The stack VMA
 * spans USTACKSIZE; there is no demand-paging handler, so only these pages are
 * backed and the unmapped remainder of the VMA acts as an overflow guard. */
#define USTACK_INIT_PAGES 4

/* elf_seg_perm - translate ELF segment flags into VMA flags and PTE bits. */
static void
elf_seg_perm(uint32_t p_flags, uint32_t *vm_flags, uint32_t *perm)
{
    *vm_flags = 0;
    *perm = PTE_U | PTE_V;
    if (p_flags & ELF_PF_X)
    {
        *vm_flags |= VM_EXEC;
        *perm |= PTE_X;
    }
    if (p_flags & ELF_PF_W)
    {
        *vm_flags |= VM_WRITE;
        /* RISC-V reserves W without R, so a writable page is also readable. */
        *perm |= PTE_W | PTE_R;
    }
    if (p_flags & ELF_PF_R)
    {
        *vm_flags |= VM_READ;
        *perm |= PTE_R;
    }
}

/* map_user_range - mm_map() that first rejects any overlap with an existing
 * VMA, so a crafted ELF cannot reach the overlap assert in insert_vma_struct. */
static int
map_user_range(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags)
{
    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
    list_entry_t *list = &(mm->mmap_list), *le = list;
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *vma = le2vma(le, list_link);
        if (start < vma->vm_end && vma->vm_start < end)
        {
            return -E_INVAL_ELF;
        }
    }
    return mm_map(mm, addr, len, vm_flags, NULL);
}

/* load_segment - back every page of [va, va + memsz) with a zeroed page and
 * copy the filesz bytes of file data into it (BSS stays zero). */
static int
load_segment(struct mm_struct *mm, const unsigned char *src, uintptr_t va,
             size_t filesz, size_t memsz, uint32_t perm)
{
    uintptr_t end = va + memsz, file_end = va + filesz;
    uintptr_t la;
    for (la = ROUNDDOWN(va, PGSIZE); la < end; la += PGSIZE)
    {
        struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
        if (page == NULL)
        {
            return -E_NO_MEM;
        }
        unsigned char *kva = page2kva(page);
        memset(kva, 0, PGSIZE);
        uintptr_t from = (la > va) ? la : va;
        uintptr_t to = (la + PGSIZE < file_end) ? la + PGSIZE : file_end;
        if (from < to)
        {
            memcpy(kva + (from - la), src + (from - va), to - from);
        }
    }
    return 0;
}

/* load_icode - build a new user address space from the ELF image binary and
 * install it as the image of current. The new mm is built completely while
 * the old one is still active; only after every check and allocation has
 * succeeded is it committed and current->tf rewritten for the U-mode entry. */
static int
load_icode(unsigned char *binary, size_t size)
{
    struct elfhdr elf;
    struct proghdr ph;
    struct mm_struct *mm;
    int ret = -E_INVAL_ELF;
    uint16_t i;
    bool has_load = 0;

    /* (1) ELF header, copied out because the image may be unaligned. */
    if (binary == NULL || size < sizeof(struct elfhdr))
    {
        return -E_INVAL_ELF;
    }
    memcpy(&elf, binary, sizeof(struct elfhdr));
    if (elf.e_magic != ELF_MAGIC || elf.e_phnum == 0 ||
        elf.e_phentsize != sizeof(struct proghdr) ||
        elf.e_phoff > size ||
        (size - elf.e_phoff) / sizeof(struct proghdr) < elf.e_phnum)
    {
        return -E_INVAL_ELF;
    }

    /* (2) A fresh mm with its own root page table (kernel half copied). */
    ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL)
    {
        return ret;
    }
    if (setup_pgdir(mm) != 0)
    {
        mm_destroy(mm);
        return ret;
    }

    /* (3) Map and fill every PT_LOAD segment. */
    for (i = 0; i < elf.e_phnum; i++)
    {
        memcpy(&ph, binary + elf.e_phoff + (size_t)i * sizeof(struct proghdr),
               sizeof(struct proghdr));
        if (ph.p_type != ELF_PT_LOAD)
        {
            continue;
        }
        ret = -E_INVAL_ELF;
        if (ph.p_filesz > ph.p_memsz || ph.p_offset > size ||
            ph.p_filesz > size - ph.p_offset)
        {
            goto bad_mm;
        }
        if (ph.p_memsz == 0)
        {
            continue;
        }
        if (ph.p_va + ph.p_memsz < ph.p_va ||
            !USER_ACCESS(ph.p_va, ph.p_va + ph.p_memsz))
        {
            goto bad_mm;
        }
        uint32_t vm_flags, perm;
        elf_seg_perm(ph.p_flags, &vm_flags, &perm);
        if ((ret = map_user_range(mm, ph.p_va, ph.p_memsz, vm_flags)) != 0)
        {
            goto bad_mm;
        }
        if ((ret = load_segment(mm, binary + ph.p_offset, ph.p_va,
                                ph.p_filesz, ph.p_memsz, perm)) != 0)
        {
            goto bad_mm;
        }
        has_load = 1;
    }
    ret = -E_INVAL_ELF;
    if (!has_load)
    {
        goto bad_mm;
    }

    /* (4) The entry point must be inside an executable segment. */
    struct vma_struct *entry_vma = find_vma(mm, elf.e_entry);
    if (entry_vma == NULL || elf.e_entry < entry_vma->vm_start ||
        !(entry_vma->vm_flags & VM_EXEC))
    {
        goto bad_mm;
    }

    /* (5) User stack: reserve USTACKSIZE below USTACKTOP, back the top pages. */
    if ((ret = map_user_range(mm, USTACKTOP - USTACKSIZE, USTACKSIZE,
                              VM_READ | VM_WRITE | VM_STACK)) != 0)
    {
        goto bad_mm;
    }
    ret = -E_NO_MEM;
    for (i = 1; i <= USTACK_INIT_PAGES; i++)
    {
        struct Page *page = pgdir_alloc_page(mm->pgdir, USTACKTOP - i * PGSIZE,
                                             PTE_U | PTE_V | PTE_R | PTE_W);
        if (page == NULL)
        {
            goto bad_mm;
        }
        memset(page2kva(page), 0, PGSIZE);
    }

    /* (6) Commit: from here on nothing can fail. */
    struct mm_struct *old_mm = current->mm;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        mm_count_inc(mm);
        current->mm = mm;
        current->pgdir = PADDR(mm->pgdir);
        lsatp(current->pgdir);
        flush_tlb();
    }
    local_intr_restore(intr_flag);
    /* Code was written through kernel aliases; sync the instruction stream. */
    asm volatile("fence.i" ::: "memory");
    release_mm(old_mm);

    /* (7) Trapframe for the first sret into the new image: user stack top,
     *     ELF entry, SPP=0 (return to U), SPIE=1 (interrupts on after sret). */
    struct trapframe *tf = current->tf;
    uintptr_t sstatus = read_csr(sstatus);
    memset(tf, 0, sizeof(struct trapframe));
    tf->gpr.sp = USTACKTOP;
    tf->epc = elf.e_entry;
    tf->status = (sstatus & ~(SSTATUS_SPP | SSTATUS_SIE)) | SSTATUS_SPIE;
    return 0;

bad_mm:
    exit_mmap(mm);
    put_pgdir(mm);
    mm_destroy(mm);
    return ret;
}

/* do_execve - replace the image of current with the ELF program binary.
 * On failure the caller's old image is left intact. */
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
{
    struct mm_struct *mm = current->mm;
    char local_name[PROC_NAME_LEN + 1];
    int ret;

    if (current->tf == NULL)
    {
        return -E_INVAL;
    }
    if (len > PROC_NAME_LEN)
    {
        len = PROC_NAME_LEN;
    }
    memset(local_name, 0, sizeof(local_name));

    if (name == NULL || binary == NULL || size == 0)
    {
        return -E_INVAL;
    }
    /* A user caller (mm != NULL) passes untrusted pointers that must lie in
     * readable VMAs of its own address space; a kernel thread (only
     * kernel_execve) passes kernel image data. */
    if (mm != NULL)
    {
        lock_mm(mm);
        if ((len != 0 && !copy_from_user(mm, local_name, name, len, 0)) ||
            !user_mem_check(mm, (uintptr_t)binary, size, 0))
        {
            unlock_mm(mm);
            return -E_INVAL;
        }
        unlock_mm(mm);
    }
    else
    {
        memcpy(local_name, name, len);
    }

    if ((ret = load_icode(binary, size)) != 0)
    {
        return ret;
    }
    set_proc_name(current, local_name);
    return 0;
}

/* do_exit - called by sys_exit, by a killed process on its way back to user
 * mode, or when a kernel thread function returns.
 *   1. leave and release the user address space;
 *   2. become PROC_ZOMBIE with exit_code;
 *   3. wake a parent blocked in do_wait and hand all children to initproc;
 *   4. schedule() and never run again.
 * The kernel stack and PCB are still in use here; the parent frees them in
 * do_wait. */
int do_exit(int error_code)
{
    if (current == idleproc)
    {
        panic("idleproc exit.\n");
    }
    if (current == initproc)
    {
        panic("initproc exit.\n");
    }

    struct mm_struct *mm = current->mm;
    if (mm != NULL)
    {
        /* Switch to the kernel-only page table before the user one can go. */
        lsatp(boot_pgdir_pa);
        flush_tlb();
        current->mm = NULL;
        current->pgdir = boot_pgdir_pa;
        release_mm(mm);
    }

    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);
    {
        current->state = PROC_ZOMBIE;
        current->exit_code = error_code;

        proc = current->parent;
        if (proc->wait_state == WT_CHILD)
        {
            wakeup_proc(proc);
        }
        /* Re-parent every child (running or zombie) to initproc. */
        while (current->cptr != NULL)
        {
            proc = current->cptr;
            current->cptr = proc->optr;

            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL)
            {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;
            if (proc->state == PROC_ZOMBIE && initproc->wait_state == WT_CHILD)
            {
                wakeup_proc(initproc);
            }
        }
    }
    local_intr_restore(intr_flag);

    schedule();
    panic("do_exit will not return!! %d.\n", current->pid);
}

/* do_wait - wait for child pid (or any child when pid == 0) to become a
 * zombie, store its exit code to *code_store if given, and reclaim it.
 * Returns -E_BAD_PROC when there is no matching child and -E_INVAL when
 * code_store is not a writable address of the caller. */
int do_wait(int pid, int *code_store)
{
    struct mm_struct *mm = current->mm;
    struct proc_struct *proc;
    bool intr_flag, haskid;

    if (code_store != NULL && mm != NULL &&
        !user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1))
    {
        return -E_INVAL;
    }

repeat:
    haskid = 0;
    /* Scan and go to sleep with interrupts off so that a child's exit
     * cannot slip in between the check and the state change. */
    local_intr_save(intr_flag);
    if (pid != 0)
    {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    else
    {
        for (proc = current->cptr; proc != NULL; proc = proc->optr)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    if (haskid)
    {
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        local_intr_restore(intr_flag);
        schedule();
        if (current->flags & PF_EXITING)
        {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
    local_intr_restore(intr_flag);
    return -E_BAD_PROC;

found:
    local_intr_restore(intr_flag);
    if (proc == idleproc || proc == initproc)
    {
        panic("wait idleproc or initproc.\n");
    }
    if (code_store != NULL)
    {
        if (mm == NULL)
        {
            *code_store = proc->exit_code;
        }
        else if (!copy_to_user(mm, code_store, &proc->exit_code, sizeof(int)))
        {
            return -E_INVAL;
        }
    }
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc); // includes nr_process--
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);
    return 0;
}

/* do_yield - ask the scheduler to run another process at the next safe point
 * (trap return to user mode, or cpu_idle). */
int do_yield(void)
{
    current->need_resched = 1;
    return 0;
}

/* do_kill - mark process pid as exiting. The target exits by itself on its
 * next return to user mode (trap()) or when it wakes up in do_wait; a target
 * in an interruptible sleep is woken now. */
int do_kill(int pid)
{
    struct proc_struct *proc;
    int ret = -E_INVAL;
    bool intr_flag;
    local_intr_save(intr_flag);
    if ((proc = find_proc(pid)) != NULL && proc != initproc)
    {
        if ((proc->flags & PF_EXITING) || proc->state == PROC_ZOMBIE)
        {
            ret = -E_KILLED;
        }
        else
        {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED)
            {
                wakeup_proc(proc);
            }
            ret = 0;
        }
    }
    local_intr_restore(intr_flag);
    return ret;
}



#define __KERNEL_EXECVE(name, binary, size) ({           \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, binary, (size_t)(size));         \
})

#define KERNEL_EXECVE(x) ({                                    \
    extern unsigned char _binary_obj___user_##x##_out_start[], \
        _binary_obj___user_##x##_out_size[];                   \
    __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,    \
                    _binary_obj___user_##x##_out_size);        \
})

#define __KERNEL_EXECVE2(x, xstart, xsize) ({   \
    extern unsigned char xstart[], xsize[];     \
    __KERNEL_EXECVE(#x, xstart, (size_t)xsize); \
})

#define KERNEL_EXECVE2(x, xstart, xsize) __KERNEL_EXECVE2(x, xstart, xsize)

/* kernel_execve - turn the calling kernel thread into a user process.
 * A kernel ebreak with a7 = T_SYSCALL and a0 = SYS_exec (a1..a4 = name, len,
 * binary, size) is routed by exception_handler to syscall(); do_execve then
 * rewrites this trap's frame (U-mode status, ELF entry, user sp) and the
 * common __trapret path finishes with sret. Control comes back here only if
 * exec failed, with the error in a0. */
static int
kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    /* Evaluate every call before binding the register variables. */
    size_t len = strlen(name);
    register uintptr_t a0 asm("a0") = SYS_exec;
    register uintptr_t a1 asm("a1") = (uintptr_t)name;
    register uintptr_t a2 asm("a2") = (uintptr_t)len;
    register uintptr_t a3 asm("a3") = (uintptr_t)binary;
    register uintptr_t a4 asm("a4") = (uintptr_t)size;
    register uintptr_t a7 asm("a7") = T_SYSCALL;
    asm volatile("ebreak"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7)
                 : "memory");
    return (int)a0;
}

// user_main - kernel thread used to exec a user program
static int
user_main(void *arg)
{
    int ret;
#ifdef TEST
    ret = KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    ret = KERNEL_EXECVE(exit);
#endif
    cprintf("user_main: kernel_execve failed, error %d.\n", ret);
    return ret;
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();

    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0)
    {
        schedule();
    }

    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == 2);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));
    /* SLOB never returns arena pages, so exact page equality is not an
     * invariant; require that no pages appeared and that kmalloc balanced. */
    assert(nr_free_pages() <= nr_free_pages_store);
    assert(kernel_allocated_store == kallocated());
    cprintf("init check memory pass.\n");
    return 0;
}

// proc_init - set up the first kernel thread idleproc "idle" by itself and
//           - create the second kernel thread init_main
void proc_init(void)
{
    int i;

    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL)
    {
        panic("cannot alloc idleproc.\n");
    }

    // idleproc is the code already running on bootstack. It is neither hashed
    // nor put on proc_list, and the scheduler never enqueues it.
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process++;

    current = idleproc;

    int pid = kernel_thread(init_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

/* Keep switch.S offsets checked against the C layout at build time. */
#define CHECK_CONTEXT(field, slot) \
    _Static_assert(__builtin_offsetof(struct context, field) == \
                   (slot) * sizeof(uintptr_t), "switch_to context offset")
CHECK_CONTEXT(ra, 0); CHECK_CONTEXT(sp, 1);
CHECK_CONTEXT(s0, 2); CHECK_CONTEXT(s1, 3);
CHECK_CONTEXT(s2, 4); CHECK_CONTEXT(s3, 5);
CHECK_CONTEXT(s4, 6); CHECK_CONTEXT(s5, 7);
CHECK_CONTEXT(s6, 8); CHECK_CONTEXT(s7, 9);
CHECK_CONTEXT(s8, 10); CHECK_CONTEXT(s9, 11);
CHECK_CONTEXT(s10, 12); CHECK_CONTEXT(s11, 13);
_Static_assert(sizeof(struct context) == 14 * sizeof(uintptr_t),
               "switch_to context size");
#undef CHECK_CONTEXT

// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
void cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load base addr of "proc"'s new PDT
void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = proc;
            /* pgdir is a physical address; lsatp() sets MODE=Sv39 | PPN.
             * No ASIDs are assigned, so discard old translations. */
            lsatp(next->pgdir);
            flush_tlb();
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}


// For lab6, set the process's priority (bigger value will get more CPU time)
void lab6_set_priority(uint32_t priority)
{
    cprintf("set priority to %d\n", priority);
    if (priority == 0)
        current->lab6_priority = 1;
    else
        current->lab6_priority = priority;
}
