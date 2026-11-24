#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <riscv.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <vmm.h>
#include <kdebug.h>
#include <unistd.h>
#include <syscall.h>
#include <error.h>
#include <sched.h>
#include <sync.h>
#include <sbi.h>

#define TICK_NUM 100

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S */
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe *tf)
{
    return (tf->status & SSTATUS_SPP) != 0;
}

void print_trapframe(struct trapframe *tf)
{
    cprintf("trapframe at %p\n", tf);
    // cprintf("trapframe at 0x%x\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  tval 0x%08x\n", tf->tval);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr)
{
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

extern struct mm_struct *check_mm_struct;

//lab3 请补充你在lab3中的代码

static size_t trap_instruction_length(uintptr_t epc) {
    const volatile uint16_t *instruction = (const volatile uint16_t *)epc;
    return ((*instruction & 0x3) == 0x3) ? 4 : 2;
}

static const char *const irq_level[] = {"User", "Supervisor", "Hypervisor", "Machine"};
static const char *const irq_type[] = {"software", "timer", "external"};

void interrupt_handler(struct trapframe *tf)
{
    intptr_t cause = (tf->cause << 1) >> 1;
    switch (cause)
    {
    case IRQ_U_SOFT: case IRQ_S_SOFT: case IRQ_H_SOFT: case IRQ_M_SOFT:
    case IRQ_U_TIMER:                 case IRQ_H_TIMER: case IRQ_M_TIMER:
    case IRQ_U_EXT:  case IRQ_S_EXT:  case IRQ_H_EXT:  case IRQ_M_EXT:
        cprintf("%s %s interrupt\n", irq_level[cause & 3], irq_type[cause >> 2]);
        break;
    case IRQ_S_TIMER:
        clock_set_next_event();
        ticks++;
        if (ticks % TICK_NUM == 0)
        {
            print_ticks();
        }
        /* An interrupt resumes the interrupted instruction; do not skip epc. */
        break;
    default:
        print_trapframe(tf);
        break;
    }
}

//lab5 function1 YOUR CODE update exception_handler
/* Keep the inherited exception diagnostics alongside the Lab5 syscall path. */
static size_t
kernel_exec_breakpoint_size(struct trapframe *tf)
{
    /* kernel_execve is still a separate member's task. Its optional ebreak
     * bridge uses the existing T_SYSCALL token in a7, SYS_exec in a0, and
     * name/len/binary/size in a1..a4. An unmarked breakpoint stays a breakpoint.
     */
    if (!trap_in_kernel(tf) || tf->gpr.a7 != T_SYSCALL ||
        tf->gpr.a0 != SYS_exec)
    {
        return 0;
    }

    /* A 32-bit instruction may only be halfword-aligned when C is enabled. */
    const volatile uint16_t *instruction = (const volatile uint16_t *)tf->epc;
    uint16_t low = instruction[0];
    if (low == MATCH_C_EBREAK)
    {
        return 2;
    }
    if (low == (uint16_t)MATCH_EBREAK &&
        instruction[1] == (uint16_t)(MATCH_EBREAK >> 16))
    {
        return 4;
    }
    return 0;
}

static void __noreturn
store_page_fault_abort(struct trapframe *tf, int error)
{
    print_trapframe(tf);
    cprintf("Store page fault: address=0x%016llx, epc=0x%016llx, error=%d\n",
            (unsigned long long)tf->tval, (unsigned long long)tf->epc, error);

    /* A kernel fault or an inconsistent/reserved process is not a user task
     * that the normal process-exit path can safely reclaim.
     */
    if (trap_in_kernel(tf) || current == NULL || current == idleproc ||
        current == initproc || current->state != PROC_RUNNABLE)
    {
        panic("unrecoverable store page fault");
    }

    if (current->flags & PF_EXITING)
    {
        error = -E_KILLED;
    }
    do_exit(error);
    /* Even an unfinished/broken do_exit must not retry the same bad store. */
    panic("do_exit returned after an unhandled store page fault");
}

static void
handle_store_page_fault(struct trapframe *tf)
{
    uintptr_t fault_addr = tf->tval;
    if (trap_in_kernel(tf) || current == NULL)
    {
        store_page_fault_abort(tf, -E_FAULT);
    }

    struct mm_struct *mm = current->mm;
    if (mm == NULL || mm->pgdir == NULL)
    {
        store_page_fault_abort(tf, -E_FAULT);
    }

    /* COW integration point: vmm.h/pmm.h currently provide no COW fault API.
     * Agree the actual interface with member C before adding a call here.
     * Pass mm and the full fault_addr; memory management must verify the
     * VMA/PTE COW state, not merely the Store Page Fault cause.
     * After confirmed repair, return with tf->epc unchanged to retry the store.
     * A non-COW result or failure must reach store_page_fault_abort, propagating
     * allocation errors instead of returning through the invalid mapping.
     */
    cprintf("COW fault interface pending: mm=%p, address=0x%016llx\n",
            mm, (unsigned long long)fault_addr);
    store_page_fault_abort(tf, -E_FAULT);
}

void exception_handler(struct trapframe *tf)
{
    switch (tf->cause)
    {
    case CAUSE_STORE_PAGE_FAULT:
        handle_store_page_fault(tf);
        break;
    case CAUSE_USER_ECALL:
        /* ECALL is always 32 bits. Advance before a handler can replace epc. */
        tf->epc += 4;
        syscall();
        break;
    case CAUSE_ILLEGAL_INSTRUCTION:
        cprintf("Exception: Illegal instruction\n");
        cprintf("  Bad instruction address: 0x%016llx\n", (unsigned long long)tf->epc);
        tf->epc += trap_instruction_length(tf->epc);
        break;
    case CAUSE_BREAKPOINT:
    {
        size_t size = kernel_exec_breakpoint_size(tf);
        if (size != 0)
        {
            /* Successful exec may replace epc/status in this same frame. */
            tf->epc += size;
            syscall();
            break;
        }
        cprintf("Exception: Breakpoint\n");
        cprintf("  Bad instruction address: 0x%016llx\n", (unsigned long long)tf->epc);
        tf->epc += trap_instruction_length(tf->epc);
        break;
    }
    default:
        print_trapframe(tf);
        break;
    }
}


static inline void trap_dispatch(struct trapframe *tf)
{
    if ((intptr_t)tf->cause < 0)
    {
        // interrupts
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        exception_handler(tf);
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    //    cputs("some trap");
    if (current == NULL)
    {
        trap_dispatch(tf);
    }
    else
    {
        struct trapframe *otf = current->tf;
        current->tf = tf;

        bool in_kernel = trap_in_kernel(tf);

        trap_dispatch(tf);

        current->tf = otf;
        if (!in_kernel)
        {
            if (current->flags & PF_EXITING)
            {
                do_exit(-E_KILLED);
            }
            if (current->need_resched)
            {
                schedule();
            }
        }
    }
}
