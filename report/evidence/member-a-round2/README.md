# 甲第二轮验证记录（2026-10-15）

工作分支 `member-a/report-gdb`，基于 `lab2` 的 `cac93ad`，代码改动只有两处：`buddy_test.c` 新增预期 panic 用例 6，`check_a.py` 增加对应的期望信息。所有运行都显式使用 QEMU 4.1.1（见 `context.txt`）。日志中的本机主目录已规范化为 `~`。

| 检查 | 结果 | 日志 |
| --- | --- | --- |
| 标准 `make -j2` | BUILD_EXIT=0 | `make-standard.log` |
| 标准 `make qemu` | Best-Fit 自检和 SLUB 测试通过 | `make-qemu-standard.log` |
| `make grade` | 20/20 | `make-grade-standard.log` |
| `check_a.py` | 12/12：Page Init ×3、Buddy ×3、预期 panic 用例 1–6 | `check_a.log`、`check_a/` |
| `debug_a.py` | 客户机 Sv39 核对及 7 个 Page Init 注入用例全部符合预期 | `debug_a.log`、`debug_a/` |
| 补充的 Page Init 注入 | base=`0x80300000` 时 panic，信息为 `invalid DTB memory range`；base=`0x80100000` 时 `pages[0..255]` 全部 Reserved，官方和独立布局检查 PASS | `page-init-extra/` |

补充注入复用了 `tools/debug_a.py` 中的 `run_case()`，没有修改该脚本。`gdb-page-init-base-below-kernel` 第一次运行时，脚本最后一条命令是 `continue`，GDB 等到 20 秒超时才退出（GDB_EXIT=124，检查本身已打印 PASS）。之后改为停在 `slub_init` 再 `detach`，重新运行得到 GDB_EXIT=0。目录中保存的是第二次的日志，第一次的日志已被同名文件覆盖。
