#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>
#include <sbi.h>

static uint32_t print_cnt = 0;
#define TICK_NUM 100
#define SHUTDOWN_AFTER 10  // shut down after printing "100 ticks" 10 times

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* LAB3 C 2412096: 依据实际指令编码判断异常指令长度。 */
/* RISC-V compressed instructions have low bits other than 0b11. */
static size_t trap_instruction_length(uintptr_t epc) {
    const volatile uint16_t *instruction = (const volatile uint16_t *)epc;
    return ((*instruction & 0x3) == 0x3) ? 4 : 2;
}

/* idt_init - point stvec at the single trap entry __alltraps
 * (the name is inherited from x86 ucore; RISC-V has no IDT/lidt).
 */
void idt_init(void) {
    /* LAB3 A 2411788: STEP 2，安装统一 Trap 入口。 */
    /* stvec uses Direct mode: __alltraps is 4-byte aligned, so its low two
     * bits (MODE) are 0 and every interrupt/exception enters __alltraps.
     */

    extern void __alltraps(void);
    /* Set sup0 scratch register to 0, indicating to exception vector
       that we are presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}

void print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr) {
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

/* LAB3 B 2413507: 按中断原因输出特权级和类型。 */
static const char *const irq_level[] = {"User", "Supervisor", "Hypervisor", "Machine"};
static const char *const irq_type[] = {"software", "timer", "external"};

void interrupt_handler(struct trapframe *tf) {
    // scause bit 63 is the interrupt flag; clear it to get the cause code
    intptr_t cause = (tf->cause << 1) >> 1;
    switch (cause) {
        case IRQ_U_SOFT: case IRQ_S_SOFT: case IRQ_H_SOFT: case IRQ_M_SOFT:
        case IRQ_U_TIMER:                 case IRQ_H_TIMER: case IRQ_M_TIMER:
        case IRQ_U_EXT:  case IRQ_S_EXT:  case IRQ_H_EXT:  case IRQ_M_EXT:
            // codes 0-11 are 4 * type + privilege level (U/S/H/M)
            cprintf("%s %s interrupt\n", irq_level[cause & 3], irq_type[cause >> 2]);
            break;
        case IRQ_S_TIMER:
            // "All bits besides SSIP and USIP in the sip register are
            // read-only." -- privileged spec1.9.1, 4.1.4, p59
            // In fact, Call sbi_set_timer will clear STIP, or you can clear it
            // directly.
            // cprintf("Supervisor timer interrupt\n");
            /* LAB3 EXERCISE1 B 2413507: 时钟事件、计数和关机。 */
            /*(1)设置下次时钟中断- clock_set_next_event()
             *(2)计数器（ticks）加一
             *(3)当计数器加到100的时候，我们会输出一个`100ticks`表示我们触发了100次时钟中断，同时打印次数（num）加一
            * (4)判断打印次数，当打印次数为10时，调用<sbi.h>中的关机函数关机
            */
            clock_set_next_event();          // (1) 经 SBI 预约下一次时钟事件，并清除 STIP
            ticks++;                         // (2) 每次时钟中断计数一次

            if (ticks % TICK_NUM == 0) {     // (3) 每 100 次打印一次
                print_ticks();
                print_cnt++;
                if (print_cnt >= SHUTDOWN_AFTER) {  // (4) 打印 10 行后关机
                    cprintf("System shutdown after %d prints.\n", SHUTDOWN_AFTER);
                    sbi_shutdown();
                }
            }
            // 中断的 sepc 指向被打断的指令，不修改 tf->epc
            break;
        default:
            print_trapframe(tf);
            break;
    }
}

/* LAB3 C 2412096: 异常分类、返回地址调整和兜底输出。 */
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        case CAUSE_ILLEGAL_INSTRUCTION:
             // 非法指令异常处理
             /* LAB3 C 2412096: 非法指令异常处理。 */
            /*(1)输出指令异常类型（ Illegal instruction）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Exception: Illegal instruction\n");
            cprintf("  Bad instruction address: 0x%016llx\n", (unsigned long long)tf->epc);
            tf->epc += trap_instruction_length(tf->epc); //跳过触发异常的指令
            break;
        case CAUSE_BREAKPOINT:
            //断点异常处理
            /* LAB3 C 2412096: 断点异常处理。 */
            /*(1)输出指令异常类型（ breakpoint）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Exception: Breakpoint\n");
            cprintf("  Bad instruction address: 0x%016llx\n", (unsigned long long)tf->epc);
            tf->epc += trap_instruction_length(tf->epc); //跳过触发断点的指令
            break;
        case CAUSE_MISALIGNED_FETCH:
        case CAUSE_FAULT_FETCH:
        case CAUSE_MISALIGNED_LOAD:
        case CAUSE_FAULT_LOAD:
        case CAUSE_MISALIGNED_STORE:
        case CAUSE_FAULT_STORE:
        case CAUSE_USER_ECALL:
        case CAUSE_SUPERVISOR_ECALL:
        case CAUSE_HYPERVISOR_ECALL:
        case CAUSE_MACHINE_ECALL:
        default:
            print_trapframe(tf);
            break;
    }
}

static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the sret instruction to return from the trap.
 * */
void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
