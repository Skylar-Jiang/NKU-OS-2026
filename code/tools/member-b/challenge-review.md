# Lab4 Challenge 技术复核（成员乙 2413507）

分工中乙负责全部 Challenge 答案的技术复核；答案主笔仍为甲（分页）、丙（中断开关）。
下文区分 **[源码事实]**（可在当前代码或运行日志中核对）和 **[分析结论]**。

## Challenge 1：`local_intr_save` / `local_intr_restore`

丙在 `report/report.md` 的结论总体正确，复核意见：

1. [源码事实] `__intr_save()` 读取 `sstatus.SIE`：若为 1 则 `intr_disable()`（清 SIE）并返回 1，否则返回 0；
   `__intr_restore(flag)` 仅在 flag 为 1 时 `intr_enable()`。因此嵌套使用时只有最外层恢复中断，结论正确。
2. [源码事实，需修正表述] 报告写“宏使用 do while(0) 包成一条语句”，但这只对 `local_intr_save` 成立。
   `kern/sync/sync.h` 中 `#define local_intr_restore(x) __intr_restore(x);` 自带分号，
   在 `if (c) local_intr_restore(f); else ...` 中会展开成两条语句导致 `else` 语法错误。当前代码中没有这种用法，不影响运行。
3. [分析结论 + GDB 实测] 线程**首次**运行时中断不是由 `local_intr_restore` 打开的：
   `schedule()` 关中断后经 `proc_run` → `switch_to` 进入新线程的 `forkret`，
   新线程永远不会执行旧线程栈上的 `local_intr_restore`；中断由 `__trapret` 的 `sret` 依据 trapframe 中 `SPIE=1` 打开。
   `evidence/gdb-first-run.log` 中 `forkret` 处 `tf->status=0x...46120`（SPP=1、SPIE=1、SIE=0），
   `kernel_thread_entry` 处 `sstatus=0x...46002`（SIE=1）。这就是 `kernel_thread` 必须清 SIE、置 SPIE 的原因：
   若 trapframe 中 SIE=1，`RESTORE_ALL` 先写 `sstatus` 再恢复通用寄存器，恢复过程中即可能被中断打断。
   乙的测试 `kernel_thread clears SIE even when caller has SIE=1` 对此做了回归，变异测试 `status-keeps-sie` 可被检出。
4. [分析结论] 关本地中断不能屏蔽同步异常，也不能替代多核锁，丙的边界说明正确。

## Challenge 2：Sv32 / Sv39 / Sv48 与 `get_pte`

1. [源码事实] 当前 `get_pte()`（`kern/mm/pmm.c`）对 `PDX1(la)`（第 2 级）和 `PDX0(la)`（第 1 级）
   各有一段“若不存在且允许创建，则分配页、引用计数置 1、清零、写入非叶 PTE”的代码，最后返回第 0 级 `PTX(la)` 处的 PTE 地址。
2. [分析结论] 两段相似是因为多级页表每一级的格式与处理方式相同：每级都是 1 页大小的 PTE 数组，
   用虚拟地址中一段固定位宽索引，非叶项保存下一级页表页号。不同模式只差层数和每级位宽：
   Sv32 为 2 级（10+10 位索引，4 字节 PTE），只需要 1 段“分配中间页表”代码；
   Sv39 为 3 级（9+9+9），需要 2 段；Sv48 为 4 级，需要 3 段。重复代码本质上是一个按层展开的循环，
   可写成 `for (level = LEVELS-1; level > 0; level--)` 的通用遍历。
3. [源码事实，提请甲确认] 中间页表项写的是 `PTE_U | PTE_V`。RISC-V 特权级规范规定非叶 PTE 的
   U/A/D 位保留，软件应清零。当前 QEMU 4.1.1 下 `check_pgdir`、`check_boot_pgdir` 和评分均通过，没有观察到错误，
   但这是与规范不一致的写法（该代码来自框架/Lab2 迁移，不属于乙，未修改）。
4. [分析结论] 查找与分配合并的利弊：
   - 好处：调用方（`page_insert`、`get_page`）一行即可拿到 PTE，用 `create` 参数区分只查询还是按需建表，接口简单。
   - 问题：一个函数有“纯查询”和“修改页表、申请内存”两种副作用不同的语义；
     第二级分配失败时，第一级已分配的页表页保留在树中（不算泄漏，但调用方无法区分“未映射”和“内存不足”，都返回 NULL）。
   - 建议：Lab4 规模下保留现有接口即可；若要拆分，可以提供只读的 `walk/lookup` 和按需建表的 `walk_alloc`，
     后者内部复用前者，并返回明确错误码，便于以后实现页面换出、写时复制等对“是否修改页表”敏感的功能。

## 乙自身机制复核

- PID 复用：`get_pid` 用 `last_pid` 递增，`next_safe` 记录当前已知最小的、比 `last_pid` 大的已用 PID，
  在 `(last_pid, next_safe)` 区间内可直接分配无需扫描；越过 `next_safe` 或回绕到 1 时重新扫描 `proc_list`。
  它只扫描 `proc_list`，因此 `do_fork` 在同一关中断临界区内完成 `get_pid` 与 `list_add`，避免两次分配得到同一 PID。
  idle 的 PID 0 不在链表中，也不会被分配（回绕后从 1 开始）。
- fork 生命周期：PCB（UNINIT）→ 内核栈 → `copy_mm` → `copy_thread` →（临界区）PID、哈希、链表、计数、`wakeup_proc`（RUNNABLE）。
  失败按 `put_kstack` → `kfree` 的逆序回滚，失败路径不调用 `get_pid`，因此不消耗 PID（测试 `failed forks consumed no PID`）。
- 当前没有线程回收：`do_exit` 仍为框架 `panic` 占位，`init_main` 返回后进入该 panic，这是框架预期现象。
