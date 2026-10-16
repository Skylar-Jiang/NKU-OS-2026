# Lab 2：物理内存和页表——甲负责部分初稿

> 记录日期：2026-10-14。本文基于实际源码、QEMU 运行及 GDB 证据。甲的 Page Init、Buddy 和独立测试已完成；团队标准构建仍等待乙的 Best-Fit。本文不是最终小组报告。Prompt 待三人统一整理，`prompt.md` 当前只保留分类模板。
> 丙的后续成果已于 2026-10-16 单独补入 [`member-c/`](member-c/README.md)：包括 First-Fit 分析、宿主 QEMU/GDB 的 MMU/TLB 实测、独立调试版构建、真实截图和集成验收。本文下方原有“待丙完成”的文字属于 2026-10-14 的阶段状态；涉及丙当前结论时以该完成记录和所附日志为准。

## 1. 背景、任务和协作范围

内核启动后，需要把设备树描述的物理内存转换为可管理的页集合，再通过统一的物理内存管理接口选择分配策略。甲负责物理内存初始化、Buddy 分配器、测试，以及 `pmm.c` 中的管理器接入；乙负责 Best-Fit、SLUB 和必要的 `init.c` 修改；丙负责 First-Fit 分析、QEMU/GDB 主体调试及相关思考题。姓名、学号和最终完成日期待填写。

源码中的实际启动流程为：

```text
kern_entry（保存启动参数，建立 Sv39 映射）
  -> kern_init
     -> dtb_init（解析物理内存信息）
     -> pmm_init
        -> init_pmm_manager
        -> page_init -> init_memmap
        -> check_alloc_page
        -> 记录启动页表地址
        -> 显式测试配置下的 Page Init / Buddy 测试
```

基础目标是保护 OpenSBI、内核和页描述符等占用空间，只交出合法完整空闲页。Buddy 进一步验证按 2 的幂组织空闲块、分拆和伙伴合并。未替乙、丙撰写其尚未提供的成果。

## 2. Worktree、安全迁移与环境

### 2.1 Git 基线与文件保护

- 原仓库：`/home/jianglinjing/oslab`，原工作区仍在 `lab1`、HEAD `3900390`。原有 24 个跟踪文件删除状态及未跟踪内容不作处理。
- 新工作区：`/home/jianglinjing/oslab-lab2`，分支 `lab2`，从本地 `main` 的 `9e725207f870e1ed2faba0ab0d9399f9f7747b56` 创建。
- 创建前 `main...origin/main` 差异为 `0 0`；实际 `git ls-remote --heads origin` 确认远端 main 为同一提交，远端当时没有 `lab2`。远端 lab1 为 `a732185...`，与本地 lab1 不同；未同步或改动它。
- Worktree 使用同一个团队仓库，未初始化无关独立仓库。开发验收时尚未 commit 或 push，也未执行 merge、rebase、reset 或 clean；本次按用户授权提交甲的阶段性成果。
- `main` 继承的 `.gitignore`、`AGENTS.md`、`README.md`、`lab1/` 只在新工作区搬至审计备份 `/tmp/nku-lab2-audit-i1zl6cqh/inherited-main/`。指引已先阅读。旧目录及 main/lab1 历史不变；这些搬移在开发验收时显示为 lab2 工作区中预期的 59 个未暂存删除；阶段性提交将记录该目录调整，不改变 main 或 lab1 的历史。
- 旧 Lab 2 的 39 个工程文件复制到新 `code/`，排除构建产物、缓存和 Zone.Identifier。旧源码、旧报告和包含真实 Prompt 的旧文件均原样保留；初始报告及 Prompt 另备份在同一审计目录。
- 新工作区根目录只有 `code/`、`report/`，以及 Git Worktree 必需且不提交的 `.git` 指针。`obj/`、`bin/`、缓存等通过共享的本地 `.git/info/exclude` 排除；不新增顶层配置文件。最终保护核对见 [Git/文件检查记录](evidence/final-safety-check.txt)。

### 2.2 编译与模拟器环境

- RISC-V GCC：15.1.0，位于 `/home/jianglinjing/compiler-tools/riscv-elf-toolchains/bin/`。
- RISC-V GDB：16.3.90.20250610-git，可以调试客户机；没有找到可用宿主 GDB。
- 课程 QEMU：`/home/jianglinjing/opt/qemu-4.1.1/bin/qemu-system-riscv64`，版本 4.1.1，已剥离符号。
- `/opt/qemu/bin/qemu-system-riscv64` 为 7.0.0，带调试信息；不能据此声称现有内核可在其上正常启动。
- 后续已由丙在独立源码副本和 out-of-tree build 目录构建 QEMU 4.1.1 调试版，带 `debug_info`、无 `-O` 优化并成功启动集成内核；构建和启动日志见 [`member-c/`](member-c/README.md)。没有安装覆盖课程 QEMU。
- 旧 `.envrc` 将 QEMU 4.1.1 加到 PATH 前部；新 Worktree 不会自动继承该目录配置。本次全部成功运行显式选择 QEMU 4.1.1，没有修改系统、Shell 配置或软件安装。

新终端可临时设置：

```bash
cd /home/jianglinjing/oslab-lab2/code
export PATH="/home/jianglinjing/compiler-tools/riscv-elf-toolchains/bin:$PATH"
export LAB2_QEMU="/home/jianglinjing/opt/qemu-4.1.1/bin/qemu-system-riscv64"
```

这些是当前终端的变量，不写入提交树。工具和环境检查见 [environment.txt](evidence/environment.txt)。

## 3. Page Init：问题、规格与实现

### 3.1 物理范围和描述符关系

实现位于 [pmm.c](../code/kern/mm/pmm.c) 的 `//lab2 function1 YOUR CODE` 后。保留标记，不擅填学号。

`DRAM_BASE = 0x80000000`，内核物理起点为 `0x80200000`。`entry.S` 建立的固定偏移为 `PHYSICAL_MEMORY_OFFSET = 0xFFFFFFFF40000000`。DTB 的 DRAM 起点与内核装载起点不同，不能把 `KERNBASE - memory_base` 当作偏移。

`npage` 是可管理物理内存上界的物理页号，`nbase = DRAM_BASE / PGSIZE`。描述符数量为 `npage - nbase`，描述符 `pages[i]` 对应物理地址 `(nbase + i) * PGSIZE`。

```text
物理地址：
0x80000000       0x80200000          kernel_end
    | OpenSBI 等 | 内核镜像、栈、页表 | 对齐 | Page 数组 | 对齐 | 完整空闲页 | 尾部残页
保留 ---------------------------------------------------->
                                                   free_start          managed_end
```

`pages` 位于页对齐的内核虚拟 `end` 之后；元数据的物理末端再次向上对齐，才能得到第一个空闲页。尾部不足整页的空间不注册。

### 3.2 功能规格

以下是实际实现的 SPEC 摘要；Prompt 原文留待小组统一整理。

**[PROMPT] 功能目标：** 在不更改公共 PMM 接口的条件下完成物理页描述符布局与空闲区注册。

**[RELY] 可信依赖：** 已执行的 `dtb_init`、两个内存 getter、启动映射与链接符号 `end`、`Page`、页面状态宏、地址转换宏及已初始化的 `pmm_manager`。

**[GUARANTEE] 实现接口：** `static void page_init(void)`，不改变 `pmm.h`、`memlayout.h`、DTB、First-Fit、Best-Fit 或官方测试。

**[SPECIFICATION] 行为：**

- **Pre-Condition：** 启动映射已启用，DTB 已解析，管理器已初始化，描述符尚未建立；设备树数据可能异常。
- **Post-Condition：** 偏移、上界页号和描述符地址正确；OpenSBI、内核、描述符及对齐空隙保持 Reserved；仅合法完整空闲页被注册。
- **有效范围：** 检查起点符合当前单 DRAM 框架并覆盖内核装载起点；检查非零大小和无加法溢出。范围上界截断到 `PHYSICAL_MEMORY_END` 后向下按页对齐。
- **无效范围：** 零大小、起点异常、地址溢出或内存不包含内核时 panic。
- **元数据空间：** 先用除法检验描述符数组是否放得下，再计算数组物理末端，避免乘法越界或覆盖内核。
- **无完整空闲页：** 只完成 Reserved 描述符初始化，不调用 `init_memmap` 注册零长区间。
- **Requirements：** 不硬编码空闲页数；区分 VA、PA、PPN 和描述符地址；不使用超出 `pages` 范围的转换。

### 3.3 实现步骤与证据

1. 设置固定虚实偏移，获取并验证 DTB 范围。
2. 截断并向下对齐上界，校验内核末端在范围内。
3. 设置 `npage`，计算 `count = npage - nbase` 和对齐的 `pages_pa`，在写内存前校验元数据容量。
4. 初始化所有描述符为 Reserved、ref=0、property=0，并初始化链表节点。
5. 对元数据末端向上对齐；有剩余完整页时，调用当前管理器的 `init_memmap`。框架用 Reserved 表示尚未交给 PMM 的页面，管理器对选定空闲区清除该标志。

新增 [page_init_test.c](../code/kern/mm/page_init_test.c) 会检查全部描述符的保留状态、全部地址往返和精确空闲页数，并调用老师实际提供的、未修改的 `test_page_init()`。256 MiB 场景因手册要求截断到框架上界，而官方测试直接期待原始 DTB 末端页号，因此明确跳过该官方 raw-end 检查，使用独立的截断布局检查；默认 128 MiB 场景实际通过官方函数。

本次 128 MiB Page Init 配置的日志记录：描述符物理区间 `[0x80207000, 0x80347000)`，空闲页数 31929。这里每个 `Page` 为 40 字节，数组包含 32768 个元素。Buddy 测试内核较大，其初始空闲页数为 31927；不同配置的内核大小不同，不能硬编码同一空闲页数。

## 4. Buddy：设计、接口和算法

### 4.1 数据组织与释放契约

实现为 [buddy_pmm.c](../code/kern/mm/buddy_pmm.c)、[buddy_pmm.h](../code/kern/mm/buddy_pmm.h)，注册 `buddy_pmm_manager`。公共 `Page` 布局和 `pmm_manager` 接口不变。

- 维护 0 到 15 阶的空闲链表，阶 k 对应 `2^k` 页，单独维护真实空闲页总数。最大阶与当前 128 MiB / 4 KiB 框架上界一致，有编译期断言。
- 伙伴按**物理页号**计算：`buddy_ppn = ppn XOR (1 << order)`。只有同阶、同容量、真正空闲的伙伴能够合并，普通相邻块不满足该条件。
- `PageProperty` 仅标记空闲块头，`property` 在空闲头和已分配头上均记录实际容量。内部页没有块头标志，property 为零。
- Buddy 私有 flags 使用 bit 2 表示归属、bit 3 表示已分配块头、bit 32..63 记录原请求页数。`Page.ref` 保持引用计数语义，不借它保存请求大小。
- 例如请求 3 页实际占用 4 页，空闲统计减少 4；释放时既可 `free_pages(p,3)`，也可 `free_pages(p,4)`。这两种都完整归还 4 页。
- **只支持完整块释放。** n 必须等于原请求或记录的实际容量；不支持内部指针、部分释放、错误尺寸和重复释放，均在修改空闲状态前检测并 panic。这与 First-Fit 原有自测的部分释放习惯不同，所以 Buddy 有独立测试。
- 当前为单核启动阶段，无并发和锁保证。后续 SLUB 若使用 Buddy，需要先确认完整块释放语义、私有标志位及并发约束。不能把该管理器直接替换到已有活动分配的运行环境。

### 4.2 各函数的 SPEC 与解释

所有管理回调均保持内部 `static`，外部通过管理器结构调用。另提供只读的 `buddy_validate()` 供自测检查。

| 函数 | Pre-Condition | Post-Condition / 关键 Case |
| --- | --- | --- |
| `buddy_init` | 尚未接管页面；重建时无活动分配 | 初始化所有阶链表，空闲计数为零 |
| `buddy_init_memmap(base,n)` | 合法连续描述符，n>0，未接管，符合框架 Reserved 交接约定 | 先整段检查再修改；按物理对齐及剩余长度划分最大合法幂次块；非对齐起点、非幂次长度也不丢页；已注册相邻区间可按真正伙伴关系合并 |
| `buddy_alloc_pages(n)` | n>0，已初始化 | 向上取幂；查找最小可用阶，逐级拆分并返回左半块；记录请求和容量；按容量扣数；不足或巨大请求返回 NULL，状态不变 |
| `buddy_free_pages(base,n)` | 先前分配的完整块头，n 为请求或容量 | 验证整数地址、描述符对齐、归属、头标志及尺寸；清除分配元数据；逐阶 XOR 找伙伴，满足条件才合并；按实际容量加数 |
| `buddy_nr_free_pages` | 状态已初始化 | 返回真实空闲页数，不按原请求数计量 |
| `buddy_check` | 已注册合法页面 | 调用 `buddy_validate`，验证当前状态，输出检查结果；独立场景的具体行为测试另由 `buddy_test` 执行 |

整体不变量包括：块按物理容量对齐；各空闲块不重叠；保留页不进入链表；空闲同阶伙伴不遗留未合并状态；内部描述符无伪块头；空闲链表容量之和等于空闲计数；所有已接管页数等于空闲页数与已分配容量之和。

### 4.3 算法复杂度与权衡

记最大阶数为 K（当前 K=15）。分配扫描阶并分拆，释放逐阶寻找伙伴，都为 O(K)，伙伴定位本身为 O(1)。初始化需处理全部描述符，约为 O(N + K × 初始块数)。链表头额外空间为 O(K)，每页复用现有描述符，不依赖宿主 malloc。

`buddy_validate` 是调试检查，扫描描述符及链表，不能把其成本当作正常分配路径的复杂度。幂次取整带来内部碎片，但能用明确的伙伴关系简化合并；总空闲页充足也可能没有满足请求的连续高阶块。

## 5. Buddy 独立测试：设计与实际运行

测试在实现后单独编写，代码为 [buddy_test.c](../code/kern/mm/buddy_test.c)。这不被表述成两次独立发送的用户 Prompt；正式 Prompt 由小组后续根据真实交互统一整理。

### 5.1 测试状态隔离

`buddy_test()` 只允许在启动初始化完成、当前管理区全部空闲且没有活动分配时运行。先确认初始单连续空闲区；用同一真实物理区间的子区间构造测试环境，测试间要求全部回收后才重建描述符。最后重新注册完整初始空闲区并验证计数恢复。

测试不使用模拟分配器或伪 Page 数组。分配范围来自实际 Page Init；在真实物理页的首尾写入并读回不同标记，检查跨页容量、互不覆盖和数据完整性。测试区不包含 OpenSBI、内核或描述符占用页。该入口不能用于运行中的 SLUB 堆或已有活动分配的系统。

### 5.2 用例覆盖

| 用例 | 预期与检查 |
| --- | --- |
| 真实大内存区高阶分配 | 513 页及更高阶请求取整，检查物理对齐、互不重叠、容量扣数和完整回收 |
| 1/2/4/8、3/5/6 页 | 连续容量、两两不重叠、ref/标志、实际页数统计、首尾数据标记；原请求释放与实际容量释放均验证 |
| 64 页块拆分 | 1 页和 32 页请求从合适阶获取，释放后重新分配完整 64 页 |
| 占用伙伴与递归合并 | 伙伴尚在使用时 16 页申请失败；两块 8 页归还后恢复 16 页块 |
| 相邻非伙伴防护 | 释放 4 页区间内下标 1、2 的页面，虽相邻也不能分配 2 页块；真实伙伴归还后才能合并 |
| 耗尽与巨大请求 | 分配所有单页，确认不重复、不越界、耗尽时 NULL，失败计数不变；反序释放恢复 |
| 不规则区间、最后一页 | 测试高阶未对齐起点+13页、物理上界最后1页；不丢尾页、不越界 |
| 分段注册与空洞 | 两段被空洞隔开时不可合并；补注册中间区间后才按伙伴合并 |
| 2000 次确定性混合操作 | 固定种子、32 个槽、1..12 页请求，持续校验计数、重叠和数据，间歇检查全部不变量，最终恢复整块 |
| 5 个独立 panic 启动 | 重复释放、内部指针、错误页数、保留页、未对齐描述符地址；检查确切错误文本，正常回归与 panic 启动分离 |

### 5.3 实测结果及边界

[check_a.py](../code/tools/check_a.py) 构建并启动真实 QEMU 内核，保存原始构建及串口日志。正常运行以实际 PASS 标记判定，panic 场景以 BEGIN 标记、内核 panic 来源和确切错误文本共同判定。内核最后是 `while(1)`，测试脚本只终止自己创建的 QEMU，退出码或超时本身不代表测试成功。

| 验证类别 | 实际结果 | 证据 |
| --- | --- | --- |
| Buddy 模块 freestanding 静态编译 | 通过 | 编译检查及后续实际链接 |
| 甲模块默认配置对象编译 | 通过；标准内核整体仍未通过 | [a-module-build.log](evidence/a-module-build.log) |
| Page Init 测试配置完整内核编译 | 成功 | [build-page-init-0.log](evidence/build-page-init-0.log) |
| Buddy 测试配置完整内核编译 | 成功 | [build-buddy-0.log](evidence/build-buddy-0.log) |
| Page Init：64/128 MiB | 官方 `test_page_init()` 及独立全描述符检查均 PASS | [64M](evidence/page-init-64M.log)、[128M](evidence/page-init-128M.log) |
| Page Init：256 MiB | 截断布局检查 PASS；未声称官方 raw-end 检查通过 | [256M](evidence/page-init-256M.log) |
| Buddy：64/128/256 MiB | 全部正常用例 PASS，恢复完整初始空闲区 | [64M](evidence/buddy-64M.log)、[128M](evidence/buddy-128M.log)、[256M](evidence/buddy-256M.log) |
| Buddy：5 个错误释放启动 | 全部观察到预期 panic | [1](evidence/buddy-negative-1.log)、[2](evidence/buddy-negative-2.log)、[3](evidence/buddy-negative-3.log)、[4](evidence/buddy-negative-4.log)、[5](evidence/buddy-negative-5.log) |
| 直接调用 Makefile qemu 入口 | Buddy 配置观察到全部 PASS，因最终循环由 timeout 结束 | [make-qemu-buddy.log](evidence/make-qemu-buddy.log) |
| 团队标准 `make -j2` | 失败，Best-Fit 缺少 `free_list`、`nr_free` | [standard-build.log](evidence/standard-build.log) |
| 官方 `make grade` | 未运行；标准构建不完整且标准管理器尚未接入 Best-Fit | 不报告评分或假 PASS |

最终独立回归摘要见 [final-a-check.log](evidence/final-a-check.log)。当前没有观察到 Buddy 泄漏、重复分配、错误合并或计数错误，但有限测试不构成所有输入的形式化证明。

## 6. 个人 GDB 验证、失败记录与修复

### 6.1 已完成：客户机 Sv39 地址转换

[debug_a.py](../code/tools/debug_a.py) 使用 RISC-V GDB 连接 QEMU 4.1.1，只监听本机随机端口，不连接或终止其他调试会话。

本次在 `page_init_test` 处实际观察到：

```text
GUEST_PC=0xffffffffc0201092
SATP=0x8000000000080205
ROOT_PA=0x80205000
VPN2=511
LEAF_PTE=0x200000cf
TRANSLATED_PA=0x80201092
OFFSET_PA=0x80201092
```

SATP 的 MODE=8 表示 Sv39。启动根页表第 511 项具有 V/R/W/X 标志，是 level-2 的 1 GiB 叶映射，并非经过三个非叶项的 4 KiB 页表遍历。取 PTE 中的物理基址，加虚拟地址低 30 位，得到 `0x80201092`，与减去固定偏移一致。GDB 的虚拟地址读数和 QEMU monitor 的物理字节读数也实际记录在 [gdb-guest-translation.log](evidence/gdb-guest-translation.log)。

这证明本次客户机页表及地址转换核对已完成，**不等于**在宿主 GDB 中单步观察了 QEMU 的 TLB 查找、TLB Miss 或页表遍历 C 实现。

### 6.2 Page Init 输入注入测试

在真实内核进入 `pmm_init` 时，GDB 修改已经解析的 DTB 全局值，再继续运行初始化。实际 backing RAM 仍为 128 MiB。这是明确标识的输入故障注入，不是伪称硬件提供了该异常 DTB。

- 零大小、起点为零、加法溢出：进入 `page_init: invalid DTB memory range`。
- 内存范围不含内核末端：进入 `memory does not contain the kernel`。
- 仅能包含内核、不能容纳描述符数组：进入 `no room for page descriptors`。
- 报告 64 MiB 减 17 字节：尾部不完整物理页排除，npage 及独立布局检查通过。
- 计算恰能容纳描述符、没有完整空闲页的范围：在进入 First-Fit 自测之前停止，空闲链表为空、计数为零、全部描述符 Reserved，检查通过。本例未继续执行要求非空空闲区的既有 First-Fit 自测，也未声称零空闲页内核可完整运行。

完整结果见 [final-debug-a.log](evidence/final-debug-a.log)，原始 GDB 命令和每例输出位于 `evidence/gdb-page-init-*`。

### 6.3 实际遇到的问题

1. **QEMU 4.1.1 的 32 MiB 启动无输出。** 首次自动测试未取得标记，保留 [原始失败日志](evidence/initial-qemu41-32M-no-output.log)。128 MiB 运行实际记录 DTB 地址为 `0x82200000`，已超出 32 MiB 范围，因此将课程固件的内存矩阵改为 64/128/256 MiB 并实际通过；内部失败路径未单独定位，不将该推断写成已确认的代码 Bug。
2. **GDB 对无类型汇编符号、PC 和 SATP 的类型处理。** 首先因 `boot_page_table_sv39` 类型未知而失败；改为显式 char 指针后，又发现 PC 的位运算需要整数转换，SATP 需要无符号转换才能正确判断 MODE。保留 [符号类型错误](evidence/initial-gdb-symbol-type-error.log)、[PC 算术错误](evidence/initial-gdb-pc-arithmetic-error.log)、[SATP 有符号判断错误](evidence/initial-gdb-signed-satp-error.log)。修复调试脚本的转换后重新执行成功，未改内核来迎合调试脚本。
3. **标准构建中的 Best-Fit 骨架。** `free_list` 和 `nr_free` 尚未定义，这是原有未完成模块；未替乙实现。通过显式 A-only 构建配置验证实际 First-Fit 和 Buddy 内核，没有用测试桩冒充 Best-Fit。
4. **双重 GDB 环境不齐。** 课程 QEMU 4.1.1 已剥离符号、未找到宿主 GDB。QEMU 7.0.0 虽带调试信息，启动尝试只观察到 OpenSBI，后续入口断点诊断超时，原因未确定，保留 [串口记录](evidence/gdb-qemu7-startup-diagnostic-serial.log)。未修改 `entry.S`，未重装工具或更改系统配置。仍需丙提供可用且与实验启动流程兼容的带符号 QEMU、源码路径及宿主 GDB。

本阶段没有观察到 Page Init 或 Buddy 算法的失败用例，因此没有虚构算法 Bug 或修复过程。工具脚本问题与骨架未完成均分别说明。

## 7. 构建入口与后续集成

### 7.1 当前可独立运行的方法

[Makefile](../code/Makefile) 仍通过 `KSRCDIR` 和 `function.mk` 的 wildcard 发现 `kern/mm/` 内新增 `.c`、`.S`。仅新增目录不会自动递归发现。正式 `LAB2_PROFILE=standard` 默认保留原有 First-Fit 选择，并编译乙的 Best-Fit 文件；没有提前替换正式管理器。

两个显式测试配置排除未完成的 `best_fit_pmm.c`，编译实际官方测试源以调用 Page Init 检查。未使用的 `test_best_fit` 随函数 section 被链接器回收，没有伪造其管理器定义。测试内核的对象和镜像放在不同配置子目录；公共 libs 对象复用，因为这些配置不改变库编译标志。

```bash
# 全部独立构建、QEMU 正常回归及隔离 panic 场景
python3 tools/check_a.py --qemu "$LAB2_QEMU"

# 客户机 GDB 转换核对、输入故障注入和边界检查
python3 tools/debug_a.py --qemu "$LAB2_QEMU"

# 显式编译和交互运行，最终循环可用 Ctrl-a x 退出 QEMU
make LAB2_PROFILE=page-init
make LAB2_PROFILE=page-init QEMU="$LAB2_QEMU" qemu
make LAB2_PROFILE=buddy
make LAB2_PROFILE=buddy QEMU="$LAB2_QEMU" qemu

# 手动客户机调试，需要两个终端使用同一配置
make LAB2_PROFILE=page-init QEMU="$LAB2_QEMU" debug
make LAB2_PROFILE=page-init gdb
```

正式构建未完成前，上述配置证明甲的真实内核可运行，不代表团队标准内核或官方评分已经通过。Buddy 测试不能在已有活动分配的运行内核中直接调用。

### 7.2 需要乙和丙提供的内容

- 乙：完整 `best_fit_pmm.c` 及相应头文件；管理器定义和全部回调，与原有 `basic_check`、`best_fit_check` 保持一致的状态约定；实际测试结果。当前只见框架测试和 extern 声明，未判断其其他分支或设备上的进度。
- 乙：后续 `slub.c/h`、测试和必要 `init.c` 改动，以及页分配/释放契约。若仍使用 Best-Fit，避免把 Buddy 整块释放限制带入不兼容调用。
- 丙：以上分析和实测已完成，含 First-Fit、内存探测思考题、QEMU 4.1.1 调试版构建、宿主 GDB 的 TLB/MMU 路径、断点脚本、原始日志及截图；见 [`member-c/README.md`](member-c/README.md)。甲乙可转发使用其中的 QEMU 调试构建步骤。
- 三人：姓名学号、真实 Prompt 原文、各自调试和测试截图、各负责部分报告。丙的截图已放入 `report/images/`，仍需其他成员补充各自材料。

### 7.3 正式集成顺序

乙提交后先审阅差异，保留甲负责的 `pmm.c`，仅将其标准构建分支选择改为 `&best_fit_pmm_manager`；显式 Page Init 配置仍选择 First-Fit，Buddy 配置仍选择 Buddy。不要把管理器切换发生在有活动分配的运行中。

```bash
make
make QEMU="$LAB2_QEMU" qemu

# 官方 grade 会清理当前构建产物并在临时副本注入测试。
# 使用自己创建的唯一临时目录，避免默认 /tmp/lab2_grade 的并发清理风险。
lab2_grade_tmp=$(mktemp -d /tmp/nku-lab2-grade.XXXXXX)
make QEMU="$LAB2_QEMU" GRADE_TMP="$lab2_grade_tmp" grade

# 接入队友模块后再回归甲的独立配置
python3 tools/check_a.py --qemu "$LAB2_QEMU"
python3 tools/debug_a.py --qemu "$LAB2_QEMU"
```

官方评分仅对 Page Init 和 Best-Fit 的标记各计 10 分，要求当前管理器为 Best-Fit；Buddy 和 SLUB 需要独立证据。注入器拒绝正式 Makefile 已含 `kern/test` 或 pmm.c 已含测试注入头的情况，因此不应把临时注入副本带回正式源码。老师的评分与注入文件本次均未修改。

丙已在 QEMU 4.1.1 宿主 GDB 中单步观察实际 TLB 命中和地址翻译，并将日志与客户机证据关联，详见 [`丙工作完成记录`](member-c/丙工作完成记录.md)。甲的其余职责仍按组内安排独立验收。

## 8. OS 理论、尚未覆盖内容与最终报告待办

物理页管理把连续地址空间变为描述符集合；Reserved 标志区分系统占用与可分配页。PMM 接口体现机制与策略分离，Page Init 组织初始资源，而 First-Fit、Best-Fit、Buddy 决定如何分配。

Buddy 的幂次取整引入内部碎片；空闲块分散则体现外部碎片和连续分配约束。物理连续并不意味着任意请求都能满足，必须同时满足容量、对齐和伙伴结构。

Sv39、PTE 和 SATP 属于地址翻译机制；物理页分配策略不能代替 MMU 翻译。页描述符本身通过内核虚拟地址访问，但其下标对应物理页号。TLB 是翻译缓存；QEMU 软件 TLB 与真实硬件 TLB 的区别已由丙结合源码级命中和地址翻译实测说明，见 [`丙工作完成记录`](member-c/丙工作完成记录.md)。

当前未覆盖完整虚拟内存分配、缺页异常处理、按需分页、交换、进程地址空间隔离、多核并发与实际生产系统的 NUMA/复杂固件保留区管理。DTB 模块当前只解析一个简化的 memory reg 范围，未实现任意硬件布局的通用内存管理。

后续仍需汇总乙的成果、所有练习回答和最终组员信息；丙负责的个人调试证据已在 `report/member-c/` 补齐。`report/prompt.md` 本次保持纯分类模板，所有真实 Prompt 待三人统一整理；本报告的 SPEC 及命令说明不作为虚构的历史交互记录。最终提交到团队 `lab2` 分支前，核对提交树顶层仅有 `code/`、`report/`，保留 `lab2` 注释并由本人填写学号，排除构建产物，检查所有证据引用存在。本文件仍是甲的阶段性报告，不代表 Best-Fit、SLUB、官方完整评分或最终小组报告已全部完成。
