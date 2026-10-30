# Lab3：成员乙（B）中断与时钟处理报告

成员：蒋林瀞，学号 2413507（依据小组 Lab2 报告成员表与本仓库 Git 用户核对）。当前实验角色：乙。验证日期：2026-10-28。

本文为乙的独立报告，供成员丙最终合并，不包含整体 Lab3 报告的统稿内容。

## 1. 实验目标与整体执行流

Lab3 的目标是让内核具备完整的 Trap 处理能力：发生中断或异常时保存现场，进入 C 函数按原因处理，再恢复现场返回原执行流。本实验用时钟中断验证这条链路能持续工作。

一次时钟中断从初始化到返回的完整过程如下，【乙】标出乙负责的环节：

```text
kern_init
  ├─ idt_init()        stvec ← &__alltraps                         【乙：初始化顺序】
  ├─ pmm_init()        物理内存管理                                 （甲）
  ├─ clock_init()      sie.STIE ← 1，sbi_set_timer(now + 100000)   【乙】
  ├─ intr_enable()     sstatus.SIE ← 1                             【乙】
  └─ while (1);        空转，等待中断

到达预约时间 → OpenSBI 置 sip.STIP → 硬件 Trap 进入 S 态：
  sepc ← 被打断的 PC，scause ← 0x8000000000000005，SPIE ← SIE，SIE ← 0，SPP ← 1
  → stvec → __alltraps：SAVE_ALL，a0 ← sp                         （甲）
  → trap(tf) → trap_dispatch：scause 最高位为 1                    （公共框架）
  → interrupt_handler(tf)                                          【乙】
       clock_set_next_event()、ticks++、每 100 次 print_ticks()、10 次后 sbi_shutdown()
  → __trapret：RESTORE_ALL，sret                                   （甲）
  → 回到 sepc 处继续空转，SIE 恢复为 1，等待下一次中断
```

异常（`scause` 最高位为 0）进入 `exception_handler()`，由成员丙负责。

乙负责的文件和函数：

| 位置 | 内容 |
| --- | --- |
| `kern/trap/trap.c` | `interrupt_handler()`、`idt_init()` 注释与标记、时钟相关宏 |
| `kern/driver/clock.c` | `clock_init()`、`clock_set_next_event()` |
| `kern/driver/intr.c` | `intr_enable()`、`intr_disable()` |
| `kern/init/init.c` | `kern_init()` 的中断初始化顺序 |

## 2. SPEC 与模块设计

### 2.1 interrupt_handler()

| 项目 | 约束 |
| --- | --- |
| 接口 | `void interrupt_handler(struct trapframe *tf)`，签名不变 |
| 前置条件 | `__alltraps` 已保存现场；`tf` 指向当前内核栈上的有效 Trap frame；`tf->cause` 最高位为 1 |
| 后置条件 | 时钟中断已重新预约、`ticks` 恰好加 1；其他中断输出与原因码一致的提示；`tf->epc` 和其他保存值不被修改 |
| 时钟分支 | 先重设下一次事件，再计数；`ticks % TICK_NUM == 0` 时输出 `100 ticks`；第 `SHUTDOWN_AFTER`（10）次输出后关机 |
| 其他已知中断 | 原因码 0–11 中除 5 以外，按 `IRQ_*` 含义输出“特权级 + 类型 + interrupt” |
| 未知中断 | 调用已有 `print_trapframe(tf)`，不增加恢复逻辑 |
| 评分模式 | 定义 `DEBUG_GRADE` 时，`print_ticks()` 输出 `End of Test.` 并 `panic` |

### 2.2 时钟与中断使能

| 函数 | 前置条件 | 后置状态 |
| --- | --- | --- |
| `idt_init()` | S 态，`__alltraps` 4 字节对齐 | `sscratch = 0`，`stvec = &__alltraps`，MODE = Direct |
| `clock_init()` | OpenSBI 定时器服务可用 | `sie.STIE = 1`，首次事件已预约，`ticks = 0` |
| `clock_set_next_event()` | 可调用 SBI | 下一次事件为当前 `time` + 100000，同时清除挂起的 STIP |
| `intr_enable()` / `intr_disable()` | S 态 | 仅置位或清除 `sstatus.SIE`，其他位不变 |

初始化顺序的约束：`stvec` 必须最先就绪，任何 Trap 都要进入本内核的入口；`sstatus.SIE` 必须最后打开，此前的 `sie.STIE` 和首次事件都只是“准备好”，不会提前打断初始化。

## 3. 核心机制分析

### 3.1 scause 的两段编码

RV64 的 `scause` 是 64 位寄存器。第 63 位区分中断（1）与异常（0），低位是原因码。`trap_dispatch()` 用 `(intptr_t)tf->cause < 0` 判断最高位，这是公共框架代码。`interrupt_handler()` 再用 `(tf->cause << 1) >> 1` 清掉最高位取得原因码。由于 `tf->cause` 是无符号的 `uintptr_t`，右移是逻辑右移，结果正确。

同样的低位原因码在两类 Trap 中含义不同。例如原因码 1 在中断中是 S 态软件中断，在异常中是取指访问错误。因此必须先看最高位再看原因码，第 7 节的隔离测试专门覆盖了这一点。

框架中 `IRQ_*` 的编号来自特权级规范 1.9 版，原因码 0–11 满足：

```text
原因码 = 4 × 类型 + 特权级      类型：0 软件 / 1 时钟 / 2 外部
                               特权级：0 U / 1 S / 2 H / 3 M
```

例如 `IRQ_S_TIMER = 4 × 1 + 1 = 5`。最终实现利用这一编码，用两张短字符串表生成提示，而不是为 11 个分支各写一条字符串（原因见第 4.2 节）。

### 3.2 stvec：单一入口

`stvec` 低 2 位是 MODE。`__alltraps` 4 字节对齐，因此写入其地址后 MODE = 0（Direct），所有中断和异常都跳到同一入口，由软件读取 `scause` 分拣。这与 x86 的 IDT 每个向量一个入口不同。函数名 `idt_init` 和原注释中的 `__vectors`、`lidt`、`SETGATE` 都是从 x86 版 ucore 继承的，RISC-V 版既没有 IDT 也没有 `vectors.S`，本次已将注释改为 `stvec` 的实际行为。

GDB 实测：进入 `kern_init` 时 `stvec = 0x80200000`，并不指向本内核入口；`idt_init()` 后 `stvec = 0xffffffffc0200b04`，即 `__alltraps`。

### 3.3 两级中断使能

S 态时钟中断要真正打断 CPU，需要同时满足三个条件：

| 条件 | 设置者 | 作用 |
| --- | --- | --- |
| `sip.STIP = 1` | OpenSBI（M 态定时器到期后置位） | 有中断挂起 |
| `sie.STIE = 1`（第 5 位） | `clock_init()` | 按类型允许时钟中断 |
| `sstatus.SIE = 1`（第 1 位） | `intr_enable()` | S 态全局允许中断 |

`sie` 是分类型开关，`sstatus.SIE` 是全局开关。进入 Trap 时硬件把 `SIE` 存入 `SPIE` 并清零 `SIE`，所以处理程序运行期间不会被新的中断打断；`sret` 再把 `SIE` 恢复为 `SPIE`。这也是 `kern/sync/sync.h` 中 `local_intr_save()` 只需操作 `sstatus.SIE` 的原因。

### 3.4 SBI 定时器与 ticks

S 态不能直接写 M 态的 `mtimecmp`。`sbi_set_timer(t)` 通过 `ecall` 请求 OpenSBI 在 `time` 寄存器达到 `t` 时产生一次 S 态时钟中断，并清除当前挂起的 STIP。`get_cycles()` 实际执行的是 `rdtime`，读取的是 `time` 而非 `cycle`。QEMU virt 的 `time` 频率为 10 MHz，`timebase = 100000` 对应 10 ms，即每秒 100 次中断，100 次中断约 1 秒。

SBI 一次只能预约一个事件，所以处理程序每次都必须重新预约。下一次时刻按“处理时的当前时间 + 100000”计算，处理延迟会逐次累积。正常运行 1000 次中断实测耗时约 10.2 秒，与此一致。

`ticks` 声明为 `volatile size_t`，在中断上下文中修改、在别处可能读取，`volatile` 防止编译器把它缓存在寄存器里。

## 4. 实现与修改

### 4.1 基线状态

基线来自远程 `lab3` 分支（`056ad6a`，包含成员甲的提交）。审查结论：

| 项目 | 基线状态 | 处理 |
| --- | --- | --- |
| 时钟分支逻辑 | 已正确：重设事件、计数、每 100 次输出、10 次后关机 | 保留，只把魔数 10 改为 `SHUTDOWN_AFTER` |
| `clock.c`、`intr.c` | 功能正确 | `intr.c` 不改；`clock.c` 仅修正注释 |
| 其他中断分支 | 11 个分支中 5 个提示错误（下表） | 修正 |
| `SHUTDOWN_AFTER` | 指导书列为可用宏，源码中不存在 | 在 `trap.c` 中定义为 10 |
| `kern_init()` | `idt_init()` 被调用两次 | 删除第二次 |
| 学号标记 | 乙负责代码中的 `2313508` 不属于本组成员 | 改为 2413507 |
| x86 遗留注释 | `idt_init` 的 IDT/lidt 说明、`trap()` 的 iret、`clock_init` 的 8253 | 改为 RISC-V 实际机制 |

基线中提示错误的分支：

| 原因码 | 宏 | 基线输出 | 正确类型 |
| --- | --- | --- | --- |
| 6 | `IRQ_H_TIMER` | Hypervisor software interrupt | Hypervisor timer |
| 7 | `IRQ_M_TIMER` | Machine software interrupt | Machine timer |
| 8 | `IRQ_U_EXT` | User software interrupt | User external |
| 10 | `IRQ_H_EXT` | Hypervisor software interrupt | Hypervisor external |
| 11 | `IRQ_M_EXT` | Machine software interrupt | Machine external |

### 4.2 修改内容

**interrupt_handler()**：时钟分支保持原逻辑；其余 11 个已知原因码合并为一组 `case`，由编码规则查表输出；未知原因码仍走 `default` 调用 `print_trapframe(tf)`。

```c
static const char *const irq_level[] = {"User", "Supervisor", "Hypervisor", "Machine"};
static const char *const irq_type[] = {"software", "timer", "external"};
...
case IRQ_U_SOFT: case IRQ_S_SOFT: case IRQ_H_SOFT: case IRQ_M_SOFT:
case IRQ_U_TIMER:                 case IRQ_H_TIMER: case IRQ_M_TIMER:
case IRQ_U_EXT:  case IRQ_S_EXT:  case IRQ_H_EXT:  case IRQ_M_EXT:
    // codes 0-11 are 4 * type + privilege level (U/S/H/M)
    cprintf("%s %s interrupt\n", irq_level[cause & 3], irq_type[cause >> 2]);
    break;
```

`case` 标签仍逐个列出 `IRQ_*` 宏，表下标只在这 11 个值上使用，不会越界。没有采用“每个分支写一条正确字符串”的直接改法，是因为它导致评分失败，过程见第 6.2 节。

**kern_init()**：保留 `pmm_init()` 之前的 `idt_init()`，使内存初始化期间发生的任何 Trap 都能进入本内核入口；删除其后的重复调用。`pmm_init()` 不修改 `stvec`，第二次调用没有作用。`clock_init()` 和 `intr_enable()` 的顺序不变。同时删除了一句标注“正式上传时应删去”的草稿注释。

**未修改的部分**：`exception_handler()`、`trapentry.S`、`trap.h`、`kern/mm/`、`tools/grade.sh`、`Makefile`。`exception_handler()` 中的旧标记 `2313508` 属于成员丙，留待丙处理。

## 5. 测试与结果分析

### 5.1 环境

| 组件 | 版本 |
| --- | --- |
| QEMU | 4.1.1（本机 `~/src/qemu-4.1.1/build-debug`，仅通过 `QEMU=` 变量指定，不改全局配置） |
| OpenSBI | v0.4（QEMU 4.1.1 自带） |
| GCC / binutils / GDB | riscv64-unknown-elf GCC 15.1.0、binutils 2.45、GDB 16.3.90 |

本机工具链无法直接汇编甲的 `trapentry.S`（见第 6.1 节），所有构建都附加 `DEFS=-Dsbadaddr=stval`。该参数只在预处理阶段把旧 CSR 名换成新名，两者是同一个寄存器（`riscv.h` 中 `CSR_SBADADDR = 0x143`，即 `stval` 的编号），不改变生成的指令语义。

所有测试由 `code/tools/member_b/run_checks.sh` 一键复现，日志保存在 `code/tools/member_b/evidence/`：

```bash
cd code && sh tools/member_b/run_checks.sh
```

```text
PASS  build
PASS  normal run: 10 x '100 ticks', QEMU exited by itself (rc=0)
PASS  gdb timer flow (35 assertions)
PASS  gdb isolated dispatch (17 assertions)
PASS  dispatch labels match IRQ_* codes 0-4, 6-11
PASS  unknown codes 12 and 16 print the trapframe
PASS  DEBUG_GRADE: panic after the first '100 ticks'
```

### 5.2 正常运行

- **目的**：确认时钟中断持续触发，计数、输出和关机符合要求。
- **方法**：`make clean && make`，再 `make qemu`，外层 `timeout 30` 只作保护。
- **结果**：输出 10 行 `100 ticks` 和 `System shutdown after 10 prints.`，QEMU 自行退出，返回码 0，耗时约 10.2 秒（`qemu-normal.log`）。
- **结论**：时钟事件没有在第一次后停止，也没有漏计；第 10 次输出后由 `sbi_shutdown()` 关机，不是被超时杀死。

【待人工截图】`report/images/member_b/01-make-qemu.png`：`make qemu` 的 10 行 `100 ticks` 与关机提示。

### 5.3 评分脚本与 DEBUG_GRADE

- **方法**：`make grade QEMU=... DEFS=-Dsbadaddr=stval`，评分脚本本身未改。
- **结果**：三项检查均 OK，`Total Score: 30/30`（`make-grade.log`）。

评分脚本只检查 `++ setup timer interrupts` 和至少一行 `100 ticks`，不检查 10 次输出和关机，因此满分不能代替第 5.2 节的正常运行测试。

`DEBUG_GRADE` 分支的实际情况：

| 构建方式 | 行为 | 证据 |
| --- | --- | --- |
| `make grade`（当前脚本） | 顶层直接执行 `make`，未定义 `DEBUG_GRADE`；QEMU 输出 10 行 `100 ticks` 后关机 | `grade-qemu-out.log` |
| 手动 `DEFS+=-DDEBUG_GRADE` | 第 1 行 `100 ticks` 后输出 `End of Test.`，`panic("EOT: kernel seems ok.")`，进入内核监视器，不会关机，只能由外部结束 QEMU | `qemu-debug-grade.log` |

`grade.sh` 中会附加 `-DDEBUG_GRADE` 的是 `build_run()`，它只被 `run_test()`/`quick_run()` 调用，而这两处在当前脚本中已被注释掉。因此当前评分实际走的是正常模式。

【待人工截图】`report/images/member_b/02-make-grade.png`：`make grade` 的 30/30 结果。

### 5.4 GDB：初始化 CSR 与真实时钟中断

- **方法**：QEMU 以 `-S -gdb` 启动，GDB 只用硬件断点，脚本 `tools/member_b/gdb/timer_flow.gdb` 在每个阶段读取 CSR 并断言，失败即退出。
- **结果**：35 项断言全部通过（`timer-flow-gdb.log`）。

初始化阶段：

| 断点位置 | stvec | sie | sstatus | 结论 |
| --- | --- | --- | --- | --- |
| `kern_init` 入口 | 0x80200000 | 0x0 | 0x8000000000006000 | 入口未就绪，中断全关 |
| `pmm_init`（`idt_init` 后） | 0xffffffffc0200b04 = `__alltraps` | — | — | MODE = 0，`sscratch = 0` |
| `intr_enable`（`clock_init` 后） | — | 0x20 | 0x8000000000006000 | STIE 已置位，SIE 仍为 0，`ticks = 0` |
| `intr_enable` 返回后 | — | — | 0x8000000000006002 | 只多了 SIE 位，其他位不变 |

第一次真实时钟中断，按 `__alltraps → trap → interrupt_handler → __trapret → sret` 依次停下：

| 位置 | 关键观察 |
| --- | --- |
| `__alltraps` | `scause = 0x8000000000000005`（最高位 1，原因码 5）；`sstatus = 0x...6120`：SIE = 0、SPIE = 1、SPP = 1；`sepc = 0xffffffffc0200098` = `kern_init+68`，即 `while (1)` 空转指令 |
| `trap(tf)` | `tf->cause` = `scause`，`tf->epc` = `sepc` |
| `interrupt_handler(tf)` | 收到同一个 `tf`；此时 `ticks = 0` |
| `__trapret` | `ticks = 1`；`tf->epc` 仍为 0xffffffffc0200098 |
| `sret` 之后 | PC = 0xffffffffc0200098，`sstatus = 0x...6002`，SIE 恢复为 1 |

随后连续 5 次在 `__trapret` 停下，`ticks` 依次为 2、3、4、5、6，`scause` 都是 0x8000000000000005，`sepc` 都在空转指令。最后在 `print_ticks` 停下时 `ticks = 100`。这排除了“只响应第一次”和“重复计数”。

两个细节：

1. `trap()` 编译后是尾跳转 `j interrupt_handler`，`interrupt_handler` 返回时直接回到 `__alltraps` 中 `jal trap` 的下一条，也就是 `__trapret`。GDB 回溯中 `interrupt_handler` 的上一帧显示为 `__alltraps`，原因就在这里。
2. `sret` 后 `SPIE = 0`。这是 QEMU 4.1.1 的实现行为，成员甲已在其报告中分析，乙的断言只检查 SIE 和返回 PC。

【待人工截图】`report/images/member_b/03-gdb-timer-entry.png`：`timer-flow-gdb.log` 第 6–10 段（`scause`、`sstatus`、`sepc` 与 `sret` 后状态）。

### 5.5 隔离测试：其他中断分支

本实验只使能了 `sie.STIE`，也没有外设或核间中断源，其他原因码无法由真实事件触发。因此用 GDB 在 `intr_enable` 处停下（此时 `SIE = 0`，不会混入真实中断），在启动栈底部构造假的 Trap frame，以 inferior call 的方式直接调用 `trap(tf)`。这是模拟分发，不代表真实外设中断。

| `tf->cause` | 输出 | 结论 |
| --- | --- | --- |
| 0x8000…0000 ～ 0x8000…000b（跳过 5） | 11 行提示，依次为 User/Supervisor/Hypervisor/Machine × software，User/Hypervisor/Machine timer，User/Supervisor/Hypervisor/Machine external | 与 `IRQ_*` 定义一一对应 |
| 0x8000…000c、0x8000…0010 | 各打印一次完整 Trap frame，`cause` 分别为 0xc、0x10 | 未知中断走 `print_trapframe` |
| 0x0000…0001 | 无输出 | 最高位为 0，进入 `exception_handler` 的取指访问错误分支（丙负责，该分支为空），没有被误判为 S 态软件中断 |

每次调用后 `tf->epc` 保持 0x1234，`ticks` 始终不变（`dispatch-gdb.log`、`dispatch-serial.log`，共 17 项断言）。

【待人工截图】`report/images/member_b/04-dispatch-labels.png`：`dispatch-serial.log` 中 11 行中断提示与未知中断的 Trap frame。

### 5.6 对照实验：不重设时钟事件

指导书说明“不设置下一次时钟事件，系统只能收到一次时钟中断”。为验证这一点，在临时副本中删去 `clock_set_next_event()` 调用后运行：

- **结果**：仍然输出 10 行 `100 ticks` 并关机，但从启动到退出总共约 0.09 秒，正常版本约 10.2 秒（`experiment-no-rearm.log`）。
- **分析**：OpenSBI 在 M 态定时器到期时置位 `sip.STIP`，该位只有在下一次 `sbi_set_timer()` 时才清除。S 态不重设事件，STIP 就一直挂起，`sret` 一旦恢复 SIE 就立刻再次进入 Trap，形成中断风暴，而不是停止。
- **结论**：在本实验的 QEMU 4.1.1 + OpenSBI v0.4 环境中，`clock_set_next_event()` 同时承担“预约下一次”和“清除本次挂起”两项职责。缺少它的表现是中断风暴，而不是只触发一次。

## 6. 调试中遇到的实际问题

### 6.1 新工具链无法汇编 sbadaddr

- **现象**：基线代码在本机直接 `make` 失败：`kern/trap/trapentry.S:47: Error: unknown CSR 'sbadaddr'`（`toolchain-sbadaddr-error.log`）。
- **原因**：`sbadaddr` 是特权级规范 1.9 的旧名，1.10 起改称 `stval`。binutils 2.45 不再接受旧名；成员甲使用的 GCC 10.2 工具链仍接受，所以甲的记录中没有出现此问题。
- **处理**：`trapentry.S` 属于甲，乙不修改。构建时通过 `DEFS=-Dsbadaddr=stval` 在预处理阶段替换，评分脚本和 Makefile 不变。
- **待集成**：建议甲或丙在集成时把 `trapentry.S` 中的 `sbadaddr` 改为 `stval`，使代码在新旧工具链下都能直接构建。

### 6.2 修正中断提示后评分降到 10/30

- **现象**：第一次修改采用最直接的做法，为每个分支写一条正确的字符串。`make qemu` 正常，但 `satp` 地址从 0x80205000 变为 0x80206000。在临时副本中用该版本运行 `make grade`，`check_best_fit` 失败，总分 10/30（`bug-layout-shift.log`）。
- **原因**：评分脚本写死了 `satp virtual address: 0xffffffffc0205000`。`satp` 指向 `boot_page_table_sv39`，它位于按页对齐的 `.data` 段中。在本机工具链下，基线的 `.text + .rodata` 结束于 0x…2fd8，距页边界 0x3000 只剩 40 字节。新增 5 条字符串和相应代码后结束于 0x…30b8，越过页边界，`.data` 和页表整体后移一页。

| 版本 | .text | .rodata | 只读数据结束 | 页表地址 |
| --- | --- | --- | --- | --- |
| 基线 | 0x1f30 | 0x10a0 | 0x…2fd8 | 0x…205000 |
| 第一次修改 | 0x1f6c | 0x1140 | 0x…30b8 | 0x…206000 |
| 最终版本 | 0x1f10 | 0x1060 | 0x…2f78 | 0x…205000 |

- **修复**：不修改评分脚本和链接脚本，而是减少乙代码的体积。利用第 3.1 节的编码规则，用两张短字符串表和一个格式串代替 11 条整句，代码也只剩一处 `cprintf` 调用。最终只读数据结束于 0x…2f78，距页边界 136 字节，比基线还多 96 字节余量（`layout.log`）。
- **复测**：`make grade` 30/30，正常运行与 GDB 测试全部通过。
- **待集成**：这项评分检查依赖内核映像大小和编译器版本，成员丙在 `exception_handler()` 和 Challenge 3 中新增的代码同样会占用这 136 字节余量。集成后若 `check_best_fit` 失败，应首先检查 `layout.log` 中的段边界，而不是怀疑内存管理逻辑。

### 6.3 测试脚本自身的问题

以下问题出在乙编写的测试工具，不涉及内核代码，均已修正：

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 保存的 `make grade` 日志只剩最后几行 | `grade.sh` 把 QEMU 输出重定向到 `/dev/stdout`，当标准输出是普通文件时会重新打开并截断它 | 改为经管道写入日志 |
| GDB 报错要求目标程序提供 `malloc` | 把带引号的字符串作为自定义命令参数，GDB 需要在目标内存中分配字符串 | 先把表达式结果存入 `$ok`，再用 `echo` 输出标签 |
| `scause >> 63` 判断失败 | GDB 把 `$scause` 当作有符号数，算术右移得到 -1 | 先转为 `unsigned long` |
| 第一次放在 `sp - 0x400` 的假 Trap frame 被覆盖 | inferior call 本身在当前 `sp` 之下压栈，`print_trapframe` 调用链覆盖了该区域 | 改放在 8 KB 启动栈底部，并断言距 `sp` 超过 4 KB |

## 7. 与 OS 原理的对应

| 实验中的知识点 | OS 原理中的知识点 | 关系与差异 |
| --- | --- | --- |
| `scause` 最高位区分中断与异常 | 中断是异步事件，异常是同步事件 | 原理上二者的触发来源不同；RISC-V 用同一入口、同一寄存器的一位来区分。`sepc` 对中断指向被打断的指令，返回后接着执行；对异常指向出错指令，由处理程序决定是否跳过。乙的时钟分支因此不修改 `epc` |
| `__alltraps` 保存 Trap frame，`interrupt_handler` 只读写 `tf` | 中断处理前保存现场、处理后恢复 | 硬件只保存 `sepc`、`scause`、`sstatus` 中的少数状态，通用寄存器全部由软件保存。处理函数通过 `tf` 访问被打断程序的状态，不直接依赖寄存器 |
| `sstatus.SIE` 与 `sie.STIE` | 中断屏蔽：全局开关与分类型屏蔽 | 原理中常只讲“开/关中断”；RISC-V 分两级。进入 Trap 时硬件自动关闭全局开关，因此本实验的中断处理不会嵌套 |
| `stvec` Direct 模式 | 中断向量表 | x86 由 IDT 为每个向量提供入口；RISC-V Direct 模式只有一个入口，由软件按 `scause` 分发。x86 版 ucore 的每个向量入口压入向量号后也都跳到同一个 `__alltraps`，两者最终的处理结构相同 |
| SBI 定时器 + 每次重设 | 时钟中断是分时和调度的时间基准 | 本实验只做计数和输出。若硬件不清除挂起位、软件又不重设，会出现第 5.6 节的中断风暴，说明时钟中断需要软硬件共同维持节拍 |
| `ecall` 进入 OpenSBI 设置定时器 | 特权级与受控的权限提升 | S 态无权直接访问 `mtimecmp`，只能经 M 态固件代为设置，与用户程序通过系统调用请求内核服务是同一模式 |

## 8. 本实验未覆盖的原理

- **抢占式调度**：时钟中断目前只计数。完整系统会在时钟中断中检查当前进程时间片，到期后设置调度标志，在返回前切换进程。本实验没有进程，也没有在 Trap 返回路径上切换上下文。
- **设备中断处理**：外部中断分支只打印提示。完整系统需要通过 PLIC 识别中断源、调用对应驱动、向 PLIC 确认完成。本实验没有使能 `sie.SEIE`，也没有 PLIC 驱动。
- **用户态到内核态的切换**：当前所有 Trap 都发生在 S 态，`sscratch` 恒为 0，没有内核栈切换。来自 U 态的中断需要先换到内核栈再保存现场。
- **多核同步**：`ticks` 和 `print_cnt` 只在单个 hart 上修改，关中断就能保证互斥。多核下需要每核独立的定时器事件、原子操作或自旋锁，`local_intr_save()` 不能防止其他核同时访问。
- **中断嵌套与优先级**：处理期间 `SIE = 0`，所有中断都被推迟。实际内核可能在处理低优先级中断时重新打开中断，或把耗时工作推迟到下半部执行。

## 9. 乙的工作总结

1. 审查基线，确认时钟分支逻辑正确并保留；修正 5 个提示错误的中断分支，定义指导书提到但源码缺失的 `SHUTDOWN_AFTER`，删除重复的 `idt_init()`，更新乙负责代码中的学号标记，并把 x86 遗留注释改为 RISC-V 实际机制。
2. 发现并修复“修正提示导致页表地址后移、评分降到 10/30”的问题，最终只读数据离页边界的余量从基线的 40 字节增至 136 字节。
3. 编写可重复的测试脚本：正常运行 10 次输出并自行关机；`make grade` 30/30；GDB 在真实时钟中断上验证 `stvec`、`sie.STIE`、`sstatus.SIE`、`scause`、`sepc`、`ticks` 连续递增和完整返回路径（35 项断言）；隔离测试覆盖其余 11 个原因码、未知原因码和最高位区分（17 项断言）。
4. 通过对照实验说明不重设时钟事件会产生中断风暴，并核实当前评分脚本不会进入 `DEBUG_GRADE` 分支。
5. 记录两项需要集成时处理的问题：`trapentry.S` 中的 `sbadaddr` 与新工具链不兼容；评分脚本的固定 `satp` 地址对内核体积敏感。

## 10. AI 工具与 Prompt

| 成员 | AI 编程工具 | 底层模型 |
| --- | --- | --- |
| 蒋林瀞（乙，2413507） | Claude Code（桌面应用 Code 标签页） | Claude Opus 5.5 |

成员 B 的六项原始提示词见 [提示词记录](../prompt.md)。

## 附：证据文件索引

`code/tools/member_b/evidence/` 下：

| 文件 | 内容 |
| --- | --- |
| `run_checks-summary.log` | 一键测试结果汇总 |
| `build.log`、`qemu-normal.log` | 正常构建与运行 |
| `make-grade.log`、`grade-qemu-out.log` | 评分结果及评分时的 QEMU 输出 |
| `qemu-debug-grade.log` | `DEBUG_GRADE` 构建的运行输出 |
| `timer-flow-gdb.log`、`timer-flow-serial.log` | GDB 真实时钟中断验证 |
| `dispatch-gdb.log`、`dispatch-serial.log` | GDB 隔离分发测试 |
| `layout.log`、`bug-layout-shift.log` | 段布局对比与 10/30 问题复现 |
| `experiment-no-rearm.log` | 不重设时钟事件的对照实验 |
| `toolchain-sbadaddr-error.log`、`versions.log` | 工具链问题与版本 |
