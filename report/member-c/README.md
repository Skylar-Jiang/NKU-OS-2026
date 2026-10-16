# Lab2 丙负责部分

- [First-Fit 源码分析](First-Fit源码分析.md)
- [First-Fit 与 QEMU/GDB 实测（旧记录）](First-Fit与QEMU-GDB实测.md)
- [丙工作完成记录：TLB、MMU、QEMU 调试构建、思考题和集成验收](丙工作完成记录.md)
- 原始 QEMU/GDB 日志与可复现断点脚本：[`report/evidence/member-c/`](../evidence/member-c/)
- [宿主 GDB victim TLB 命中桌面截图](../images/member-c-victim-tlb-hit.png)

截至 2026-10-16，新的完成记录补充并覆盖旧记录中“宿主 GDB/TLB 路径未完成”和“Best-Fit 尚未集成”的历史状态。旧记录现已标为 2026-10-14 的阶段快照，其构建失败和未运行 grade 的描述不代表当前结果；最新集成构建与 20/20 评分以完成记录中的原始日志为准。

GDB 脚本中的 QEMU 源码、build 目录和内核镜像路径来自实际运行环境；在其他环境复现时，应先按本机路径调整脚本。
