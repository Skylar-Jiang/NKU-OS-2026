# Lab 2 三人代码集成验收记录（2026-10-15）

本目录只存放本次集成验收实际运行产生的日志，不替代各成员已有的 `report/evidence/` 记录，也不是最终报告。日志中的本机主目录已规范化为 `~`，其余输出未改动。

## 测试对象与环境

| 项目 | 内容 |
| --- | --- |
| 集成分支 | `origin/codex/lab2-integration` @ `0d468f5`（含甲 `6c8b613`、乙 `19f9c7a`、丙 `f646930`） |
| 合并前验收位置 | 独立临时 worktree，detached HEAD `0d468f5` → `pre-merge-isolated/` |
| 合并方式 | 本地 `lab2` 由 `6c8b613` Fast-forward 到 `0d468f5`，无冲突、无新增提交 |
| 合并后复验位置 | 正式 `lab2` worktree → `post-merge-lab2/` |
| QEMU | `~/opt/qemu-4.1.1/bin/qemu-system-riscv64`（4.1.1，显式指定；系统默认 `/opt/qemu` 为 7.0.0，未使用） |
| 工具链 | riscv64-unknown-elf-gcc 15.1.0，riscv64-unknown-elf-gdb 16.3.90，GNU Make 4.3，Python 3.10.12，WSL2 |

完整版本信息见 `pre-merge-isolated/environment.txt`、`post-merge-lab2/context.txt`。

## 配置区分

- **标准配置**（`make`）：`best_fit_pmm_manager`；`kern_init()` 在 `pmm_init()` 后调用 `slub_init()`、`slub_test()`。
- **官方评分配置**（`make grade`）：在临时副本中由 `tools/inject.py` 注入测试，只检查 Page Init 与 Best-Fit；评分脚本未修改。
- **甲的独立配置**：`LAB2_PROFILE=page-init`（First-Fit + Page Init 测试）与 `LAB2_PROFILE=buddy`（Buddy + 测试，含 5 个预期 panic 用例）。
- `tools/check_a.py`、`tools/debug_a.py` 默认会覆盖 `report/evidence/` 下的旧日志，本次均以 `--logs` 改写到本目录。

## 结果

| 检查 | 合并前（隔离） | 合并后（lab2） | 日志 |
| --- | --- | --- | --- |
| `make -j2` 标准构建 | BUILD_EXIT=0 | BUILD_EXIT=0 | `*/make-standard.log` |
| `make qemu`（128M）：Best-Fit 自检 + SLUB | `check_alloc_page() succeeded!`、`slub_check() succeeded!` | 同左 | `*/make-qemu-standard.log` |
| 标准内核 64M / 128M / 256M 启动 | 3/3 通过，无 panic | — | `pre-merge-isolated/standard-*.log` |
| `make grade` | Page Init OK，Best Fit OK，**20/20** | **20/20** | `*/make-grade-standard.log` |
| `check_a.py`（Page Init ×3、Buddy ×3、Buddy 预期 panic ×5） | 11/11 | 11/11 | `*/check_a.log`、`*/check_a/` |
| `debug_a.py`（客户机 Sv39 核对 + 7 个 Page Init 注入用例） | 全部通过 | — | `pre-merge-isolated/debug_a*` |
| SLUB 在 First-Fit / Buddy 配置下完整启动 | 均 `slub_check() succeeded!` | — | `pre-merge-isolated/profile-*-full-boot-128M.log` |
| 补充 GDB：SLUB 测试前后 Best-Fit 空闲链表 | 单块 idx=839，31929 页，前后一致 | — | `pre-merge-isolated/gdb-standard-freelist.*` |

64M 以下内存未测试；256M 时内核按 `PHYSICAL_MEMORY_END` 截断到 `0x88000000`，与甲原有记录一致。

## 审查结论

- 未发现阻断性错误，本次没有修改任何源码、测试或评分脚本。
- 非阻断事项：
  - `kern_init()` 在所有配置下都无条件运行 `slub_test()`。实测在三种页管理器上都能通过，但独立配置的输出中混入了 SLUB 结果；需要时由乙决定是否加配置开关。
  - `slub_free()` 根据页首 magic 判断对象是否属于 slab，对非 SLUB 页的任意指针只能尽力而为（简化实现可以接受，报告中应说明）。
  - `~/src/qemu-4.1.1/build` 带有符号，但 configure 没有使用指导书要求的 `--enable-debug`，属于优化构建。
