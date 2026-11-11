# Lab4 进程创建与初始化

2413507（成员乙）

## 模块在整条执行链中的位置

Lab4 的启动链是 `kern_init -> pmm_init -> idt_init -> vmm_init -> proc_init -> clock_init -> cpu_idle`。在 `proc_init` 之前，内核已经有物理页分配、Sv39 内核页表、`kmalloc` 和 Trap 入口，但整个系统只有一条从 `kern_entry` 开始的执行流，没有“线程”这个对象。本模块的任务是把这条执行流登记为 0 号线程 `idleproc`，再创建第一个真正独立的内核线程 `initproc`，交给成员丙的调度器去运行。

模块对前后两侧的依赖如下：

- 向下依赖 `kmalloc/kfree` 分配 PCB，依赖 `alloc_pages/free_pages` 分配内核栈，依赖 `boot_pgdir_pa` 作为线程的页表根。
- 向上提供 `proc_list`、PID 哈希表和处于 `PROC_RUNNABLE` 的 PCB。丙的 `schedule` 从 `proc_list` 中挑选线程，`proc_run` 用 `pgdir` 写 satp，`switch_to` 用 `context` 恢复寄存器。
- 新线程第一次运行时要穿过 `forkret -> forkrets -> __trapret -> kernel_thread_entry`。这条链用到的 `context` 和 trapframe 都由本模块布置。

需要实现的函数：

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
```

全部代码位于 `code/kern/process/proc.c` 的 `//LAB4 function2 2413507` 区域，没有改动其他成员的函数。

## 实现思路

六个函数可以按“造零件，再组装”的顺序理解：`alloc_proc` 生成空白 PCB，`kernel_thread` 准备首次运行现场，`do_fork` 把 PCB、内核栈、地址空间和现场组装成可调度的线程，`copy_mm` 与 `copy_thread` 是 `do_fork` 的两个步骤，`proc_init` 最后用这些函数建立 idle 和 init。

### 1. alloc_proc：先得到一个安全的空白 PCB

`struct proc_struct` 有十几个字段，后续代码会根据其中一些字段做判断：`schedule` 看 `state`，`find_proc` 看 `pid`，`forkret` 用 `tf`，`proc_run` 用 `pgdir`。任何一个字段带着 `kmalloc` 返回的旧数据，都可能让线程在没准备好时被误用。因此 `alloc_proc` 先整体清零，再单独设置非零的初值：

```c
memset(proc, 0, sizeof(struct proc_struct));
proc->state = PROC_UNINIT;
proc->pid = -1;
proc->pgdir = boot_pgdir_pa;
list_init(&(proc->list_link));
list_init(&(proc->hash_link));
```

- `state = PROC_UNINIT` 表示 PCB 还在构造中，调度器只选 `PROC_RUNNABLE`，所以不会选中它。
- `pid = -1` 是一个不可能被分配的值。PID 必须由 `get_pid` 在登记时分配，这里不提前占用。
- `pgdir` 填 `boot_pgdir_pa`，即内核根页表的物理地址。丙的 `proc_run` 用 `(8 << 60) | (pgdir >> 12)` 拼出 satp，所以这里不能填虚拟地址 `boot_pgdir_va`，也不能填完整的 satp 值。
- 两个链表节点初始化为指向自己。这样即使出错路径对它调用 `list_del`，也不会破坏其他进程的链表。

`kmalloc` 失败时直接返回 NULL，此时还没有修改任何全局结构，不需要清理。

### 2. kernel_thread：把“函数加参数”变成一份 trapframe

内核线程要执行的是一个 C 函数 `fn(arg)`，但调度器只会恢复寄存器，不会直接调用函数。框架的做法是借用 Trap 返回路径：先伪造一份“好像刚从中断进入内核”的 trapframe，让 `__trapret` 把它恢复到寄存器里，再用 `sret` 跳到入口代码。入口代码 `kernel_thread_entry` 只有三条指令：

```asm
kernel_thread_entry:
    move a0, s1
    jalr s0
    jal do_exit
```

它把 `s1` 当参数、`s0` 当函数地址。所以 `kernel_thread` 按这个约定填写 trapframe：

```c
memset(&tf, 0, sizeof(struct trapframe));
tf.gpr.s0 = (uintptr_t)fn;
tf.gpr.s1 = (uintptr_t)arg;
tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
tf.epc = (uintptr_t)kernel_thread_entry;
return do_fork(clone_flags | CLONE_VM, 0, &tf);
```

`status` 的三个位决定 `sret` 之后的状态：

- `SPP = 1`：`sret` 返回到 S 态。内核线程运行在 S 态，不能回到 U 态。
- `SPIE = 1`：`sret` 会把 SPIE 复制到 SIE，新线程开始运行后中断是开的，时钟中断可以发生。
- `SIE = 0`：`__trapret` 的 `RESTORE_ALL` 先写 `sstatus`，再逐个恢复通用寄存器，最后恢复 `sp`。如果这里 SIE 为 1，恢复到一半时就可能进中断，而此时 `sp` 还指向 trapframe。清掉 SIE 保证中断只在 `sret` 这一刻打开。

这里的 `tf` 是 `kernel_thread` 栈上的局部变量，函数返回后就失效。`do_fork` 必须把它复制到新线程自己的内核栈上，这是 `copy_thread` 的工作。`stack` 参数传 0，表示内核线程没有调用者指定的栈。`CLONE_VM` 表示与当前进程共享地址空间，内核线程本来就共用内核页表。

### 3. do_fork：组装资源，最后才登记

`do_fork` 的核心问题是顺序：哪些步骤可能失败，失败时要释放什么，什么时候让线程对调度器可见。实现按下面的顺序进行：

```text
检查 nr_process 上限       失败: -E_NO_FREE_PROC，什么都没分配
alloc_proc                 失败: -E_NO_MEM，什么都没分配
proc->parent = current
setup_kstack               失败: kfree(proc)
copy_mm                    失败: put_kstack, kfree(proc)
copy_thread                不会失败
关中断 {
    get_pid, hash_proc, list_add, nr_process++, wakeup_proc
}
返回 pid
```

可能失败的步骤都放在登记之前。失败时线程还没有 PID，也不在任何链表里，只需按申请的逆序释放内存：

```c
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
```

两个标签上下相连，从 `copy_mm` 失败跳到上面的标签时，会先释放栈再释放 PCB；从 `setup_kstack` 失败跳到下面的标签时，只释放 PCB。这样每种失败只释放它已经拿到的资源。

登记阶段放在 `local_intr_save` 与 `local_intr_restore` 之间，原因有两个：

- `get_pid` 通过扫描 `proc_list` 判断某个 PID 是否被占用。如果分配完 PID、还没 `list_add` 时被打断，另一处创建线程的代码会扫到同样的空闲 PID。把分配和挂链放在同一临界区，就不会出现重复 PID。
- `wakeup_proc` 把状态改为 `PROC_RUNNABLE` 是最后一步。在此之前，trapframe、`context`、PID 和链表都已就绪，调度器看到它时它一定能运行。

`nr_process` 在临界区内加一，与链表的变化同时发生，所以失败路径不需要回退计数。

### 4. copy_mm：明确只支持内核线程

Lab4 只创建内核线程，它们的 `mm` 为 NULL，共用内核页表。`copy_mm` 在 `current->mm == NULL` 时让子线程的 `mm` 也为 NULL 并返回 0。

如果 `current->mm` 不为 NULL，说明调用者是一个带用户地址空间的进程。复制或共享用户地址空间要到 Lab5 才实现，这里返回 `-E_INVAL`，让 `do_fork` 走回滚路径。直接返回 0 会让调用者以为地址空间已经复制好，子进程运行时才会出错，问题更难定位。

### 5. copy_thread：在新内核栈上布置首次运行现场

`copy_thread` 把 trapframe 放在新内核栈的最高处，并设置 `context`：

```c
proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
*(proc->tf) = *tf;
proc->tf->gpr.a0 = 0;
proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;
proc->context.ra = (uintptr_t)forkret;
proc->context.sp = (uintptr_t)(proc->tf);
```

- 第一行利用指针减一，得到“栈顶减去一个 trapframe 大小”的地址。`kstack` 按页对齐，trapframe 为 36 × 8 = 288 字节，是 16 的倍数，所以 `tf` 本身满足 RISC-V 调用约定要求的 16 字节栈对齐。代码中用 `_Static_assert` 在编译期固定了这个前提。
- `a0 = 0` 是 fork 语义：子进程看到的返回值为 0。内核线程的 `a0` 随后会被 `kernel_thread_entry` 覆盖为参数，这一行主要为后续的用户态 fork 保留语义。
- `gpr.sp` 是 `__trapret` 最后恢复的栈指针。内核线程的 `esp` 为 0，如果原样写入，线程进入 `fn` 时栈指针为 0，第一次压栈就会出错。这里改为 trapframe 的地址：`sret` 之后 trapframe 已无用，它下面的空间正好作为线程的运行栈。
- `context.ra = forkret`，`context.sp = tf`。丙的 `switch_to` 恢复 `ra` 和 `sp` 后执行 `ret`，就会进入 `forkret`。

`context` 和 trapframe 的分工在这里很清楚：`context` 只有 `ra`、`sp`、`s0` 到 `s11`，是函数调用意义上的切换现场，用于 `switch_to`；trapframe 是全部 32 个通用寄存器加 4 个 CSR，用于 `sret`。新线程两者都需要：先用 `context` 进入 `forkret`，再用 trapframe 返回到 `kernel_thread_entry`。

### 6. 首次运行链

把前面几步连起来，新线程第一次被调度时的路径是：

```text
schedule -> proc_run -> switch_to(&prev->context, &proc->context)
  ret 到 context.ra = forkret，sp = tf
forkret -> forkrets(current->tf)          sp <- tf
__trapret: RESTORE_ALL                   写 sstatus/sepc，恢复 s0=fn, s1=arg, sp
sret                                     pc <- kernel_thread_entry，SIE <- SPIE
kernel_thread_entry: a0 <- s1; jalr s0   执行 fn(arg)
```

### 7. proc_init：建立 idle 和 init

`proc_init` 先初始化 `proc_list` 和 1024 个哈希桶，然后处理两个特殊线程。

`idleproc` 不是 fork 出来的。它就是正在执行 `proc_init` 的这条启动执行流，已经有自己的栈 `bootstack`，所以只需要一个 PCB 来描述它：

```c
idleproc->pid = 0;
idleproc->state = PROC_RUNNABLE;
idleproc->kstack = (uintptr_t)bootstack;
idleproc->need_resched = 1;
set_proc_name(idleproc, "idle");
nr_process++;
current = idleproc;
```

- 不为 idle 调用 `setup_kstack`，它的栈是 `entry.S` 中定义的 `bootstack`。
- `need_resched = 1` 让 `kern_init` 最后进入的 `cpu_idle` 立刻调用 `schedule`，把 CPU 交给 init。
- idle 不挂入 `proc_list` 和哈希表。丙的 `schedule` 只在 `proc_list` 中找不到可运行线程时才回到 idle，如果 idle 也在链表里，就会和普通线程一起轮转。
- idle 的 `tf`、`context` 和 `runs` 保持为 0。它第一次被切走时，`switch_to` 会把当时的寄存器保存到 `context`。

然后用 `kernel_thread(init_main, "Hello world!!", 0)` 创建 init。此时 `current` 已是 idle，所以 init 的 `parent` 是 idle；`get_pid` 在空链表上从 1 开始分配，init 得到 PID 1。最后通过 `find_proc` 取回 PCB 并命名为 `init`。创建失败时调用 `panic`，避免内核带着空的 `initproc` 进入 `cpu_idle` 空转。

## 规格与提示词

本模块采用 [PROMPT] / [RELY] / [GUARANTEE] / [SPECIFICATION] 四段式规格，按 7 个阶段推进，每个阶段单独提交。下面是 `do_fork` 阶段规格的摘录，完整提示词由组内统一整理到 `prompt.md`。

````markdown
[PROMPT]
完成从“指定内核函数及参数”到“已登记、可被调度的内核线程”的闭环。必须联合设计和验证
kernel_thread / do_fork / copy_thread / copy_mm。只修改 proc.c 的 B 区域，复用已有 get_pid、
setup_kstack、put_kstack、hash_proc、forkret、kernel_thread_entry、wakeup_proc。

[RELY]
trap.h 的 struct trapframe；entry.S 的 kernel_thread_entry（s1 为参数、s0 为函数地址）；
trapentry.S 的 forkrets -> __trapret；CLONE_VM、KSTACKSIZE、boot_pgdir_pa；
-E_NO_MEM、-E_NO_FREE_PROC；SSTATUS_SPP/SPIE/SIE。所有原型和宏从真实源码核对。

[GUARANTEE]
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);

[SPECIFICATION]
Pre-Condition：已有 current 和可用 PCB/物理页分配器；传入的 Trap Frame 有效。
Post-Condition：成功时返回唯一正 PID，线程具有独立 PCB、独立内核栈、正确父进程、页表和首次
执行现场，已挂入进程链表及 PID 哈希表，进程计数准确且状态可调度；失败时返回负错误码、
释放所有本次申请的资源、全局状态不变。
Case 2：进程上限返回 -E_NO_FREE_PROC，不改变计数。
Case 3：PCB、内核栈或 mm 处理失败，按已获得资源的逆序清理，无成功 PID。
Requirements：PID 分配、链表/哈希挂接、nr_process 更新及状态转移受中断临界区保护；
初始化完成前不能让调度器选中。
````

规格写法上有三点考虑：

- [RELY] 要求核对真实源码，而不是沿用经验写法。例如 `kernel_thread_entry` 用哪个寄存器传参、`forkrets` 如何设置 `sp`，都由仓库中的汇编决定。
- [SPECIFICATION] 把失败路径单独列为 Case，并写明“全局状态不变”。这句话可以直接转成测试断言：失败前后比较 `nr_process`、链表长度、空闲页数和 `kmalloc` 计数。
- 接口约定写进规格。`pgdir` 是物理地址、idle 不进 `proc_list`、`context.sp` 16 字节对齐，这三条是乙和丙两个模块能拼起来的前提。

## 开发过程中的问题

六个函数在第一次实现后就通过了编译、启动和正式评分，没有出现需要修改实现代码的功能错误。实际遇到的问题出在评分环境和测试本身。

**问题 1：正式评分 0 分，原因是编译器版本。** 本机原有的 `riscv64-unknown-elf-gcc` 是 15.1.0，`make grade` 输出 `Total Score: 0/30`。日志显示评分用的测试内核没有编译成功：

```text
kern/test/test_lab4.c:90:26: error: passing argument 1 of 'kernel_thread' from incompatible pointer type [-Wincompatible-pointer-types]
note: expected 'int (*)(void *)' but argument is of type 'void (*)(void *)'
```

官方测试中的 `test_lab4_thread` 返回 `void`，而 `kernel_thread` 要求返回 `int` 的函数。GCC 14 起把这种不兼容的函数指针转换默认当作错误，较早的版本只给出警告。这不是进程模块的问题，也不应该通过修改测试代码或关闭编译检查来解决。我们按实验指导书的推荐下载了 SiFive 预编译工具链（GCC 10.1.0），并用 direnv 只在本实验目录中启用它，其他课程目录仍使用原来的编译器。换用后同一份代码评分为 30/30。

**问题 2：测试没有覆盖 SIE 位。** 为了检验测试本身是否有效，我们在代码副本中逐个注入 9 种典型错误，例如不回滚 PCB、内核线程 `sp` 为 0、idle 挂进链表等，要求每种错误都让测试失败。第一轮中“`kernel_thread` 不清除 SIE”这一错误没有被发现。原因是 `proc_init` 运行时中断还没打开，`read_csr(sstatus)` 读到的 SIE 本来就是 0，去掉 `& ~SSTATUS_SIE` 结果不变。于是在 `init_main`（此时中断已开）里补充了一次创建线程的检查，之后 9 种错误全部被检出。

## 测试与验证

验证分四个层次，每一层的结论不能代替另一层。

| 层次 | 方法 | 结果 |
| --- | --- | --- |
| 编译 | `make` | 通过 |
| 模块测试 | 测试代码在临时副本中被 include 到真实 `proc.c` 末尾，可以直接调用 static 函数；用链接选项 `--wrap` 让 `kmalloc`、`alloc_pages` 按需失败；在 QEMU 中运行 | 61 项全部通过 |
| 测试有效性 | 注入 9 种错误，检查测试能否发现 | 9 种全部被发现 |
| 正式评分 | `make grade` | 30/30 |

模块测试覆盖的内容：

- `alloc_proc` 每个字段的初值、两个链表节点自环、`pgdir` 是页对齐的物理地址；`kmalloc` 失败时返回 NULL。
- idle 和 init 的 PID、状态、栈、名字、父子关系；idle 不在链表和哈希表中；init 能被 `find_proc(1)` 找到。
- 连续创建 3 个线程，PID 互不相同且连续，内核栈互不重叠，`nr_process` 计数正确。
- 进程数达到上限、PCB 分配失败、内核栈分配失败、`copy_mm` 拒绝用户 mm 四种失败，返回值正确，`nr_process`、链表长度、空闲页数和 `kmalloc` 计数与失败前一致，失败不消耗 PID。
- trapframe 位于栈顶且 16 字节对齐，`context.ra` 为 `forkret`，`epc`、`s0`、`s1`、`status` 符合 `kernel_thread_entry` 的约定。
- 线程真正运行后，在线程内部读取 `sp`、`satp`、`sstatus`：`sp` 在自己的内核栈内且 16 字节对齐，参数正确传入，satp 指向内核根页表，中断已打开。

正式评分输出：

```text
  -check PMM:                                OK
  -check Process Create:                     OK
  -check Schedule:                           OK
Total Score: 30/30
```

`make qemu` 的输出末尾：

```text
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
kernel panic at kern/process/proc.c:208:
    process exit!!.
```

`init_main` 返回后进入 `kernel_thread_entry` 的 `jal do_exit`，而 `do_exit` 在本实验框架中只有一句 `panic`，所以最后的 panic 是预期行为。

为了直接观察首次运行链，我们用 GDB 在 `proc_run`、`forkret`、`kernel_thread_entry`、`init_main` 四处设断点，记录 idle 切换到 init 的过程：

```text
== proc_run(next): idle -> init
next->pgdir=0x80208000 next->kstack=0xffffffffc0410000 next->tf=0xffffffffc0411ee0
next->context.ra=0xffffffffc02031d6 (forkret=0xffffffffc02031d6)  next->context.sp=0xffffffffc0411ee0
== forkret (after switch_to)
current pid=1  sp=0xffffffffc0411ee0  satp=0x8000000000080208
tf->status=0x8000000000046120
== kernel_thread_entry (after __trapret/sret)
sp=0xffffffffc0411ee0 sp%16=0 s0=0xffffffffc02031e4 (init_main=0xffffffffc02031e4) s1=Hello world!!
sstatus=0x8000000000046002 SIE=1 SPIE=0 SPP=0
```

这段记录逐项对应前面的设计：

- `pgdir = 0x80208000` 是物理地址，satp 为 `0x8000000000080208`，即 MODE=8（Sv39）加上页号 `0x80208`。
- `tf = 0x...1ee0` 等于栈顶 `0x...2000` 减去 `0x120`（288 字节）。
- `context.ra` 与 `forkret` 地址相同。
- `forkret` 处 trapframe 的 `status` 为 `0x...120`，即 SPP=1、SPIE=1、SIE=0。经过 `sret` 后变为 `0x...002`，SIE=1，SPP=0，与 `sret` 的定义一致。
- 进入 `kernel_thread_entry` 时 `s0` 等于 `init_main`，`s1` 指向 `"Hello world!!"`。

测试脚本、原始日志和复现命令位于 `code/tools/member-b/`。

## Challenge 技术复核

按分工，乙负责复核全组的 Challenge 答案。复核以源码和实测为依据，结论如下。

**中断开关（丙负责）。** 报告对 `__intr_save` / `__intr_restore` 的说明正确：只有原来开着中断的那一层负责重新打开，嵌套时内层不会提前开中断。有一处表述需要修正：报告说宏用 `do { } while (0)` 包成一条语句，这只对 `local_intr_save` 成立。`local_intr_restore` 的定义是 `#define local_intr_restore(x) __intr_restore(x);`，自带分号，写在没有花括号的 `if ... else` 中会导致语法错误。当前代码没有这样的用法，不影响运行。

另外可以补充一点：新线程第一次运行时，中断不是由 `local_intr_restore` 打开的。`schedule` 关中断后切换到新线程，新线程从 `forkret` 开始执行，永远不会回到旧线程栈上的那句 `local_intr_restore`。它的中断由 `sret` 按 trapframe 中的 SPIE 打开，上面的 GDB 记录显示了这个变化。这也是 `kernel_thread` 必须设置 SPIE 的原因：如果不设置，新线程在 `sret` 之后中断仍是关闭的。

**分页模式与 `get_pte`（甲负责）。** `get_pte` 中两段相似的代码分别处理 Sv39 的第 2 级和第 1 级页表：检查页表项是否有效，无效时分配一页、清零、写入指向新页的页表项。多级页表每一级的结构相同，都是一页大小的页表项数组，用虚拟地址中的 9 位作下标，所以处理代码也相同。Sv32 只有两级，需要一段；Sv48 有四级，需要三段。这两段代码本质上是一个按级展开的循环。

复核中发现一处与规范不一致：`get_pte` 写中间级页表项时设置了 `PTE_U | PTE_V`。RISC-V 特权级规范规定，非叶子页表项的 U、A、D 位保留，软件应写 0。在 QEMU 4.1.1 上 `check_pgdir` 和评分都通过，没有观察到错误，但建议甲在报告中说明。

关于查找与分配是否拆开：合在一起时，调用者用一个 `create` 参数就能选择“只查”或“查不到就建”，接口简单。代价是返回 NULL 时无法区分“没有映射”和“内存不足”，而且一个函数既可能只读页表、又可能修改页表。Lab4 的使用场景不多，保留现有写法即可；以后实现换页或写时复制时，拆成只读的查找函数和按需建表的函数会更清楚。

## 实验知识点与 OS 原理的对应

| 实验中的内容 | OS 原理中的概念 | 关系与差异 |
| --- | --- | --- |
| `struct proc_struct` | 进程控制块 PCB | 原理中的 PCB 包含状态、寄存器、内存信息、打开文件等。本实验的 PCB 只有调度和创建需要的字段，`mm` 和文件相关内容留到后续实验。 |
| `PROC_UNINIT` / `PROC_RUNNABLE` | 进程状态模型中的新建态、就绪态 | 原理中区分就绪和运行；uCore 用 `PROC_RUNNABLE` 同时表示二者，用 `current` 区分正在运行的那个。 |
| `do_fork` | fork 系统调用 | 原理中 fork 复制父进程的地址空间；本实验只创建共享内核地址空间的内核线程，`copy_mm` 不做复制。 |
| 内核线程共享 `boot_pgdir_pa` | 线程共享所属进程的地址空间 | 本实验的所有内核线程可视为同属“内核”这一个进程的多个线程，各有独立的栈和寄存器现场。 |
| 每个线程独立的 `kstack` | 线程私有栈 | 栈保存函数调用链和局部变量，必须私有；地址空间可以共享。测试中检查了各线程栈不重叠。 |
| `context` 与 trapframe | 上下文切换与中断现场保存 | `context` 是主动调用 `switch_to` 时保存的现场，只需保存被调用者保存寄存器；trapframe 是中断随时发生时的现场，必须保存全部寄存器。新线程借用 trapframe 和 `sret` 完成第一次启动。 |
| `idleproc` | 空闲进程 | 原理中空闲进程在没有就绪进程时占用 CPU。本实验中它还是启动执行流本身，PID 为 0，不参与普通调度。 |
| `get_pid` 与临界区 | PID 分配、临界区与互斥 | 分配 PID 和登记进程必须是一个不可分割的操作。本实验是单核，关中断即可保证；多核系统需要锁。 |
| `do_fork` 失败回滚 | 资源分配的原子性 | 进程创建要么全部成功，要么不留下任何部分结果，类似事务的思想。 |

## 本实验未覆盖的重要知识点

- **进程退出与回收。** `do_exit` 在本实验中只有 `panic`，没有僵尸状态、父进程 `wait`、孤儿进程交给 init 收养等机制，PCB 和内核栈创建后不会被释放。
- **用户进程与特权级切换。** 所有线程都在 S 态运行，没有用户地址空间、系统调用和 U 态与 S 态之间的切换。
- **地址空间复制与写时复制。** `copy_mm` 只处理内核线程，没有页表复制，也没有 Copy-on-Write 的缺页处理。
- **调度策略与抢占。** 只有按链表顺序的简单选择，没有时间片、优先级和多级反馈队列，时钟中断也没有用来触发抢占。
- **多核同步。** 关本地中断只能保护单核上的临界区，多核下需要自旋锁等同步原语。
- **线程间同步。** 没有信号量、条件变量等机制，也没有线程的阻塞和唤醒场景。

---

# Lab4 调度与上下文切换

马传志 2411788

## 整体流程

线程创建模块准备 PCB、独立内核栈和首次运行现场后，cpu_idle 检查调度标志，schedule 选择下一个可运行线程，proc_run 切换 current 和页表，switch_to 完成寄存器现场切换。新线程第一次通过预构造的 ra 进入 forkret，再经 forkrets、__trapret 和 sret 到达 kernel_thread_entry。后续已经运行过的线程，则从上次 switch_to 的返回位置继续执行。

本次实现后三步切换及 idle 循环。首次运行链的正确性还依赖创建模块与 Trap 恢复代码，不能仅凭独立测试推断整条链已经通过。

## 核心机制

调度选择采用链表顺序查找，从当前节点之后扫描并回环，过滤非就绪状态；没有其他可运行线程时可继续当前线程，没有任何普通就绪线程时选择 idle。它具有轮转选择特点，但未实现时间片管理，不宣称具备抢占式轮转调度。

上下文只保存 ra、sp、s0-s11，因为 switch_to 是按照函数调用约定进入的切换边界。与任意指令处发生的 Trap 不同，调用者已负责跨普通函数调用需要保留的 caller-saved 值。每个字段偏移通过编译期断言与汇编对应；RV64 上结构为 112 字节。

写 satp 时使用物理页表根的页号并设置 MODE=8；TLB 使用 sfence.vma 失效。切换前后共享内核映射是继续执行切换代码的前提。不能直接把完整物理地址或虚拟地址塞入 satp，也不能使用参数截断为 32 位的 Sv32 遗留接口。

## Challenge 中断开关

local_intr_save 调用 __intr_save：读取 sstatus.SIE，原来开启则关闭并返回真，否则返回假。local_intr_restore 根据保存的标志决定是否重新开启。作用是临时保护本 hart 的临界区，并保留调用前状态，而非无条件关闭后又无条件开启。

嵌套调用时，外层保存真并关闭，内层发现已关闭而保存假；内层恢复时不重新开启，直到外层恢复才开启。因此不能用 intr_enable 简单替代每次 restore。宏使用 do while(0) 包成一条语句，避免多语句展开影响 if/else。

schedule 和 proc_run 均保护各自关键区域。switch_to 返回并不是目标线程从调用点直接返回，而是原线程再次被恢复后继续原调用栈，所以原中断标志仍属于恢复后的旧线程。关本地中断不能屏蔽同步异常，也不能替代多核锁；当前实现以单 hart 为前提。

## 实际验证

QEMU 版本为本机已有 4.1.1。M 态和 S 态独立环境各通过 12 项检查，日志位于 code/tools/member-c/evidence/c-runtime.log 和 c-runtime-s.log。S 态环境使用三个不同物理页表根，各自提供相同的 RAM/MMIO 恒等映射，验证实际 satp 切换及切换后继续执行。它不验证不同地址空间中具有不同物理目标的映射隔离。

12 项检查为：空表回退 idle、跳过全部非就绪状态、仅当前可运行、三个线程往返并睡眠返回 idle、跨表头顺序、原中断关闭状态保持、s0-s11 哨兵和栈返回、原中断开启状态恢复、嵌套内层恢复、嵌套外层恢复、proc_run(current) 不切换、cpu_idle 触发调度。寄存器哨兵由汇编设置并比较，避免只依赖 C 局部变量被编译器优化后的结果。

GDB 在真实 switch_to 首条保存指令断下，执行 14 条保存后比较旧 SP/RA，执行 14 条加载后比较目标 SP/RA，四次比较均为 1。见 code/tools/member-c/evidence/gdb-switch.log。独立测试没有使用 A/B 创建函数，没有模拟正式评分输出。

## 问题与边界

测试 ELF 首次链接缺 memset，原因是结构清零被编译为库调用；加入项目已有 string.c 后通过。正式整包 make 则首先因 best_fit_pmm.h 缺失失败，这是旧实验继承内容尚未补齐，与 C 的三个文件编译通过是不同层面的结论。

原包缺 tools/inject.py，而 make grade 依赖该脚本。当前没有修改评分脚本，也没有正式评分结果。待 A/B 及旧实验继承完成、官方注入脚本补齐后，需要再验证完整启动、首次线程入口、正常输出和正式 30 分测试。

本实验尚不等于完整多进程系统：没有验证用户态进程、不同用户地址空间隔离、多核调度、时间片抢占、进程等待与回收；原 do_exit 的 panic 占位保留。


## C 部分实现规格

# 调度与上下文切换规格

前提为单 hart RV64 Sv39 内核，PCB、内核栈和内核映射已正确初始化。所有可运行线程的 pgdir 都必须包含切换函数、旧栈及目标栈可用的内核映射；页表根为页对齐物理地址。关本地中断不提供多核互斥。

cpu_idle 不返回，只在 need_resched 为真时调用 schedule。当前采用忙等，不引入 wfi 唤醒顺序和新中断源的额外要求。

schedule 保存并关闭本地 S 态中断，清 current->need_resched。从当前普通线程之后开始查找，遇链表头继续回环；idle 从表头开始。跳过所有非 PROC_RUNNABLE 状态，遍历一圈可选中仍可运行的 current，找不到则回退 idleproc。选中线程 runs 加一，不为此创建 PROC_RUNNING 新状态。只在目标不同于 current 时调用 proc_run，重新得到执行权后恢复原中断状态。

proc_run 的入参非空、可运行且 pgdir 对齐；目标等于 current 时不切换。实际切换期间关闭中断，将旧 current 记为 prev，更新 current，写入 Sv39 satp，执行带 memory 屏障的 sfence.vma，再调用 switch_to。没有分配 ASID，因此采用全局本地 TLB 失效。函数从 switch_to 继续执行时，意味着这个旧线程已被再次调度；此时才恢复该调用栈保存的中断标志。

switch_to 依据 context 的 14 个字段顺序保存旧 ra、sp、s0-s11 并加载新值，最后 ret。普通函数调用 ABI 负责调用者需要跨调用保持的其他值。它不是 Trap 入口，不能照搬 36 槽现场，也不负责 scause、sepc 或调度策略。编译期检查覆盖所有字段偏移和结构大小。

验收包括调度顺序、非就绪过滤、空链表、仅当前可运行、真实多线程往返、寄存器哨兵、satp 写入、中断状态及嵌套保护。全内核验收另行依赖 A/B 与继承代码。
