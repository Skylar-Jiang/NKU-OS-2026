# Lab5 提示词记录

## 成员 A 2412096-王麒萱

以下为原提示词文档中的五次任务记录。

```text
[PROMPT]
任务：修改 kern/trap/trapentry.S，完成 Lab5 的 Trap 上下文保存、恢复及用户态与内核态切换机制。Lab3 主要处理内核态产生的中断和异常，Lab5 引入了用户进程，需要同时支持用户态和内核态产生的 Trap，并根据异常来源和返回目标完成栈切换及上下文恢复。
具体要求如下：
完善 Trap 入口的栈切换机制，正确处理 sscratch 和 sp。用户态发生异常后切换至当前进程的内核栈，内核态发生异常后继续使用当前内核栈。
完善上下文保存机制，保存通用寄存器、原栈指针及相关 CSR，在内核栈上构造与 struct trapframe 一致的栈帧。
完善 Trap 入口处理流程，将 trapframe 地址传递给 trap()，异常处理结束后进入上下文恢复流程。
完善上下文恢复机制，根据保存的上下文恢复寄存器、栈指针及相关 CSR，正确处理返回用户态和返回内核态两种情况。
完善 __trapret 返回流程，通过 sret 完成特权级切换和 PC 跳转，同时支持用户进程通过预先构造的 trapframe 首次进入用户态。
检查上述实现与 Lab5 进程内核栈、上下文管理及异常处理机制是否兼容，为后续系统调用和用户程序执行提供基础。
操作要求：必须在项目中修改实际文件。按照 Lab5 实验手册和课堂说明，检查已经迁移的代码，补全缺失功能，修正存在的问题。保留已有正确实现、官方注释及实验标记，不修改其他成员负责的代码。
输出要求：直接进行文件操作，根据以下 RELY、GUARANTEE 和 SPECIFICATION 完成任务，给出实际修改内容及编译结果。
[RELY]
kern/trap/trap.h 中的 struct pushregs 和 struct trapframe。
kern/trap/trapentry.S 中已有的寄存器保存、恢复及栈帧布局。
kern/trap/trap.c 中的 trap() 函数。
libs/riscv.h 中的 SSTATUS_SPP、SSTATUS_SPIE 等相关宏定义。
kern/process/proc.c 中已有的进程内核栈和上下文管理机制。
RISC-V 的 sscratch、sstatus、sepc、scause、stval 及 sret 指令。
修改前核对工程中的实际定义，确保结构体字段、栈帧大小、寄存器偏移及函数接口一致。
[GUARANTEE]
必须检查并完善以下汇编宏及入口：
SAVE_ALL
RESTORE_ALL
__alltraps
__trapret
允许根据需要修改模块内部的汇编指令和局部标签，不改变现有公开接口。
[SPECIFICATION]
SAVE_ALL
Pre-Condition：
处理器因中断或异常进入 Trap，硬件已经设置相关 CSR，异常可能来自用户态或内核态。
Post-Condition：
在正确的内核栈上建立完整的 trapframe，保存通用寄存器、原栈指针及异常相关 CSR，保证栈帧布局与 struct trapframe 一致。
Case 1：
异常来自用户态，切换至当前进程的内核栈，保存原用户栈指针及其他寄存器。
Case 2：
异常来自内核态，继续使用当前内核栈保存上下文，不进行错误的栈切换。
Requirements：
栈切换过程中不得丢失原有寄存器值，正确维护 sscratch，保证内核态再次进入 Trap 时能够正常处理。
RESTORE_ALL
Pre-Condition：
当前内核栈中存在有效的 trapframe，其中保存了目标执行状态及返回地址。
Post-Condition：
正确恢复通用寄存器、栈指针及异常返回所需的 CSR，使处理器能够通过 sret 恢复执行。
Case 1：
返回用户态，恢复用户栈，并设置下次进入内核所需的 sscratch。
Case 2：
返回内核态，恢复原内核栈及寄存器状态，使 sscratch 符合内核态异常处理约定。
Requirements：
恢复过程中不得提前破坏栈帧地址，不遗漏寄存器，不错误恢复仅用于记录异常原因的 CSR，保留异常处理函数对 trapframe 所作的有效修改。
__alltraps
Pre-Condition：
处理器已经跳转至 Trap 入口。
Post-Condition：
完成栈切换和上下文保存，将 trapframe 地址作为参数传递给 trap()，异常处理结束后进入统一的上下文恢复流程。
Requirements：
参数传递和栈对齐符合 RISC-V 调用约定，保证 trap() 使用正确的内核栈。
__trapret
Pre-Condition：
当前内核栈中存在有效的异常返回上下文。
Post-Condition：
恢复上下文，通过 sret 跳转至目标地址并进入对应特权级。
Case 1：
普通 Trap 返回，恢复异常处理后的程序执行上下文。
Case 2：
用户进程首次执行，根据进程管理模块预先构造的 trapframe 恢复用户上下文并进入用户态。
Requirements：
特权级切换和 PC 跳转通过 sret 完成，保证用户态和内核态的返回路径均符合 RISC-V 特权级规范。
完成后检查汇编代码及栈帧布局，重新编译相关目标文件。若整体编译失败是由于其他成员的功能尚未实现，记录具体原因即可，不修改其他成员的代码。
本次只完成 Trap 上下文保存、恢复及特权级切换，不提前实现系统调用或 COW，不执行额外专项测试及 make grade，暂不 commit 或 push。
2
[PROMPT]
任务：修改 kern/syscall/syscall.c 和 kern/trap/trap.c，完整实现 Lab5 基础功能模块一，使用户程序能够通过系统调用进入内核，执行相应的内核服务，并在系统调用结束后恢复执行。
具体要求如下：
检查用户态系统调用封装、系统调用号定义及参数传递方式，明确用户态与内核态之间的系统调用接口。
完善 exception_handler 中与系统调用有关的异常处理逻辑，正确识别用户态 ecall，并处理异常返回地址，避免返回后重复执行同一条系统调用指令。
按照实验手册和官方源码，检查内核态 ebreak 与 kernel_execve 的调用关系，完成所需的异常分发逻辑，保证后续第一个用户程序能够通过已有 Trap 机制进入用户态。
实现 syscall()，从当前进程的 trapframe 中获取系统调用号和参数，根据系统调用号查找对应的处理函数，将执行结果写回 trapframe 中约定的返回值寄存器。
完成 sys_exit、sys_fork、sys_wait、sys_exec、sys_yield、sys_kill、sys_getpid、sys_putc、sys_pgdir 九个系统调用封装函数，实现用户程序与内核进程管理、调度及输出功能之间的接口。
正确处理无效系统调用号，避免分发表越界或调用空函数指针。
操作要求：必须在项目中修改实际文件。严格按照 Lab5 实验手册、官方源码及课堂说明完成模块一，保留已有正确代码、官方注释及实验标记。不修改其他成员负责的进程管理和内存管理实现，不修改评分脚本及官方测试程序。
输出要求：直接进行文件操作，按照以下 RELY、GUARANTEE 和 SPECIFICATION 完成任务，给出实际修改内容及编译验证结果。
[RELY]
kern/trap/trap.h 中的 struct pushregs 和 struct trapframe。
kern/trap/trap.c 中的 exception_handler、trap_dispatch 和 trap。
kern/trap/trapentry.S 中已经完成的上下文保存、恢复及异常返回机制。
kern/syscall/syscall.c 中已有的系统调用分发表及相关定义。
user/libs/syscall.c 中用户态系统调用的内联汇编、系统调用号、参数寄存器和返回值约定。
libs/unistd.h 中的系统调用号定义。
kern/process/proc.h 中的 struct proc_struct、current 及进程管理函数声明。
kern/process/proc.c 中的 do_exit、do_fork、do_wait、do_execve、do_yield、do_kill 和 kernel_execve 相关接口。
工程已有的字符输出、页表输出函数及 RISC-V 异常原因定义。
修改前读取以上相关源码，核对实际宏定义、函数签名、参数类型和寄存器约定，不使用其他版本 uCore 的接口替换当前工程。
[GUARANTEE]
必须实现或修改以下主要接口：
void syscall(void);
static int sys_exit(uint64_t arg[]);
static int sys_fork(uint64_t arg[]);
static int sys_wait(uint64_t arg[]);
static int sys_exec(uint64_t arg[]);
static int sys_yield(uint64_t arg[]);
static int sys_kill(uint64_t arg[]);
static int sys_getpid(uint64_t arg[]);
static int sys_putc(uint64_t arg[]);
static int sys_pgdir(uint64_t arg[]);
void exception_handler(struct trapframe *tf);
上述签名需要与官方源码及现有声明核对，若存在差异，以当前工程实际定义为准。
完善现有系统调用分发表。允许根据需要添加模块内部使用的静态辅助函数，不改变已有公开接口。
[SPECIFICATION]
exception_handler
Pre-Condition：
处理器因同步异常进入内核，Trap 入口已经完成上下文保存，传入的 trapframe 有效。
Post-Condition：
根据异常原因执行相应的处理逻辑，正确处理 Lab5 所需的系统调用异常，保留已有的其他异常处理功能。
Case 1：
用户态执行 ecall，正确更新异常返回地址，进入系统调用处理流程。系统调用正常结束后，从原系统调用指令的下一条指令继续执行。
Case 2：
内核态通过 ebreak 触发 kernel_execve 相关流程，按照官方源码约定识别并处理该调用，保证能够复用系统调用及异常返回机制。
Case 3：
其他中断和异常继续按照原有机制处理，不将普通非法指令、断点或访存异常错误识别为系统调用。
Requirements：
根据实际指令及官方调用约定处理异常返回地址，避免重复执行异常指令或跳过正常指令。保留 Lab3 已完成的异常处理逻辑，不提前实现 COW。
syscall
Pre-Condition：
当前进程及其 trapframe 有效，系统调用号和参数已经由调用方按照约定存入寄存器。
Post-Condition：
正确获取系统调用号及参数，调用相应的系统调用处理函数，并将返回值写回约定的寄存器。
Case 1：
系统调用号有效，且分发表中存在对应处理函数，正确传递参数并保存执行结果。
Case 2：
系统调用号无效或未注册，按照官方源码规定进行错误处理，不访问越界位置或空函数指针。
Requirements：
系统调用号、参数及返回值的寄存器约定必须与用户态封装一致。正确处理 RV64 参数宽度，不能截断地址或指针。不得破坏其他需要保留的寄存器及异常上下文。
sys_exit
Pre-Condition：
当前进程发起退出请求，参数中包含退出状态码。
Post-Condition：
正确提取退出状态码并调用内核进程退出接口。进程退出后的状态转换、资源释放和调度由底层进程管理函数负责。
sys_fork
Pre-Condition：
当前进程及其 trapframe 有效，进程发起 Fork 请求。
Post-Condition：
调用内核进程复制接口，按照官方 Fork 语义传递当前进程上下文和相关参数，将执行结果返回给系统调用分发函数。
Requirements：
正确使用当前进程的 trapframe 和用户栈信息。子进程创建、地址空间复制和父子进程返回值处理由底层进程管理函数负责。
sys_wait
Pre-Condition：
当前进程发起等待子进程的请求，参数中包含目标进程标识和退出状态保存地址。
Post-Condition：
正确提取并传递参数，调用内核等待接口，返回执行结果。
Requirements：
保留用户地址的完整宽度，不在系统调用封装层擅自访问未经检查的用户地址。
sys_exec
Pre-Condition：
当前进程发起程序执行请求，相关参数已经按照工程约定传入。
Post-Condition：
正确提取程序名称、长度、二进制程序地址及大小等实际接口所需参数，调用内核程序装载接口，并返回执行结果。
Requirements：
参数类型和顺序必须与 do_execve 的实际声明以及 kernel_execve 的调用约定一致。ELF 加载、地址空间替换和用户态上下文构造由底层函数负责。
sys_yield
Pre-Condition：
当前进程主动请求让出 CPU。
Post-Condition：
调用内核对应接口，使当前进程能够按照已有调度机制让出 CPU，并返回执行结果。
sys_kill
Pre-Condition：
当前进程发起终止指定进程的请求，参数中包含目标进程 PID。
Post-Condition：
正确提取目标 PID，调用内核进程终止接口，并返回执行结果。
sys_getpid
Pre-Condition：
当前进程有效。
Post-Condition：
返回当前进程的 PID，不修改进程状态及其他资源。
sys_putc
Pre-Condition：
调用方传入需要输出的字符。
Post-Condition：
通过工程已有的字符输出接口完成输出，并按照系统调用约定返回执行结果。
sys_pgdir
Pre-Condition：
当前进程发起页表信息输出请求，相关内存管理机制已经初始化。
Post-Condition：
调用工程已有的页表输出接口，按照官方要求完成页表信息输出，并返回执行结果，不修改现有页表映射。
完成后检查九个系统调用函数是否全部实现，检查分发表及异常入口是否正确，重新编译 syscall.o 和 trap.o，并执行 make 检查整体编译结果。
若其他成员尚未完成的函数导致编译或链接失败，记录具体文件、函数及错误原因，不越权修改，不添加临时替代实现。
本次只完成基础功能模块一，不提前实现 COW、用户程序加载及底层进程管理功能，不执行额外专项测试及 make grade，暂不 commit 或 push。
3
[PROMPT]
任务：修改 kern/trap/trap.c，完成 Lab5 COW Challenge 中写入页面异常的识别、分发及处理接口接入，使用户进程写入 COW 共享页面后能够通过异常处理机制恢复正常执行。
具体要求如下：
阅读实验手册中的 COW Challenge 要求，检查现有异常处理机制及成员 C 负责的 COW 内存管理接口，明确 Trap 层和内存管理层的职责。
完善 exception_handler 中的 Store Page Fault 处理逻辑，识别用户进程写入受保护页面产生的异常，获取异常虚拟地址及当前进程的地址空间。
根据与成员 C 约定的接口，将写入页面异常交给 COW 内存管理模块处理，区分合法的 COW 写入异常与普通非法访存异常。
COW 异常处理成功后保留原异常指令地址，使处理器返回用户态后重新执行原写入指令。
对于不属于 COW 的页面异常或处理失败的情况，按照实验手册及工程已有的异常处理机制进行处理，不能错误恢复执行或无限重复触发异常。
保留此前完成的系统调用、断点、非法指令及其他中断异常处理逻辑。
操作要求：必须在项目中修改实际文件。仅负责 Trap 层的异常识别和接口调用，不实现物理页复制、页表权限修改、引用计数维护及 TLB 刷新。保留官方源码结构、注释及实验标记，不修改其他成员负责的代码。
输出要求：直接进行文件操作，根据以下 RELY、GUARANTEE 和 SPECIFICATION 完成任务，说明实际修改内容、COW 接口约定及编译结果。
[RELY]
kern/trap/trap.h 中的 struct trapframe。
kern/trap/trap.c 中已有的 exception_handler、trap_dispatch 和 trap。
kern/trap/trapentry.S 中已经完成的上下文保存、恢复及异常返回机制。
libs/riscv.h 中与 Store Page Fault 及特权级判断相关的宏定义。
kern/process/proc.h 中的 current、struct proc_struct 及进程地址空间相关字段。
kern/mm/vmm.h、kern/mm/pmm.h 中已有的虚拟内存管理接口及成员 C 提供的 COW 异常处理接口。
修改前核对工程中的实际定义。COW 接口名称、参数、返回值及错误处理方式必须与成员 C 的实现保持一致，不自行假定接口已经存在。
[GUARANTEE]
必须修改以下接口：
exception_handler(struct trapframe *tf)
完善其中与 Store Page Fault 有关的异常处理分支。
允许根据需要添加 Trap 模块内部使用的 static 辅助函数，不改变已有公开接口。
COW 内存管理接口由成员 C 提供。如果该接口尚未确定，先给出需要协商的接口及返回值约定，不自行实现底层功能或添加临时替代函数。
[SPECIFICATION]
exception_handler
Pre-Condition：
处理器因同步异常进入内核，Trap 入口已经保存完整上下文，传入的 trapframe 有效。
Post-Condition：
正确识别写入页面异常，将符合 COW 处理条件的异常交给内存管理模块处理，并根据处理结果决定是否恢复程序执行。其他异常继续按照已有机制处理。
Case 1：
用户进程写入 COW 共享页面，产生 Store Page Fault。获取异常虚拟地址及当前进程的地址空间，调用 COW 异常处理接口。处理成功后保留原 epc，通过已有异常返回机制恢复执行，使原写入指令重新执行。
Case 2：
用户进程写入普通只读页面、未映射页面或其他不符合 COW 条件的地址。不得将其作为合法 COW 写入处理，按照工程已有的非法访存处理机制执行。
Case 3：
当前进程不存在有效地址空间，或者 COW 处理过程中出现内存不足等错误。按照对应错误及进程状态进行异常处理，不得继续使用无效映射，也不得直接返回导致同一异常无限触发。
Case 4：
异常不属于需要处理的 COW 写入异常。保留现有异常处理逻辑，不影响系统调用、普通断点、非法指令及其他访存异常。
Requirements：
不能仅根据 Store Page Fault 就认定页面属于 COW，必须由相应内存管理机制确认页面状态。
COW 处理成功后不得增加 epc，保证原写入指令能够重新执行。
异常处理接口必须正确区分处理成功、非 COW 异常和处理失败等情况。
不得擅自修改页表权限、物理页引用计数及内存管理策略。
不得破坏此前完成的 Trap 上下文恢复机制和系统调用处理逻辑。
完成后检查修改范围及异常分支，重新编译 trap.o。
如果成员 C 的接口尚未提供，说明当前能够完成的部分、接口依赖及后续接入位置，不添加临时函数绕过编译，不越权修改内存管理代码。
本次只完成 COW 异常处理接入，不执行额外专项测试及 make grade，暂不 commit 或 push。
4
[PROMPT]
任务：修改 kern/trap/trap.c，接入成员 C 已完成的 COW 异常处理接口，补全 Store Page Fault 的处理流程，完成 Lab5 Challenge 中 Trap 层负责的功能。
此前已经完成写入页面异常的识别、异常地址获取和失败处理逻辑，现在需要调用正式的 COW 内存管理接口，根据返回结果分别处理合法 COW 写入、非法访存及内存分配失败。
对于合法的 COW 写入异常，完成页面修复后保留原 epc，通过已有 Trap 返回机制重新执行原写入指令。对于无法处理的异常，进入相应的错误处理路径，避免错误恢复执行或重复触发异常。
操作要求：必须在项目中修改实际文件。保留此前已经完成的异常处理和系统调用逻辑，只补全 COW 接口调用及相关处理流程，不修改成员 C 已完成的内存管理代码。
输出要求：根据以下 RELY、GUARANTEE 和 SPECIFICATION 完成任务，说明实际修改内容及编译结果。
[RELY]
kern/trap/trap.h 中的 struct trapframe，其中 epc 保存异常指令地址，tval 保存异常相关地址信息。
kern/process/proc.h 中的 current、struct proc_struct 及 mm 字段。
kern/trap/trap.c 中已经完成的 Store Page Fault 异常识别和失败处理逻辑。
kern/mm/vmm.h 中声明、kern/mm/vmm.c 中实现的 COW 异常处理接口：
int cow_handle_store_fault(struct mm_struct *mm, uintptr_t fault_addr);
返回 0 表示合法 COW 写异常且页面修复成功。
返回 -E_INVAL 表示不属于可处理的 COW 写异常，包括非法地址和真正的只读页面。
返回 -E_NO_MEM 表示复制所需的物理页分配失败。
页面状态判断、页面复制、权限修改、引用计数维护及 TLB 刷新均由该接口负责。
工程已有的异常终止机制及错误码定义。
[GUARANTEE]
必须修改以下接口：
void exception_handler(struct trapframe *tf);
完成 Store Page Fault 分支中 cow_handle_store_fault 的调用、返回值判断及后续异常处理。
函数签名以当前工程实际声明为准，不改变已有公开接口，不重复实现底层 COW 功能。
[SPECIFICATION]
exception_handler
Pre-Condition：
处理器因异常进入内核，Trap 入口已经保存完整上下文，传入的 trapframe 有效。COW 内存管理接口已经实现。
Post-Condition：
正确处理符合条件的 COW 写入异常，根据内存管理接口的返回结果恢复程序执行或进入错误处理流程，保证其他异常处理功能不受影响。
Case 1：
当前异常符合 COW 处理条件，当前进程具有有效的地址空间。将 current->mm 和 tf->tval 传入 cow_handle_store_fault。接口返回 0 后，结束当前异常处理，保持 epc 不变，使原写入指令能够重新执行。
Case 2：
接口返回 -E_INVAL，说明当前异常不属于可处理的 COW 写异常，按照已有非法访存异常处理机制执行，不允许将普通只读页面错误转换为可写页面。
Case 3：
接口返回 -E_NO_MEM 或其他错误，按照已有失败处理机制终止当前非法或无法继续执行的操作，避免返回后重复触发同一异常。
Case 4：
当前进程或地址空间无效，或者异常不符合 COW 处理条件，保留已有的异常处理逻辑，不调用不适用的 COW 接口。检查内核访问用户内存产生写异常的情况，确保处理方式与当前工程的内存访问机制一致。
Requirements：
COW 处理成功后不得修改 epc。
不得仅根据 Store Page Fault 判断页面属于 COW，页面状态必须由内存管理接口确认。
不得重复修改页表权限、物理页引用计数或执行 TLB 刷新。
不得破坏已经完成的系统调用、Trap 上下文恢复及其他异常处理逻辑。
完成后检查接口调用、返回值处理及异常返回路径，重新编译 trap.o，并执行 make 检查整体编译结果。
若整体编译仍因成员 B 尚未完成的功能而失败，记录具体原因即可，不修改其他成员负责的代码。
本次不重复执行成员 C 已完成的专项测试，不执行 make grade，暂不 commit 或 push。
5
[PROMPT]
任务：修改 kern/trap/trap.c，修复三人代码整合后发现的时钟中断调度和用户态读页面异常处理问题，保证相关官方测试能够正常运行并结束。
具体要求如下：
完善 interrupt_handler 中 IRQ_S_TIMER 分支的处理逻辑，每经过 TICK_NUM 次时钟中断，将当前进程的 need_resched 设置为 1，通过 trap() 中已有的调度机制完成进程切换，解决 spin 和 waitkill 测试中的调度问题。
完善 exception_handler 中 Load Page Fault 的处理逻辑。用户进程发生无法处理的读页面异常后，应调用已有的进程退出接口终止当前进程，避免返回用户态后反复执行同一条非法访存指令。
检查 faultread 和 faultreadkernel 测试所触发的异常，保证非法地址读取和用户态访问内核地址的行为能够得到正确处理。
区分用户态和内核态产生的读页面异常。用户态非法访存应终止对应进程，不影响其他进程运行；内核态异常按照工程已有的异常处理机制处理。
保留此前完成的 Store Page Fault、COW、系统调用、断点及其他中断异常处理逻辑。
操作要求：必须在项目中修改实际文件，仅修改 Trap 模块中与本次问题有关的代码。保留官方注释及实验标记，不修改其他成员负责的代码，不修改官方测试程序和评分脚本。
输出要求：按照以下 RELY、GUARANTEE 和 SPECIFICATION 完成任务，说明两个问题产生的原因、实际修改内容及验证结果。
[RELY]
kern/trap/trap.c 中已有的 interrupt_handler、exception_handler、trap 和 TICK_NUM。
kern/trap/trap.h 中的 struct trapframe。
kern/driver/clock.h 中的相关声明：
extern volatile size_t ticks;
void clock_set_next_event(void);
kern/process/proc.h 中的 current、struct proc_struct 及 need_resched 字段。
kern/process/proc.c 中已经实现的进程退出接口：
int do_exit(int error_code);
kern/schedule/sched.h 中的调度接口：
void schedule(void);
trap() 中已有的 need_resched 判断及调度逻辑。
libs/riscv.h 中的 IRQ_S_TIMER、读页面异常原因及 SSTATUS_SPP 等相关宏定义。
工程已有的异常来源判断函数、非法访存错误码及 panic 处理机制。
官方测试程序 spin、waitkill、faultread 和 faultreadkernel。
修改前核对工程中的实际函数声明、宏定义及异常处理约定。
[GUARANTEE]
必须修改以下函数：
interrupt_handler(struct trapframe *tf);
exception_handler(struct trapframe *tf);
完善 interrupt_handler 中的 IRQ_S_TIMER 分支。
完善 exception_handler 中的 Load Page Fault 分支。
检查 trap() 中已有的进程调度逻辑是否能够正确响应 need_resched，不重复实现调度机制。
函数签名以当前工程实际声明为准，不改变已有公开接口。
[SPECIFICATION]
interrupt_handler
Pre-Condition：
处理器收到时钟中断，Trap 入口已经保存完整上下文，传入的 trapframe 有效。
Post-Condition：
完成下一次时钟事件设置和时钟计数，每经过 TICK_NUM 次时钟中断，根据当前进程状态设置重新调度标记。
Case 1：
ticks 达到 TICK_NUM 的整数倍，且 current 有效，将 current->need_resched 设置为 1，使已有的 Trap 返回流程能够进行进程调度。
Case 2：
ticks 尚未达到重新调度条件，正常完成时钟中断处理，不额外设置重新调度标记。
Case 3：
当前进程尚未初始化，只完成时钟事件设置及计数，不访问无效进程指针。
Case 4：
其他类型的中断继续按照已有机制处理。
Requirements：
保留现有时钟事件设置、ticks 计数和必要的输出逻辑。
每经过 TICK_NUM 次时钟中断设置重新调度标记，不自行修改时钟频率和 TICK_NUM。
时钟中断不得修改 epc。
使用已有的 need_resched 和 schedule() 机制，不在时钟中断分支中直接调用调度器。
保证持续占用 CPU 的用户进程能够正常让出 CPU。
exception_handler
Pre-Condition：
处理器因同步异常进入内核，Trap 入口已经保存完整上下文，传入的 trapframe 有效。
Post-Condition：
正确处理无法恢复的读页面异常，终止发生非法访存的用户进程，避免反复触发同一异常，保留其他异常处理功能。
Case 1：
用户进程读取未映射地址，产生无法处理的 Load Page Fault。调用已有的 do_exit 接口终止当前进程，并使用工程已有的非法访存错误码。
Case 2：
用户进程读取无权访问的内核地址，产生 Load Page Fault。按照非法访存处理逻辑终止当前用户进程，不允许继续访问内核地址。
Case 3：
内核态产生 Load Page Fault，按照工程已有的内核异常处理机制处理，不将所有内核异常直接作为普通用户进程退出处理。
Case 4：
当前进程不存在或状态无效，按照工程已有的异常处理机制执行，避免访问无效进程指针。
Case 5：
其他异常继续按照此前的处理逻辑执行，不影响系统调用和 COW 写页面异常处理。
Requirements：
正确区分异常来源和异常类型。
不得通过增加 epc 跳过非法访存指令来代替异常处理。
不得在未修复页面异常的情况下直接返回，避免重复触发同一异常。
用户进程非法访存不得导致整个内核 panic。
不得破坏已完成的 COW 异常处理及正常系统调用功能。
完成后重新编译 trap.o，并执行 make 检查整体编译结果。
若 print_pgdir 尚未实现，记录链接失败原因，不添加空函数或临时占位实现。
整体编译通过后，使用官方测试程序验证 spin、waitkill、faultread 和 faultreadkernel。
检查 spin 和 waitkill 是否能够正常完成进程调度、终止及回收。
检查 faultread 和 faultreadkernel 是否能够及时终止非法访存进程，不再反复触发同一异常。不能仅依据评分脚本未检测到 panic 就认定测试成功。
记录实际运行输出、退出状态及测试结果。
不增加无关测试，不修改评分脚本，暂不 commit 或 push。
```

## 成员 B 2413507-蒋林瀞

### 总体任务与约束

你现在是南开大学 2026 年操作系统课程 uCore Lab5 的开发 Agent，负责三人小组中成员 B（乙，学号 2413507）的全部任务。

仓库：https://github.com/Skylar-Jiang/NKU-OS-2026/

本次负责功能模块二（创建加载第一个用户进程）、功能模块四（其余系统调用功能）、用户程序预加载相关思考题，以及 Lab4 进程管理基础代码的必要迁移和 COW 生命周期兼容性验证。

请首先阅读当前工作区中的 `AGENTS.md` 及 `docs/` 下提供的实验资料，重点阅读：

- 2026 版 `lab5.md`、`lab4.md` 和 `lab0.5.md`。
- `lab5.zip` 官方起始源码。
- 11 月 24 日实验课录音文字稿。
- 三人分工细则。
- NKU OS 每次 Lab 开始前的环境准备说明。
- 必要时参考 Lab2、Lab3 指导书和 appendix。

所有功能均以本次官方 Lab5 源码、2026 版指导书及真实工程接口为依据，不得从其他版本 uCore 仓库直接复制完整实现。

1. 代码范围

主要负责：

- `code/kern/process/proc.c`
- `code/kern/process/proc.h`

若 Lab4 的调度基础设施尚未迁移，允许在核实分工及队友进度后，补充：

- `code/kern/schedule/sched.c` 中的 Lab4 调度实现。
- `code/kern/process/switch.S` 中的 Lab4 上下文切换实现。

允许编写独立命名的 B 成员专项测试。

不得修改成员 C 负责的 `do_fork()`、`copy_mm()`、`dup_mmap()`、`copy_range()` 及 COW 核心实现；不得擅自修改成员 A 负责的 Trap、系统调用分发及异常处理代码。

由于 B、C 共同修改 `proc.c`，必须按函数和代码块划分修改范围，禁止全文覆盖、批量格式化及无关重构。公共接口需要调整时，先记录原因及协作要求，不得单方面改变既定接口。

2. Git 协作规范

最终实验分支为 `lab5`，个人开发分支为 `lab5-member-b`。

在开始时重新获取远程分支和提交记录，不假设队友尚未开始开发。保留已有成果，不重复实现或覆盖他人的代码。

所有开发和提交仅在本人分支进行。每完成一个阶段，检查修改范围、执行适用测试并创建有明确描述的 commit。按照分工要求将本人阶段提交 push 到本人分支，不直接 push 或合并公共 `lab5`，不修改其他成员分支。

禁止 force push、擅自 rebase、`git reset --hard`、`git clean -fd` 等可能破坏现有工作的操作。禁止自动添加 Claude、Codex 或其他 AI 的 `Co-authored-by` trailer。

保留官方 `lab2`、`lab3`、`lab4`、`lab5` 实验标记；仅将本人负责并实际完成位置的 `YOUR CODE` 替换为学号 2413507，不提前认领其他成员任务，不改变此前实验的真实署名。

3. 环境与测试规范

使用现有 WSL Ubuntu、RISC-V 交叉编译工具链和 QEMU 4.1.1。遵守已有独立 Worktree 方案，优先使用 `~/oslab-lab5`，不得破坏之前的 Lab 工作区。

不得擅自修改全局 PATH、QEMU 默认版本、Shell 配置及其他课程环境。`docs/`、`.envrc` 等本地文件不得提交或随意修改、删除。

不得修改官方 `tools/grade.sh`、`tools/inject.py`、`tools/test_lab5.c` 及既有测试断言来获得通过结果，不得伪造评分输出。

如果由于 A、C 的任务尚未完成而不能运行完整测试，应进行当前能够执行的编译、静态检查及独立测试，明确记录阻塞接口，不得擅自完成其他成员的任务，也不得将部分通过描述为完整通过。

所有阶段按顺序执行，但对于存在跨成员依赖的动态测试，可以在最终集成阶段补验。

### 阶段 0：环境检查、源码审计与团队基线同步

[PROMPT]

任务：检查当前工作区、Git 分支、官方 Lab5 源码及队友开发进度，建立正确的个人开发基线，明确需要从 Lab2/3/4 迁移的代码。

操作要求：直接检查真实项目和远程仓库，不修改其他 Lab 工作区，不覆盖已有代码。先审计后开发。

输出要求：给出当前基线提交、待实现函数、已实现函数、需要迁移的代码及跨成员依赖清单。

[RELY]

- `origin/main`、`origin/lab2`、`origin/lab3`、`origin/lab4`、`origin/lab5`
- 已存在的成员开发分支及提交记录
- 官方 `lab5.zip`、实验指导书、分工细则
- `git status`、`git fetch`、`git log`、`git diff`、`git show`、`git worktree list`
- QEMU、交叉编译工具链、官方 Makefile

[GUARANTEE]

1. 建立或继续使用 `lab5-member-b`。
2. 核实官方 Lab5 基线及当前成员 B 的任务完成情况。
3. 确认 Lab2/3/4 迁移范围及负责人。
4. 确认 QEMU 版本、编译工具链与测试入口。
5. 建立后续阶段的实现与验收清单。

[SPECIFICATION]

Pre-Condition：已获得当前项目的文件访问权限及仓库访问权限，官方实验资料可用。

Post-Condition：个人工作区建立在正确的 Lab5 基线上，已明确现有实现、缺失功能和跨成员依赖，旧实验与队友代码均未受破坏。

Case 1：远程 `lab5` 已存在。 获取最新提交，检查已有代码及队友分支；不存在个人分支时从公共 `lab5` 建立，已存在时保留个人历史并安全同步。

Case 2：公共基线或队友代码发生变化。 通过 Git diff 和函数级比较识别变化，避免覆盖、重复迁移或重复定义。

Case 3：前序实验代码缺失。 根据 Lab5 的实验标记，从小组已完成的 Lab2/3/4 实现中进行函数级迁移与适配。B 仅负责自身范围，其他缺失项记录为依赖。

Requirements：

- 不得直接将整个 Lab4 分支合并进 Lab5，也不得用旧 Lab 的 `code/` 覆盖官方 Lab5。
- 检查 `make -n qemu` 和实际评分使用的 QEMU，确保使用正确版本。
- 尝试记录初始编译结果，区分官方未完成代码导致的错误与环境问题。
- 检查 `Makefile`、`tools/inject.py`、`tools/test_lab5.c` 和 `tools/grade.sh`，以真实测试约束指导后续实现。

本阶段如无实际需要提交的文件变更，则记录基线 SHA 和检查结果，不制造空 commit。

### 阶段 1：迁移 Lab4 进程管理基础设施

[PROMPT]

任务：在官方 Lab5 框架中恢复 Lab4 已实现的进程管理基础代码，适配 Lab5 扩展后的 PCB、内核线程和进程调度机制。

操作要求：参考小组 `origin/lab4` 的实际实现，逐函数迁移，仅修改本人负责的范围。保留 Lab5 新增接口和既有辅助函数。

输出要求：完成真实代码修改，列出迁移来源、适配内容及测试结果。

[RELY]

- `struct proc_struct`、`struct context`、`struct trapframe`
- `PROC_UNINIT`、`PROC_RUNNABLE`、`boot_pgdir_pa`、`bootstack`
- `proc_list`、`hash_list`、`idleproc`、`initproc`、`current`、`nr_process`
- `exit_code`、`wait_state`、`cptr`、`yptr`、`optr`
- `CLONE_VM`、`SSTATUS_SPP`、`SSTATUS_SPIE`、`SSTATUS_SIE`
- `kmalloc()`、`kfree()`、`list_init()`、`kernel_thread_entry`
- `do_fork()`、`forkret()`、`forkrets()`、`switch_to()`
- `local_intr_save()`、`local_intr_restore()`、`lsatp()`

所有类型、宏和接口均须核实当前源码定义。

[GUARANTEE]

实现或适配以下函数：

- `static struct proc_struct *alloc_proc(void);`
- `int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);`
- `void proc_init(void);`
- `static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);`
- `void proc_run(struct proc_struct *proc);`
- `void cpu_idle(void);`

在确认尚未由队友完成且属于约定迁移范围时，补充 `schedule()` 和 `switch_to()`。

不修改 `do_fork()` 和 `copy_mm()`。

[SPECIFICATION]

alloc_proc()

Pre-Condition：内核内存分配机制已初始化。

Post-Condition：成功时返回处于安全初始状态的 PCB，尚未分配正式 PID、内核栈及用户地址空间，所有后续可能访问的字段均已正确初始化。

Case 1：分配成功。设置未初始化状态、默认 PID、内核根页表地址，并正确初始化 Lab5 新增的退出、等待及进程关系字段。

Case 2：分配失败。返回 NULL，不遗留资源。

Requirements：不得因复用旧 PCB 内存而继承无效指针、旧进程关系或错误的链表状态。

kernel_thread() 与 copy_thread()

Pre-Condition：调用者提供有效的内核线程入口及参数，进程创建路径能够分配子进程内核栈。

Post-Condition：新线程拥有独立、正确对齐的内核执行现场，并能够通过首次上下文恢复进入预定入口。

Case 1：内核线程。构造符合 RISC-V ABI 的初始上下文，返回到 S 态，使用自身内核栈。

Case 2：用户进程复制。保留正确的用户栈指针，并保证子进程看到的 Fork 返回值为 0。

Requirements：trapframe 布局与当前 Lab5 的 Trap 汇编一致；与成员 C 的 `do_fork()` 和成员 A 的 `RESTORE_ALL` 保持一致。

proc_init()

Pre-Condition：内存管理与进程基础设施可用。

Post-Condition：进程链表与 PID 哈希表正确初始化；idleproc 为 PID 0，initproc 为 PID 1，initproc 的父进程为 idleproc。

Requirements：idleproc 复用启动执行现场及 bootstack，不通过普通 Fork 创建；保留官方测试所需的函数名称和定义形式。

proc_run()、cpu_idle()、schedule()、switch_to()

Pre-Condition：目标进程处于合法可运行状态。

Post-Condition：CPU 能够正确完成进程选择、页表切换及上下文切换；没有普通可运行进程时回到 idleproc。

Requirements：

- 保证页表物理地址与虚拟地址转换正确。
- 使用本次 Lab5 的 Sv39 SATP 实现及正确的 TLB 刷新机制。
- 保证调度和上下文切换的临界区完整性。
- 保留 Lab5 已有 `wakeup_proc()` 对等待状态的处理，不用旧版本覆盖。
- 不重复增加 `nr_process`，不破坏 Lab5 已有的 `set_links()` 和 `remove_links()` 语义。

完成后进行编译检查和可执行的进程初始化验证，检查官方 Test Process Create 的要求。

阶段提交：

`lab5: migrate and adapt process management infrastructure`

### 阶段 2：实现 ELF 装载与程序映像替换

[PROMPT]

任务：在 `kern/process/proc.c` 中完成 `load_icode()` 和 `do_execve()`，实现 ELF 用户程序解析、用户地址空间构建、页面映射、BSS 初始化、用户栈建立及程序映像替换。

操作要求：使用官方 Lab5 现有 ELF、页表、VMA 和内存管理接口。不得自行引入文件系统、交换机制或修改 C 负责的内存管理实现。

输出要求：直接修改项目源码，并验证正常加载、非法 ELF、内存不足及失败清理等路径。

[RELY]

- `struct elfhdr`、`struct proghdr`、`ELF_MAGIC`、`ELF_PT_LOAD`
- `ELF_PF_R`、`ELF_PF_W`、`ELF_PF_X`
- `struct mm_struct`、`struct vma_struct`、`struct trapframe`
- `VM_READ`、`VM_WRITE`、`VM_EXEC`、`VM_STACK`
- `PTE_U`、`PTE_R`、`PTE_W`、`PTE_X`
- `USTACKTOP`、`USTACKSIZE`、`PGSIZE`
- `mm_create()`、`mm_map()`、`setup_pgdir()`、`put_pgdir()`
- `pgdir_alloc_page()`、`page_insert()`、`page2kva()`
- `mm_count()` 及引用计数接口、`exit_mmap()`、`mm_destroy()`
- `user_mem_check()`、`copy_from_user()`、`memcpy()`、`memset()`
- `current->mm`、`current->pgdir`、`current->tf`
- `lsatp()`、`flush_tlb()`、`SSTATUS_SPP`、`SSTATUS_SPIE`

[GUARANTEE]

- `static int load_icode(unsigned char *binary, size_t size);`
- `int do_execve(const char *name, size_t len, unsigned char *binary, size_t size);`

必要时允许增加本模块内部使用的 static 辅助函数，不改变公共接口。

[SPECIFICATION]

load_icode()

Pre-Condition：待加载的 ELF 二进制数据及其长度可用，当前进程处于允许建立新用户地址空间的状态。

Post-Condition：成功后当前进程拥有合法的用户地址空间、正确的 ELF 段映射、已初始化的用户栈及可返回 U 态的 trapframe；失败后不遗留部分建立的地址空间或已分配资源。

Case 1：合法 ELF。根据 ELF Program Header 装载 `ELF_PT_LOAD` 段，按照虚拟地址及权限建立 VMA 和页表映射，并复制文件中的有效数据。

Case 2：BSS 及未初始化区域。对于内存大小大于文件大小的部分，保证应初始化为零的区域全部清零，包括跨页及页内边界情况。

Case 3：建立用户栈。遵守官方用户空间布局、栈区映射及保护页约束，确保用户程序可正常使用栈，不与内核栈混淆。

Case 4：非法 ELF。检查 ELF 魔数、头部范围、Program Header 范围、文件偏移、段大小关系、整数溢出、用户地址范围及入口合法性，返回框架约定的错误，不进行越界访问。

Case 5：资源不足或映射失败。释放已经分配的页面、页表和 VMA 等资源，避免泄漏及悬空指针。

Requirements：

- 根据 ELF 段权限设置页表和 VMA 权限，不能为了便于测试而将全部页面设置为可写、可执行。
- 避免覆盖内核映射和非目标用户区域。
- 正确设置 `current->mm`、根页表物理地址及引用计数。
- trapframe 的 EPC 指向 ELF 入口，SP 指向合法用户栈顶，SPP 为 0，并正确处理 SPIE、SIE 及其他必要状态位。
- 确保切换页表和后续返回路径安全，必要时进行 TLB 及指令缓存同步。
- 不假定所有 ELF 段均按页对齐，正确处理页内偏移及多段映射。

do_execve()

Pre-Condition：当前进程、程序名称和二进制输入符合当前系统调用契约。

Post-Condition：成功时当前进程保持原有 PID 与进程关系，但拥有新程序映像及执行现场；失败时按照指导书和现有框架语义返回错误或进入安全退出路径。

Case 1：当前为内核线程且无旧用户地址空间。直接建立新的程序映像。

Case 2：当前已有用户地址空间。正确验证并保存仍需使用的输入数据，切换离开即将释放的旧页表，减少旧 mm 的引用计数，仅在最后一个引用释放时销毁其资源。

Case 3：名称或二进制参数非法。禁止直接信任用户指针，按照当前地址空间及接口契约处理，避免内核非法访问。

Case 4：新程序装载失败。如果旧地址空间已经不可恢复，必须走安全失败或退出路径，不能继续执行已销毁的旧映像。

Requirements：

- 不得在释放旧地址空间后继续读取位于该空间中的名称、ELF 数据或其他参数。
- 程序名称正确截断并保证字符串终止，避免越界读取。
- 正确区分 `CLONE_VM` 的 mm 共享与 COW 的物理页共享。
- 不重复释放仍被其他进程引用的地址空间和物理页。
- 与成员 C 的地址空间销毁和引用计数语义保持一致。

完成后针对 ELF 文件头、程序段、BSS、用户栈及 trapframe 进行专项检查；保存真实编译、ELF 分析及可执行测试结果。

阶段提交：

`lab5: implement ELF loading and process image replacement`

### 阶段 3：实现第一个用户进程启动流程

[PROMPT]

任务：完成 `init_main()`、`user_main()` 和 `kernel_execve()`，将 Lab4 的内核线程运行机制与 Lab5 的 ELF 装载和用户态返回机制连接起来。

操作要求：阅读官方 Makefile、用户程序链接脚本、KERNEL_EXECVE 宏、Trap 返回路径及课堂录音，选择与成员 A 兼容的首次进入用户态方式。

输出要求：完成实际代码，建立从 initproc 到首个用户程序执行的完整调用链。

[RELY]

- `initproc`、`current`、`kernel_thread()`、`find_proc()`
- `KERNEL_EXECVE`、`KERNEL_EXECVE2`
- Makefile 中的 `TEST`、`TESTSTART`、`TESTSIZE`
- `do_execve()`、`do_wait()`、`schedule()`、`set_proc_name()`
- `struct trapframe`、`KSTACKSIZE`、`forkrets()`、`__trapret`
- `sscratch`、`sstatus`、`sepc`、`sret`
- 官方 `tools/inject.py` 和 `tools/grade.sh`

[GUARANTEE]

- `static int init_main(void *arg);`
- `static int user_main(void *arg);`
- `static int kernel_execve(const char *name, unsigned char *binary, size_t size);`

[SPECIFICATION]

init_main()

Pre-Condition：initproc 已经由 proc_init 创建并获得 CPU。

Post-Condition：正确创建并管理首个用户程序对应的过渡线程，在用户进程退出后完成等待、回收及实验规定的内存状态检查。

Case 1：创建成功。等待用户进程及后续需要由 initproc 接管的子进程结束。

Case 2：创建失败。按照框架语义报告错误，不使用无效 PID 或产生不完整进程关系。

Requirements：initproc 不得被作为普通进程退出；正确记录用户进程创建前后的内存状态，保留官方真实验收信息，不通过硬编码输出模拟测试通过。

user_main()

Pre-Condition：用户程序已由官方构建系统链接进内核映像。

Post-Condition：依据默认配置或测试构建参数选择正确的用户程序，并通过 kernel_execve 完成执行映像转换。

Case 1：普通运行。使用指导书和框架规定的默认用户程序。

Case 2：评分运行。正确处理 `TEST`、`TESTSTART`、`TESTSIZE`，使 `make grade` 能分别启动目标测试程序，不得始终硬编码为 hello 或 exit。

Requirements：成功进入用户程序后，不应继续执行过渡内核线程的旧逻辑。

kernel_execve()

Pre-Condition：当前为过渡内核线程，具有合法的内核栈、进程上下文和有效的内嵌 ELF 输入。

Post-Condition：完成当前进程映像装载，并通过统一 Trap 恢复路径真正进入 U 态，从 ELF 指定入口开始执行。

Case 1：采用手动构造 trapframe 的方案。确保 `current->tf` 有效且不被正在使用的内核栈帧破坏，使用现有返回接口进入 `__trapret`，最终执行 sret。

Case 2：若成员 A 已经约定并实现了 ebreak 入口方案，则通过真实的内核态断点处理契约进入 exec 和 Trap 返回路径，不额外引入不兼容的第二套机制。

Case 3：程序装载或上下文构造失败。返回或安全退出，不得使用错误的 EPC、SP 和特权级状态继续执行。

Requirements：

- 不能仅调用 `do_execve()` 后直接从普通 C 函数返回，以此假定已进入 U 态。
- 区分硬件自动维护的 CSR、软件保存的 trapframe 及调度使用的 context。
- 验证 Trap 返回路径对用户栈、内核栈和 sscratch 的约定。
- 保证切换过程中不会出现中断导致的半完成状态。
- 不擅自修改 A 负责的 trap.c 或 trapentry.S。
- 保留官方测试注入所需要的 `init_main()` 和 `proc_init()` 函数边界。

完成后尝试运行 `make qemu`、`make run-nox-hello`、`make run-nox-testbss`，具体命令以实际 Makefile 为准。

在依赖尚未完成时，记录真实阻塞原因；集成后补充首次用户态执行的 GDB 或运行证据。

阶段提交：

`lab5: implement first user process bootstrap`

### 阶段 4：实现进程退出、等待、让出与终止

[PROMPT]

任务：在 `kern/process/proc.c` 中实现 `do_exit()`、`do_wait()`、`do_yield()`、`do_kill()`，完成进程生命周期闭环。

操作要求：遵循现有进程树、PID 哈希表、等待状态及调度机制，不改变成员 C 的进程创建逻辑，不重复释放资源。

输出要求：完成全部函数及相应边界情况的验证。

[RELY]

- `PROC_SLEEPING`、`PROC_RUNNABLE`、`PROC_ZOMBIE`
- `PF_EXITING`、`WT_CHILD`、`WT_INTERRUPTED`
- `current`、`idleproc`、`initproc`
- `parent`、`cptr`、`yptr`、`optr`
- `exit_code`、`wait_state`、`need_resched`
- `find_proc()`、`set_links()`、`remove_links()`、`unhash_proc()`
- `wakeup_proc()`、`schedule()`
- `exit_mmap()`、`put_pgdir()`、`mm_destroy()`、`put_kstack()`、`kfree()`
- `user_mem_check()`、`copy_to_user()`、`local_intr_save()`、`local_intr_restore()`
- `-E_INVAL`、`-E_BAD_PROC`、`-E_KILLED` 及框架实际错误码

[GUARANTEE]

- `int do_exit(int error_code);`
- `int do_wait(int pid, int *code_store);`
- `int do_yield(void);`
- `int do_kill(int pid);`

[SPECIFICATION]

do_exit()

Pre-Condition：当前进程为允许退出的普通进程。

Post-Condition：释放当前进程不再拥有的用户地址空间资源，记录退出状态，进入僵尸态，通知等待者，并永久停止当前进程的正常执行。

Case 1：普通退出。正确释放用户地址空间引用，在最后一个引用释放时销毁对应资源。

Case 2：存在子进程。将未退出和已退出的子进程正确移交给 initproc，维护完整进程树，并在必要时唤醒等待中的 initproc。

Case 3：父进程正在等待。通知其子进程已经退出，使其能够完成最终回收。

Case 4：idleproc 或 initproc 试图退出。按照官方框架约定阻止非法退出。

Requirements：

- 不得释放当前正在使用的内核栈或 PCB。
- 僵尸进程不能重新被调度执行。
- 内核栈和 PCB 的最终释放应由等待回收方完成。
- 进程树变更、状态转换与唤醒操作必须保持一致性。

do_wait()

Pre-Condition：当前进程请求等待指定子进程或任意子进程。

Post-Condition：成功时回收符合条件的僵尸子进程，并在提供合法地址时写回退出状态；失败时保持其他进程和资源状态正确。

Case 1：指定合法 PID 且对应子进程已退出。获取退出状态，删除其哈希表和进程树记录，释放内核栈和 PCB。

Case 2：pid 为 0。等待任意符合条件的子进程，并仅回收实际退出的一个子进程。

Case 3：存在目标子进程但尚未退出。进入正确的等待状态，调度其他进程；被唤醒后重新检查条件。

Case 4：不存在符合条件的子进程或 PID 非法。按照官方契约返回 `-E_BAD_PROC`，包括 `do_wait(-1, NULL)`。

Case 5：退出状态存储地址非法。返回框架约定的错误，不写入非法地址，不提前回收目标子进程，允许后续合法等待。

Case 6：等待期间当前进程被 kill。按照退出标记及等待状态约定结束等待并进入相应退出路径。

Requirements：

- 保证状态检查、睡眠和唤醒之间不会发生丢失唤醒。
- 不重复回收同一子进程。
- 不破坏兄弟进程链接，不重复减少进程数量。
- 注意内核向用户退出状态缓冲区写数据时的 COW 兼容性，与成员 C 协商安全写入路径，不得绕过页面写保护。

do_yield()

Pre-Condition：当前进程合法运行。

Post-Condition：设置重新调度标记，并返回 0，使当前进程能够在适当的调度时机让出 CPU。

Requirements：不直接将当前进程设置为僵尸态或无条件睡眠，不破坏当前 Trap 返回语义。

do_kill()

Pre-Condition：接收到待终止进程的 PID。

Post-Condition：对于合法目标，设置退出标志，并在目标处于可中断等待状态时将其唤醒，使其在安全时机完成退出。

Case 1：目标存在且尚未被标记退出。设置退出意图，返回成功。

Case 2：目标处于可中断等待。正确唤醒目标，使退出流程能够继续。

Case 3：目标不存在、PID 非法、属于受保护进程或已被标记退出。遵循官方框架的错误码和状态约定，其中 `do_kill(-1)` 必须返回 `-E_INVAL`。

Requirements：不得从当前执行流直接释放其他正在运行或等待中的进程内核栈和 PCB；保证退出标记、唤醒和后续 Trap 退出检查相互配合。

完成后优先验证 `yield`、`badarg`、`exit`、`spin`、`forktest`，并在依赖可用时额外运行 `forktree` 和 `waitkill`。

阶段提交：

`lab5: implement process exit wait yield and kill`

### 阶段 5：完整检查、协同验证与实验素材收集

[PROMPT]

任务：对成员 B 的全部实现进行审计和回归测试，检查与 A、C 的接口兼容性，验证 COW 下的进程生命周期，并收集后续实验报告和答辩所需的真实素材。

操作要求：重新获取团队最新代码，根据实际进度开展集成验证。仅修复属于 B 的问题，不擅自修改其他成员的实现。

输出要求：提交本阶段真实代码修复、必要专项测试及运行证据，提供完整验收结果和遗留问题清单。

[RELY]

- 已完成的 B 成员代码
- A 提供的 Trap、syscall 和异常处理
- C 提供的普通 Fork、地址空间复制、COW 及引用计数机制
- `Makefile`、`tools/grade.sh`、`tools/inject.py`、`tools/test_lab5.c`
- 官方用户程序及 RISC-V GDB、readelf、objdump、nm
- 实验指导书及课堂录音

[GUARANTEE]

1. 核实成员 B 所有规定函数均已实现。
2. 完成可执行的官方及专项测试。
3. 检查 COW 下 exec、exit、wait 与地址空间回收。
4. 完成用户程序预加载相关思考题的分析素材。
5. 收集真实测试日志、必要截图、代码修改与 Bug 修复记录。
6. 检查本人全部阶段提交及远程个人分支状态。

[SPECIFICATION]

官方测试与专项测试

Pre-Condition：本人代码已完成，明确 A、C 的接口实现情况。

Post-Condition：每项测试均具有真实执行结果，明确区分通过、失败、未运行和依赖阻塞。

Case 1：队友实现尚未完成。继续进行本人模块的编译检查、静态审计和可隔离测试，记录缺失接口，不通过伪造实现获得虚假通过。

Case 2：队友实现已经可用。在不破坏个人分支及公共历史的临时集成 Worktree 或本地测试分支中验证，保留参与集成的各分支 SHA。不得将未经协商的队友代码混入本人的功能提交。

Case 3：发现 B 负责范围内的缺陷。根据真实失败信息形成补充 SPEC，修复后重新运行对应测试及必要回归。

Case 4：发现 A、C 负责范围内的缺陷。记录复现命令、相关调用链、错误现象和建议，不擅自跨分工修改。

Requirements：

- 完整集成后执行 `make clean`、`make`、`make qemu`、`make grade`。
- 官方 `make grade` 使用固定临时目录 `/tmp/lab5_grade`，避免同一环境中多个评分任务并发产生覆盖。
- 以官方 13 项测试为回归目标：badsegment、divzero、softint、faultread、faultreadkernel、hello、testbss、pgdir、yield、badarg、exit、spin、forktest。
- 重点验证 ELF 段加载、BSS 清零、用户栈、退出状态写回、重复 wait、非法参数、多个子进程回收、孤儿进程接管及等待期间被 kill。
- 对非法 ELF、资源分配失败和地址空间释放等无法由官方用户程序直接覆盖的路径，采用合理的独立测试；不得宣称未实际执行的测试通过。

COW 兼容性

Pre-Condition：成员 C 的 COW 实现及成员 A 的异常入口已经可用。

Post-Condition：进程执行 exec、exit 和 wait 时不会错误释放共享物理页，不会破坏其他进程的数据或权限。

重点验证：

- 父进程或子进程先退出时，另一进程仍可正确访问合法共享页面。
- exec 替换原地址空间时，共享物理页引用计数正确减少。
- 最后一个引用释放后，页面及地址空间正确回收。
- wait 向用户空间写入退出状态时，即使目标页处于 COW 状态，也不得破坏父子进程隔离。
- 不混淆 mm 引用计数与物理页引用计数。

若发现内存管理或异常处理接口不支持上述语义，记录并通知对应成员。不得自行接管 COW 核心实现。

思考题与课堂知识点

请收集可用于后续报告及答辩的分析素材，回答：

1. 用户程序在编译、链接、内核镜像加载和真正建立用户地址空间这几个阶段分别发生了什么？
2. 官方 Makefile 中 `--format=binary` 及 `_binary_obj___user_*_out_start`、`_size` 符号的作用是什么？
3. 为什么 Lab5 尚无文件系统，却能够加载 ELF 用户程序？
4. Lab5 的预加载方式与现代操作系统通过文件系统、虚拟内存映射及按需分页加载程序有何区别？
5. 如何通过构造 trapframe 和 sret 实现第一次进入用户态？
6. 课堂讲述的 ebreak 方案与手动构造 trapframe 方案有什么联系和区别？
7. 为什么进程退出时不能立即释放自己的内核栈及 PCB？

尽可能通过实际 Makefile、链接符号、ELF Program Header、反汇编及真实 GDB 观察支持分析，不虚构寄存器值或运行结果。

实验报告素材

当前阶段暂不编写 `report/report.md` 和 `report/prompt.md`，但必须保留：

- 各阶段实际使用的四段式 SPEC 及 Prompt。
- 各阶段 commit SHA、修改函数、关键实现及设计决策。
- 真实的编译错误、运行错误、Bug 原因、修复过程和重新验证结果。
- 官方测试与专项测试的完整日志。
- ELF 加载及首次进入用户态的分析证据。
- 用户程序预加载思考题的答案素材。
- 后续报告所需的 OS 理论知识对应关系，以及实验未涉及的重要理论内容。

原始日志和开发记录保存在独立的本地资料目录中，不将无关日志和构建产物提交 GitHub。

真实运行截图可以按 `report/images/` 规范保存，文件名使用 `b_` 前缀避免与队友冲突。无法自动截图时，记录需要人工截图的命令和位置，不得根据文字输出伪造终端截图。

阶段提交（按实际变更选择）：

`lab5: verify process lifecycle and add regression tests`

如果本阶段只有验证而无代码或正式素材变更，则记录测试结果，不制造空 commit。

### 最终交付要求

完成上述阶段后，进行最终 Git diff 和函数级审查，确认：

- 本人负责的 Lab5 功能模块二、四全部实现。
- Lab4 进程管理相关代码已按需正确迁移。
- 未覆盖其他成员的代码，未修改官方评分标准。
- 本人负责的官方实验标记保留且学号填写正确。
- 已完成的代码按照阶段独立 commit，并 push 至 `lab5-member-b`。
- 合适的话可以push到lab5
- 未提交 `docs/`、`.envrc`、凭据、构建产物及无关文件。

最终课程提交结构由团队在公共 `lab5` 分支统一整理：

```
code/
report/
├── report.md
├── prompt.md
└── images/
```

本次不提前撰写正式实验报告及 prompt.md，也不提前删除公共辅助文件或其他成员资料。

请直接执行上述任务，不要仅给出开发计划或示例代码。遇到一般技术问题时自主诊断并修复；对于真正依赖其他组员的事项，完成所有不受阻塞的工作并明确说明待集成验证项。

## 成员 C 2411788-马传志

以下为仓库 lab5 分支 report/prompt.md 中的四段式整理记录，不是逐字历史对话。

### 用户进程复制

```text
[PROMPT]
以官方 2026 Lab5 为基线，实现 do_fork、copy_mm、copy_thread、dup_mmap 和 copy_range。
迁移小组已完成的内存实现，不覆盖 A 的 Trap 文件或 B 的加载和退出函数。
[RELY]
proc_struct、mm_struct、vma_struct、trapframe；alloc_proc、setup_kstack、setup_pgdir、
set_links、hash_proc、wakeup_proc、mm_create、mm_count、get_pte、page_insert、exit_mmap。
注意 set_links 已增加进程计数，Lab5 Trap 字段为 tval，所有定义以真实源码为准。
[GUARANTEE]
保持四个模块三官方函数原型。成功返回子PID，子现场a0为0；失败返回负错误码。
[SPECIFICATION]
Pre：父线程和用户 mm 有效，分配器及内核映射可用。
Post：子PCB、栈、现场、父子关系及PID登记正确；共享整个mm和独立mm路径明确。
Cases：内核线程、CLONE_VM、普通Fork、各阶段内存不足、部分复制后失败。
Requirements：在登记前完成可失败分配，按已获得资源逆序清理；PID与挂链在临界区完成。
Validation：用实际生产函数验证返回值、页号、引用、计数、列表和失败资源恢复。
```

### COW 与内核写入

```text
[PROMPT]
加入COW：普通Fork使用独立mm/页表共享物理页，写时再复制。保留普通立即复制配置。
[RELY]
PTE软件位、VM_WRITE、Page引用、页表权限、sfence.vma、copy_to_user和用户地址校验。
[GUARANTEE]
提供cow_handle_store_fault(mm,fault_addr)，成功0，非法或OOM返回负值；给A说明成功后EPC保持不变。
[SPECIFICATION]
Pre：只处理合法可写VMA上的用户COW映射，当前仅保证单hart非抢占内核。
Post：写者获得独占可写映射，其他进程数据保持原值；真正只读区域仍不可写。
Cases：父先写、子先写、单引用恢复、多次Fork、退出、部分复制失败、内存不足、内核copyout。
Requirements：父子都撤销写权限并刷新TLB；更新映射和引用必须一致；禁止直接写共享物理页。
失败后父项可合法保留单引用COW，不要求PTE位模式完全恢复，但要求数据及资源一致。
Validation：对照立即复制与COW，记录物理页号、权限和引用；真实写错误必须重试原指令。
```

### 专项测试和 Dirty COW

```text
[PROMPT]
用独立QEMU环境先验证C能完成的模块，不补写A/B实现。分析Dirty COW并做受控错误模拟。
[RELY]
真实pmm/vmm/proc代码，测试专用分配器及静态环境，现有GCC/QEMU/GDB；NVD漏洞记录。
[GUARANTEE]
提供普通Fork、COW、GDB及变异测试脚本和原始结果；明确替代组件及未覆盖范围。
[SPECIFICATION]
Pre：独立编译真实模块，测试接口与正式接口一致。
Post：正常实现通过断言；临时副本绕过copyout的COW检查时被同一隔离断言检出。
Cases：正常共享/分离、只读拒绝、引用归零释放、所有分配失败、store fault与EPC不变。
Requirements：变异代码仅保留于忽略目录；不宣称原Linux竞态已经复现，不虚构官方成绩或截图。
Validation：保留实际日志、GDB数值和COW状态图。完整13项及生产U/S切换待A/B合并后回归。
```
