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


// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code)
{
    panic("process exit!!.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

//LAB4 function2 2413507

// alloc_proc - allocate a PCB and put every field into a well-defined blank
// state. The PCB owns no kernel stack, PID or list membership yet.
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
    return proc;
}

// The trapframe sits at the top of a page-aligned kernel stack, so its size
// must keep the frame (and therefore the first-run sp) 16-byte aligned.
_Static_assert(sizeof(struct trapframe) % 16 == 0,
               "trapframe must keep the kernel stack 16-byte aligned");

// kernel_thread - create a kernel thread that runs fn(arg).
// The temporary trapframe only lives during do_fork; copy_thread copies it
// onto the new thread's own kernel stack.
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

// copy_mm - Lab4 only creates kernel threads, which have no user mm.
// A non-NULL user mm needs the Lab5 duplication/sharing code, which does not
// exist yet, so it is rejected instead of being reported as copied.
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    if (current == NULL || current->mm == NULL)
    {
        proc->mm = NULL;
        return 0;
    }
    return -E_INVAL;
}

// copy_thread - place the trapframe at the top of proc's kernel stack and
// make the first switch_to land in forkret with sp pointing at that frame.
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;
    // Child observes 0 as the fork return value.
    proc->tf->gpr.a0 = 0;
    // esp == 0 means a kernel thread: after __trapret its sp is the top of its
    // own stack area (the frame is dead by then), never 0.
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}

// do_fork - create a child of current with its own PCB and kernel stack.
// Returns the child's PID, or a negative error after releasing everything
// this call allocated.
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

    if ((ret = setup_kstack(proc)) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    if ((ret = copy_mm(clone_flags, proc)) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(proc, stack, tf);

    // PID allocation scans proc_list, so allocation and registration form one
    // critical section; the child becomes RUNNABLE only once fully built.
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        list_add(&proc_list, &(proc->list_link));
        nr_process++;
        wakeup_proc(proc);
        ret = proc->pid;
    }
    local_intr_restore(intr_flag);

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// proc_init - wrap the boot execution flow as idleproc (pid 0) and create
// the first ordinary kernel thread initproc (pid 1) running init_main.
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

    // idleproc is the code already running on bootstack, not a forked
    // thread. It is neither hashed nor put on proc_list: schedule() uses it
    // only as the fallback when no ordinary thread is runnable.
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process++;

    current = idleproc;

    int pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    if (initproc == NULL)
    {
        panic("initproc pid %d not found.\n", pid);
    }
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
    assert(initproc->parent == idleproc && initproc->mm == NULL);
}


//LAB4 function3 2411788

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
_Static_assert(sizeof(uintptr_t) == 8, "Lab4 uses RV64 Sv39");
#undef CHECK_CONTEXT

void cpu_idle(void)
{
    for (;;) {
        if (current->need_resched) {
            schedule();
        }
    }
}

void proc_run(struct proc_struct *proc)
{
    assert(proc != NULL && current != NULL);
    assert(proc->state == PROC_RUNNABLE);
    assert((proc->pgdir & (PGSIZE - 1)) == 0);
    if (proc != current) {
        bool intr_flag;
        local_intr_save(intr_flag);
        struct proc_struct *prev = current;
        current = proc;
        /* pgdir is a physical address. lsatp() is the old Sv32 helper. */
        write_csr(satp, ((uintptr_t)8 << 60) |
                        (proc->pgdir >> RISCV_PGSHIFT));
        /* No ASIDs are assigned in this lab; discard old translations. */
        asm volatile("sfence.vma" ::: "memory");
        switch_to(&prev->context, &proc->context);
        /* This executes when prev is selected again, not in the new thread. */
        local_intr_restore(intr_flag);
    }
}
