# Lab 2 Prompt 汇总

各成员只收录自己真实发送过的 Prompt 原文，并注明来源会话、时间和是否为独立消息；无法核实的条目保留 [待整理]，不补写或改写。甲的部分已于 2026-10-15 按原始会话记录整理，乙、丙的部分由本人核实后填写。

## 甲

来源：甲本人在 Codex CLI（0.162.1，模型 `gpt-6.1-sol`，effort high）中，于 **2026-10-14 01:17:55（UTC+8）** 发送的**一条**总控指令《NKU OS Lab 2：独立工作区迁移与甲负责模块完整开发》。该指令共七个阶段，下面的 Page Init、Buddy 实现、Buddy 测试分别是其中第二、三、四阶段的原文（由原始会话记录逐字截取，未改写）。

> 这三段是**同一条消息的组成部分**，不是三次独立发送给模型的 Prompt。第四阶段原文自己也写明“这份总控指令不应被事后虚构为两次独立发送给模型的用户消息”。该会话中甲的其余用户消息只有工作区初始化（同日 00:43）和阶段性提交、推送（同日 12:32）两条，都与代码生成无关，原文未收录。会话中没有针对 Bug 的追加修复 Prompt：Page Init 和 Buddy 的生成代码在测试中没有出现算法失败，调试脚本的问题由 Agent 在同一轮执行中自行修正（见 `report.md` 中甲的 Bug 记录）。

### Page Init（总控指令第二阶段原文）

对照 Lab 0.5 四段式要求：具备 [PROMPT]/[RELY]/[GUARANTEE]/[SPECIFICATION]，函数规格含 Pre/Post-Condition、Case 1–5 和 Requirements。与规范的差异：[RELY] 只列出了名称，并要求 AI 自行“阅读并核实”源码，没有按 Lab 0.5 的建议把真实定义复制进来；另外一条消息里同时包含了 Git 和流程约束，粒度比“一个功能一个 Prompt”大。

`````text
# 第二阶段：实现物理内存初始化 Page Init

本阶段遵循 Lab 0.5 的四段式 SPEC 规范。

## [PROMPT]

**任务：**

实现 uCore Lab 2 的物理内存初始化功能，在 `code/kern/mm/pmm.c` 中补全 `page_init()`。

该功能负责将设备树提供的物理内存信息转换为内核可以管理的物理页描述符布局，并把排除系统已占用区域后的合法空闲物理页面交给当前 `pmm_manager`。

**操作要求：**

- 必须修改实际源码文件。
- 保留已有代码和 `lab2` 注释。
- 不擅自填写未知学号。
- 不修改 Best-Fit、SLUB 或 First-Fit 的实现。
- 不修改官方测试及评分脚本。
- 不提前切换正式物理内存管理器。
- 优先在甲负责的文件范围内完成实现。
- 以真实工程定义为准，不引入无关依赖。

本阶段不修改 `report/prompt.md`。

## [RELY]

请阅读并核实：

- `kern/mm/pmm.c`
- `kern/mm/pmm.h`
- `kern/mm/memlayout.h`
- `kern/mm/mmu.h`
- `kern/driver/dtb.c`
- `kern/driver/dtb.h`
- `tools/test_lab2.c`

可依赖的内容包括：

- `get_memory_base()`
- `get_memory_size()`
- `init_memmap()`
- `pa2page()`
- `page2pa()`
- `PADDR`
- `struct Page`
- `pages`
- `npage`
- `nbase`
- `va_pa_offset`
- `PGSIZE`
- `PPN`
- `ROUNDUP`
- `ROUNDDOWN`
- `SetPageReserved`
- `PageReserved`
- `end`
- `panic()`
- `cprintf()`

所有函数签名和宏语义均应以实际源码为准。

## [GUARANTEE]

必须实现：

`static void page_init(void);`

允许添加必要的 `static` 辅助函数。

不改变现有公共物理内存管理接口。

## [SPECIFICATION]

### page_init()

**Pre-Condition：**

- 内核已经建立并使用所需的启动页表。
- DTB 初始化流程已执行。
- 当前物理内存管理器已经初始化。
- 物理页描述符布局尚未建立。
- 设备树内存数据可能有效或异常。

**Post-Condition：**

- `va_pa_offset` 正确反映内核虚实地址映射。
- `npage` 正确表示可管理物理页号的上界。
- `pages` 正确指向 Page 描述符数组。
- Page 描述符能够与实际物理页面正确对应。
- 已占用页面被保留，不会被分配器接管。
- 当前物理内存管理器只接管合法、完整、空闲的物理页面。

**Case 1：有效物理内存范围**

根据 DTB 提供的数据确定可管理物理内存范围。

正确处理物理内存上界、页号与 Page 描述符之间的关系。

**Case 2：无效物理内存信息**

设备树信息缺失、范围异常、大小无效或出现地址溢出时，不得继续初始化非法内存。

**Case 3：系统已占用内存**

OpenSBI、内核镜像和 Page 描述符数组所占用的页面不得进入空闲分配器。

内核末端未对齐时，其所在页面仍必须受到保护。

**Case 4：页面对齐**

只向管理器注册完整的空闲物理页面。

不包含对齐空隙和不完整尾页。

**Case 5：不存在可用完整页面**

不调用 `init_memmap()` 注册无效空闲区。

**Requirements：**

- 明确区分虚拟地址、物理地址、物理页号、Page 描述符地址。
- 避免 `pages` 数组越界。
- 避免描述符元数据覆盖内核空间。
- 不硬编码空闲页面数量。
- 正确兼容当前框架对 `npage` 和 `nbase` 的定义。
- 默认 QEMU 配置下满足 `test_page_init()` 的要求。

### 本阶段验证

1. 检查实际修改的源码。
2. 查看 Git diff。
3. 进行静态检查和可执行的编译测试。
4. 对照 `test_page_init()` 检查关键行为。
5. 有条件时运行 Page Init 的动态测试。
6. 如因乙尚未实现 Best-Fit 导致完整构建失败，明确标记阻塞原因。
7. 不允许为了完成构建擅自修改乙的代码。
8. 不根据静态分析虚构 PASS。

完成后进入第三阶段。
`````

### Buddy 实现（总控指令第三阶段原文）

对照 Lab 0.5：四段齐全，每个回调都有 Pre/Post-Condition 和 Case，释放契约在 Requirements 中要求明确。差异：[GUARANTEE] 的函数签名省略了返回类型（例如 `buddy_init(void)`），[RELY] 同样只列名称。

`````text
# 第三阶段：实现 Buddy System

Buddy System 是 Lab 2 功能模块二的正式任务，不是选做 Challenge。

本阶段负责生成算法实现，下一阶段单独生成测试。

## [PROMPT]

**任务：**

从零实现符合现有 uCore 物理内存管理框架的 Buddy System 伙伴分配器。

优先新增：

- `code/kern/mm/buddy_pmm.c`
- `code/kern/mm/buddy_pmm.h`

算法应支持以 2 的整数次幂大小组织物理页块，并正确完成分配、拆分、释放、伙伴合并和空闲页统计。

**操作要求：**

- 在项目中修改实际文件。
- 参考现有 `pmm_manager` 接口。
- 使用现有 Page 描述符、链表和内核支持函数。
- 可以新增必要的内部数据结构。
- 不覆盖 First-Fit 和 Best-Fit。
- 不修改乙负责的 SLUB。
- 不永久替换正式版的 Best-Fit。
- 不修改官方评分函数。
- 不将普通相邻块合并冒充 Buddy 伙伴合并。

本阶段不修改 `report/prompt.md`。

## [RELY]

现有：

`struct pmm_manager`

其中提供：

- `init`
- `init_memmap`
- `alloc_pages`
- `free_pages`
- `nr_free_pages`
- `check`

可使用：

- `struct Page`
- `PageReserved`
- `PageProperty`
- `SetPageProperty`
- `ClearPageProperty`
- `page2pa`
- `pa2page`
- `list_entry_t`
- `libs/list.h`
- `assert`
- `panic`
- `cprintf`

请检查真实定义与可见性。

## [GUARANTEE]

实现对应接口：

- `buddy_init(void)`
- `buddy_init_memmap(struct Page *base, size_t n)`
- `buddy_alloc_pages(size_t n)`
- `buddy_free_pages(struct Page *base, size_t n)`
- `buddy_nr_free_pages(void)`
- `buddy_check(void)`

注册：

`const struct pmm_manager buddy_pmm_manager;`

头文件声明该管理器。

内部辅助函数可以为 `static`。

## [SPECIFICATION]

### buddy_init()

**Pre-Condition：**

Buddy 尚未初始化。

**Post-Condition：**

- 各阶空闲块管理状态正确建立。
- 初始空闲页数为零。
- 可以接收合法的可用内存区间。

### buddy_init_memmap(base, n)

**Pre-Condition：**

- `base` 指向合法的 Page 描述符。
- `n > 0`。
- 输入范围对应连续、可用的物理页。
- 页面尚未被 Buddy 接管。

**Post-Condition：**

- 合法页面均被纳入 Buddy。
- 空闲块大小符合 2 的整数次幂。
- 各块不重叠。
- 空闲页数正确。
- 不管理保留页或非法地址范围。

**Case 1：区间适合高阶空闲块**

可以组织成较大阶块。

**Case 2：区间起点不满足高阶对齐要求**

能够通过较小阶块合法组织页面。

**Case 3：可用页数不是 2 的幂**

仍可管理全部合法页面，不丢失剩余页。

### buddy_alloc_pages(n)

**Pre-Condition：**

- Buddy 已初始化。
- `n > 0`。
- 当前可能存在或不存在足够的空闲页面。

**Post-Condition：**

- 成功时返回至少满足 n 页的连续物理页。
- 不重复分配正在使用的页面。
- 空闲块状态及统计正确。
- 失败时返回 NULL，原状态保持有效。

**Case 1：存在足够大小的块**

正确分配。

**Case 2：需要拆分更高阶块**

能够通过逐级拆分获得目标块。

**Case 3：请求不是 2 的幂**

例如请求 3 页，内部可能分配 4 页。

必须正确记录实际分配容量，保证以后能够完整回收。

**Case 4：空间不足**

返回 NULL，不破坏原有空闲状态。

### buddy_free_pages(base, n)

**Pre-Condition：**

- 输入符合已定义的合法释放契约。
- 对应页面属于先前的有效分配。

**Post-Condition：**

- 实际分配块得到完整释放。
- 空闲页统计正确恢复。
- 合法伙伴能够合并。
- 不错误合并非伙伴或正在使用的页面。

**Case 1：伙伴仍被占用**

不合并。

**Case 2：同阶伙伴空闲**

合并为更高阶块。

**Case 3：连续多个阶满足合并条件**

支持递归或迭代向上合并。

**Case 4：原始请求不是 2 的幂**

能够根据分配元数据完整恢复实际占用块。

**Case 5：非法或重复释放**

按照明确的错误处理策略进行检测，不能悄悄破坏管理状态。

**Requirements：**

- 明确 `free_pages(base,n)` 的调用契约。
- 明确是否支持部分释放。
- 若仅支持完整块释放，必须清楚说明限制和原因。
- 不得通过忽略额外分配的页来造成内存泄漏。

### buddy_nr_free_pages()

**Post-Condition：**

返回当前真实空闲页面总数。

统计结果与各阶空闲块容量之和一致。

### buddy_check()

**Post-Condition：**

提供合法的自检入口，并为下一阶段独立测试提供支持。

### 整体要求

- 遵守 uCore freestanding 环境。
- 不使用宿主机 `malloc` 等用户态内存分配函数。
- 不修改官方评分脚本。
- 优先保持公共接口不变。
- 对复杂度不做额外要求，以正确性为先。
- 保持 Buddy 与现有管理器框架兼容。
- 确认新增源码可以被 Makefile 正确识别。

### 本阶段验证

检查实际代码、接口、状态不变量和编译结果。

重点检查非 2 的幂请求、真实占用容量、释放与计数一致性。

如果整个内核尚无法编译，应尽可能进行 Buddy 模块级验证，并明确未验证部分。

随后进入第四阶段。
`````

### Buddy 测试（总控指令第四阶段原文）

这是老师要求的第二份 Buddy Prompt（测试生成）。它作为同一条总控指令中单独的一个阶段，在实现阶段之后执行，不是单独发送的消息。

`````text
# 第四阶段：Buddy System 独立测试

实验指导书要求单独设计一份 Buddy 测试 Prompt。

本阶段作为与实现阶段分开的测试规格执行。不得因为实现由 AI 生成，就默认其行为正确。

这份总控指令不应被事后虚构为两次独立发送给模型的用户消息。以后整理正式 Prompt 时，应根据真实交互记录和老师要求，由小组三人分别整理实现 Prompt 与测试 Prompt。

## [PROMPT]

**任务：**

针对实际生成的 Buddy System 实现，设计可在 uCore 环境运行的独立测试。

优先新增：

`code/kern/mm/buddy_test.c`

必要时增加对应头文件或最小测试入口。

**操作要求：**

- 先阅读 Buddy 的真实实现。
- 根据行为规格定义预期结果。
- 生成实际测试代码。
- 不重写算法来适配错误测试预期。
- 不修改官方 `basic_check()`、`best_fit_check()` 或评分脚本。
- 不声称尚未运行的测试已经 PASS。

本阶段不修改 `report/prompt.md`。

## [RELY]

使用上一阶段的 Buddy 接口，以及现有：

- Page 描述符
- 分配和释放接口
- 空闲页统计接口
- `assert`
- `cprintf`
- 内核可用的测试辅助函数

## [GUARANTEE]

交付：

- Buddy 测试函数或模块。
- 实际测试入口。
- 测试用例集合。
- 可读的测试结果输出。
- 测试运行方法。
- 实际发现的问题及修复建议。

## [SPECIFICATION]

### Buddy 测试入口

**Pre-Condition：**

- Buddy 已正确初始化。
- 测试使用合法的页面范围。
- 每个测试有明确的初始状态。

**Post-Condition：**

- 测试结果可与预期比较。
- 正常测试结束后系统状态能够恢复。
- 不破坏仍在使用的物理页面。

**Case 1：基本分配**

测试 1、2、4、8 页的分配、连续性、互不重叠和回收。

**Case 2：非 2 的幂请求**

测试 3、5、6 页的分配和完整释放。

验证内部实际占用容量及空闲页统计。

**Case 3：大块拆分**

验证从较大阶块逐步拆分得到目标块。

**Case 4：伙伴合并**

验证合法同阶伙伴释放后能够合并。

**Case 5：多层合并**

验证经过多次释放能够逐层恢复较大空闲块。

**Case 6：错误合并防护**

验证非伙伴块及正在使用的伙伴不会被错误合并。

**Case 7：内存不足**

验证分配失败时返回 NULL，并保持原有空闲状态。

**Case 8：不规则内存范围**

验证起点不满足高阶对齐要求、区域大小不是 2 的幂等情况。

**Case 9：非法释放**

根据实现定义检查重复释放和非法释放。

预期触发 panic 的测试必须隔离，不能破坏正常回归测试。

**Requirements：**

- 检查空闲页数量变化。
- 检查分配页面不重叠。
- 检查释放后的状态恢复。
- 检查不同阶的块拆分和合并。
- 不得降低测试强度以得到 PASS。
- 不得伪造日志或结果。
- 如果当前不能执行 QEMU 测试，应如实说明原因。

### 本阶段验收

整理测试用例、真实执行结果及问题清单。

如果发现 Buddy 的实现问题，先保留必要的失败证据，再进行最小修复并重新测试。

不修改乙负责的模块。

完成后进入第五阶段。
`````

### QEMU/GDB 地址转换调试

来源：甲本人在 Claude Code（模型 `claude-opus-5-5`）中，于 2026-10-15 20:31（UTC+8）发送的第二轮任务指令《NKU OS Lab 2：合并后独立验收、GDB 实验与报告完善》第 3 节原文（逐字截取）。该指令还包含报告、截图和 Git 等其他节，未收录。它是自然语言任务说明，不是四段式代码生成 Prompt。

`````text
3. 甲亲自复现一次 QEMU/GDB 地址转换
先复用丙已经准备好的带符号 QEMU 4.1.1、构建说明及真实源码路径，尽可能独立复现，不必重复构建已经可用的环境。
要求：

1. 用宿主 GDB 调试 QEMU 源码，观察 `riscv_cpu_tlb_fill()`、`get_physical_address()` 及实际 PTE 读取过程。
2. 选择明确的内核虚拟地址，记录命中断点时的地址、访问类型、SATP、根页表地址、VPN 索引、PTE、物理地址及页内偏移。
3. 解释 TLB Miss 后为什么进入该路径，以及 Sv39 超级页映射为何可以提前结束遍历。
4. 区分宿主 GDB 与客户机 RISC-V GDB 的用途。
5. 如果老师要求观察 TLB Hit、Bare 模式或 load/store 翻译，则根据指导书和实际源码继续完成对应部分；这些扩展不应被假定已经验证。
6. 保留本次独立复现的真实命令、日志和结果，不将丙的日志冒充甲的实验记录。

如果缺少宿主 GDB、带符号 QEMU 或 GUI 权限，优先尝试安全复用现有环境；确实受阻时，保留可复现步骤并继续其他独立任务，不为了完成实验破坏系统配置。
`````

### 其他调试 Prompt

- [待整理]：没有其他可核实的、用于代码生成或调试的独立 Prompt。

## 乙

来源：乙提供的《实验二_乙》Word 文档；原文由乙整理在 [member-b/prompt.md](member-b/prompt.md)，以下逐字收录，字段与顺序未改写。其中“三人分工”一段用于全组任务划分，其余各段依次对应 Best-Fit 实现、SLUB 实现、SLUB 测试、SLUB 测试检查和 SLUB 测试修正。

### 三人分工

```text
[PROMPT]
任务：检查 Lab 2 实验源码及手册，确定全部基础、进阶及 QEMU 调试任务的三人分工。
操作要求：只分析源码，不修改文件。
输出要求：给出三人分工、涉及文件、工作量及执行顺序。
[RELY]
以实际工程源码、实验手册及 lab2、YOUR CODE 标记为依据。
[GUARANTEE]
必须覆盖 page_init、First-Fit、Best-Fit、Buddy、SLUB、QEMU+GDB 调试及实验报告。
[SPECIFICATION]
Pre-Condition：已获得 Lab 2 源码及实验手册。
Post-Condition：形成完整、合理的三人分工表。
Requirements：
工作量尽量均衡，明确每人任务、涉及文件及依赖关系，避免多人修改同一文件造成冲突，不遗漏任何必做、进阶及分支任务，本阶段不编写或修改代码，实验报告三人各自完成自己的部分
```

### Best-Fit 实现

```text
[PROMPT]
完成 Lab 2 的 Best-Fit 连续物理页分配器，直接编写代码。
[RELY]
参考 default_pmm.c、best_fit_pmm.c、pmm.h、memlayout.h 和 libs/list.h，严格遵循现有 pmm_manager 接口及实验手册要求。
[GUARANTEE]
1. 只修改 kern/mm/best_fit_pmm.c。
2. 保留 //lab2 function2-2 YOUR CODE 标记。
3. 不修改原有 basic_check()、best_fit_check()。
4. 不修改甲负责的 pmm.c，也不修改其他组员文件。
5. 不修改评分脚本，不执行编译、测试、提交或推送。
[SPECIFICATION]
Pre-Condition：First-Fit 已实现，Best-Fit 只有原始框架和测试函数。
Post-Condition：完成可注册的 best_fit_pmm_manager。
Requirements：
1. 实现初始化、空闲块登记、页分配、页释放、空闲页统计和管理器注册。
2. 分配时选择满足请求且大小最小的空闲块。
3. 正确处理空闲块拆分、相邻块合并、链表维护及 Page.property、nr_free 更新。
4. 保证与原有测试接口兼容。
5. 使用简短中文注释，保持原有代码风格。
完成后列出修改文件和实现函数，等待我自行编译运行。
```

### SLUB 实现

```text
[PROMPT]
完成 Lab 2 Challenge：实现简化版 SLUB 内存对象分配器，直接编写代码。
[RELY]
参考 pmm.h、pmm.c、memlayout.h、best_fit_pmm.c、kern/init/init.c 和实验手册中的 SLUB 要求。
[GUARANTEE]
1. 仅新增 kern/mm/slub.c、kern/mm/slub.h，必要时修改 kern/init/init.c。
2. 不修改甲负责的 pmm.c、Buddy 模块和 Best-Fit 源码。
3. 不修改 Makefile、原有测试或评分脚本。
4. 不使用标准库 malloc/free。
5. 不执行编译、测试、提交或推送。
[SPECIFICATION]
Pre-Condition：工程已有物理页分配接口，尚无 SLUB 实现。
Post-Condition：完成可接入内核的简化 SLUB 两层分配器。
Requirements：
1. 底层通过 alloc_pages()、free_pages() 管理物理页。
2. 上层按对象大小管理页内空间，支持不同大小对象的分配与回收。
3. 实现 slub_init()、slub_alloc()、slub_free() 等接口。
4. 正确处理对象对齐、空闲槽位复用、页回收和内存不足。
5. 正确进行物理地址与内核虚拟地址转换。
6. 在 pmm_init() 后接入 SLUB 初始化，不破坏原有启动逻辑。
7. 保持实现简洁，添加必要的中文注释。
本阶段只完成分配器实现，不编写测试。
完成后列出新增文件、核心函数及支持的对象大小，等待后续测试。
```

### SLUB 测试

```text
[PROMPT]
为刚刚实现的 SLUB 分配器编写独立测试代码，按照实验手册要求完成 Challenge 测试。
[RELY]
以当前 slub.c、slub.h、pmm.h、kern/init/init.c 的实际接口为依据。
[GUARANTEE]
1. 新增 kern/mm/slub_test.c。
2. 必要时修改 slub.h 和 kern/init/init.c 接入测试。
3. 不修改 SLUB 算法实现、Best-Fit、Buddy、pmm.c、Makefile 或评分脚本。
4. 不删除断言，不伪造成功输出。
5. 不执行编译、QEMU、提交或推送。
[SPECIFICATION]
Pre-Condition：SLUB 分配器代码已完成。
Post-Condition：生成可在 QEMU 内核环境中执行的 SLUB 测试模块。
Requirements：
1. 测试不同大小对象分配，包括 8、32、64、128、512、1024 字节及非 2 的幂大小。
2. 检查指针有效性、地址对齐和对象不重叠。
3. 测试连续分配、交替释放及空间复用。
4. 测试零大小、超出支持范围和内存不足等边界情况。
5. 根据实际实现验证空闲页回收。
6. 使用断言验证结果，通过后再由 cprintf() 输出测试信息。
7. 在 SLUB 初始化完成后调用测试，不影响原有内核功能。
8. 测试发现算法 Bug 时只报告，不擅自修改实现。
完成后列出测试项目和修改文件。
```

### SLUB 测试检查

```text
[PROMPT]
检查当前 SLUB 测试代码的逻辑正确性，重点检查物理页耗尽测试。
[RELY]
依据当前 slub.c、slub_test.c、slub.h、pmm.h 的实际实现。
[GUARANTEE]
只读检查，不修改文件，不运行测试，不提交代码。
[SPECIFICATION]
Pre-Condition：SLUB 测试代码已经编写完成，尚未运行。
Post-Condition：确认测试不会因错误假设导致误判、死循环或内存损坏。
Requirements：
检查物理页耗尽后，SLUB 是否仍可能利用已有空闲对象槽位。
检查内存不足测试是否有明确的循环边界。
检查测试过程中申请的物理页是否全部归还。
检查空闲页计数的断言是否合理。
检查测试是否可能破坏原有 PMM 状态。
仅报告发现的问题和对应位置，不修改代码。
```

### SLUB 测试修正

```text
[PROMPT]
修复 Lab 2 SLUB 测试中的内存耗尽误判和资源清理问题，采用最小改动原则，直接修改代码。
[RELY]
依据当前工程中的 kern/mm/slub_test.c、slub.c、slub.h、pmm.h 及此前静态检查发现的问题。
[GUARANTEE]
1. 仅修改 kern/mm/slub_test.c。
2. 不修改 SLUB 分配器、Best-Fit、Buddy、pmm.c、Makefile 或评分脚本。
3. 不删除内存不足测试，不降低测试标准。
4. 不伪造测试结果。
5. 不执行编译、QEMU、Git 提交或推送。
[SPECIFICATION]
Pre-Condition：
现有测试使用 assert(slub_alloc(16) == NULL) 判断内存耗尽，但 SLUB 可能仍存在可复用的对象槽位。断言失败还会阻止后续物理页回收。
Post-Condition：
内存耗尽测试不再依赖此前的 SLUB 缓存状态，能够正确验证分配失败并回收临时资源。
Requirements：
1. 区分物理页耗尽与 SLUB 对象槽位耗尽。
2. 允许复用已有空闲槽位，但必须最终验证空间不足时分配返回 NULL。
3. 使用有明确上限的测试逻辑，避免死循环。
4. 正确记录并释放测试期间申请的物理页及对象。
5. 先记录验证结果，再回收临时资源，最后执行断言，避免验证失败时遗漏清理。
6. 检查测试前后 nr_free_pages() 是否恢复。
7. 保留其他测试及 slub_check() succeeded! 成功输出。
8. 保持原有代码风格，添加简短中文注释。
完成后列出修改位置、修改原因及代码 Diff，等待我自行编译运行。
```

## 丙

- First-Fit 分析：[待整理]
- QEMU/GDB 调试：[待整理]
- 其他 Prompt：[待整理]
