# Lab 2 Prompt 汇总

本文按成员和模块汇总本实验使用的 Prompt。各项均按 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 四部分组织，代码块中保留 Prompt 内容。

## 甲：蒋林瀞（2413507）

**使用模型：** Codex CLI，`gpt-6.1-sol`，推理强度 `high`。

### Page Init

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

### Buddy System

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

### Buddy System 测试

`````text
# 第四阶段：Buddy System 独立测试

实验指导书要求单独设计一份 Buddy 测试 Prompt。

本阶段作为与实现阶段分开的测试规格执行。不得因为实现由 AI 生成，就默认其行为正确。

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

## 乙：王麒萱（2412096）

**使用模型：** VS Code 内置 Codex，`gpt-5.6-terra`，推理强度 `xhigh`。

### Best-Fit 实现

`````text
## [PROMPT]

实现 Best-Fit 连续物理页分配器，选择能够满足请求的最小空闲块。

- 仅修改 `kern/mm/best_fit_pmm.c`，保留 `//lab2 function2-2 YOUR CODE`。
- 保留 `basic_check()`、`best_fit_check()`、既有接口、数据结构和宏。
- 保持其他组员文件、Makefile 和评分脚本原样。
- 可增加 static 辅助函数，沿用原有风格，注释用简短中文。
- 列出修改函数和 Diff，编译及运行由我完成。

## [RELY]

- `default_pmm.c`、`best_fit_pmm.c`
- `pmm.h`、`memlayout.h`、`libs/list.h`
- `pmm_manager`、`free_area_t`、`struct Page`
- Property 标志、引用计数及双向链表操作

## [GUARANTEE]

- `static void best_fit_init(void);`
- `static void best_fit_init_memmap(struct Page *base, size_t n);`
- `static struct Page *best_fit_alloc_pages(size_t n);`
- `static void best_fit_free_pages(struct Page *base, size_t n);`
- `static size_t best_fit_nr_free_pages(void);`
- `const struct pmm_manager best_fit_pmm_manager;`

## [SPECIFICATION]

### best_fit_init()

**Pre-Condition：** 尚未登记空闲区。

**Post-Condition：** 空闲链表为空，`nr_free == 0`。

### best_fit_init_memmap(base, n)

**Pre-Condition：** `n > 0`，区间有效且各页均为 Reserved。

**Post-Condition：** 登记页的标志、property 和引用计数清零；仅块头设置 `property = n` 及 Property 标志，按地址插入空闲链表，`nr_free` 增加 n。

### best_fit_alloc_pages(n)

**Pre-Condition：** `n > 0`，空闲链表和计数一致。

**Post-Condition：** 成功返回连续 n 页的首页，清除其 Property 标志，`nr_free` 减少 n；失败返回 NULL，链表及计数不变。

**Case 1：存在多个候选块**

选择不小于 n 的最小块；大小相同则选地址较低的块。

**Case 2：恰好匹配或需要拆分**

恰好匹配时移除块头；有剩余时以 `page + n` 为新块头，记录剩余大小，保持链表地址顺序。

**Case 3：分配失败**

总空闲页不足，或总量足够但没有足够大的连续块，均返回 NULL。

### best_fit_free_pages(base, n)

**Pre-Condition：** `n > 0`，区间已分配、非 Reserved，且没有重复释放。

**Post-Condition：** 引用计数清零，空闲页增加 n；按地址插入后分别与前后相邻块合并，清除失效块头的标志及链表节点。

### best_fit_nr_free_pages()

**Post-Condition：** 返回空闲页数，不改变状态；计数等于所有空闲块 property 之和。
`````

### SLUB 实现

`````text
## [PROMPT]

在物理页分配器之上实现简化 SLUB，分别管理整页和页内对象。

- 新增 `kern/mm/slub.c`、`kern/mm/slub.h`，必要时在 `kern/init/init.c` 接入初始化。
- 保留 pmm.c、Best-Fit、Buddy、官方测试、Makefile 和评分脚本。
- 使用现有页分配接口，禁止调用标准库 malloc/free。
- 先完成分配器，测试代码另行编写。

## [RELY]

- `pmm.h`、`memlayout.h`、`mmu.h`、`defs.h`、`init.c`
- `alloc_page()`、`free_page()`，即单页的 alloc_pages/free_pages
- `page2pa(page) + va_pa_offset`、PGSIZE 及内核地址映射

## [GUARANTEE]

- `void slub_init(void);`
- `void *slub_alloc(size_t size);`
- `void slub_free(void *obj);`
- 16、32、64、128、256、512、1024 字节大小类，16 字节对齐
- 可定义私有 cache、slab 和空闲槽辅助结构

## [SPECIFICATION]

### slub_init()

**Pre-Condition：** pmm_init 已完成。

**Post-Condition：** 首次调用建立全部大小类，partial 链表为空；重复调用保留已有分配状态。初始化放在 pmm_init 之后。

### slub_alloc(size)

**Post-Condition：** 成功返回至少容纳 size 字节、16 字节对齐且与存活对象不重叠的内核虚拟地址；失败返回 NULL，已有分配保持不变。

**Case 1：无效请求**

未初始化、size 为 0 或超过 1024 时返回 NULL；其他请求选择不小于 size 的最小大小类，8 字节请求使用 16 字节类。

**Case 2：已有空闲槽**

优先复用已有 slab 的槽位；物理空闲页为零时，只要还有槽位仍可分配。

**Case 3：需要新页**

无可用槽时才申请整页。页首保存元数据，其后对齐划分槽位，容量扣除元数据所占空间；建页失败时归还已申请资源。

**Case 4：slab 用满**

更新 inuse 和槽链表，用满时从 partial 链表移除。

### slub_free(obj)

**Pre-Condition：** obj 为本分配器返回的有效存活对象或 NULL。

**Post-Condition：** 对象回到所属 slab；满 slab 释放首个对象后重入 partial；最后一个对象释放后移除 slab 并归还整页。

**Case 1：空指针或尚未初始化**

直接返回，不改变状态。

**Case 2：槽位校验**

对仍有效的 slab 检查 magic、对象范围、对齐及空闲链表，拒绝已在空闲链表中的重复槽，避免 inuse 下溢。

不要求检测任意未映射地址，或页回收、再利用后的陈旧指针。
`````

### SLUB 独立测试

`````text
## [PROMPT]

为 SLUB 编写独立测试，新增 `kern/mm/slub_test.c`。

- 必要时仅修改 slub.h 和 init.c，声明并调用 slub_test。
- 保留 SLUB 算法、Best-Fit、Buddy、pmm.c、Makefile 和评分脚本。
- 调用顺序为 `pmm_init() -> slub_init() -> slub_test()`。
- 只编写测试代码，编译、QEMU 和评分由我运行。

## [RELY]

- `slub.c`、`slub.h`、`pmm.h`、`init.c`
- `assert`、`cprintf`、PGSIZE、nr_free_pages
- 分配器的大小类、槽位链表和整页释放规则

## [GUARANTEE]

- `void slub_test(void);`
- 大小与对齐、不重叠、槽位复用、整页回收及内存耗尽检查函数
- 全部断言通过后输出 `slub_check() succeeded!`

## [SPECIFICATION]

**Pre-Condition：** SLUB 已初始化，没有其他使用者并发分配；测试只释放自己申请的资源。

**Post-Condition：** 所有临时对象和物理页已归还，nr_free_pages 恢复测试前值。

**Case 1：不同大小与不重叠**

同时申请 8、24、32、64、100、128、512、513、1024 字节对象，检查非空、16 字节对齐，并按大小类长度检查两两区间不重叠。

**Case 2：释放与复用**

连续分配同类对象，释放部分后重新申请，检查槽位复用且未影响仍存活的对象。

**Case 3：整页回收**

该类无现存 slab 时，申请一个 1024 字节对象使空闲页减少一页；释放全部对象后恢复。

**Case 4：请求边界**

slub_alloc(0) 和 slub_alloc(1025) 返回 NULL，空闲页计数不变。

**Case 5：耗尽与清理**

区分物理页耗尽与对象槽耗尽，循环有上限，每次成功分配均有记录。先保存结果、清理全部资源，再执行最终断言。
`````

### SLUB 耗尽测试检查

`````text
## [PROMPT]

只读检查 slub_test.c 的耗尽测试，核对断言前提、循环边界和资源清理；列出问题位置、触发条件及修正建议，保留所有文件。

## [RELY]

- `slub_test.c`、`slub.c`、`slub.h`、`pmm.h`
- partial slab、空闲槽、nr_free_pages 及断言中止行为

## [GUARANTEE]

- 物理页耗尽后的槽位复用检查
- 循环终止及对象、物理页清理检查
- 空闲页计数与 PMM 状态检查

## [SPECIFICATION]

**Pre-Condition：** 测试代码已生成，尚未运行。

**Post-Condition：** 每个问题对应代码位置和触发条件，并说明是分配器问题还是测试假设错误。

**Case 1：耗尽判定**

检查 `assert(slub_alloc(16) == NULL)` 是否忽略已有空闲槽；分配成功时，返回对象是否被记录。

**Case 2：循环结束**

检查分配失败、达到上限和验证不符三条路径，是否都能归还此前申请的资源。

**Case 3：清理与计数**

检查提前 assert 是否跳过清理，先前缓存是否影响页数断言，临时链表是否覆盖 PMM 仍使用的元数据。
`````

### SLUB 耗尽测试修正

`````text
## [PROMPT]

修正物理页耗尽后立即断言 slub_alloc(16) 为 NULL 的错误，以及断言中止导致的资源遗漏。

- 仅修改 `kern/mm/slub_test.c`。
- 保留内存不足测试、其他测试及成功输出。
- 保留分配器、其他组员源码、Makefile 和评分脚本。
- 给出修改位置和 Diff，测试由我运行。

## [RELY]

- 上一次静态检查结果
- `slub_test.c`、`slub.c`、`slub.h`、`pmm.h`
- 空闲槽复用、页回收及 nr_free_pages

## [GUARANTEE]

- 修正耗尽检查函数
- 分别记录物理页耗尽、对象分配失败和空闲页恢复结果
- 记录全部成功取得的页和对象，用于统一清理

## [SPECIFICATION]

**Pre-Condition：** 旧测试会将正常槽位复用误判为失败，且 assert 可能跳过资源清理。

**Post-Condition：** 允许复用已有槽，最终空间不足时验证 NULL；清理全部临时资源后再执行断言。

**Case 1：有限循环**

页申请以进入该阶段的可用页数为界。对象上限使用 npage 等总管理页上界乘以 PGSIZE/对象大小类，乘法防溢出；上限须覆盖已有 slab，不能只用剩余空闲页数。

**Case 2：保存资源**

记录每次成功申请的页和对象；分配失败、达到上限和验证失败均进入清理。

**Case 3：清理后断言**

先保存验证结果，清理时先保存 next 再释放。全部资源归还后检查 nr_free_pages，再断言耗尽及恢复结果。
`````

### SLUB 复用与耗尽测试补充

`````text
## [PROMPT]

补上耗尽前未保留 slab、复用测试只释放单个对象这两处遗漏。仅修改 slub_test.c，保留上次修正的循环上限、统一清理和清理后断言，其他源码及评分脚本原样保留。

## [RELY]

- `slub_test_reuse()`、`slub_test_memory_exhaustion()`
- slub.c 的单页大小类和后进先出空闲槽链表
- pmm.h、上一次修正 Diff

## [GUARANTEE]

- `pages_exhausted`、`reused_slot`、`objects_exhausted`、`pages_restored` 四项检查
- 两个对象交替释放及再次分配检查

## [SPECIFICATION]

**Pre-Condition：** 该大小类没有外部存活对象；旧版未覆盖物理页耗尽后的已有槽复用。

**Post-Condition：** 至少发生一次槽位复用，随后耗尽对象槽并返回 NULL；清理后全部页恢复。

**Case 1：预留 slab**

先保留一个 slub_alloc(1024) 对象，再读取 available_pages，有界占用其余物理页。

**Case 2：两层耗尽**

nr_free_pages 为零后继续申请 1024 字节对象，至少成功一次，再检查最终返回 NULL。PGSIZE/1024 作为单页槽数上限，容量还需扣除元数据。

**Case 3：完整清理**

记录所有成功对象，探测结束后先释放对象、再归还临时页，最后断言四项结果。

**Case 4：交替释放**

申请 a、b、c、d 四个 32 字节对象，依次释放 b、d；再申请两次，返回值分别等于 d、b。最后释放所有仍存活对象，避免重复归还。
`````

### 交叉编译器路径排查

`````text
## [PROMPT]

定位构建日志中的 `riscv64-unknown-elf-gcc: not found`，给出当前 WSL 终端的修正命令。保持工程、已安装工具链和系统配置原样，不自动执行构建。

## [RELY]

- `make-clean-build.log`、`environment-20261015.txt` 及后续构建日志
- `/home/lenovo/riscv-elf-toolchains/bin`
- Makefile 的工具前缀及当前 PATH

## [GUARANTEE]

- command -v 与编译器版本检查命令
- 仅对当前终端生效的 PATH 设置
- 构建失败原因及原始退出码说明

## [SPECIFICATION]

**Pre-Condition：** 日志显示编译器未找到，`COMMAND_EXIT=2`。

**Post-Condition：** 命令复用已有工具链，不重装、不升级，也不改全局配置。

**Case 1：命令查找失败**

将工具链 bin 目录加入当前终端 PATH，保留原 PATH；由我检查 command -v 和版本后重新构建。

**Case 2：退出码**

区分编译器未找到、C 代码编译错误，以及 shell 的 Error 127 和 make 的总退出码。
`````

### QEMU 终端与构建警告排查

`````text
## [PROMPT]

分析 VS Code 终端中 QEMU 无输出、timeout/中断及构建时钟偏差。给出排查命令或修正运行脚本，由我运行；保留内核、测试和原日志。

## [RELY]

- `vscode-final/qemu-no-output.log`、`qemu-job-control-stop.log`
- `qemu.log`、`build.log`、README
- QEMU 路径、nographic 标准输入及进程组行为

## [GUARANTEE]

- 标准输入与后台作业检查
- 本次 QEMU 的受控停止方法
- 完整输出、进程退出状态和日志检查结果

## [SPECIFICATION]

**Pre-Condition：** 初次日志只有 make Interrupt 和退出码 124，后续日志另有完整启动输出。

**Post-Condition：** 分别说明终端问题、内核断言、受控结束和构建警告。

**Case 1：超时或无输出**

124 仅代表 timeout 超时。核对标准输入和作业控制，再检查日志中的 Best-Fit 管理器、check_alloc_page、slub_check 标记及异常信息；空日志不算通过。

**Case 2：受控终止**

停止操作只针对本次启动的进程或进程组。分别保留 make/QEMU 的退出状态和脚本检查退出码。

**Case 3：Clock skew detected**

保留警告，检查文件时间戳并给出干净重建建议；保持系统时钟和算法原样。
`````

### Best-Fit 与 SLUB 的 GDB 调试

`````text
## [PROMPT]

编写 Best-Fit、SLUB 客户机 GDB 脚本，检查参数、返回值、调用栈和耗尽测试状态。脚本由我在 VS Code 终端运行，保留内核及全部断言。

## [RELY]

- `bin/kernel`、`best_fit_pmm.c`、`slub.c`、`slub_test.c`
- `best-fit.gdb`、`slub.gdb` 及对应日志
- RISC-V GDB、本次 QEMU 的 gdbstub 端口

## [GUARANTEE]

- best_fit_alloc_pages 断点脚本
- slub_alloc 边界及正常请求的返回值脚本
- 耗尽测试清理后、最终断言前的状态检查脚本

## [SPECIFICATION]

**Pre-Condition：** 内核已构建，QEMU 暂停，端口、符号及源码一致。

**Post-Condition：** 记录参数、源码位置、返回值及调用栈；观察后关闭无用断点并 detach。

**Case 1：Best-Fit 调用**

命中 best_fit_alloc_pages，读取 n 及 basic_check、best_fit_check、pmm_init 调用栈，确认普通配置调用 Best-Fit。

**Case 2：SLUB 返回值**

分别观察 size 为 1025 和 8 的请求，用 finish 读取返回值，检查 NULL 与有效对齐地址。

**Case 3：耗尽结果**

按当前源码定位清理完成、最终断言之前的断点，不使用过期行号。

**Case 4：optimized out**

记录为变量不可见，不能当作 0 或通过；保留已有断言，由人工补充观察。
`````

### QEMU 地址转换调试

`````text
## [PROMPT]

编写 QEMU 地址转换的宿主 GDB 断点脚本，观察一次内核访存从 TLB Miss 到页表遍历和表项写入的过程。复用已有带符号 QEMU，仅生成脚本和观察命令，由我运行并截图。

## [RELY]

- QEMU 4.1.1 的 `target/riscv/cpu_helper.c`、`accel/tcg/cputlb.c`
- 内核 `entry.S`、`mmu.h`、`bin/kernel`
- 已有 TLB 截图及页表记录

## [GUARANTEE]

- riscv_cpu_tlb_fill、get_physical_address、tlb_set_page 条件断点
- SATP、根页表、VPN、PTE、物理地址及返回值观察命令
- 宿主 GDB 与客户机 RISC-V GDB 的操作分工

## [SPECIFICATION]

**Pre-Condition：** 带符号 QEMU 与内核版本匹配，已选定要观察的内核访存。

**Post-Condition：** 能用寄存器、PTE 和地址计算核对翻译结果，区分 TLB Miss 与 Hit。

**Case 1：选定访存**

观察 kern_init 栈写入等访问，记录 address、size、access_type、mmu_idx，区分目标访存、OpenSBI 访问和调试器读内存。

**Case 2：Sv39 遍历**

从 SATP 求根页表地址，核对 VPN 索引、PTE 地址和内容；遇到超级页叶子时提前结束遍历，按页大小及偏移计算物理地址。

**Case 3：TLB 填充**

检查 tlb_set_page 参数和 tlb_fill 返回值，区分 Sv39 映射大小与 QEMU 缓存表项粒度。

**Case 4：调试范围**

地址按本次内核符号和寄存器设置，不固定照搬旧记录；只观察到 Miss 时，不据此判断 Hit 路径。
`````
