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

## 报告

`../report/` 保存实验报告 `report.md` 和提示词 `prompt.md`；`images/` 保存图片。
