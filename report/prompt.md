# Lab6 成员 C：任务与提示词记录

成员：2411788 马传志。以下区分用户真实任务文本与实施后整理的四段式 SPEC；整理部分不是聊天逐字导出，不将整理过的规格冒称为原始提示词。

## 1. 本次原始请求

用户在提供官方手册、源码 ZIP 和三人分工后要求：

> 你看看lab6的c部分你能完成么，完成的话直接就帮我做完就好

其中 C 的任务范围为：恢复 sched.c 调度框架；实现 proc_stride_comp_f、stride_init、stride_enqueue、stride_dequeue、stride_pick_next、stride_proc_tick 和 stride_sched_class；使用官方斜堆，处理优先级、零值、回绕；用 priority.c 验证份额；接入 RR、Stride、FIFO、SJF、CFS；设计同条件多轮性能测试，测量等待、周转、响应、CPU 分配、公平性与调度开销；维护集成测试入口。共享调度文件和 Makefile 归 C 维护，A/B 各自算法文件不覆盖。

## 2. 调度框架与 Stride 的完整实施 SPEC（整理版）

### PROMPT

以本次 2026 官方 Lab6 框架为基础，补齐 sched.c 中前序 schedule，完成 default_sched_stride.c 的 //lab6 function3，实现并定义官方要求的全部函数与 stride_sched_class。按三人分工接入 A 的 FIFO/SJF 和 B 的 RR/CFS。保留老师任务标记，将本人负责部分的 YOUR CODE 改为 2411788。不得拿其他 uCore 整体替换框架，不覆盖 A/B 文件。

### RELY

1. proc.h、sched.h、default_sched.h、skew_heap.h 的真实声明是接口依据。B 已初始化 rq、run_link、time_slice、lab6_run_pool、lab6_stride、lab6_priority，以及 CFS 字段。
2. 只有当前运行进程和 rq 外任务才可以安全修改排序键；运行队列操作由本地中断临界区保护，内核为单 hart。
3. schedule 的 pick_next 调用后立即 dequeue 被选中者。idle 不进入普通队列，睡眠或僵尸不重新排队。
4. 优先级可为 0 或 uint32 最大值；pass 可以跨越 0，但模序的活跃比较范围须在半圈以内。

### GUARANTEE

- schedule 完成当前任务状态判断、策略选择、移除、runs 更新、上下文切换及中断状态恢复；无候选者时回退 idle。
- Stride 以累计 pass 最小为准，通过官方斜堆维护；parent 指针、rq、proc_num 和移除节点状态一致。
- 零优先级按权重 1，步长不为 0；等 pass 选择可复现；有可解释的回绕语义。
- 每个时钟节拍扣减时间片，耗尽置 need_resched；保持已有请求，不产生负时间片。
- 新任务使用当前虚拟时间，落后的唤醒者不囤积无限信用；CPU 密集且权重稳定时长程份额近似权重比例。
- 同一目录可以用 SCHED=rr/stride/fifo/sjf/cfs 重建并启动，非法选择报错；不提供热切换。

### SPECIFICATION

- BIG_STRIDE=1<<20；step=max(1,BIG_STRIDE/(priority?priority:1))；较大权重会因整数精度饱和。
- 比较器必须返回斜堆能够识别的 -1/0/1，使用无符号模差判断回绕，等键按 PID。
- 入队时按需补充时间片、插堆并更新归属/计数。出队使用 skew_heap_remove 并清空脱离节点。
- pick_next 对堆根预记 step，然后由 schedule 立即移除，不能把加键后的根留在堆中反复选。
- 真实测试发现首个时间片中 setpriority 导致旧权重预记账偏差后，增加 run_queue 的 stride_running/stride_charge，并在同一运行者重新入队时按新权重校正；增加专门回归检查。
- sched_select.h 仅在编译/启动时选择；Makefile 的配置戳使策略、观测开关或 DEFS 改变后重编译 sched.o。
- 保持官方 RR、Stride 测试不变并执行 make grade；另用 ASan/UBSan 检查队列结构、错误操作、回绕、极值与 7000 次加权调度。

## 3. 统一性能评估 SPEC（整理版）

### PROMPT

编写同一套真实用户态程序，在五种完整内核策略下分别运行 CPU 等量、不同长度、不同权重和长短 Burst 主动 yield 的负载，每种重复三轮。保留所有真实日志和逐进程指标，生成 CSV 与图表。不能用预先填写的数据替代运行，不能把图表称为终端截图。

### RELY

五种算法及前序内存、进程、Trap 已整合。QEMU 4.1.1 和交叉工具链使用现有安装，不升级环境。运行时不改变算法。所有测试副本放在 mktemp 内，正式 A/B 文件不插桩。

### GUARANTEE

同一工作负载、编译与模拟器条件下进行真实 fork/exit/wait 与调度。测量定义、单位、观察窗口和边界明确；无缺失值插补，无虚构轮次。输出每进程 CSV、每次运行 CSV、均值与样本标准差、原始日志及独立图片。

### SPECIFICATION

- 固定循环工作量，不使用“等待也会消耗任务截止时间”的假 CPU 工作。
- BENCH=1 在 C 的调度框架中启用可关闭观测，记录首次就绪、首次选中、退出、就绪等待、CPU 占用区间、时间片与派发次数。
- 四类 × 五策略 × 三次 = 60 个启动，检查每次四个子进程结束、schedbench pass、init check memory pass。
- 比较使用 QEMU 指令计时模式 -icount shift=3；rdtime 按 virt 10 MHz 换算。明确这是虚拟时间，不是主机性能。
- 公平性在第一个工作进程结束时取共同窗口，使用按权重归一化的 Jain；不能用所有相同工作量最终完成后的 CPU 总量伪装权重公平。
- 决策开销仅覆盖选择区间和轻量探针，不能冒称完整页表/寄存器切换开销。
- 官方 priority.c 另在普通时间模式运行，保留 acc 和实际比例。Stride/CFS 的有限样本比例允许事先设定的 25% 误差，不强行修改输出凑整数。
- SJF 使用 A 的历史预测，不赋予它未来作业长度；yield 混合不是实际磁盘 I/O。

## 4. 集成与问题处理 SPEC（整理版）

### PROMPT

补齐阻塞 C 完整编译与测试的接口依赖，并复用队友已提交的检查验证整合后没有破坏已有模块。完成报告中的框架六问、Stride 份额解释和性能结论。

### RELY

独立模块已经由 A/B 实现。仅对 C 的调度/测试/构建文件工作；发现前序迁移缺少 print_pgdir 时，使用本组 Lab5 的自有实现迁入独立 debug 文件，不改写 A 的 pmm.c。

### GUARANTEE

官方源码比较和真实测试结果可追溯。B 的静态 alloc_proc 断言只在临时副本中插入；算法选择统一走 SCHED，不依赖旧测试脚本替换某一行源文本。没有未完成实现占位或伪造截图。

### SPECIFICATION

- 保留 A/B 的独立 Git 提交并以合并方式接入，不强推公共历史。
- 复跑 95 项 FIFO/SJF、121 项 RR/CFS、27 项 Stride 检查。
- RR/Stride/CFS 分别运行 B 的 b_proc 和 b_spin，共 6 次完整系统验证。
- 生成 report/report.md、report/prompt.md、report/images 下的图表与 code/tests/results 下的原始数据。
- 最终推送 lab6-member-c 供小组审阅；公共 lab6 的合并通过 PR 处理，不直接覆盖。

## 5. 真实 Bug 记录

- sys_pgdir 链接缺少 print_pgdir：以独立迁移文件补齐；并根据此 Lab6 框架把旧 PTE_COW 名称改为实际 RSW 位诊断。
- 首次用户 setpriority 的 Stride 预记步长偏差：按重新入队时最新权重校正，增加宿主机回归并观察 priority 输出。
- 并行构建期间扫描根目录导致 tar 报 file changed：改为明确复制源码目录，不忽略复制失败。
- 策略切换的旧对象风险：增加配置戳，并以真实不同策略的启动输出、行为与数据检验。

原始用户消息、工具调用和失败修复顺序保留在本次对话；本文件只对任务与实施规格作结构化整理，不捏造未发生的逐轮对话。
