# Lab4 成员乙（2413507）：进程创建与初始化

本文是技术交接笔记，不是实验报告。

## 基线与提交

| 项目 | 值 |
| --- | --- |
| 开发分支 | `feature/lab4-member-b` |
| 基线 | `origin/lab4` = `4344410`（2026-11-11 fetch；已含 Lab2/Lab3 迁移、甲的页映射、丙的调度与切换） |
| 基线 `make` | 链接失败，唯一错误：`init.c:29 undefined reference to 'proc_init'` |

| 提交 | 内容 |
| --- | --- |
| `0da5d80` docs(lab4-b): audit process module and define verification plan | 基线审计 |
| `7d23050` feat(lab4-b): initialize safe process control blocks | `alloc_proc` |
| `330aff3` feat(lab4-b): create runnable kernel threads with rollback | `kernel_thread`、`copy_mm`、`copy_thread`、`do_fork` |
| `9dbd0f1` feat(lab4-b): initialize idle and init kernel processes | `proc_init` |
| `806391c` test(lab4-b): cover process initialization and failure rollback | 测试与证据 |
| 本文所在提交 docs(lab4-b): hand off tested process module and evidence | 交接、GDB 证据、Challenge 复核 |

生产代码只改动 `code/kern/process/proc.c` 中 `//LAB4 function2 2413507` 注释之后、`//LAB4 function3 2411788` 之前的区域。
丙的区域、`sched.c`、`switch.S`、`pmm.c`、评分脚本与 `test_lab4.c` 均未修改。

## 实现要点

- `alloc_proc`：`kmalloc` 失败返回 NULL；成功时整体清零后设 `state=PROC_UNINIT`、`pid=-1`、
  `pgdir=boot_pgdir_pa`（页对齐物理地址，不是 satp），两个链表节点自环。
- `kernel_thread`：栈上临时 trapframe 全部清零；`s0=fn`、`s1=arg`（对应 `kernel_thread_entry`）；
  `epc=kernel_thread_entry`；`status = (sstatus | SPP | SPIE) & ~SIE`；调用 `do_fork(clone_flags | CLONE_VM, 0, &tf)`。
- `copy_mm`：当前进程 `mm == NULL`（内核线程）时返回 0 且子 `mm=NULL`；
  非空用户 mm 需要 Lab5 的复制/共享代码，返回 `-E_INVAL`，不假装成功。
- `copy_thread`：trapframe 放在子内核栈顶（288 字节，编译期断言为 16 的倍数，因此对齐）；
  `a0=0`；`esp==0` 时 `gpr.sp=tf`（避免内核线程带空栈进入 C 函数）；`context.ra=forkret`、`context.sp=tf`。
- `do_fork`：上限检查 → PCB → 内核栈 → `copy_mm` → `copy_thread`；
  关中断临界区内完成 `get_pid`、`hash_proc`、`list_add`、`nr_process++`、`wakeup_proc`。
  失败按逆序 `put_kstack` / `kfree` 回滚，不消耗 PID。
- `proc_init`：初始化链表与哈希；idle 使用 `bootstack`、PID 0、RUNNABLE、`need_resched=1`、名称 idle，
  不进入 `proc_list` / 哈希；`current=idleproc`；`kernel_thread(init_main, "Hello world!!", 0)` 创建 init，
  设名称 init；任何一步失败都 `panic`。

## 与甲/丙的接口约定（均已实测）

- `proc->pgdir` 为页对齐 Sv39 根页表物理地址；丙的 `proc_run` 自行拼 satp（GDB 观测 `satp=0x8000000000080208`）。
- idle 不在 `proc_list`，由丙的 `schedule` 兜底选择；普通线程挂在 `proc_list` 头部。
- 首次运行：`context.ra=forkret`、`context.sp=tf`（16 字节对齐）。

## 验证结果（2026-11-11）

工具：QEMU 4.1.1（worktree 的 `.envrc` 局部 PATH）；指导书推荐的 SiFive 预编译工具链
`riscv64-unknown-elf-gcc 10.1.0 (2020.08.2)`，安装在 `~/opt/sifive/`，由本地 `.envrc` 只在本工作区内生效。

| 层级 | 结果 | 证据 |
| --- | --- | --- |
| 编译 | `make` 通过（GCC 10.1 与 GCC 15.1 均通过） | `evidence/member-b-build.log` |
| 乙独立测试 | 61/61 PASS，真实 `proc.c` + QEMU | `evidence/member-b-qemu.log` |
| 变异测试 | 9 个注入缺陷全部被检出 | `evidence/mutation.txt` |
| 完整启动 | `make qemu` 打印 init 三行后进入框架 `do_exit` panic 占位（预期） | `evidence/official-qemu.log` |
| 首次运行 GDB | idle → init 的 `proc_run` / `forkret` / `kernel_thread_entry` / `init_main` 现场 | `evidence/gdb-first-run.log` |
| 正式评分 | `make grade`：PMM / Process Create / Schedule 均 OK，**Total Score: 30/30**（GCC 10.1） | `evidence/make-grade-gcc10.log` |

### 发现的问题

- 环境：本机原有 `riscv64-unknown-elf-gcc 15.1.0` 下 `make grade` 为 0/30。原因是官方 `tools/test_lab4.c`
  把 `void (*)(void *)` 传给 `kernel_thread(int (*)(void *), ...)`，GCC 14 起该情况默认为错误，测试内核编译失败
  （`evidence/gcc15-official-test-build-error.log`）。不是进程模块缺陷，也没有修改测试或评分脚本；
  改用指导书推荐的 SiFive GCC 10.1 后为警告，评分 30/30。建议全组评分时统一使用该工具链。
- 测试自身：第一版测试中 `status-keeps-sie` 变异存活——`proc_init` 时中断本来关闭，原检查无法区分；
  补充了“在 `init_main`（SIE=1）中创建线程”的检查后被检出。

## 复现

本地工作区根目录的 `.envrc`（不提交，由 direnv 加载）只在该目录内把 QEMU 4.1.1 和 SiFive GCC 10.1 放到 PATH 前面，
离开目录后恢复原有工具链：

```bash
PATH_add "$HOME/src/qemu-4.1.1/build-debug/riscv64-softmmu"
PATH_add "$HOME/opt/sifive/riscv64-unknown-elf-gcc-10.1.0-2020.08.2-x86_64-linux-ubuntu14/bin"
```

在该目录内直接运行（没有 direnv 时，可给每条命令加 `GCCPREFIX=<工具链>/bin/riscv64-unknown-elf-`）：

```bash
cd code
make grade
bash tools/member-b/tests/run.sh        # 乙测试 + 官方注入测试，日志写入 evidence/
bash tools/member-b/tests/mutation.sh   # 变异测试
```

GDB 首次运行现场：`make` 后，启动
`qemu-system-riscv64 -machine virt -nographic -bios default -device loader,file=bin/ucore.img,addr=0x80200000 -S -gdb tcp:127.0.0.1:12507`，
另一终端运行 `riscv64-unknown-elf-gdb -batch -nx -x tools/member-b/tests/first-run.gdb bin/kernel`。

测试原理：`tests/prepare.py` 在临时副本中把 `tests/member_b_tests.inc` include 到真实 `proc.c` 末尾
（可直接调用 static 函数），在 `proc_init` 末尾和 `init_main` 开头插入钩子，并仅为该副本链接
`--wrap=kmalloc,kfree,alloc_pages` 做一次性故障注入。真实源码不变；QEMU 以 20 秒超时结束，
是否通过只看日志中的 `SUMMARY` 与 FAIL 行。

## 剩余边界

- `do_exit` 仍为框架 panic 占位，不在六个函数范围内；没有线程回收、用户进程和多核验证。
- `report/report.md` 中丙“原包缺 tools/inject.py、无正式评分”的描述已过时（当前已有且评分 30/30），由全组统一整理报告时更新。

## 报告素材点位

- `evidence/make-grade-gcc10.log` 末尾三项 OK 与 30/30。
- `evidence/gdb-first-run.log`：`context.ra == forkret`、`sret` 后 `SIE=1/SPP=0`、`s1 = "Hello world!!"`。
- `evidence/member-b-qemu.log`：回滚与 PID 检查。
- `evidence/mutation.txt`：测试有效性。
- Challenge 复核见 [challenge-review.md](challenge-review.md)。
