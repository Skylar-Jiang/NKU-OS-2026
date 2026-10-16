# 甲：宿主 GDB 调试 QEMU 4.1.1 的地址翻译（2026-10-15）

这是甲本人在本机独立运行的记录，不是丙的日志。分析见 `report.md` 第 10.3 节。

| 文件 | 内容 |
| --- | --- |
| `run.sh` | 一键复现：在 `code/` 下执行 `make` 后运行 `sh ../report/evidence/member-a-qemu-host-gdb/run.sh` |
| `host.gdb` | 宿主 GDB 脚本：在 `riscv_cpu_tlb_fill`、`get_physical_address`（M/Bare 分支、PTE 读出、叶子计算）、`write_satp`、`helper_tlb_flush`、`tlb_flush`、`get_page_addr_code`、`tlb_set_page` 处设置条件断点 |
| `guest.gdb` | 客户机 GDB 脚本：停在 `kern_entry`（Bare）、`kern_init`（Sv39）和 `sd ra,8(sp)`，用于对照 |
| `host-gdb.log`、`guest-gdb.log` | 第二次运行的输出，报告引用这一次 |
| `run1-host.gdb`、`run1-host-gdb.log`、`run1-guest-gdb.log` | 第一次运行。调试器读内存触发的页表遍历用完了计数上限，因此改进了脚本；原始输出保留不改 |
| `environment.txt` | QEMU、宿主 GDB 和客户机 GDB 的版本与来源 |

宿主 GDB 是从 Ubuntu 官方 deb 包解压到 `~/opt/gdb-12.1-ubuntu/` 的，没有安装到系统。QEMU 使用 `~/src/qemu-4.1.1/build` 中带符号的 `-O2` 构建，不是 `--enable-debug` 构建。

逐行单步（`--enable-debug` 构建）、数据访问 TLB 命中（TCI 构建）的记录见 [`../member-a-qemu-step/`](../member-a-qemu-step/README.md)。
