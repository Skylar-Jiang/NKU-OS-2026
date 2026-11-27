# Lab5 进程复制与 COW 对接说明

马传志 2411788。基线为官方 Lab5 分支提交 23d1cf2；不包含 A 的生产 Trap/系统调用实现或 B 的加载、退出、等待实现。

## 接口约定

- 新增 `PTE_COW=0x100`，使用 PTE 软件保留位；不是硬件 W 位，不改变 VMA 的 VM_WRITE。
- `copy_range(..., share)`：false 为立即复制，true 为 COW 共享。它不是 CLONE_VM；失败时目标页表可能已有部分映射，由调用方统一销毁目标空间。
- `LAB5_COW` 默认 1；编译 vmm.c 时设置 `-DLAB5_COW=0` 可验证普通 Fork。
- `int cow_handle_store_fault(struct mm_struct *mm, uintptr_t fault_addr)` 在 vmm.h 声明。0 表示修复成功；-E_INVAL 表示不是合法 COW 写；-E_NO_MEM 表示资源不足。调用方只能对写页错误调用，成功不增加 EPC，返回后重试原指令。
- Lab5 Trap 帧的异常地址字段为 `tf->tval`，不是 Lab4 的 badvaddr。
- 当前单 hart、内核不可抢占假设下，页引用和 PTE 更新以本地中断临界区保护。处理函数不重新获取 mm_lock，避免已经持有该锁的 copyout 路径自锁；多核实现需要统一页表锁、引用保护和远端 TLB shootdown，当前未覆盖。

保留 `do_cow_fault` 作为已有内部调用的兼容名称；A 统一使用 `cow_handle_store_fault`。两个名称返回语义完全一致。

## A 接入生产异常入口

对 store page fault（RISC-V 原因码 15）调用 `cow_handle_store_fault(current->mm, tf->tval)`。成功直接返回，不执行系统调用的 `epc += 4`；失败按用户异常/OOM 策略处理，不循环重试。内核操作用户缓冲区时也可能产生 S 态 store fault，不能只按 SPP=0 才尝试 COW；仍必须校验异常地址属于当前用户空间。不能把所有写保护异常恢复成可写。

本目录 tests/trapentry.S 是隔离测试专用的 S 态入口，沿用小组已完成 Lab4 的保存恢复布局；它不支持生产 U/S 双向栈切换，禁止用它覆盖 A 的 kern/trap/trapentry.S。

## B 的进程与回收接口

- B 提供 alloc_proc，完成 Lab5 新字段初始化，包括 exit_code、wait_state、cptr/yptr/optr 等。普通内核线程 pgdir 初值为 boot_pgdir_pa，mm 为 NULL。
- C 负责 copy_thread。不要再从 Lab4 粘贴一个同名定义；用户栈沿用参数或原用户 SP，内核线程的零 stack 才使用新 Trap 帧位置。
- set_links 已包含 nr_process++，C 的 do_fork 只调用一次，不重复挂链或计数。
- CLONE_VM 共享同一 mm 并增加 mm_count；普通 Fork 新建 mm，其数据页可使用 COW。mm_count 与每个 Page 的映射引用计数必须分别维护。
- 退出和 exec 先切离旧地址空间，再减少 mm_count；只有减到零才 exit_mmap、put_pgdir、mm_destroy。共享物理页由撤销映射时的 Page 引用控制释放，不能跳过引用判断直接 free_page。
- 内核向用户结果指针写入时使用 copy_to_user；不要自行转成物理地址绕过 COW。该函数已经支持预检查、解除 COW 和按页复制。原始裸用户指针写入则依赖 A 的 S 态写错误对接。

## 验证与复现

在 code 目录执行：

```bash
bash tools/member-c/tests/run.sh
bash tools/member-c/tests/gdb.sh
bash tools/member-c/tests/mutation.sh
```

默认使用本机已有 SiFive GCC 10.2.0、QEMU 4.1.1。编译产物在被 Git 忽略的 tools/member-c/build 中；脚本路径可按其他成员机器调整。

隔离环境链接真实 pmm.c、vmm.c，runtime.c 直接包含真实 proc.c 来测试 static copy_mm 等函数。替代组件为测试页分配器、kmalloc、B 的 alloc_proc、调度和唤醒入口。分页运行在 S 态，真实硬件 store fault 通过测试 Trap 入口调用生产 cow_handle_store_fault。它不验证生产 U/S 栈切换、ecall、ELF、do_wait/do_exit/exec，也不能代替官方 13 项回归。

已通过 COW/复制路径 52 项、普通 Fork 配置 8 项；GDB 记录 cause=15、EPC 保持不变、物理页改变、PTE 从 COW 只读变为独占可写。删除临时副本 copy_to_user 的 COW 检查后，测试在父子隔离断言失败，恢复实现后通过；这不是原 Linux Dirty COW 的并发复现。

## 失败路径语义

复制失败后，目标空间由 copy_mm 全部撤销和回收。已处理过的父页可能保留只读 COW、引用回到 1：这是合法的延迟恢复状态，后续写直接恢复 W，不分配新页。保证父数据、引用和资源不泄漏，不保证失败后父 PTE 的位模式逐位恢复原值。copy_to_user 分配失败也可能使此前检查过的页变成独占，但在整段预检查完成前不复制用户载荷。

## 尚待整合

正式 make 当前因 A 的 sys_* 封装未实现而编译失败，见 evidence/full-build.log。A/B 尚未完成时不运行官方 grade，不记录虚构分数。集成后须跑官方 13 项、COW 用户态写入、退出/exec/等待共享页回收，并补真实桌面截图。


## 2026-11-25：补齐 print_pgdir

在 pmm.c 实现 print_pgdir()，从当前 satp 提取 Sv39 根页表，不固定打印启动页表。递归最多三级，区分非叶项与 4 KiB、2 MiB、1 GiB 叶项，输出规范化虚拟地址、物理地址、大小、RWX/U/G/A/D 权限和 COW 位。不分配物理页，不修改页表或引用计数；异常表指针只输出诊断，不继续解引用。

新增 5 项实际 QEMU 断言：当前用户根及 COW、2 MiB 叶项、高位规范地址及 1 GiB 叶项、无状态修改、测试资源回收。现在 COW 隔离配置共 57 项、普通 Fork 配置共 13 项通过；GDB 和 copyout 变异检测也再次通过。

另外，在隔离的构建目录导出公共 lab5 的 c11f317，只插入本次打印函数（保留公共分支已有的 A/B/C 整合修改），完整 make 链接成功。使用 build-pgdir、DEBUG_GRADE 和 QEMU 4.1.1 实际运行，出现 pgdir pass.、all user-mode processes have quit. 和 init check memory pass.，最后到框架的 initproc exit 终止路径。见 evidence/integration-print-pgdir-build.log 与 integration-print-pgdir-runtime.log。本次没有运行全部 13 个官方程序，不能据此宣称满分。截图中 A 的定时调度、用户读缺页终止问题仍由 A 检查。


## 三阶段 GDB 取证

先运行 tests/run.sh，再运行 tests/cow-proof.sh。后者使用独立端口 12446，输出 evidence/cow-proof-*.txt；断言检查共享、复制、EPC 不变与数据隔离。report/images/cow-proof-*.png 是这些实际输出的排版导出，明确标注非桌面截图，不可称为原生窗口截图。测试范围为隔离 C 模块的 S 态写 U/COW 页面。


## 公共分支合并验证（2026-11-25）

在公共 lab5 的 9923025 上合入 C 的三阶段 GDB 材料，保留三人的完整报告与 Prompt。C 隔离测试原来定义了 alloc_proc 替代函数，与 B 的正式实现重复；测试脚本现在自动识别正式实现，并禁用替代函数，不改动生产 proc.c。合并后 COW 配置 57 项、立即复制配置 13 项检查通过；完整 make 和 make grade 成功，官方评分输出 Total Score: 130/130（见 evidence/merge-build.log、merge-grade.log）。此评分不替代 Challenge 的全部专项验证；三张 COW 图仍明确标为日志导出图，非原生桌面截图。


## 整合版用户 COW 演示（2026-11-27）

从 code/ 执行 `bash tools/member-c/tests/integrated-cow.sh`。脚本构建 user/cowproof.c 并在完整内核中运行，默认自动完成三阶段 GDB 观察（端口 12447），检查真实 U 态故障、共享、复制、EPC 不变和程序回收结果。需要逐阶段暂停时设置 `COW_NATIVE_PAUSE=1`，观察 build/native-cow/stageN.ready 后在同目录创建 stageN.next 继续；不要同时运行两个使用该目录和端口的会话。

report/images/cow-native-*.png 是 Windows 记事本查看实测阶段日志时截取的原生窗口图，不是 GDB 终端窗口截图。报告已用它们替换旧排版图，并改为记录整合版测试。生产内存管理和 Trap 源码没有为演示插桩或改动。
