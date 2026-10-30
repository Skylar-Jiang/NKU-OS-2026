# Lab3 三人提示词

从 Trap 入口到异常返回

## 一、A 马传志（2411788）

### A1 继承 Lab2 内存管理

[PROMPT]

任务：核对小组 GitHub 的 lab2 分支最新版本，继承其 Page Init 与 Best-Fit 核心算法。

操作要求：补齐本次提供的 Lab3 源码中的内存管理空缺。

输出要求：验证初始化、分配、拆分、释放和合并。

[RELY]

来源为 Skylar-Jiang/NKU-OS-2026 的 origin/lab2@8b06753。

RV64、页大小4096、DTB提供RAM范围；Page数组覆盖[nbase,npage)物理页号。

分配释放调用者遵守合法区间契约；内核是当前单hart实验配置。

[GUARANTEE]

涉及的内存初始化和 Best-Fit 接口：

```text
static void page_init(void);
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);
static void best_fit_check(void);
```

[SPECIFICATION]

page_init()

Pre-Condition：DTB提供RAM范围，Page数组覆盖[nbase,npage)物理页号。

Post-Condition：内核、固件、描述符数组及对齐占用不进入空闲页集合。

Requirements：验证DTB范围和整数加法边界，按PHYSICAL_MEMORY_END限制管理范围。

检查描述符所需空间，先全部标保留，再初始化合法空闲区域。

Best-Fit 分配、释放和自检

Pre-Condition：分配释放调用者遵守合法区间契约；内核是当前单hart实验配置。

Post-Condition：Best-Fit选择最小足够块，等长按低地址；空闲块按地址有序，计数准确。

Requirements：只将空闲块头挂链；正确拆分、合并和更新头页标记及长度。

保留Lab3已有的local_intr_save/local_intr_restore包装。

不复制Lab2无中断保护的包装函数，不改输入框架自检和原评分规则。

用独立位图参考模型核对3000次操作，并测试不同RAM配置。

### A2 检查 Trap 入口和现场恢复

[PROMPT]

任务：逐项检查输入源码已有的RESTORE_ALL、__alltraps和__trapret。

操作要求：修正发现的问题，保留正确实现。

输出要求：补充布局约束及实际寄存器恢复验证。

[RELY]

沿用给定SAVE_ALL、struct trapframe和trap(struct trapframe *tf)接口。

RV64下36槽、每槽8字节；当前内核栈有效且16字节对齐。

C层负责中断/异常处理及必要的返回epc调整。

[GUARANTEE]

涉及的汇编入口和恢复接口：

```text
.macro RESTORE_ALL
__alltraps:
__trapret:
```

保持 void trap(struct trapframe *tf) 的接口。

[SPECIFICATION]

RESTORE_ALL、__alltraps 和 __trapret

Pre-Condition：SAVE_ALL 已保存现场；当前内核栈有效且16字节对齐。

Post-Condition：trap()的a0指向当前Trap frame；C层返回后进入统一恢复路径。

从对应槽恢复sstatus、sepc和通用寄存器；原sp最后恢复。

通过sret返回Trap frame指定的位置和应有状态。

Requirements：不改变SAVE_ALL、结构体字段和函数签名。

注意s1/s2在写回CSR后还要恢复原通用寄存器值。

x0不必回写；scause/stval用于诊断，不作为常规恢复目标。

增加编译期布局断言；对恢复前人为改写GPR进行实际QEMU/GDB验证。

记录QEMU4.1.1的SPIE历史行为，不将其与较新实现混为一谈。

## 二、B 蒋林瀞（2413507）

### B1 实验基线审查与开发准备

[PROMPT]

任务：在 ~/oslab-lab3 中审查 Lab3 当前代码、实验要求和开发环境，确定成员 B 的实际工作范围。本阶段以审查为主，暂不修改源码。

操作要求：

1. 首先阅读本地 docs/ 中的老师课堂录音或转录材料、Lab3 指导书、实验报告模板，以及相关 Lab2 材料。

2. 将课堂要求、指导书、三人分工和实际代码交叉核对。如存在影响实现正确性的冲突，指出依据并暂停相关开发；一般性表述差异记录即可。

3. 检查当前工作区的 Git 状态、feature/timer-interrupt 与 origin/lab3 的关系，以及成员 A 已有的修改和测试。

4. 检查 QEMU 4.1.1、RISC-V 编译器、GDB 和 direnv 配置，不修改全局工具链。

5. docs/、.envrc 为本地资料和配置，不得删除、修改或提交。

输出要求：提供简洁的审查结论、任务清单、风险与依赖。已有正确实现不重复开发。

[RELY]

依赖当前 lab3 分支、docs/ 指导材料以及以下工程文件：

code/kern/trap/trap.c、trap.h

code/kern/driver/clock.c、clock.h

code/kern/driver/intr.c、intr.h

code/kern/init/init.c

code/libs/riscv.h、sbi.h

code/Makefile、code/tools/grade.sh

report/ 已有材料

以实际代码中的宏定义、类型和函数声明为准。

[GUARANTEE]

本阶段不新增或修改接口。必须完成以下审查：

interrupt_handler(struct trapframe *tf)

clock_init()、clock_set_next_event()

intr_enable()、intr_disable()

idt_init()、kern_init()

相关 Trap 分发与时钟计数机制

[SPECIFICATION]

Pre-Condition：当前 Lab3 Worktree 已建立，个人分支可访问，工作区中可能含有本地非跟踪资料。

Post-Condition：

明确哪些功能已经实现，哪些仍有缺陷，哪些需要增加测试。

区分成员 B 的任务与成员 A、C 的任务。

明确后续可安全修改的文件与代码范围。

记录基线提交、环境版本和已有验证结果。

Case 1：已有实现正确

保留现有代码，说明已满足的规格和仍需验证的行为，不制造额外修改。

Case 2：实现存在问题

记录具体位置、预期行为、实际行为及影响，并归入后续开发阶段。

Case 3：资料与代码存在冲突

优先确认教师正式要求及当前框架接口；若无法确定，暂停受影响部分，不自行猜测。

Requirements：

特别检查 init.c 中重复的 idt_init()、interrupt_handler() 中的非时钟中断提示，以及 DEBUG_GRADE 行为。

不将成员 A 已经完成的 Trap 汇编、物理内存管理重新列为 B 的开发任务。

不生成或修改报告文件；prompt.md 由实验整理阶段统一维护。

本阶段结束后汇报审查结果，等待下一份提示词。

### B2 中断识别与分发功能

[PROMPT]

任务：检查并完善 code/kern/trap/trap.c 中的中断处理功能，使内核能够正确识别各类中断，持续处理 Supervisor Timer Interrupt，并在达到指定计数后关闭系统。

操作要求：

在上一阶段审查结果的基础上，直接修改工程中的实际代码。保留已有正确实现，仅修改 B 负责的中断处理逻辑和必要的辅助代码。

trap.c 由 B、C 共同负责。不得修改 C 的 exception_handler() 和 Challenge 3，不修改 A 的 Trap 汇编与内存管理。

输出要求：汇报改动、验证结果及仍需后续测试的项目。本阶段不编写完整实验报告。

[RELY]

使用工程实际定义的以下资源：

struct trapframe、tf->cause、tf->epc

IRQ_* 中断原因常量

clock_set_next_event()

volatile size_t ticks

TICK_NUM、现有打印计数器和关机阈值

print_ticks()、print_trapframe()

cprintf()、sbi_shutdown()

libs/riscv.h、libs/sbi.h

指导书使用 SHUTDOWN_AFTER 表示关机阈值，但当前工程不一定定义该宏。应先核实已有状态，不引入重复或冲突的定义。

[GUARANTEE]

必须保持接口：

```text
void interrupt_handler(struct trapframe *tf);
```

可以按需要调整该模块内部的静态变量、宏或辅助函数，但不得改变公开接口和其他成员负责的处理函数。

[SPECIFICATION]

#### interrupt_handler()

Pre-Condition：

Trap 入口已经保存处理器上下文。

tf 指向有效的 Trap frame。

tf->cause 表示本次中断的原因。

Post-Condition：

能正确提取 RV64 scause 的中断原因码。

Supervisor Timer Interrupt 能被持续处理。

其他已知中断具有准确的分类输出。

未知中断输出完整 Trap frame。

正常返回时不破坏原执行上下文。

Case 1：Supervisor Timer Interrupt

每次收到中断均安排下一次时钟事件。

ticks 每次准确增加 1。

每累计 TICK_NUM = 100 次时钟中断，输出一行 100 ticks。

正常运行模式累计输出 10 次后，调用 sbi_shutdown()。

保证不存在重复计数、错误打印频率、重复关机或仅响应一次的问题。

时钟中断是异步事件，不应像异常指令一样给 tf->epc 增加固定偏移。

Case 2：其他已知中断

按照工程现有 IRQ_* 类型正确区分软件中断、时钟中断和外部中断，输出准确的类型名称。不将外部中断误输出为软件中断，不将 Hypervisor Timer 误标为软件中断。

本实验未要求的设备处理中断，不添加不存在的设备驱动。只在框架允许的范围内实现类型识别和提示。

Case 3：未知中断

使用 print_trapframe(tf) 输出诊断信息，不臆造恢复方式。

Case 4：DEBUG_GRADE

保留原评分模式的预期输出和终止机制，确保评分模式与正常运行模式不相互影响。不能为了获得连续十次输出而破坏评分脚本的 End of Test. 检查。

Requirements：

正确区分 scause 中断标志位与具体原因码，保持 RV64 类型和位宽语义。

不随意修改标准输出内容。

不改动 trap_dispatch()、trap() 或异常处理函数的行为。

保留本任务所需的 LAB3 注释，按指导书要求处理 B 负责部分的学号标记，不修改其他成员标记；不知道学号时先标明待确认，不得猜测。

完成后检查 Diff、编译及基本运行行为；通过后创建一次有意义的 Git commit。

### B3 时钟初始化与中断使能

[PROMPT]

任务：检查并完善 clock.c、intr.c、init.c 中的时钟初始化与中断使能机制，使内核启动后能够正确配置 Trap 入口、设置首次时钟事件并持续响应时钟中断。

操作要求：

直接检查和修改实际文件。已有正确代码保持不变，只处理有依据的问题。重点检查初始化顺序、重复初始化、时钟事件设置和 CSR 使能状态。

本阶段不得修改其他成员负责的内存管理、Trap 汇编或异常处理代码。

输出要求：汇报各函数实际状态、修改原因和验证结果。

[RELY]

使用工程现有接口与数据：

clock_init()、clock_set_next_event()

intr_enable()、intr_disable()

idt_init()、pmm_init()、kern_init()

rdtime、sbi_set_timer()

stvec、sie、sstatus

SSTATUS_SIE、MIP_STIP 等已定义宏

clock.h、intr.h、riscv.h、sbi.h

当前 QEMU 4.1.1 与 OpenSBI 环境

需先确认具体宏和函数的语义，不假定所有名称在不同版本的框架中一致。

[GUARANTEE]

保持现有接口不变：

```text
void clock_init(void);
void clock_set_next_event(void);
void intr_enable(void);
void intr_disable(void);
void idt_init(void);
int kern_init(void);
```

其中 kern_init() 必须保留原有不返回的契约。

[SPECIFICATION]

#### clock_init() / clock_set_next_event()

Pre-Condition：OpenSBI 时钟服务可用，内核已经建立必要的执行环境。

Post-Condition：

时钟中断使能配置正确。

首次时钟事件被正确设置。

时钟计数具有明确、正确的初始状态。

后续处理程序能够持续重设时钟事件。

Case 1：正常初始化

保持时钟间隔与实验的预期频率一致。核对 timebase 的单位、当前硬件时间的读取以及下一次事件时间的计算。

Case 2：时钟中断持续触发

检查初始事件与后续事件的设置关系，确认不会只触发一次或在启动阶段丢失必要的配置。

#### intr_enable() / intr_disable()

Pre-Condition：内核运行于支持相应 CSR 操作的特权环境。

Post-Condition：仅按预期设置或清除全局 S 模式中断使能位，不破坏其他状态位。

Case 1：开启中断

使 sstatus.SIE 处于允许状态。

Case 2：关闭中断

使 sstatus.SIE 处于禁止状态。

#### idt_init() / kern_init()

Pre-Condition：内核已进入初始化阶段，必要的启动环境可用。

Post-Condition：

stvec 正确指向当前 RISC-V Trap 入口。

内存、Trap、时钟与全局中断使能按正确依赖顺序初始化。

内核能够进入持续运行状态。

Case 1：正常启动

保持必要的 DTB、控制台、物理内存管理、Trap 和时钟初始化，确保在允许中断之前，相关处理机制已经准备好。

Case 2：重复初始化

检查当前 init.c 中两次调用 idt_init() 的必要性。如果确认重复且无实际作用，则进行最小清理，并通过运行验证未引入回归。

Requirements：

当前 idt_init() 中存在历史 x86 IDT 说明，应核对并整理为准确的 RISC-V 描述；保留教师要求的实验标记。

不新增完整调度器、多核中断机制或无关设备驱动。

不修改全局 QEMU 路径及 .envrc。

不改变评分脚本和测试标准。

完成后编译、验证初始化及连续时钟输出，检查 Diff 后按实际改动提交一次 Git commit。

### B4 独立测试、评分与 GDB 调试

[PROMPT]

任务：对成员 B 的中断分发、时钟管理和初始化机制进行独立验证，覆盖功能正确性、连续中断、CSR 状态、Trap 执行路径和原评分脚本，并保存真实证据。

操作要求：

在现有功能实现的基础上执行实际测试。允许编写必要的独立测试脚本，但不得修改评分规则，不得通过注释掉功能或减弱断言获得通过结果。

优先复用成员 A 已有的底层验证设施，新增 B 独立负责的关键验证。

输出要求：完成后检查 Diff 并提交测试相关改动，输出测试矩阵、通过情况、失败原因及证据位置。

[RELY]

code/Makefile、code/tools/grade.sh

trap.c、clock.c、intr.c、init.c

struct trapframe、ticks、stvec、sie、sstatus、scause、sepc

QEMU 4.1.1、RISC-V GDB、OpenSBI

code/tools/member_a/ 中可用的参考验证材料

当前代码的调试符号、已有测试和输出格式

可以在 code/tools/member_b/ 中放置独立测试工具及必要的原始记录；报告所需截图放在 report/images/member_b/。

[GUARANTEE]

本阶段不要求修改生产代码的公开接口，交付以下验证结果：

正常构建及启动

原 make grade 评分结果

连续时钟中断和计数行为

中断类型识别

关键 CSR 状态

Trap 进入、分发与返回过程

可复现测试命令与真实证据

[SPECIFICATION]

Pre-Condition：

成员 B 的功能代码可以编译。

工作区处于正确功能分支。

QEMU 和 GDB 版本已核实。

测试修改与正常构建隔离。

Post-Condition：

关键需求均有对应的测试证据或明确的未验证说明。

正常内核未被独立测试工具污染。

失败原因得到定位，修复后的项目重新验证。

Case 1：构建与评分

执行 make clean、make、make grade，记录命令、结果和原始输出。确认正常构建与评分构建的条件差异。

不能以评分满分替代全部功能验收。

Case 2：正常时钟运行

运行 make qemu，核对：

每 100 次时钟中断输出一次 100 ticks。

正常模式共输出 10 次。

关机由预定阈值触发。

系统没有只收到第一次时钟中断、异常循环或重复打印。

在可行情况下核对 ticks 的实际变化，区分计数正确性与单纯的输出正确性。

Case 3：中断分发

验证 Supervisor Timer 原因码为 5，并正确区分 scause 的最高位标志和低位原因。

检查其他已知中断分支以及未知中断的诊断行为。无法从当前设备触发的中断类型可以使用隔离测试，但应明确区分“处理函数测试”与“真实硬件中断测试”。

Case 4：GDB 调试

检查以下执行链：

```text
__alltraps → trap() → interrupt_handler() → __trapret → sret
```

重点观察：

stvec 的入口配置。

进入处理前后的 sstatus.SIE。

sie 的时钟中断允许位。

scause 和 Trap frame 中保存的原因。

sepc 与中断返回位置。

时钟事件重设和 ticks 递增。

区分中断进入时硬件保存的状态、Trap frame 保存的上下文和最终 sret 后的状态。对 CSR 的特殊位按实际 QEMU 4.1.1 行为解释，不因版本差异随意修改内核。

Case 5：失败与边界情况

测试失败时先排查源码、队友依赖、工具链、超时和测试脚本。只修复有明确依据的问题，并执行回归测试。

Requirements：

保留关键测试日志、GDB 命令与实际输出。

保存真实截图，数量以能够证明关键结论为准。

如果无法自动截图，给出精确的人工截图命令和步骤，不能伪造。

不把成员 A 的结果当作 B 独立执行过的测试。

提交测试工具时保持正常构建不变，排除构建产物和无意义的临时文件。

### B5 成员 B 实验报告

[PROMPT]

任务：根据 Lab3 指导书和成员 B 实际完成的代码、测试与 GDB 结果，撰写结构完整、表述清楚、便于答辩的实验报告。

操作要求：

1. 阅读 docs/ 中的实验报告模板、课堂要求、Lab3 指导书。

2. 读取实际源码、Git Diff、真实测试记录和截图。

3. 先写入 report/member_b.md，供成员 C 后续合并到最终 report/report.md。

4. 不覆盖成员 A 已有报告，不提前整合其他成员的内容。

5. 不撰写或虚构尚未发生的 Prompt 设计与迭代过程；真实 Prompt 汇总由实验整理阶段负责。

输出要求：完成后检查报告结构、图片引用、技术准确性及重复内容，并汇报待成员 C 集成的部分。

[RELY]

以实际工程和实验记录为依据，重点包括：

interrupt_handler() 及相关静态状态

clock_init()、clock_set_next_event()

intr_enable()、intr_disable()

idt_init()、kern_init()

scause、stvec、sie、sstatus、sepc

SBI 定时器、Trap frame 和中断返回流程

已完成的 QEMU、评分和 GDB 记录

report/images/member_b/ 中的真实截图

[GUARANTEE]

本阶段不修改功能代码接口。

交付一份 Markdown 报告，围绕 B 的实际工作说明设计、实现、验证、调试与 OS 原理，并直接回答指导书中适用于本模块的问题。

[SPECIFICATION]

Pre-Condition：

成员 B 已完成开发并形成可复现的验证记录，相关证据能够对应到实际代码。

Post-Condition：

报告具有完整的逻辑链、准确的技术说明和必要的证据，能够独立用于成员 B 的答辩准备，并可整合进小组最终报告。

Case 1：实验整体逻辑

说明 Lab3 的目标和实现主线：

中断初始化、硬件触发、Trap 入口、上下文保存、C 层分发、时钟处理、上下文恢复与返回。

重点解释成员 B 在整个执行链中承担的功能，不重复展开 A 负责的每一条汇编指令。

Case 2：SPEC 与实现分析

对成员 B 的主要函数说明：

功能和接口

前置条件与后置状态

核心行为及关键分支

依赖的 CSR 和 SBI 接口

实际代码的实现原理

必要的边界与限制

SPEC 应体现功能要求，不机械复制源码；实现分析要具体解释为何能够满足这些规格。

Case 3：测试和结果

按照“测试目的 → 测试方法 → 关键证据 → 实际结果 → 结论”组织。

重点呈现正常十次时钟输出、原评分脚本、scause 类型识别、关键 CSR 和 Trap 路径。

截图应配简短标题，并在正文解释观察到的现象。不要堆砌多张含义相近的截图。

Case 4：实际问题和调试

只记录真实遇到的问题，包括错误现象、定位过程、原因、修复和复测。未实际发生的问题不得编写成 Bug 记录。

Case 5：OS 原理与思考

解释：

中断与异常的区别

异步硬件事件与同步异常

Trap 入口与中断服务程序的关系

stvec、scause、sepc 的职责

sie 与 sstatus.SIE 的区别

中断响应与时钟驱动机制

时钟中断与进程调度的联系及区别

中断屏蔽、临界区和原子性之间的关系

简要指出当前实验未完整实现的抢占调度、设备中断处理、多核中断同步等内容，不作过度扩展。

Requirements：

严格遵循老师的报告格式和 Markdown 文件规范。

语言直接、干净，按因果关系组织叙事。

避免防御性写作、迂回立论、实质性重复、冗长复合句和滥用破折号。

不使用第二人称或 AI 说明式措辞。

区分已有框架、队友工作与 B 的实际修改。

不虚构结果，不夸大测试覆盖范围。

不把实验写成纯代码流水账，必须说明关键设计原因和验证依据。

不因追求简洁而省略必要的技术分析。

图片引用路径应有效，缺少截图时明确留出待补位置。

涉及 Prompt 的报告章节暂不编写，可保留简短的待补标记。

### B6 Git 提交审查与最终交接

[PROMPT]

任务：对成员 B 已完成的 Lab3 开发、测试和实验报告进行最终核查，整理真实 Git 提交记录，准备将成果交由成员 C 集成。

操作要求：

先检查当前分支、已有提交、工作区差异、测试日志、报告和图片。只修复实际发现的遗留问题，不无理由重写已验证代码。

本阶段不自动合并到 lab3，不强制推送，不覆盖队友代码。

输出要求：最终以简洁的验收表说明通过、失败、未验证和待集成事项。

[RELY]

当前 feature/timer-interrupt 分支

远程 origin/lab3

git status、git diff、git log

前期真实功能提交和测试记录

code/、report/ 的实际文件

课程要求的最终提交结构

成员 A、B、C 的分工及集成顺序

[GUARANTEE]

本阶段不新增功能接口。

交付经过核验的代码、测试证据、成员 B 报告及清晰的集成说明。

最终小组 lab3 分支应按课程要求只保留所需提交内容：

code/：实现后的实验代码

report/report.md：完整小组实验报告

report/prompt.md：最终确认的真实提示词记录

report/images/：报告引用的测试截图

其中 member_b.md 是供集成使用的中间材料，不应在最终版本中替代 report.md。

[SPECIFICATION]

Pre-Condition：

成员 B 的功能实现、独立验证与报告草稿已完成。

Post-Condition：

功能代码与测试记录相互对应。

Git 提交内容和实际开发工作一致。

本地资料及其他成员代码未被误提交。

未完成事项和集成依赖均已明确。

工作区在完成必要提交后保持干净。

Case 1：检查代码

审核本轮修改范围，只保留成员 B 负责的功能及必要的测试。检查 trap.c 中是否意外包含成员 C 的异常处理改动，检查是否误覆盖 A 的内存管理或 Trap 汇编。

Case 2：检查验证证据

检查各项测试的真实执行记录，特别是正常运行、make grade、连续时钟处理、CSR 与 GDB 验证。对于未验证的功能明确标记，不以假设替代测试结果。

Case 3：检查 Git 提交

按照实际完成顺序检查已有 commit：

中断分发相关修改

时钟初始化相关修改

测试工具及必要记录

成员 B 报告

如果某项工作没有源码变更，不需要为它制造额外提交。不得修改提交时间、伪造历史或拆分无实际意义的 commit。

Case 4：检查课程规范

保留教师要求的 LAB3 标记。

B 负责的待填位置按真实学号处理。

不修改其他成员的身份标记。

不上传 docs/、.envrc、构建缓存、临时文件和无关工具。

确认代码、报告和截图路径符合规范。

prompt.md 应以真实使用和最终确认的版本为准，不添加虚构交互。

Case 5：团队集成

整理供成员 C 使用的交接信息，包括：

实际修改的文件及作用

关键 commit

测试命令、结果和证据位置

与 A/C 代码的依赖

需要整合到最终 report.md 的内容

需要集成后重新执行的测试

Requirements：

不擅自合并、rebase 或强推其他成员分支。

需要推送个人功能分支时，先展示提交清单和推送目标，待确认后操作。

不因追求整洁而删除真实测试记录。

不把其他成员尚未完成的任务算作成员 B 完成。

## 三、C 王麒萱（2412096）

### C1 实现异常处理功能

[PROMPT]

任务：修改 kern/trap/trap.c 中的 exception_handler()，完成 Lab3 的异常处理和 Challenge 3。

操作要求：直接修改项目中的实际代码，保留原有结构和 LAB3 注释，不修改其他组员负责的部分。

输出要求：完成修改后执行 make 编译，展示修改内容和编译结果。

[RELY]

1. struct trapframe 定义在 kern/trap/trap.h 中，cause 表示异常原因，epc 表示异常指令地址。

2. CAUSE_ILLEGAL_INSTRUCTION = 0x2

3. CAUSE_BREAKPOINT = 0x3

4. print_trapframe(tf) 用于输出异常现场。

5. Trap 返回时会将 tf->epc 恢复到 sepc，再通过 sret 返回。

[GUARANTEE]

需要修改的函数：

```text
void exception_handler(struct trapframe *tf);
```

允许添加必要的 static 辅助函数，不改变原有函数接口。

[SPECIFICATION]

函数：exception_handler()

Pre-Condition： Trap 上下文已经保存，tf 包含当前异常信息。

Post-Condition： 正确识别并处理异常，更新需要跳过的指令地址，保证能够正常返回。

Case 1：非法指令异常 输出异常类型和地址，根据实际指令长度将 epc 增加 2 或 4 字节。

Case 2：断点异常 输出断点信息，普通 ebreak 跳过 4 字节，压缩 c.ebreak 跳过 2 字节。

Case 3：其他异常 调用 print_trapframe(tf) 输出异常现场，不随意修改 epc。

Requirements： 1. 根据实际指令编码判断长度。

2. 异常地址输出需要支持完整的 64 位地址。

3. 不修改 interrupt_handler()、trapentry.S 等其他模块。

4. 保留原有 LAB3 标记。

5. 修改后执行 make，检查编译是否成功，并展示 Git Diff。

### C2 编写四项异常测试

[PROMPT]

任务：为 Lab3 的异常处理模块编写测试，验证基础要求和 Challenge 3。

操作要求： 直接在项目中编写测试代码，使用独立的临时测试工作区，不影响正常实验版本。需要在 QEMU 中实际触发异常，不能只模拟调用异常处理函数。

输出要求： 完成测试后展示代码、运行输出和测试结论。暂时不要提交或推送 Git。

[RELY]

1. exception_handler() 位于 kern/trap/trap.c，已经完成异常分类和指令长度判断。

2. kern/init/init.c 中的 kern_init() 是内核初始化入口。

3. idt_init() 用于初始化 Trap 入口。

4. CAUSE_ILLEGAL_INSTRUCTION = 0x2。

5. CAUSE_BREAKPOINT = 0x3。

6. RISC-V 普通指令为 4 字节，压缩指令为 2 字节。

7. 当前交叉编译工具链位于 /home/lenovo/riscv-elf-toolchains/bin/。

[GUARANTEE]

新增临时测试函数：

```text
void lab3_exception_test(void);
```

可以添加必要的测试辅助函数，但不能修改 exception_handler() 的接口。

[SPECIFICATION]

函数：lab3_exception_test()

Pre-Condition： Trap 入口已经初始化，内核能够正常处理中断和异常。

Post-Condition： 四项异常测试都能正常返回，并输出各自的通过标记。

Case 1：普通断点 执行 ebreak，验证异常输出和 epc 前进 4 字节。

Case 2：压缩断点 执行 c.ebreak，验证异常输出和 epc 前进 2 字节。

Case 3：16 位非法指令 执行确定会触发非法指令异常的 16 位编码，验证 epc 前进 2 字节。

Case 4：32 位非法指令 执行确定会触发非法指令异常的 32 位编码，验证 epc 前进 4 字节。

Requirements： 1. 必须通过真实 QEMU Trap 流程测试。

2. 每项测试前后输出清楚的标记，方便判断是否正常返回。

3. 测试指令的长度和编码需要先检查，不能直接用 .word 0 代替 32 位非法指令。

4. 可以在临时工作区给 kern_init() 添加测试调用，但不能把测试入口合并到正式代码。

5. 不修改 A、B 负责的正式代码，不影响原来的时钟中断。

6. 使用 LAB3_EXCEPTION_TEST 宏控制测试代码。

7. 编译时使用正确的 GCCPREFIX，并确认测试宏实际生效。

8. 测试失败时保留报错，不要擅自修改正式异常处理代码。

9. 测试完成后保留临时测试代码和运行记录，正式分支保持干净。

请完成代码编写、编译和 QEMU 运行，并逐项说明四种异常是否通过。

### C3 使用 GDB 调试异常处理

[PROMPT]

任务：完成 Lab3 成员 C 的 GDB 异常调试，并自动保存真实截图，用于实验报告。

操作要求：在已有临时测试工程中使用 QEMU + GDB 完成调试。你负责执行命令、检查结果和截图，我不再手动输入命令。

输出要求：每个测试分别保存截图，图片清晰、内容完整，并说明测试是否通过。

[RELY]

1. 测试目录：D:\NKU-OS-2026\lab3-exception-test\code

2. 使用 WSL Ubuntu。

3. GDB：/home/lenovo/riscv-elf-toolchains/bin/riscv64-unknown-elf-gdb

4. 编译工具链：/home/lenovo/riscv-elf-toolchains/bin/riscv64-unknown-elf-

5. 测试宏：LAB3_EXCEPTION_TEST

6. 异常处理函数：exception_handler(struct trapframe *tf)

7. 测试代码已通过 QEMU 4/4 验证。

[GUARANTEE]

不修改正式代码，只调试现有函数：

```text
void exception_handler(struct trapframe *tf);
```

允许使用 GDB 断点、观察点和单步执行，不提交或推送代码。

[SPECIFICATION]

Pre-Condition：测试内核能够编译，QEMU 和 GDB 可以正常连接。

Post-Condition：完成四种异常的真实 GDB 调试，保存对应截图。

Case 1：32 位 ebreak

检查 scause、sepc、tf->epc，验证 epc 增加 4 字节。

Case 2：16 位 c.ebreak

检查异常类型和返回地址，验证 epc 增加 2 字节。

Case 3：16 位非法指令

确认异常类型为 0x2，验证 epc 增加 2 字节。

Case 4：32 位非法指令

确认异常类型为 0x2，验证 epc 增加 4 字节。

Requirements：

1. 必须真实运行 GDB，不能伪造命令、输出或测试结果。

2. 使用观察点或单步调试，直接记录 epc 修改前后的值。不要使用 finish 后读取可能被复用的 Trap frame。

3. 每种异常至少保存一张包含异常类型、原始 epc、修改后 epc 和差值的截图。

4. 截图只保留 VS Code 终端中对应的有效输出区域，不要截整个桌面、侧边栏或无关内容。

5. 截图前把鼠标移到截图区域外，保证图片内没有鼠标指针。

6. 通过真实界面截图并裁剪，不允许重新绘制、拼接或伪造终端输出。

7. 图片使用 PNG 格式，文字清晰，不能截断重要命令和结果。

8. 图片保存在临时工程的 test-results/gdb_screenshots/ 目录，依次命名 C05-01 至 C05-04。

9. 截图完成后逐张检查，确保显示的值与实际调试结果一致。

10. 如果无法操作桌面或截图，明确说明，不要假装已经完成。

11. 不修改正式 Lab3 工程，不影响组员 A、B 的代码。

完成后列出四项测试的实际结果、截图文件路径及是否通过。

### C4 编写其他异常的兜底测试

[PROMPT]

任务：为 Lab3 的其他异常兜底处理编写测试代码。

操作要求：只在 lab3-exception-test 临时工程中添加测试代码，不修改正式实验代码。

输出要求：完成代码编写并展示 Diff，本阶段不编译、不运行、不提交。

[RELY]

1. exception_handler() 位于 kern/trap/trap.c。

2. struct trapframe 定义在 kern/trap/trap.h。

3. tf->cause 表示异常类型，tf->epc 表示异常地址。

4. print_trapframe(tf) 用于输出异常现场。

[GUARANTEE]

测试现有接口：

```text
void exception_handler(struct trapframe *tf);
```

允许新增临时测试函数和辅助函数，不改变原有接口。

[SPECIFICATION]

Pre-Condition： 异常处理函数已实现，已有四项异常测试通过。

Post-Condition： 测试代码能够检查其他异常的兜底处理行为。

Case 1： 构造未实现恢复逻辑的异常，检查是否输出 Trap frame。

Case 2： 记录处理前后的 epc，检查是否保持不变。

Requirements： 1. 测试至少覆盖取指错误和访存错误。

2. 避免直接触发无法恢复的真实硬件异常。

3. 不影响之前的四项异常测试。

4. 保留独立测试入口，通过宏控制。

5. 只编写代码，不进行编译或测试。

6. 完成后展示新增文件和代码位置。

### C5 编写连续异常和寄存器恢复测试

[PROMPT]

任务：为 Lab3 编写连续异常和寄存器现场恢复测试。

操作要求：只在 lab3-exception-test 临时工程中新增测试代码，不修改正式实验代码。

输出要求：完成代码修改，展示 Diff。本阶段不编译、不运行、不提交。

[RELY]

1. exception_handler() 位于 kern/trap/trap.c。

2. Trap 入口由 SAVE_ALL 保存寄存器，RESTORE_ALL 恢复寄存器。

3. ebreak 为 4 字节，c.ebreak 为 2 字节。

4. 之前四项异常测试均已通过。

5. 现有测试通过宏控制，在 kern_init() 初始化 Trap 后调用。

[GUARANTEE]

新增测试接口：

```text
void lab3_trap_sequence_test(void);
```

允许新增必要的测试辅助函数或汇编测试代码。

[SPECIFICATION]

Pre-Condition： Trap 已初始化，内核能够正常处理断点异常。

Post-Condition： 连续异常能够正常返回，相关寄存器值保持正确。

Case 1：连续异常 连续执行 ebreak、c.ebreak、ebreak，中间不插入其他函数调用，检查三次异常能否依次返回。

Case 2：寄存器现场恢复 在触发异常前给选定的通用寄存器设置测试值，异常返回后比较其是否保持一致。

Case 3：执行流程 确认三次异常全部处理完成，程序能够继续执行后面的代码。

Requirements： 1. 使用真实 RISC-V 异常指令。

2. 测试代码通过 LAB3_TRAP_SEQUENCE_TEST 宏控制。

3. 注意保护测试函数原有寄存器状态，符合 RISC-V 调用约定。

4. 输出每项测试的 PASS 或 FAIL，只有真正通过检查才能输出 PASS。

5. 不影响现有四项异常测试和时钟中断。

6. 不修改正式工程、Trap 汇编及异常处理函数。

7. 本阶段只编写代码，不编译、不运行。
