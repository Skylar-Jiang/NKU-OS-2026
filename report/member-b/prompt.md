# Lab 2 成员乙：实际使用的 Prompt 原文

来源：用户提供的 `实验二_乙.docx`；仅移除 DOCX 空段，以下文字、字段和顺序未改写。

## 三人分工

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

## Best-Fit 实现

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

## SLUB 实现

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

## SLUB 测试

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

## SLUB 测试检查

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

## SLUB 测试修正

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
