# RR / CFS 与进程管理专项测试

本目录测试 Lab6 中的进程管理、时钟中断驱动调度、RR 调度类（`kern/schedule/default_sched.c`）和简化版 CFS 调度类（`kern/schedule/cfs_sched.c`）。测试分两层，两层都直接调用项目中的实现，没有另写一套调度算法。

| 层次 | 入口 | 覆盖内容 | 无法覆盖的内容 |
| --- | --- | --- | --- |
| 宿主机单元测试 | `bash run_host.sh` | 通过 `struct sched_class` 回调测试 RR、CFS 的队列、时间片、vruntime、权重、抢占、公平性和边界情况 | RISC-V Trap、上下文切换、真实时钟 |
| QEMU 内核测试 | `bash run_qemu.sh [rr\|cfs\|all] [gdb]` | PCB 初始化、fork/exec/exit/wait/kill/yield、系统调用、用户态非法访问、时钟抢占、CPU 时间分配 | — |

## 运行环境

- WSL2 Ubuntu 22.04，宿主机 GCC 11.4.0（带 AddressSanitizer / UBSan）
- QEMU 4.1.1，SiFive riscv64-unknown-elf-gcc 10.1.0 / gdb 9.1（与 `code/Makefile` 使用的工具链相同）

## 宿主机单元测试（`host/`）

`host/Makefile` 用 `-D__riscv_xlen=64` 原样编译 `default_sched.c`、`cfs_sched.c`，使用真实的 `proc.h`、`sched.h` 和 `list.h`，并复用成员 A 的结果输出模块 `tests/fifo_sjf/host_io.c`。

- `test_main.c` 里的 `sim_*` 驱动只复现 `sched.c` 在调度类外围做的事：时钟节拍 → `proc_tick` → 若 `need_resched` 则 `enqueue(current)` → `pick_next` → `dequeue`。驱动本身不包含任何调度策略。
- 断言类负向测试（重复入队、重复出队、从错误队列出队、未初始化的 CFS 队列）在 `fork` 出的子进程中执行，预期内核 `assert` 触发并以退出码 2 结束；另有一个合法操作序列作为对照，预期不触发断言。

RR 部分覆盖：空队列初始化与选择；单进程入队、选择、出队；多进程按 FIFO 轮转；`proc_num` 与链表长度一致；时间片为 0、负数或超过上限时重新分配，合法剩余时间片保留；时间片从 1 减到 0 时置位 `need_resched`，减到 0 后不再变为负数；已有的 `need_resched` 不被清除；重新入队后排到队尾；最大时间片为 5、2、1 时的 30 节拍轮转轨迹和切换次数。

CFS 部分覆盖：空队列、单进程、多进程；相同 vruntime 时按入队先后选择；选择 vruntime 最小的进程；每节拍的 vruntime 增量（权重 1、2、3、4、1024，优先级 0，以及被截断到 1024 的超大优先级）；`cfs_slice_exec` 饱和不回绕；vruntime 跨越 2^64 回绕后仍能正确比较；修改优先级后不重置历史 vruntime；独自运行 1000 节拍不发生抢占；理想时间片和最小粒度两种抢占条件；1:1:1、1:2、1:2:4、1:8 的长期 CPU 份额；新进程加入长期运行的系统；睡眠 3000 节拍后被唤醒；出队后再入队保留运行历史。

份额误差容限在运行前就已确定，取值为“每个进程一个调度周期（10 节拍）”除以总节拍数：在固定节拍内结束时，最多有一个周期内的时间片尚未轮完。

## QEMU 内核测试（`qemu/` 与 `run_qemu.sh`）

对每种调度器，脚本先把 `code/` 复制到 `mktemp` 临时目录再修改，仓库中的源码不受影响：

1. 把 `qemu/user/b_proc.c` 和 `qemu/user/b_spin.c` 作为额外的用户程序加入临时副本；
2. `alloc_proc` 是 `static` 函数，因此把 `qemu/kernel/b_kcheck.c` 追加到副本的 `proc.c` 中，并在 `proc_init()` 末尾调用，以检查新 PCB 中原有字段、6 个 Lab6 调度字段和 CFS 字段的初值；
3. 选择 `cfs` 时，把副本中 `sched_init()` 的 `sched_class` 改为 `&cfs_sched_class`。成员 C 的统一选择机制合入后，需要按该机制修改这一步。

接着分别运行官方 `priority`、`b_proc` 和 `b_spin`，并根据真实输出判定结果：

- 每次运行都检查：PCB 初值、`all user-mode processes have quit.`、`init check memory pass.`；只允许出现结尾的 `initproc exit.` 一次内核 panic，不允许出现用户 panic，也不允许出现 Lab3 的 `100 ticks` 或 EOT 输出。
- `b_proc`：被测进程的 pid；`gettime` 随时钟节拍递增；`wait`、`waitpid`、`kill` 的错误码；fork 后父子进程的返回值和退出码；子进程写入的数据不影响父进程（普通复制语义）；向 `wait` 传入内核地址作为状态指针时返回 `-E_INVAL`；kill 一个不主动 yield 的 CPU 密集型子进程，以及重复 kill；用户态读 NULL、写内核地址、写只读代码段时只结束出错的子进程；32 轮 fork/exit/wait 后内存检查仍通过；`setpriority` 和 `yield` 可以正常返回。
- `b_spin`：三个从不主动 yield 的子进程都在 1500 ms 之前开始运行，说明时钟中断能够抢占；比较优先级相同（1:1:1）和不同（1:2:4）时子进程循环计数的比值。
- 比值容限在运行前定为 ±20%，用于吸收 QEMU TCG 计时抖动、`gettime` 的 10 ms 粒度和 fork 先后带来的偏差。RR 不使用优先级，预期 1:2:4 配置下各子进程份额仍为 1:1:1；CFS 预期为 1:2:4。
- `gdb` 选项：在 GDB 中统计到第一次 `do_exit` 为止 `sched_class_proc_tick` 和调度类 `proc_tick` 的命中次数，并与内核的 `ticks` 计数比较。

## 本次结果（2026-11-27）

两次完整运行均全部通过，下面是第二次运行（即提交的日志）的数据；第一次运行中 CFS 1:2:4 的比值为 100 : 199 : 405。

| 测试 | 结果 | 记录 |
| --- | --- | --- |
| 宿主机单元测试 | 121 / 121 通过 | `results/host.txt` |
| QEMU，RR + CFS，含 GDB 统计 | 88 / 88 通过 | `results/qemu/summary.txt` 和各程序日志 |

主要数据如下：

| 场景 | RR | CFS |
| --- | --- | --- |
| `priority`：优先级 1–5 的 acc | 628k / 684k / 648k / 644k / 660k，`sched result: 1 1 1 1 1` | 240k / 452k / 700k / 928k / 1168k，`sched result: 1 2 3 4 5` |
| `b_spin`：1:1:1 比值 | 100 : 98 : 96 | 100 : 101 : 100 |
| `b_spin`：1:2:4 比值 | 100 : 101 : 101 | 100 : 190 : 382 |
| GDB 统计（到第一次 `do_exit`） | ticks = 201，`sched_class_proc_tick` = 201，`RR_proc_tick` = 201 | ticks = 201，`sched_class_proc_tick` = 201，`CFS_proc_tick` = 201 |
| 宿主机模拟 1:2:4（7000 节拍） | — | 1000 / 2000 / 4000 节拍，与理论份额相同 |

另外做了变异检查：在临时副本中故意改坏实现，包括 RR 改为队首插入、不限制超大时间片、时间片减为负数，CFS 忽略权重、新进程从 0 开始、取消睡眠补偿上限、改用无符号比较、单进程也抢占、相同 vruntime 时选择后入队者、优先级 0 映射错误。每一种改动都至少导致一项宿主机测试失败。

运行说明：本次 QEMU 测试时，成员 C 的 `schedule()` 和成员 A 的 `print_pgdir()` 尚未合入 `lab6`，因此通过 `LAB6_OVERLAY` 在临时副本中补入了标准 Lab6 流程的临时实现（`summary.txt` 中以 `overlay:` 开头的行），这两个临时实现没有提交到仓库。上表不代表完整的 `make grade` 结果；`make grade` 需要成员 C 的 Stride 调度类，请在团队代码合入后重新运行。
