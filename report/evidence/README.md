# 验证证据索引

这些是实际命令输出及复现用 GDB 命令，不是 Prompt，也不是截图。

- `final-a-check.log`、`a-check-summary.txt`：最终独立构建/QEMU 回归汇总。
- `build-*.log`、`page-init-*.log`、`buddy-*.log`：实际构建和串口输出。
- `a-module-build.log`：甲模块默认配置对象编译。
- `standard-build.log`：标准构建被 Best-Fit 骨架阻塞的实际输出。
- `make-qemu-buddy.log`：Makefile qemu 入口的实际运行；最终循环由 timeout 结束。
- `final-debug-a.log`、`gdb-guest-translation.*`：客户机 GDB 及 Sv39 核对。
- `gdb-page-init-*`：明确标识的 GDB 输入注入、边界测试。
- `initial-*.log`：修复或调整前保留的真实失败证据。
- `gdb-qemu7-startup-diagnostic-serial.log`：未完成的 QEMU 7 启动诊断，不作为内核运行成功证据。
- `environment.txt`、`final-safety-check.txt`：工具环境和最终文件/Git 保护检查。

32 MiB 的失败日志保留作环境限制记录。256 MiB 的 Page Init 只报告独立上界截断检查，不声称官方 raw-end 检查通过。预期 panic 与正常回归分离。

`final-safety-check.txt` 是开发验收时的原始快照，其中未提交状态描述的是当时情况；原始验证输出不因本次提交而改写。

阶段性提交空白检查：git diff --cached --check 返回 2。其中 36 处提示来自原始课程源码中已存在的行末空白或末尾空行，938 处来自保留原格式的串口/调试日志（包括 CRLF）。已逐项核对来源；新增实现和测试脚本没有空白提示。为保持课程源码及真实输出不变，本次保留这些已知格式项。完整检查输出另保留在本地审计目录，不作为实验运行日志提交。
