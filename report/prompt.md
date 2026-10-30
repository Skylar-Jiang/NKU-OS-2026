# Lab 3 Prompt 汇总

本文按成员和模块整理关键技术 Prompt，采用 Lab 0.5 的四段式 SPEC：`[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]`。以下为依据实际实验任务整理的技术版内容。

## 甲：马传志（2411788）

**使用模型：** Codex，`gpt-6.1-sol`。

### Lab2 代码继承：Page Init 与 Best-Fit

`````text
# 第一阶段：继承物理内存初始化与 Best-Fit 分配器

本阶段遵循 Lab 0.5 的四段式 SPEC 规范。

## [PROMPT]

**任务：**

参考小组已完成的 Lab2 代码，补齐并核对 Lab3 中的物理内存管理继承部分。
重点检查 Page Init 的内存边界、Best-Fit 的分配与回收规则，以及 Lab3 新增的中断保护。

**操作要求：**

- 核对小组仓库 lab2 分支的版本，以提交 8b06753 为本阶段参考。
- 优先修改 kern/mm/pmm.c、best_fit_pmm.c 及必要的内存布局常量。
- 保留 Lab3 已有的 local_intr_save/local_intr_restore 包装。
- 保留分配器公共接口和框架自检，不改评分逻辑。
- 不覆盖其他成员负责的中断、时钟和异常处理代码。
- 以真实工程定义为准，不复制无关模块，不硬编码空闲页面数量。

## [RELY]

请阅读并核实：

- kern/mm/pmm.c、pmm.h
- kern/mm/best_fit_pmm.c、best_fit_pmm.h
- kern/mm/memlayout.h、mmu.h
- kern/driver/dtb.c、dtb.h
- kern/sync/sync.h
- libs/list.h、atomic.h

可依赖的接口与数据结构包括：

- get_memory_base()、get_memory_size()、init_memmap()
- struct Page、struct pmm_manager、free_area_t
- pages、npage、nbase、va_pa_offset
- PADDR、pa2page()、page2pa()、PGSIZE、ROUNDUP、ROUNDDOWN
- PageReserved、PageProperty 及相应标记操作
- list_init、list_add、list_add_before、list_del、list_next、list_prev
- local_intr_save、local_intr_restore

目标为 RV64、单 hart 实验配置。接口签名和宏语义均以实际源码为准。

## [GUARANTEE]

保持已有接口：

- page_init(void)
- best_fit_init(void)
- best_fit_init_memmap(struct Page *base, size_t n)
- best_fit_alloc_pages(size_t n)
- best_fit_free_pages(struct Page *base, size_t n)
- best_fit_nr_free_pages(void)

允许添加必要的内部检查，不改变公共物理内存管理接口。
正常配置启用 Best-Fit，并使 alloc_pages/free_pages 保留调用前的中断使能状态。

## [SPECIFICATION]

### Page Init

**Pre-Condition：**

- 启动页表已建立，DTB 初始化已执行。
- 当前物理内存管理器已初始化，页描述符布局尚未建立。
- 设备树内存范围需要经过有效性和溢出检查。

**Post-Condition：**

- npage 表示可管理物理页号的排他上界，描述符数量为 npage - nbase。
- pages 指向合法且对齐的描述符数组，虚实地址换算一致。
- 固件、内核映像、描述符数组和对齐占用受到保护。
- 管理器仅接管合法、完整的空闲物理页。

**Case 1：有效内存范围**

依据 DTB 信息及 PHYSICAL_MEMORY_END 确定管理范围，使用物理地址进行上限比较。

**Case 2：无效范围或空间不足**

对零长度、地址溢出、无法容纳内核或描述符数组等情况明确报错，不继续访问非法内存。

**Case 3：页面对齐与保留空间**

内核结束位置和描述符占用按实际布局处理，完整空闲页的起点向上对齐、终点向下对齐。
不存在可用完整页面时，不登记无效空闲区。

### Best-Fit

**Pre-Condition：**

- 分配页数大于零。
- 初始化区间合法且不重叠；释放参数对应合法、未重复释放的已分配区间。

**Post-Condition：**

- 空闲块头按地址递增挂链，PG_property 与 property 的含义一致。
- 分配选择足够大的最小块，等长时选择较低地址；必要时正确拆分。
- 释放后正确插入，并与物理相邻的前后空闲块合并。
- nr_free 等于空闲链表各块页数之和。

**Case 1：精确匹配与等长候选**

精确匹配不产生剩余块，等长候选的选择保持确定性。

**Case 2：有剩余空间**

返回前 n 页，剩余块保持正确位置和长度；已分配块不保留空闲块头标记。

**Case 3：空闲总量不足或碎片不足**

无法满足连续分配时返回 NULL，失败不改变链表和计数。

**Case 4：释放与合并**

覆盖空表插入、无相邻块、仅前块相邻、仅后块相邻和双向合并。
以物理连续关系判断合并，不能仅凭链表节点相邻。

**Requirements：**

- 不将单 hart 关中断保护描述为多核互斥。
- 不整体覆盖为 Lab2 中缺少中断保护的接口包装。
- 不修改原 basic_check/best_fit_check 的验证内容。

### 本阶段验证

1. 检查实际源码和 Git diff，确认正常配置选择 Best-Fit。
2. 编译并执行框架自检及原 make grade。
3. 在独立测试构建中覆盖初始化、拆分、失败与前后合并。
4. 用独立位图参考模型核对分配地址、空闲总量和链表状态。
5. 改变 QEMU 内存配置，核对描述符、保留区和管理上限。
`````

### Trap 上下文恢复与入口

`````text
# 第二阶段：核对 Trap 入口、上下文恢复与返回流程

## [PROMPT]

**任务：**

对照 SAVE_ALL 和 struct trapframe，核对 kern/trap/trapentry.S 中的
RESTORE_ALL、__alltraps、__trapret，完成必要的完善和布局检查。

建立“保存现场 → C 处理 → 恢复现场 → sret 返回”的完整衔接关系。
输入包中已有的正确实现应保留并验证，不因重写而引入无关变化。

**操作要求：**

- 保持 SAVE_ALL、struct trapframe 字段和 trap() 函数签名不变。
- 逐项核对保存区偏移，不能自行假定另一种结构布局。
- 原 SP 必须从对应保存槽恢复，并安排在最后。
- 不在底层统一返回代码中给 epc 添加固定偏移。
- 不修改乙、丙负责的 interrupt_handler()/exception_handler()。
- 本阶段针对当前 S 态内核栈，不扩展用户态栈切换和浮点/向量现场。

## [RELY]

请阅读并核实：

- kern/trap/trapentry.S、trap.h、trap.c
- libs/riscv.h 中 REGBYTES、LOAD、STORE 的定义
- RISC-V 的 a0 参数传递约定和栈对齐要求
- sstatus、sepc、sscratch、scause 及 sret 的含义

RV64 下共有 36 个寄存器宽度的槽位，每槽 8 字节，总大小为 288 字节。
槽 0～31 对应整数通用寄存器，槽 32～35 对应 status、epc、badvaddr、cause。
SAVE_ALL 通过 sscratch 保存原 SP，并将其写入槽 2。

## [GUARANTEE]

对外保持：

- __alltraps：统一 Trap 入口。
- trap(struct trapframe *tf)：接收当前保存区地址。
- __trapret：恢复现场并执行 sret。

C 层返回后，按 Trap frame 中的值恢复通用寄存器、原 SP、返回 PC 和相应状态。
使用编译期检查约束 C 结构布局与汇编保存区的对应关系。

## [SPECIFICATION]

**Pre-Condition：**

- stvec 已指向统一入口。
- 进入 Trap 前内核栈有效且满足 ABI 对齐要求。
- C 处理函数遵守调用约定，并负责必要的 tf->epc 调整。

**Post-Condition：**

- 保存后 a0 指向当前 Trap frame，作为 trap() 的第一个参数。
- C 函数返回后进入 __trapret，保存区仍可正确访问。
- sstatus、sepc 从对应槽位写回，通用寄存器恢复为保存值。
- 原 SP 最后恢复，sret 返回到处理后的 epc。

**Case 1：CSR 与临时寄存器恢复**

先完成 sstatus/sepc 的写回，再恢复临时使用的 s1、s2 原值。
避免 CSR 临时值最终残留在通用寄存器中。

**Case 2：SP 恢复顺序**

所有依赖当前保存区地址的加载完成后，再恢复 x2；不能提前失去保存区基址。

**Case 3：时钟中断与同步异常**

统一入口沿用 C 层处理后的 epc。
时钟中断保留原返回位置，异常指令的长度处理由异常处理函数负责。

**Case 4：诊断字段与硬件状态**

x0 恒为零，无需回写；scause/stval 是诊断快照，不作为本实验常规恢复目标。
sret 会更新部分状态位，不能要求整个 sstatus 与 Trap 前逐位相同。

**Requirements：**

- 保持保存区大小和关键偏移一致，保证 C 调用时的栈对齐。
- 检查实际 QEMU 版本对 SPIE 的行为，不为迎合错误预期而改写内核。
- 不把当前验证范围扩展为已经支持用户态切换或完整嵌套中断。

### 本阶段验证

1. 编译检查保存区大小和关键字段偏移。
2. 对照反汇编核对入口调用、恢复顺序和最终 sret。
3. 用 GDB 观察入口、C 参数、保存区、__trapret 和实际返回位置。
4. 比较原 SP、通用寄存器和应恢复的中断状态。
`````

### GDB 调试与独立测试

`````text
# 第三阶段：验证内存管理与 Trap 现场恢复

## [PROMPT]

**任务：**

为甲负责的内存管理继承和 Trap 底层机制建立可重复验证。
结合定向测试、独立参考模型和实际 QEMU/GDB 调试，检查关键行为与边界情况。

**操作要求：**

- 测试代码集中在 code/tools/member_a/tests，使用独立构建注入。
- 不向正式 init.c、trap.c 插入测试入口，不改变原评分规则。
- 保留原始命令输出，说明测试覆盖范围及失败原因。
- 不根据静态分析推断 PASS，不将测试计划写成已经通过的结果。

## [RELY]

- 使用本机已安装的 RISC-V 工具链、GDB 和 QEMU 4.1.1。
- 正式内核可以构建，现有 C 处理函数支持显式 32 位断点测试的返回。
- GDB 可连接 QEMU 的客体调试接口。
- 测试运行目录与源码目录分开，避免复制构建时递归包含自身。

## [GUARANTEE]

提供独立测试入口、完整运行日志及可复现的 GDB 检查步骤。
测试完成后，正式源码中的共享处理函数和框架自检内容保持原义。

## [SPECIFICATION]

**Pre-Condition：**

- 工具版本已核对，待测源码版本明确。
- 独立测试使用合法分配、释放区间和有效的内核栈。

**Post-Condition：**

- 每项结果能够对应实际输出，失败时明确保留原因。
- 能核对保存区、恢复后寄存器和返回位置。
- 原框架评分与额外验证分别记录，覆盖范围明确。

**Case 1：分配器参考模型**

采用三个固定种子，合计 3000 次操作；独立计算最小可用连续区间。
每次核对返回地址、空闲总页数、链表顺序、双向连接和块头标记。

**Case 2：内存范围变化**

检查 64、128、256 MiB 配置下的描述符布局、保留页和管理终点。

**Case 3：寄存器保存与恢复**

用汇编设置可区分的寄存器值，保持有效 SP，触发显式 32 位 ebreak。
连续执行三轮，并检查所有整数寄存器槽位、原 SP 和返回状态。

**Case 4：恢复前主动扰动**

用 GDB 在 __trapret 处改写除 x0、SP 外的 30 个通用寄存器。
执行恢复后逐项比较保存值，检查是否存在遗漏恢复。

**Case 5：中断状态与工具差异**

检查调用前 SIE 为 0/1 时的临界区状态恢复。
GDB 对 SP 做整数运算前明确转换类型。
QEMU 4.1.1 的 sret 会清零 SPIE，应结合实际源码解释观测结果。

**Requirements：**

- GDB 使用指令地址断点检查现场，必要时用 si 对照汇编单步。
- 额外测试只使用明确的 32 位断点，不据此宣称 c.ebreak 或 Challenge 3 已通过。
- 说明 do...while(0) 将多语句宏组织为单语句的作用，以及原中断状态恢复的必要性。
- 整组代码合并后重新验证，不能将甲部分结果直接视为整组最终验收。
`````
