# 操作系统 Lab4：进程管理

RISC-V RV64（Sv39）uCore Lab4：在 Lab2/Lab3 的物理内存管理、页表与中断处理基础上，实现内核线程的创建、初始化、调度与上下文切换。

## 目录

| 路径 | 内容 |
| --- | --- |
| `code/` | 实验源码（以课程提供的 `lab4` 代码为基础） |
| `report/` | 实验报告与提示词记录 |
| `code/tools/member-b/`、`code/tools/member-c/` | 成员独立测试脚本与运行日志 |
| `docs/lab4/` | 课程指导书归档与 Lab2/Lab3 前置迁移记录 |

## 分工与实现位置

| 成员 | 模块 | 函数 | 位置 |
| --- | --- | --- | --- |
| 甲 | 虚拟内存页操作 | `page_insert`、`page_remove` | `code/kern/mm/pmm.c` |
| 乙（2413507） | 进程创建与初始化 | `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread`、`copy_mm` | `code/kern/process/proc.c`（`LAB4 function2 2413507`） |
| 丙（2411788） | 调度与上下文切换 | `cpu_idle`、`proc_run`、`schedule`、`switch_to` | `proc.c`（`LAB4 function3 2411788`）、`code/kern/schedule/sched.c`、`code/kern/process/switch.S` |

Lab2（物理内存管理、页表初始化）和 Lab3（中断入口与处理）的前置代码已迁入 `code/`，迁移说明见 `docs/lab4/prerequisite-migration.md`。

## 构建与运行

环境：WSL2 Ubuntu，QEMU 4.1.1，指导书推荐的 SiFive 预编译工具链 `riscv64-unknown-elf-gcc 10.1.0`。

```sh
cd code
make            # 构建 bin/ucore.img
make qemu       # 启动内核
make grade      # 官方评分：注入测试后运行 QEMU
```

工具链不在 PATH 中时，可用 `make grade GCCPREFIX=<工具链目录>/bin/riscv64-unknown-elf-` 指定。

> 说明：GCC 14 及以上版本把“不兼容的函数指针类型”默认作为错误。官方测试 `tools/test_lab4.c` 中
> 向 `kernel_thread` 传入了 `void (*)(void *)` 类型的函数，因此用新版 GCC 时评分用测试内核无法编译。
> 请使用 GCC 10 等较早版本（此时仅为警告），我们没有修改评分脚本或测试代码。

## 当前结果

- `make grade`（GCC 10.1、QEMU 4.1.1）：`check PMM`、`check Process Create`、`check Schedule` 全部 OK，**Total Score: 30/30**。
- `make qemu` 输出 `this initproc, pid = 1, name = "init"` 等三行后进入 `do_exit` 中的框架 `panic` 占位，这是本实验框架的预期行为（进程退出在后续实验实现）。
- 各成员的独立测试与日志见 `code/tools/member-b/README.md`、`code/tools/member-c/README.md`。
