# 操作系统实验报告

Lab4：进程管理

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| 实验名称 | Lab4：进程管理 |
| 小组成员 | 2411788-马传志、2413507-蒋林瀞、2412096-王麒萱 |
| 完成日期 | 2026.11.11 |

### 小组分工

练习分工：

| 成员 | 负责的练习/模块 |
| --- | --- |
| A 2412096-王麒萱 | 功能模块一：虚拟内存页操作（`page_insert`、`page_remove`）；Lab2/Lab3 前置代码迁移与集成；Challenge 2 分页模式与 `get_pte` |
| B 2413507-蒋林瀞 | 功能模块二：进程创建与初始化（`alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread`、`copy_mm`）；进程创建测试与首次运行验证；全部 Challenge 答案的技术复核 |
| C 2411788-马传志 | 功能模块三：进程调度（`cpu_idle`、`proc_run`、`schedule`、`switch_to`）；调度与上下文切换的独立测试；Challenge 1 中断开关 |

实验报告分工：每位成员撰写自己负责模块的实现、提示词、迭代过程和测试记录，提示词汇总在 `report/prompt.md`。整体逻辑、集成测试和总结部分由三人共同核对。

## 一、实验目的

Lab2 和 Lab3 之后，内核已经能管理物理内存、建立 Sv39 页表并响应中断，但始终只有一条从启动入口开始的执行流。本实验让内核具备多个执行流轮流使用 CPU 的能力。

本次完成三项内容：

1. 完善页表映射的建立与撤销，理解页表项、物理页引用计数和 TLB 三者如何保持一致。
2. 实现内核线程的创建与初始化，理解进程控制块、内核栈和首次运行现场的组织方式。
3. 实现内核线程的调度与上下文切换，理解调度器如何选择线程，以及切换时保存哪些寄存器。

## 二、实验环境

在 Windows 下的 WSL Ubuntu 中编译、运行和调试，通过 QEMU 模拟 RISC-V 64 位处理器。

| 项目 | 配置 |
| --- | --- |
| 终端环境 | Windows 下的 WSL Ubuntu |
| 目标架构 | RISC-V 64 位，Sv39 分页 |
| 交叉编译器 | SiFive 预编译工具链 riscv64-unknown-elf-gcc 10.1.0（指导书推荐） |
| 调试器 | riscv64-unknown-elf-gdb |
| 运行方式 | QEMU 4.1.1 Virt Machine，OpenSBI v0.4 |

编译器和 QEMU 通过 direnv 只在本实验目录中生效，不影响其他目录的工具版本。不使用 GCC 14 及以上版本的原因见 6.1。

三人使用的 AI 工具如下。

| 成员 | AI 编程工具 | 底层模型 | 备注 |
| --- | --- | --- | --- |
| A 2412096-王麒萱 | Codex | gpt-6.1-sol | 用于前置代码迁移、页表操作实现与集成 |
| B 2413507-蒋林瀞 | Claude Code | Claude Opus 5.5 | 用于基线审查、进程创建实现、QEMU 内测试与故障注入、GDB 脚本 |
| C 2411788-马传志 | Codex | gpt-6.1-sol | 用于调度与切换实现、独立运行测试与 GDB 单步核对 |

## 三、实验整体逻辑与原理

### 3.1 本章的逻辑主线

本章围绕“内核线程”展开：让内核从单一执行流变成可以创建、登记、调度多个线程的系统。启动链为：

```text
kern_init -> pmm_init -> idt_init -> vmm_init -> proc_init -> clock_init -> cpu_idle
                                                    |                          |
                                       创建 idleproc 与 initproc       schedule -> proc_run -> switch_to
```

`proc_init` 之前的部分准备内存和地址空间，`proc_init` 生成线程，`cpu_idle` 之后的部分让线程真正轮流运行。

### 3.2 功能的逐步实现

1. 首先迁移 Lab2/Lab3 的前置代码。线程的 PCB 由 `kmalloc` 分配，内核栈由页分配器分配，线程的首次启动借用 Lab3 的 Trap 返回路径 `__trapret`。没有这些代码，后面的模块无法编译，也无法运行。
2. 接着完成虚拟内存页操作（功能模块一）。`page_insert` 和 `page_remove` 维护页表映射。启动时的 `check_pgdir` 和评分中的 PMM 测试都依赖它们，地址空间正确是线程运行的前提。
3. 然后完成进程创建与初始化（功能模块二）。有了内存分配和页表，才能为线程分配 PCB 和内核栈，并在栈上布置首次运行现场。这一步结束时系统中有 idle 和 init 两个线程，但还没有发生切换。
4. 最后完成调度与上下文切换（功能模块三）。调度器从模块二登记的线程中选择下一个，切换页表和寄存器。新线程第一次被选中时，`switch_to` 恢复的是模块二预先布置的现场。

模块二和模块三之间的接口在开发前就确定下来：`pgdir` 保存页对齐的根页表物理地址；idle 不进入普通线程链表；新线程的 `context.ra` 指向 `forkret`，`context.sp` 16 字节对齐。三人据此并行开发，最后合并时没有修改对方的函数。

### 3.3 两种现场：context 与 trapframe

本实验同时用到两种保存寄存器的结构，区分它们是理解线程切换的关键。

| | `struct context` | `struct trapframe` |
| --- | --- | --- |
| 内容 | `ra`、`sp`、`s0`~`s11`，共 14 个 | 32 个通用寄存器加 `status`、`epc`、`badvaddr`、`cause`，共 36 个 |
| 何时保存 | 线程调用 `switch_to` 主动让出 CPU 时 | 中断或异常在任意指令处发生时 |
| 为何是这些寄存器 | `switch_to` 是一次普通函数调用，调用者保存的寄存器已由调用方负责 | 被打断的代码不知道会被打断，所有寄存器都要保存 |
| 恢复方式 | `switch_to` 加载后 `ret` | `__trapret` 加载后 `sret` |

新线程两者都要用：`switch_to` 先用 `context` 跳到 `forkret`，`forkret` 再用 trapframe 经 `sret` 进入线程函数。模块二负责布置这两份现场，模块三负责使用它们。

## 四、实验过程

### 4.1 前置代码迁移

负责人：A 2412096-王麒萱。

将 Lab2 的页分配器、`page_init` 和 Lab3 的 Trap 入口与处理代码迁入本实验代码中标有 lab2、lab3 的位置。迁移后整包编译时，链接阶段只剩 `proc_init` 未定义，即只缺本实验三个模块的实现。

### 4.2 功能模块一：虚拟内存页操作

负责人：A 2412096-王麒萱。

#### 模块功能描述

需要实现的函数：

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

`page_insert` 把物理页 `page` 映射到线性地址 `la`，`page_remove` 撤销 `la` 上的映射。两个函数都要同时维护三样东西：页表项的内容、物理页的引用计数、TLB 中缓存的旧翻译。

#### 实现说明

撤销一个有效映射的公共逻辑放在辅助函数 `page_remove_pte` 中：清空页表项，使该地址的 TLB 失效，引用计数减一，减到 0 时释放物理页。`page_remove` 用 `get_pte(pgdir, la, 0)` 只查不建，查不到时直接返回，所以对未映射的地址重复删除不会出错。

`page_insert` 需要处理三种情况：

- 原来没有映射：引用计数加一，写入新页表项。
- 原来映射到同一物理页，只是更新权限：先加一再减一，计数不变，直接覆盖页表项。如果先删除旧映射，计数会降到 0，物理页被错误释放。
- 原来映射到其他物理页：先给新页加引用，再调用 `page_remove_pte` 撤销旧映射，最后写入新页表项。

`get_pte(pgdir, la, 1)` 可能已经为 `PDX1(la)` 对应的页表项新建了一页页表，随后在为 `PDX0(la)` 分配页表页时失败并返回 NULL。此时 `page_insert` 检查 `PDX1` 页表项是否是本次新建的，如果是，就恢复原值并释放这一页，然后返回 `-E_NO_MEM`，不留下半完成的页表结构。由于可能新建了非叶子页表项，写入后调用 `flush_tlb` 刷新整个 TLB。

#### 最终提示词

<!-- 待 A 补充：功能模块一的最终提示词（四段式）。 -->

#### 实现迭代过程

<!-- 待 A 补充：迭代过程、遇到的问题与修改方式。 -->

### 4.3 功能模块二：进程创建与初始化

负责人：B 2413507-蒋林瀞。

#### 模块功能描述

需要实现的函数：

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
```

在 `proc_init` 之前，内核已有物理页分配、内核页表、`kmalloc` 和 Trap 入口，但整个系统只有一条从 `kern_entry` 开始的执行流，没有“线程”这个对象。本模块把这条执行流登记为 0 号线程 `idleproc`，再创建第一个独立的内核线程 `initproc`，交给模块三的调度器运行。

本模块向下依赖 `kmalloc/kfree` 分配 PCB、`alloc_pages/free_pages` 分配内核栈、`boot_pgdir_pa` 作为线程的页表根；向上提供 `proc_list`、PID 哈希表和处于 `PROC_RUNNABLE` 的 PCB。

六个函数按“造零件，再组装”的顺序理解：`alloc_proc` 生成空白 PCB，`kernel_thread` 准备首次运行现场，`do_fork` 把 PCB、内核栈、地址空间和现场组装成可调度的线程，`copy_mm` 和 `copy_thread` 是 `do_fork` 的两个步骤，`proc_init` 最后用这些函数建立 idle 和 init。

#### 4.3.1 alloc_proc：得到一个安全的空白 PCB

`struct proc_struct` 的字段会被不同的代码读取：`schedule` 看 `state`，`find_proc` 看 `pid`，`forkret` 用 `tf`，`proc_run` 用 `pgdir`。任何一个字段带着 `kmalloc` 返回的旧数据，都可能让线程在没准备好时被误用。因此 `alloc_proc` 先整体清零，再单独设置非零的初值：

```c
memset(proc, 0, sizeof(struct proc_struct));
proc->state = PROC_UNINIT;
proc->pid = -1;
proc->pgdir = boot_pgdir_pa;
list_init(&(proc->list_link));
list_init(&(proc->hash_link));
```

- `state = PROC_UNINIT` 表示 PCB 还在构造中。调度器只选 `PROC_RUNNABLE`，不会选中它。
- `pid = -1` 是一个不可能被分配的值。PID 在登记时由 `get_pid` 分配，这里不提前占用。
- `pgdir` 填内核根页表的物理地址 `boot_pgdir_pa`。模块三的 `proc_run` 用 `(8 << 60) | (pgdir >> 12)` 拼出 satp，所以这里不能填虚拟地址，也不能填完整的 satp 值。
- 两个链表节点初始化为指向自己。即使出错路径对它调用 `list_del`，也不会破坏其他进程的链表。

`kmalloc` 失败时直接返回 NULL，此时没有修改任何全局结构。

#### 4.3.2 kernel_thread：把“函数加参数”变成一份 trapframe

调度器只会恢复寄存器，不会直接调用函数。框架借用 Trap 返回路径来启动线程：先构造一份“好像刚从中断进入内核”的 trapframe，由 `__trapret` 恢复到寄存器中，再用 `sret` 跳到入口代码。入口代码 `kernel_thread_entry` 只有三条指令：

```asm
kernel_thread_entry:
    move a0, s1
    jalr s0
    jal do_exit
```

它把 `s1` 当参数、`s0` 当函数地址，所以 `kernel_thread` 按这个约定填写 trapframe：

```c
memset(&tf, 0, sizeof(struct trapframe));
tf.gpr.s0 = (uintptr_t)fn;
tf.gpr.s1 = (uintptr_t)arg;
tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
tf.epc = (uintptr_t)kernel_thread_entry;
return do_fork(clone_flags | CLONE_VM, 0, &tf);
```

`status` 的三个位决定 `sret` 之后的状态：

- `SPP = 1`：`sret` 返回到 S 态。内核线程只在 S 态运行。
- `SPIE = 1`：`sret` 把 SPIE 复制到 SIE，新线程开始运行后中断是开的。
- `SIE = 0`：`RESTORE_ALL` 先写 `sstatus`，再逐个恢复通用寄存器，最后恢复 `sp`。如果这里 SIE 为 1，恢复到一半时就可能进入中断，而此时 `sp` 还指向 trapframe。清掉 SIE 保证中断只在 `sret` 这一刻打开。

这里的 `tf` 是栈上的局部变量，`kernel_thread` 返回后就失效，`do_fork` 必须把它复制到新线程自己的内核栈上。`stack` 参数传 0，表示内核线程没有调用者指定的栈。`CLONE_VM` 表示与当前进程共享地址空间。

#### 4.3.3 do_fork：组装资源，最后才登记

`do_fork` 的核心问题是顺序：哪些步骤可能失败，失败时要释放什么，什么时候让线程对调度器可见。实现顺序如下：

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

可能失败的步骤都在登记之前。失败时线程还没有 PID，也不在任何链表里，只需按申请的逆序释放内存：

```c
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
```

两个标签上下相连。`copy_mm` 失败时跳到上面的标签，先释放栈再释放 PCB；`setup_kstack` 失败时跳到下面的标签，只释放 PCB。每种失败只释放它已经拿到的资源。

登记阶段放在 `local_intr_save` 与 `local_intr_restore` 之间，原因有两个：

- `get_pid` 通过扫描 `proc_list` 判断 PID 是否被占用。如果分配完 PID、还没 `list_add` 时被打断，另一处创建线程的代码会扫到同一个空闲 PID。分配和挂链在同一临界区内，就不会出现重复 PID。
- `wakeup_proc` 把状态改为 `PROC_RUNNABLE` 是最后一步。调度器看到这个线程时，它的现场、PID 和链表都已就绪。

`nr_process` 在临界区内与链表同时变化，所以失败路径不需要回退计数。

#### 4.3.4 copy_mm：明确只支持内核线程

Lab4 只创建内核线程，它们的 `mm` 为 NULL，共用内核页表。`copy_mm` 在 `current->mm == NULL` 时让子线程的 `mm` 也为 NULL 并返回 0。

如果 `current->mm` 不为 NULL，说明调用者带有用户地址空间。复制或共享用户地址空间要到 Lab5 才实现，这里返回 `-E_INVAL`，让 `do_fork` 走回滚路径。如果直接返回 0，调用者会以为地址空间已经复制好，问题要到子进程运行时才暴露。

#### 4.3.5 copy_thread：在新内核栈上布置首次运行现场

```c
proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
*(proc->tf) = *tf;
proc->tf->gpr.a0 = 0;
proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;
proc->context.ra = (uintptr_t)forkret;
proc->context.sp = (uintptr_t)(proc->tf);
```

- 第一行用指针减一得到“栈顶减去一个 trapframe 大小”的地址。`kstack` 按页对齐，trapframe 为 36 × 8 = 288 字节，是 16 的倍数，所以 `tf` 满足 RISC-V 调用约定的 16 字节栈对齐。代码中用 `_Static_assert` 在编译期固定了这个前提。
- `a0 = 0` 是 fork 的语义：子进程看到的返回值为 0。内核线程的 `a0` 随后会被 `kernel_thread_entry` 改成参数，这一行为后续实验的用户态 fork 保留语义。
- `gpr.sp` 是 `__trapret` 最后恢复的栈指针。内核线程的 `esp` 为 0，原样写入的话，线程进入 `fn` 后第一次压栈就会访问 0 附近的地址。这里改为 trapframe 的地址：`sret` 之后 trapframe 已经无用，它下面的空间正好作为线程的运行栈。
- `context.ra = forkret`，`context.sp = tf`。模块三的 `switch_to` 恢复这两个值后执行 `ret`，就进入 `forkret`。

把 4.3.2 到 4.3.5 连起来，新线程第一次被调度时的路径是：

```text
schedule -> proc_run -> switch_to(&prev->context, &proc->context)
  ret 到 context.ra = forkret，sp = tf
forkret -> forkrets(current->tf)          sp <- tf
__trapret: RESTORE_ALL                   写 sstatus/sepc，恢复 s0=fn, s1=arg, sp
sret                                     pc <- kernel_thread_entry，SIE <- SPIE
kernel_thread_entry: a0 <- s1; jalr s0   执行 fn(arg)
```

#### 4.3.6 proc_init：建立 idle 和 init

`proc_init` 先初始化 `proc_list` 和 1024 个哈希桶，然后处理两个特殊线程。

`idleproc` 不是 fork 出来的，它就是正在执行 `proc_init` 的启动执行流，已经在使用 `bootstack`，所以只需要一个 PCB 来描述它：

```c
idleproc->pid = 0;
idleproc->state = PROC_RUNNABLE;
idleproc->kstack = (uintptr_t)bootstack;
idleproc->need_resched = 1;
set_proc_name(idleproc, "idle");
nr_process++;
current = idleproc;
```

- 不为 idle 分配新栈，它的栈是 `entry.S` 中定义的 `bootstack`。
- `need_resched = 1` 让 `kern_init` 最后进入的 `cpu_idle` 立即调用 `schedule`，把 CPU 交给 init。
- idle 不挂入 `proc_list` 和哈希表。模块三的 `schedule` 只在 `proc_list` 中找不到可运行线程时才回到 idle；如果 idle 也在链表里，它会和普通线程一起轮转。
- idle 的 `tf`、`context` 和 `runs` 保持为 0。它第一次被切走时，`switch_to` 会把当时的寄存器保存到 `context`。

然后用 `kernel_thread(init_main, "Hello world!!", 0)` 创建 init。此时 `current` 已是 idle，所以 init 的 `parent` 是 idle；`get_pid` 在空链表上从 1 开始分配，init 得到 PID 1。最后用 `find_proc` 取回 PCB，命名为 `init`。创建失败时调用 `panic`，避免内核带着空的 `initproc` 进入 `cpu_idle`。

#### 最终提示词

本模块采用 [PROMPT] / [RELY] / [GUARANTEE] / [SPECIFICATION] 四段式规格，分 7 个阶段推进，每个阶段单独提交。下面是 `do_fork` 阶段的摘录，完整提示词见 `prompt.md`。

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

- [RELY] 要求核对真实源码，而不是沿用经验写法。`kernel_thread_entry` 用哪个寄存器传参、`forkrets` 如何设置 `sp`，都由仓库中的汇编决定。
- [SPECIFICATION] 把失败路径单独列为 Case，并写明“全局状态不变”。这句话可以直接转成测试断言：失败前后比较 `nr_process`、链表长度、空闲页数和 `kmalloc` 计数。
- 与模块三的接口约定写进规格（见 3.2），两个模块并行开发后可以直接合并。

#### 实现迭代过程

本模块的实现经历了 2 次迭代。

##### 第一次迭代

按阶段依次实现 `alloc_proc`、四个创建函数和 `proc_init`，每个阶段完成后编译。`proc_init` 完成后整包编译通过，`make qemu` 输出了 init 线程的三行文字。

**遇到的问题：**

- 用本机原有的 GCC 15.1 运行 `make grade`，得分 0/30。原因是评分测试代码无法用新版编译器编译，与本模块的实现无关，分析见 6.1。

**问题解决策略：**

- 在提示词的 Requirements 中写明“使用指导书推荐的工具链，不修改评分脚本和测试代码”，换用 GCC 10.1 后评分 30/30。

##### 第二次迭代

为检验测试本身是否有效，在代码副本中逐个注入 9 种典型错误，要求每种错误都让测试失败。

**遇到的问题：**

- “`kernel_thread` 不清除 SIE”这一错误没有被发现。`proc_init` 运行时中断还没打开，`read_csr(sstatus)` 读到的 SIE 本来就是 0，去掉 `& ~SSTATUS_SIE` 结果不变，原有检查无法区分。

**问题解决策略：**

- 在提示词中要求“在中断已打开的上下文中再检查一次 trapframe 的 status”。补充的检查在 `init_main` 中创建线程，此时 SIE 为 1，9 种错误全部被检出。

**最终结果：**

- ✅ 编译通过
- ✅ 本模块 61 项测试和正式评分全部通过
- ✅ GDB 观察到的首次运行现场与设计一致（5.4）

### 4.4 功能模块三：进程调度

负责人：C 2411788-马传志。

#### 模块功能描述

需要实现的函数：

```c
void cpu_idle(void);
void proc_run(struct proc_struct *proc);
void schedule(void);
void switch_to(struct context *from, struct context *to);
```

线程创建模块准备 PCB、独立内核栈和首次运行现场后，cpu_idle 检查调度标志，schedule 选择下一个可运行线程，proc_run 切换 current 和页表，switch_to 完成寄存器现场切换。新线程第一次通过预构造的 ra 进入 forkret，再经 forkrets、__trapret 和 sret 到达 kernel_thread_entry。后续已经运行过的线程，则从上次 switch_to 的返回位置继续执行。

#### 实现说明

调度选择采用链表顺序查找，从当前节点之后扫描并回环，过滤非就绪状态；没有其他可运行线程时可继续当前线程，没有任何普通就绪线程时选择 idle。它具有轮转选择特点，但未实现时间片管理，不宣称具备抢占式轮转调度。

上下文只保存 ra、sp、s0-s11，因为 switch_to 是按照函数调用约定进入的切换边界。与任意指令处发生的 Trap 不同，调用者已负责跨普通函数调用需要保留的 caller-saved 值。每个字段偏移通过编译期断言与汇编对应；RV64 上结构为 112 字节。

写 satp 时使用物理页表根的页号并设置 MODE=8；TLB 使用 sfence.vma 失效。切换前后共享内核映射是继续执行切换代码的前提。不能直接把完整物理地址或虚拟地址塞入 satp，也不能使用参数截断为 32 位的 Sv32 遗留接口。

#### 最终提示词

```text
背景：针对提供的 RV64 Sv39 Lab4 原始源码，实现成员 C 的调度模块。
范围：仅补 proc.c 的 cpu_idle、proc_run，sched.c 的 schedule 和 switch.S 的 switch_to。保留 B 的创建函数占位，不覆盖原 wakeup_proc。
约束：context 为 ra/sp/s0-s11；按 ABI 保存恢复，布局必须有编译期检查。pgdir 是物理地址，按 Sv39 构造 satp，不用 Sv32 lsatp。切换关闭并恢复原本地中断状态，切换页表后刷新 TLB。调度从当前之后寻找 runnable，正确回环，允许仅当前 runnable，找不到则使用 idle。保留 LAB4 注释，代码标记学号 2411788。
验收：实际编译并在本机 QEMU 中运行；不得通过伪造 PASS 代替测试。A/B 未完成时，用明确隔离的测试环境验证 C 的实现，并记录正式整合尚未通过的原因。
```

依据上述提示词形成的实现规格：

前提为单 hart RV64 Sv39 内核，PCB、内核栈和内核映射已正确初始化。所有可运行线程的 pgdir 都必须包含切换函数、旧栈及目标栈可用的内核映射；页表根为页对齐物理地址。关本地中断不提供多核互斥。

cpu_idle 不返回，只在 need_resched 为真时调用 schedule。当前采用忙等，不引入 wfi 唤醒顺序和新中断源的额外要求。

schedule 保存并关闭本地 S 态中断，清 current->need_resched。从当前普通线程之后开始查找，遇链表头继续回环；idle 从表头开始。跳过所有非 PROC_RUNNABLE 状态，遍历一圈可选中仍可运行的 current，找不到则回退 idleproc。选中线程 runs 加一，不为此创建 PROC_RUNNING 新状态。只在目标不同于 current 时调用 proc_run，重新得到执行权后恢复原中断状态。

proc_run 的入参非空、可运行且 pgdir 对齐；目标等于 current 时不切换。实际切换期间关闭中断，将旧 current 记为 prev，更新 current，写入 Sv39 satp，执行带 memory 屏障的 sfence.vma，再调用 switch_to。没有分配 ASID，因此采用全局本地 TLB 失效。函数从 switch_to 继续执行时，意味着这个旧线程已被再次调度；此时才恢复该调用栈保存的中断标志。

switch_to 依据 context 的 14 个字段顺序保存旧 ra、sp、s0-s11 并加载新值，最后 ret。普通函数调用 ABI 负责调用者需要跨调用保持的其他值。它不是 Trap 入口，不能照搬 36 槽现场，也不负责 scause、sepc 或调度策略。编译期检查覆盖所有字段偏移和结构大小。

#### 实现迭代过程

本轮先在 M 态隔离环境验证，再补 S 态页表测试和 cpu_idle 路径。测试程序曾因编译器将结构清零转换为 memset 而链接失败，加入原项目 string.c 后解决。GDB 初次读取旧 QEMU 的虚拟 priv 寄存器得到与启动流程不符的值，因此移除该读数，不把它作为特权级证明；S 态入口由测试启动代码设置 MPP=S 后执行 mret 构造。

### 4.5 Challenge 1：local_intr_save 与 local_intr_restore 如何开关中断

负责人：C 2411788-马传志；技术复核：B 2413507-蒋林瀞。

local_intr_save 调用 __intr_save：读取 sstatus.SIE，原来开启则关闭并返回真，否则返回假。local_intr_restore 根据保存的标志决定是否重新开启。作用是临时保护本 hart 的临界区，并保留调用前状态，而非无条件关闭后又无条件开启。

嵌套调用时，外层保存真并关闭，内层发现已关闭而保存假；内层恢复时不重新开启，直到外层恢复才开启。因此不能用 intr_enable 简单替代每次 restore。local_intr_save 使用 do while(0) 包成一条语句，避免多语句展开影响 if/else。local_intr_restore 的定义是 `__intr_restore(x);`，自带分号，写在没有花括号的 if/else 中会多出一条空语句，导致 else 无法匹配。当前代码没有这样的用法。

schedule 和 proc_run 均保护各自关键区域。switch_to 返回并不是目标线程从调用点直接返回，而是原线程再次被恢复后继续原调用栈，所以原中断标志仍属于恢复后的旧线程。关本地中断不能屏蔽同步异常，也不能替代多核锁；当前实现以单 hart 为前提。

新线程第一次运行时，中断不是由 local_intr_restore 打开的。schedule 关中断后切换到新线程，新线程从 forkret 开始执行，不会回到旧线程栈上的那句 local_intr_restore；它的中断由 sret 按 trapframe 中的 SPIE 打开（GDB 记录见 5.4）。这也是 kernel_thread 必须设置 SPIE 的原因。

### 4.6 Challenge 2：分页模式与 get_pte

负责人：A 2412096-王麒萱；技术复核：B 2413507-蒋林瀞。

**两段相似代码的原因。** `get_pte` 中两段相似的代码分别处理 Sv39 的第 2 级和第 1 级页表：检查页表项是否有效，无效时分配一页、清零，再写入指向新页的页表项，最后返回第 0 级的页表项地址。多级页表每一级的结构相同，都是一页大小的页表项数组，用虚拟地址中固定宽度的一段作下标，非叶子项保存下一级页表的页号，所以每一级的处理代码也相同。各模式的差别只在层数和每级的位宽：

| 模式 | 虚拟地址位数 | 页表级数 | 每级索引位数 | 页表项大小 | get_pte 中相似代码段数 |
| --- | --- | --- | --- | --- | --- |
| Sv32 | 32 | 2 | 10 | 4 字节 | 1 |
| Sv39 | 39 | 3 | 9 | 8 字节 | 2 |
| Sv48 | 48 | 4 | 9 | 8 字节 | 3 |

这几段代码本质上是一个按级展开的循环，可以改写成从最高级向下遍历、在每级按需建表的 for 循环。

**查找与分配是否应拆开。** 合在一起时，调用者用 `create` 参数选择“只查”或“查不到就建”，接口简单，`page_insert` 和 `get_page` 都只需一次调用。代价有两点：返回 NULL 时无法区分“没有映射”和“内存不足”；同一个函数既可能只读页表，也可能修改页表，调用者需要清楚自己传入的参数。Lab4 中调用场景少，保留现有写法即可。后续实现换页或写时复制时，对“是否修改页表”更敏感，拆成只读的查找函数和按需建表的函数会更清楚，`page_insert` 中对第二级页表的回滚也可以移入建表函数内部。

**复核发现。** `get_pte` 写中间级页表项时设置了 `PTE_U | PTE_V`。RISC-V 特权级规范规定，非叶子页表项的 U、A、D 位保留，软件应写 0。在 QEMU 4.1.1 上启动检查和评分都通过，没有观察到错误，但这一写法与规范不一致。该代码来自框架和 Lab2 迁移，本次未修改。

## 五、测试结果

### 5.1 运行内核

执行 `make qemu`，依次输出内存管理检查、时钟初始化和 init 线程的文字：

```text
check_alloc_page() succeeded!
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
use SLOB allocator
kmalloc_init() succeeded!
check_vma_struct() succeeded!
check_vmm() succeeded.
++ setup timer interrupts
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
kernel panic at kern/process/proc.c:208:
    process exit!!.
```

<!-- 截图：make qemu 输出，放入 images/ 后在此引用。 -->

`check_pgdir` 和 `check_boot_pgdir` 通过，说明模块一的页表操作正确。init 线程打印了三行文字，说明它被创建、被调度并从 `kernel_thread_entry` 进入了 `init_main`。`init_main` 返回后执行 `jal do_exit`，而 `do_exit` 在本实验框架中只有一句 `panic`，所以最后的 panic 是预期行为。

### 5.2 正式评分

执行 `make grade`。评分脚本把三项测试注入代码副本：在 `pmm_init` 末尾测试页表操作，在 `proc_init` 末尾检查 idle 和 init 的字段，在 `init_main` 开头创建两个线程并检查它们是否都被调度过。

```text
  -check PMM:                                OK
  -check Process Create:                     OK
  -check Schedule:                           OK
Total Score: 30/30
```

<!-- 截图：make grade 输出，放入 images/ 后在此引用。 -->

### 5.3 进程创建模块测试

模块二的测试把测试代码 include 到真实 `proc.c` 的末尾，可以直接调用 static 函数；用链接选项 `--wrap` 让 `kmalloc`、`alloc_pages` 在指定的一次调用中失败；测试在 QEMU 中运行，`proc_init` 结束时和 `init_main` 开始时各执行一组检查。测试代码不进入正式内核。

| 检查内容 | 项数 | 结果 |
| --- | --- | --- |
| `alloc_proc` 的字段初值、链表节点自环、`pgdir` 为物理地址、分配失败返回 NULL | 11 | 通过 |
| idle 与 init 的 PID、状态、栈、名字、父子关系、链表与哈希表 | 18 | 通过 |
| 连续创建 3 个线程：PID 唯一且连续、内核栈不重叠、计数正确 | 7 | 通过 |
| 进程上限、PCB 分配失败、内核栈分配失败、`copy_mm` 拒绝用户 mm：返回值正确，计数、链表、空闲页、`kmalloc` 计数不变，不消耗 PID | 10 | 通过 |
| `copy_thread` 对非零 `esp` 的处理 | 2 | 通过 |
| 创建探测线程，线程运行后在线程内读取 `sp`、`satp`、`sstatus`、参数，以及开中断时创建线程的 SIE | 13 | 通过 |
| 合计 | 61 | 全部通过 |

在代码副本中注入 9 种典型错误（不回滚 PCB、不回滚内核栈、内核线程 `sp` 为 0、`a0` 不清零、不清 SIE、`pid` 未初始化、`copy_mm` 假装成功、不更新 `nr_process`、idle 挂入链表），9 种都使测试失败，说明测试能发现这些错误。

### 5.4 使用 GDB 观察 idle 到 init 的首次切换

在 `proc_run`、`forkret`、`kernel_thread_entry`、`init_main` 四处设断点：

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

<!-- 截图：GDB 首次切换记录，放入 images/ 后在此引用。 -->

记录逐项对应 4.3 的设计：

- `pgdir = 0x80208000` 是物理地址；进入 `forkret` 时 satp 为 `0x8000000000080208`，即 MODE=8（Sv39）加页号 `0x80208`，说明模块三按约定写入了页表根。
- `tf = 0x...1ee0` 等于栈顶 `0x...2000` 减去 288 字节（`0x120`）。
- `context.ra` 与 `forkret` 地址相同，`switch_to` 返回后进入 `forkret`。
- `forkret` 处 trapframe 的 `status` 低位为 `0x120`，即 SPP=1、SPIE=1、SIE=0；经过 `sret` 后 `sstatus` 低位为 `0x002`，SIE=1、SPP=0。
- 进入 `kernel_thread_entry` 时 `s0` 等于 `init_main`，`s1` 指向 `"Hello world!!"`，`sp` 16 字节对齐。

### 5.5 调度与切换模块测试

QEMU 版本为本机已有 4.1.1。M 态和 S 态独立环境各通过 12 项检查。S 态环境使用三个不同物理页表根，各自提供相同的 RAM/MMIO 恒等映射，验证实际 satp 切换及切换后继续执行。它不验证不同地址空间中具有不同物理目标的映射隔离。

12 项检查为：空表回退 idle、跳过全部非就绪状态、仅当前可运行、三个线程往返并睡眠返回 idle、跨表头顺序、原中断关闭状态保持、s0-s11 哨兵和栈返回、原中断开启状态恢复、嵌套内层恢复、嵌套外层恢复、proc_run(current) 不切换、cpu_idle 触发调度。寄存器哨兵由汇编设置并比较，避免只依赖 C 局部变量被编译器优化后的结果。

GDB 在真实 switch_to 首条保存指令断下，执行 14 条保存后比较旧 SP/RA，执行 14 条加载后比较目标 SP/RA，四次比较均为 1。独立测试使用静态 PCB 和私有恒等映射代替模块一、模块二，正式环境中的首次运行链和完整调度由 5.2、5.4 验证。

各模块测试的脚本和原始日志位于 `code/tools/member-b/`、`code/tools/member-c/`。

## 六、调试记录

### 6.1 新版编译器下评分为 0 分

用本机原有的 riscv64-unknown-elf-gcc 15.1.0 执行 `make grade`，三项都显示 `no $qemu_out`，总分 0/30。评分脚本先把测试注入代码副本再编译，编译日志中的错误为：

```text
kern/test/test_lab4.c:90:26: error: passing argument 1 of 'kernel_thread' from incompatible pointer type [-Wincompatible-pointer-types]
note: expected 'int (*)(void *)' but argument is of type 'void (*)(void *)'
```

官方测试中的线程函数 `test_lab4_thread` 返回 `void`，而 `kernel_thread` 要求返回 `int` 的函数。GCC 14 起把不兼容的函数指针转换默认作为错误，较早版本只给出警告。测试内核没有编译出来，QEMU 也就没有输出。

修改测试代码或关闭编译检查都会改变评分条件。我们按实验指导书的推荐下载 SiFive 预编译工具链（GCC 10.1.0），同一份代码的评分为 30/30，编译日志中该处只是警告。

## 七、实验总结与收获

### 7.1 本实验知识点与 OS 原理的对应

| 本实验知识点 | 对应的 OS 原理 | 含义与关系 | 差异 |
| --- | --- | --- | --- |
| `page_insert` / `page_remove` 与引用计数 | 页表管理与物理页共享 | 一个物理页可被多个虚拟地址映射，引用计数为 0 时才能释放 | 原理中常只讲地址翻译；实验中必须自己维护计数和 TLB 一致性 |
| `tlb_invalidate` / `sfence.vma` | TLB 一致性 | 修改页表后，TLB 中的旧翻译必须失效 | 本实验无 ASID，切换页表时刷新整个 TLB |
| `struct proc_struct` | 进程控制块 PCB | 记录一个执行实体的状态、栈、现场和关系 | 原理中的 PCB 还包括打开文件、信号等；本实验只保留创建和调度需要的字段 |
| `PROC_UNINIT` / `PROC_RUNNABLE` | 进程状态模型中的新建态、就绪态 | 新建的 PCB 在资源齐备后才转为就绪 | uCore 用 `PROC_RUNNABLE` 同时表示就绪和运行，用 `current` 区分正在运行的线程 |
| `do_fork` 及失败回滚 | 进程创建 fork；资源分配的原子性 | 创建要么全部成功，要么不留下任何部分结果 | 原理中 fork 复制父进程地址空间；本实验只创建共享内核地址空间的内核线程 |
| 内核线程共享 `boot_pgdir_pa`、各有 `kstack` | 线程共享地址空间、私有栈 | 所有内核线程可视为同属内核这一个进程的多个线程 | 原理中线程属于某个用户进程；本实验没有用户进程 |
| `idleproc` | 空闲进程 | 没有就绪线程时占用 CPU | 本实验中它还是启动执行流本身，PID 为 0，不参与普通轮转 |
| `get_pid` 与关中断临界区 | 临界区与互斥 | 分配 PID 和登记进程必须不可分割 | 单核下关中断即可；多核需要锁 |
| `schedule` 的链表顺序选择 | 进程调度、轮转调度 | 从当前线程之后选择下一个就绪线程 | 没有时间片和抢占，线程必须主动让出 CPU |
| `context` 与 `switch_to` | 上下文切换 | 保存当前线程的执行位置，恢复另一个线程 | 只保存被调用者保存寄存器，依赖函数调用约定；中断现场则由 trapframe 保存全部寄存器（3.3） |
| `proc_run` 写 satp | 地址空间切换 | 切换线程时切换页表根 | 本实验所有线程使用同一页表，写入的值相同，为后续用户进程做准备 |
| `local_intr_save` / `local_intr_restore` | 中断屏蔽与临界区 | 保存原中断状态，嵌套时只由最外层恢复 | 只对单个 hart 有效，不能屏蔽同步异常 |

### 7.2 OS 原理中重要但本实验未对应的知识点

- 进程退出与回收：`do_exit` 在本实验中只有 `panic`，没有僵尸状态、父进程 `wait`、孤儿进程交给 init 收养等机制，PCB 和内核栈创建后不会被释放。
- 用户进程与特权级切换：所有线程都在 S 态运行，没有用户地址空间、系统调用和 U 态与 S 态之间的切换，也没有验证不同地址空间之间的隔离。
- 地址空间复制与写时复制：`copy_mm` 只处理内核线程，没有页表复制和 Copy-on-Write 的缺页处理。
- 调度策略与抢占：只有按链表顺序的选择，没有时间片、优先级和多级反馈队列；时钟中断只计数，不触发调度。
- 多核调度与同步：关本地中断只能保护单核上的临界区，多核下需要自旋锁等同步原语和每核独立的运行队列。
- 线程间同步：没有信号量、条件变量等机制，也没有线程阻塞与唤醒的场景。

### 7.3 AI 协作开发的经验

1. 先约定接口，再并行开发。三个模块写在同一个 `proc.c` 中，开发前在提示词里写明 `pgdir` 的含义、idle 是否进链表、`context` 的初值，三人各自开发后合并时没有冲突，也没有修改对方的函数。
2. 让 AI 以真实源码为准。提示词要求所有接口和宏先在源码中核对。`kernel_thread_entry` 用哪个寄存器传参、`lsatp()` 为何不能用，都来自仓库代码和指导书，而不是 AI 对旧版 uCore 的记忆。
3. 测试要验证测试本身。第一版测试全部通过，但注入错误后发现“不清 SIE”这一错误测不出来。之后对每类检查都用一个故意写错的版本确认它会失败。
4. 区分代码问题和环境问题。评分 0 分时，先看编译日志确定是测试代码无法编译，再查到编译器版本的差异，最后按指导书更换工具链，而不是修改实现或评分条件去凑分数。
5. 分层记录验证结果。编译通过、模块测试通过、完整启动、正式评分是四个不同的结论。在各模块独立测试阶段，报告只写已经验证的层次，合并后再补齐整体结果。
