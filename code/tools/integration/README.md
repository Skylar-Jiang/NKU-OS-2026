# Lab3 整合测试记录

在 `code/` 中执行：

```sh
export PATH=/home/lenovo/riscv-elf-toolchains/bin:$PATH
sh tools/run_integration.sh
```

本次使用 QEMU 4.1.1、riscv64-unknown-elf GCC 15.1.0 和已有 GDB，完整输出见 `evidence/all-tests.log`。

- A：默认构建、原评分、真实时钟 Trap、完整寄存器恢复、分配器独立测试和 64M/256M 内存配置通过。
- B：十次时钟输出后正常关机、35 项真实时钟调试断言、17 项隔离分发断言，以及 DEBUG_GRADE 检查通过。
- C：四种异常的真实 GDB 单步记录、取指和访存错误的构造现场检查、连续异常和寄存器恢复通过。
- 原评分脚本：30/30；完整测试最终输出 `LAB3_INTEGRATION_ALL_PASS`，退出码为零。

`tools/member_c/evidence/exceptions-gdb.log` 记录在修改 epc 的语句处单步取得的地址差值。兜底异常采用构造 Trap frame 的处理函数测试；四种异常和连续异常使用真实 RISC-V 指令。

测试工具的整合调整包括：每次运行独立保存 A 的输入快照；B 根据实际观测的时钟计数检查相对增量；C 使用当前 GDB 支持的原生命令。正式内核和评分标准保持各自的功能要求。
