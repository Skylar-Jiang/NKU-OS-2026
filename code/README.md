# Lab3 中断和异常

## 编译

在 WSL Ubuntu 中执行，先将已有交叉工具链加入 PATH：

```sh
export PATH=/home/lenovo/riscv-elf-toolchains/bin:$PATH
make GCCPREFIX=/home/lenovo/riscv-elf-toolchains/bin/riscv64-unknown-elf-
make qemu GCCPREFIX=/home/lenovo/riscv-elf-toolchains/bin/riscv64-unknown-elf-
make grade GCCPREFIX=/home/lenovo/riscv-elf-toolchains/bin/riscv64-unknown-elf-
```

## 完整测试

```sh
export PATH=/home/lenovo/riscv-elf-toolchains/bin:$PATH
sh tools/run_integration.sh
```

- A：继承 Page Init、Best-Fit，检查 Trap frame 布局，使用独立副本执行分配器和完整寄存器恢复测试，包含不同 RAM 配置。
- B：检查初始化、时钟计数、关机、真实时钟 Trap，以及其他中断的隔离分发。
- C：在独立副本中启用三个测试宏，执行四项异常的真实 QEMU/GDB 单步调试、取指和访存错误的构造现场测试、连续异常及寄存器恢复测试。

正式 `kern_init()` 保持正常启动流程。测试脚本只在独立副本注入临时测试入口。

测试日志分别保存在 `tools/member_a/evidence/`、`tools/member_b/evidence/` 和 `tools/member_c/evidence/`。

## 整合来源

- A：`Skylar-Jiang/NKU-OS-2026` 的 `lab3`，提交 `056ad6a3043e2bd1c87daa3e4607567745ae2f1b`。
- B：同仓库 `feature/timer-interrupt`，提交 `8a914e5e32afaf84212001eefc2401a43ae5d877`。
- C：本地已经完成的异常处理、三个宏控制的临时测试及 GDB 截图。

整合时保留 C 的异常处理，采用 A 的内存管理和布局断言、B 的中断分发及初始化顺序。Trap 入口读取 `0x143`（`stval`）以兼容现有工具链。B 的隔离测试同时检查 C 对取指错误的现场输出；课程评分脚本保留原检查规则。

## 报告

`../report/` 保存本次报告和提示词的 Word 文件及对应 Markdown；`images/` 保存图片，`materials/` 保存 A、B 的原始材料。三人的提示词分别收录各项原始任务。
