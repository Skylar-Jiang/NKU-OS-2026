# Lab4 成员乙（2413507）：进程创建与初始化

本文是技术交接笔记，不是实验报告。

## 基线

| 项目 | 值 |
| --- | --- |
| 开发分支 | `feature/lab4-member-b`（独立 worktree） |
| 基线提交 | `origin/lab4` = `434441030d54e117cf4d189138f55c7c0cddea1d`（2026-11-11 fetch） |
| 基线已含 | Lab2/Lab3 前置迁移、成员甲 `page_insert`/`page_remove`、成员丙 `cpu_idle`/`proc_run`/`schedule`/`switch_to` |
| 基线 `make` | 失败，链接期唯一错误：`init.c:29 undefined reference to 'proc_init'`（属于乙） |

## 职责边界

乙只修改 `code/kern/process/proc.c` 中 `//LAB4 function2 ...` 注释下的区域：
`alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_thread`、`copy_mm`，以及该区域内必要的 static 辅助。

不修改：`//LAB4 function3 2411788` 区域（丙）、`sched.c`、`switch.S`、`pmm.c`（甲）、
Lab2/Lab3 迁移代码、`tools/grade.sh`、`tools/inject.py`、`tools/test_lab4.c`。

## 关键依赖（已从源码核对）

- `kernel_thread_entry`（`process/entry.S`）：`a0 <- s1`，`jalr s0`，返回后 `jal do_exit`。
- 首次运行链：`switch_to` 恢复 `context.ra=forkret`、`context.sp` → `forkret()` →
  `forkrets(current->tf)`（`sp <- a0`）→ `__trapret`（`RESTORE_ALL` 先写 `sstatus/sepc`，最后恢复 `x2`）→ `sret`。
- `struct trapframe` = 32 个 GPR + `status/epc/badvaddr/cause` = 288 字节（16 的倍数）。
- `proc_run`（丙）要求 `proc->pgdir` 是页对齐的根页表**物理地址**，用 `8<<60 | pgdir>>12` 写 satp。
- `schedule`（丙）只遍历 `proc_list`；idle 不在链表中，作为兜底。
- `get_pid` 只扫描 `proc_list`，因此 PID 分配必须和 `list_add` 处于同一临界区。
- `bootstack`（`init/entry.S`）大小 `KSTACKSIZE = 2 * PGSIZE`；`KSTACKPAGE = 2`。
- 错误码：`-E_NO_MEM`、`-E_NO_FREE_PROC`、`-E_INVAL`（`libs/error.h`）。
- 评分注入：`tools/inject.py` 在临时副本中把 `test_lab4_process_create()` 插入 `proc_init` 末尾，
  把 `test_lab4_schedule()` 插入 `init_main` 开头；因此 `proc_init` 末尾时 idle 的 `runs/tf/context.ra` 必须仍为 0。

## 工具版本

```
riscv64-unknown-elf-gcc 15.1.0
QEMU emulator version 4.1.1   (worktree .envrc 局部 PATH_add，未改全局)
GNU Make 4.3, Python 3.10.12
```

## 验收分级

“编译成功”“乙独立测试通过”“完整启动成功”“正式评分通过”分开记录，互不替代。

## 证据位置

- `tools/member-b/tests/`：可重跑的测试脚本与测试源码。
- `tools/member-b/evidence/`：真实运行日志。
