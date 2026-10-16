# 甲：用 --enable-debug 构建的 QEMU 4.1.1 逐行单步地址翻译（2026-10-15）

这是甲本人在本机独立运行的记录，分析见 `report.md` 第 10.3 节。

## 调试版 QEMU 的构建

按指导书的要求重新编译带调试信息的 QEMU，且**不执行 `make install`**，系统里的课程 QEMU（`~/opt/qemu-4.1.1`）保持不变。源码 `~/src/qemu-4.1.1` 已有一个用于安装的构建目录 `build/`，为了不清理它（指导书的 `make distclean` 会清掉），这里在新目录中做 out-of-tree 构建。configure 参数与指导书相同，额外的 `--python=python3 --disable-werror` 与原有构建保持一致：

| 目录 | configure 参数 | 用途 |
| --- | --- | --- |
| `build-debug/` | `--target-list=riscv32-softmmu,riscv64-softmmu --enable-debug` | 指导书要求的调试版：`CFLAGS -g`（不优化），`TCG debug enabled yes`；用于取指路径的逐行单步 |
| `build-debug-tci/` | 同上，再加 `--enable-tcg-interpreter` | TCG 解释器后端：每次客户机 load/store 都调用 C 函数 `load_helper`/`store_helper`，用于逐行单步数据访问的 TLB 查询（包括命中） |

两份构建的 configure 输出和编译摘要见 `build/`。两个二进制都能完整启动本仓库的标准内核（Best-Fit 自检和 SLUB 测试通过）。

## 自动复现（脚本）

在 `code/` 下执行 `make` 之后：

```bash
sh ../report/evidence/member-a-qemu-step/run.sh fetch   # 取指：TLB Miss -> Sv39 遍历 -> 填入 TLB -> 再次查找命中
sh ../report/evidence/member-a-qemu-step/run.sh data    # 数据：store Miss、store 命中、load 命中（TCI 构建）
sh ../report/evidence/member-a-qemu-step/run.sh refill  # 代码页被挤出 TLB 的原因
```

| 文件 | 内容 |
| --- | --- |
| `trace.py` | 宿主 GDB 的 Python 命令 `trace-fn`：从当前函数开始反复 `next`，每一步打印“函数 文件:行号 源码”，以及执行后值发生变化的关键变量；`next` 在被调函数的断点处停下时跟进去，直到起始函数返回 |
| `fetch-host.gdb` / `fetch-guest.gdb` | 取指实验的宿主、客户机 GDB 脚本 |
| `fetch-host-gdb.log` / `fetch-guest-gdb.log` | 取指实验的输出：[A] 一次 Miss 的完整逐行过程（`get_page_addr_code` → `tlb_fill` → `riscv_cpu_tlb_fill` → `get_physical_address` → `tlb_set_page_with_attrs`），[B] 随后一次命中 |
| `data-host.gdb` / `data-guest.gdb` | 数据实验的宿主、客户机 GDB 脚本 |
| `data-host-gdb.log` / `data-guest-gdb.log` | 数据实验的输出：[C] `sd ra,8(sp)` store Miss，[D] `sd ra,40(sp)` store 命中，[E] `ld a1, boot_hartid` load 命中 |
| `refill-host.gdb` / `refill-guest.gdb` / `refill-host-gdb.log` | 验证代码页被重新填入 TLB 的原因：在每次填入前打印 0 号槽位原来的内容（`run.sh refill`） |
| `run1/` | 每个实验的第一次运行。之后为了减少输出噪声，调整了 `trace.py` 的变量打印规则：函数入口只打印参数，局部变量等值变化后再打印。翻译过程和数值与正式日志相同 |

QEMU 在宿主 GDB 中作为子进程启动（`gdb --args qemu ... -S -gdb tcp::PORT`），效果等同于指导书的 `sudo gdb` + `attach`。这里不用 `attach` 是因为本机 `kernel.yama.ptrace_scope = 1`，非 root 用户不能 attach。

## 按指导书三个终端手动操作（可截图）

以下步骤与指导书“调试步骤详解”一一对应，只是用命令行参数覆盖 `QEMU` 变量，不修改 Makefile。

**终端 1：启动 QEMU 模拟器**

```bash
cd ~/oslab-lab2/code && make
make debug QEMU=$HOME/src/qemu-4.1.1/build-debug/riscv64-softmmu/qemu-system-riscv64
```

**终端 2：附加调试 QEMU 进程**

```bash
pgrep -f qemu-system-riscv64
sudo $HOME/opt/gdb-12.1-ubuntu/bin/gdb-host
```

```text
(gdb) attach <上面查到的 PID>
(gdb) handle SIGPIPE nostop noprint
(gdb) source /home/jianglinjing/oslab-lab2/report/evidence/member-a-qemu-step/trace.py
(gdb) break get_page_addr_code if (addr & ~0xfffUL) == 0xffffffffc0200000UL && env->priv == 1
(gdb) continue
```

**终端 3：调试 uCore 内核**

```bash
cd ~/oslab-lab2/code && make gdb
```

```text
(gdb) break kern_init
(gdb) continue
```

终端 2 会停在 `get_page_addr_code`（`kern_init` 所在页的第一次查找）。随后：

```text
(gdb) bt
(gdb) set scheduler-locking step
(gdb) break get_physical_address
(gdb) next                 # 逐行执行；也可以用 trace-fn 自动逐行并打印变量
(gdb) print/x addr
(gdb) print/x entry->addr_code
(gdb) continue             # 遇到 get_physical_address 断点后继续 next，并 print/x idx、pte_addr、pte、ppn、*physical
```

最后执行 `set scheduler-locking off` 和 `continue`，终端 3 会停在 `kern_init`，此时可以执行 `p/x $satp`、`x/4bx $pc`。

如果要单步数据访问，把终端 1 的 QEMU 换成 `build-debug-tci/` 中的二进制，终端 2 改用 `break store_helper if addr == 0xffffffffc0204fe8UL`（`dtb_init` 中的 `sd ra,40(sp)`）。

**建议截图（每张只说明一个结论）：**

1. 终端 2 在 `get_physical_address` 中 `next` 到 254 行后，`print/x idx`、`print/x pte_addr`、`print/x pte` 的输出（Sv39 第一级遍历和 PTE）；
2. 终端 2 在 `get_page_addr_code` 第二次命中时，`next` 从 1030 行直接到 1039 行（TLB 命中，不再调用 `tlb_fill`）；
3. 终端 3 的 `p/x $satp` 和 `x/4bx $pc`，与终端 2 中宿主指针处的字节对照。
