# Lab2 丙负责部分：First-Fit 与 QEMU/GDB 实测

## 范围与基线

依据团队仓库 `Skylar-Jiang/NKU-OS-2026` 的 `lab2` 分支（基线提交 `6c8b613c5ce7412dc3341d08c1e61b010b179f9c`）。本记录仅包含 First-Fit 分析、真实内核运行和 QEMU 源码级调试；未修改内核源码。

## First-Fit 客户机调试

在真实内核的 `default_alloc_pages(n=5)` 入口设置客户机 GDB 断点，实际得到：

- 调用栈：`default_alloc_pages` ← `default_check` ← `check_alloc_page` ← `pmm_init`。
- 请求 5 页，分配前 `nr_free=31929`；函数返回后 `nr_free=31924`，恰减少 5 页。
- 返回描述符为 `0xffffffffc020f318`。Page 描述符区起点对应物理地址 `0x80207000`，每个 `struct Page` 为 40 字节；返回描述符在数组中的偏移为 `0x8318 = 839 × 40`。它对应的物理地址为 `(nbase+839)×0x1000 = (524288+839)×0x1000 = 0x80347000`，与 First-Fit 从唯一空闲块首部分配相符。

对应实际输出见 [`first-fit-gdb.log`](../evidence/member-c/first-fit-gdb.log) 和 [`first-fit-serial.log`](../evidence/member-c/first-fit-serial.log)。这些记录中的本机文件路径已规范化，数值、函数调用栈和串口输出保持不变。

另在课程 QEMU 4.1.1 下运行分支提供的 Page Init/First-Fit 启动检查，64 MiB、128 MiB、256 MiB 均出现预期 PASS 标记。摘要与串口记录见 [`a-check-summary.txt`](../evidence/member-c/a-check-summary.txt) 及 [`page-init-64M.log`](../evidence/member-c/page-init-64M.log)、[`page-init-128M.log`](../evidence/member-c/page-init-128M.log)、[`page-init-256M.log`](../evidence/member-c/page-init-256M.log)。这不替代团队标准构建或官方评分。

## QEMU 4.1.1 宿主 GDB：Sv39 地址转换

用带调试信息的 QEMU 4.1.1 构建版启动上述内核，在宿主 GDB 对 `riscv_cpu_tlb_fill()`、`get_physical_address()` 和 PTE 读取后的源码位置设置断点。实际观察到：

1. `riscv_cpu_tlb_fill()` 的调用类型为 `MMU_INST_FETCH`，`mmu_idx=1`（S 态），访问虚拟地址 `0xffffffffc02000d8`。调用栈经过 QEMU 的 `tlb_fill()` 和 `get_page_addr_code()`。
2. `get_physical_address()` 中 SATP 为 `0x8000000000080205`，MODE=8，即 Sv39；根页表物理基址为 `0x80205000`。
3. 根级 VPN 索引为 `0x1ff`，PTE 物理地址为 `0x80205ff8`，读取值为 `0x200000cf`。`i=0`、`ptshift=18`，该 PTE 是根级 1 GiB 叶映射，遍历在第一层结束，并非每次都走完三级页表。
4. QEMU 得到物理页基址 `0x80200000`；页内偏移 `0xd8`，故完整物理字节地址为 `0x802000d8`。

完整断点输出与源码片段见 [`host-sv39-gdb.log`](../evidence/member-c/host-sv39-gdb.log)。本次实际验证了 QEMU TLB 填充慢路径中一次 S 态取指的 Sv39 转换；没有覆盖所有访存权限、异常类型或 TLB 命中路径。

## 构建状态

用当前分支实际执行标准 `make -j2`，仍在 `kern/mm/best_fit_pmm.c` 因 `free_list` 和 `nr_free` 未声明而失败，记录见 [`standard-build-current.log`](../evidence/member-c/standard-build-current.log)。因此本次没有运行 `make grade`；必须待 Best-Fit 实现补齐后再做团队标准验收。

## 调试器版本差异

本机 RISC-V GDB 10.2 执行分支的 `tools/debug_a.py` 时，在 GDB 命令的 `%llx` 格式处报 `long long not supported in printf`，整套脚本未能复现。该失败记录为 [`gdb-guest-translation.log`](../evidence/member-c/gdb-guest-translation-failure.log)。First-Fit 和宿主 QEMU 调试使用兼容命令另行完成；未修改团队分支中的脚本。
