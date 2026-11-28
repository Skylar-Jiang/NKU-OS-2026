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
#include <proc.h>

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

//lab6 function1-2 YOUR CODE update interrupt_handler

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
        // Keep the timer firing and count the tick. An interrupt resumes the
        // interrupted instruction, so epc is left unchanged.
        clock_set_next_event();
        ticks++;
        break;
    default:
        print_trapframe(tf);
        break;
    }
}

void kernel_execve_ret(struct trapframe *tf, uintptr_t kstacktop);


//lab5 请补充你在lab5中的代码

static size_t
trap_instruction_length(uintptr_t epc)
{
    const volatile uint16_t *instruction = (const volatile uint16_t *)epc;
    return ((*instruction & 0x3) == 0x3) ? 4 : 2;
}

/* kernel_exec_breakpoint_size - size of the kernel_execve ebreak, or 0.
 * kernel_execve marks its ebreak with a7 = T_SYSCALL and a0 = SYS_exec;
 * an unmarked breakpoint stays an ordinary breakpoint. */
static size_t
kernel_exec_breakpoint_size(struct trapframe *tf)
{
    if (!trap_in_kernel(tf) || tf->gpr.a7 != T_SYSCALL ||
        tf->gpr.a0 != SYS_exec)
    {
        return 0;
    }
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

/* page_fault_exit - an unrepairable page fault.
 * Lab6 forks with private page copies (no COW), so no page fault is ever
 * repaired: retrying the same epc would loop forever. A fault taken by an
 * ordinary user process terminates only that process; a kernel fault or a
 * fault in idleproc/initproc is fatal. */
static void __noreturn
page_fault_exit(struct trapframe *tf, const char *kind)
{
    print_trapframe(tf);
    cprintf("%s page fault: address=0x%016llx, epc=0x%016llx, error=%d\n",
            kind, (unsigned long long)tf->tval,
            (unsigned long long)tf->epc, -E_FAULT);
    if (trap_in_kernel(tf) || current == NULL || current == idleproc ||
        current == initproc || current->state != PROC_RUNNABLE)
    {
        panic("unrecoverable %s page fault", kind);
    }
    do_exit((current->flags & PF_EXITING) ? -E_KILLED : -E_FAULT);
    panic("do_exit returned after an unhandled page fault");
}

void exception_handler(struct trapframe *tf)
{
    switch (tf->cause)
    {
    case CAUSE_FETCH_PAGE_FAULT:
        page_fault_exit(tf, "Instruction");
    case CAUSE_LOAD_PAGE_FAULT:
        page_fault_exit(tf, "Load");
    case CAUSE_STORE_PAGE_FAULT:
        page_fault_exit(tf, "Store");
    case CAUSE_USER_ECALL:
        /* ECALL is always 32 bits. Advance before a handler can replace epc
         * (a successful exec rewrites the whole frame). */
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
