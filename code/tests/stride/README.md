# C：Stride 验证与统一入口

从 code/ 执行 bash tests/run_all.sh host、grade 或 qemu。完整顺序执行用 all。

- run_host.sh：直接编译当前生产 Stride 文件，ASan/UBSan，27 项检查。I/O 和错误隔离夹具复用 A/B 已提交的支持代码；调度算法本身不替换。
- mutation.sh：先运行 run_host.sh，再在 build/ 的临时副本去掉首次改权的预记账校正。必须以指定回归断言失败退出，证明检查能发现该真实问题；不改正式源码。
- build.sh：现有交叉工具链下 make clean、make、官方 make grade。记录 tests/results/build.txt 和 grade.txt。
- ../benchmark/run_process.sh：RR/Stride/CFS 分别运行 B 的进程与 CPU 程序，6 次 QEMU 启动，临时副本内调用 B 的真实 PCB 断言。
- ../benchmark/run_qemu.sh：5 策略、4 负载、每种 3 次，另运行五次官方 priority。统一由 SCHED 选择，默认 BENCH=0 不启用采样。
- ../benchmark/analyze.py：只解析实际存在且通过不变量的日志，不补缺失记录，生成 CSV 和摘要。
- ../benchmark/plot.py：已有 matplotlib 环境下从 CSV 生成报告图；不会安装或升级软件。

默认 SCHED=stride。RR/FIFO/SJF/CFS 的来源保留 A/B 原始提交。这里只推 lab6-member-c，公共 lab6 通过 PR 审阅合入。

真实终端截图不由绘图脚本生成；报告图是标准统计图，已经标明 QEMU 虚拟计时口径。需要截图时在现场手动截取实际 make grade、priority 或统计脚本窗口。
