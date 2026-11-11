# Lab4 C 部分交付

成员：马传志，2411788。基线为本次提供的 lab4.zip（包内目录 code）。

本分支的 code/ 为本次 Lab4 源码及 C 部分实现；A/B 模块和旧实验继承尚未完成。

## 已完成

- cpu_idle、schedule、proc_run、switch_to，以及 context 布局的编译期检查。
- QEMU 4.1.1 下 M 态和 S 态各 12 项独立运行检查。
- GDB 在 switch_to 内单步核对旧 SP/RA 保存、目标 SP/RA 恢复，四次比较均为 1。
- C 部分报告、规格、提示词整理及中断开关 Challenge 分析。

## 文件与合并

相对于原始 ZIP，仅修改 code 下的三个实现文件：

- kern/process/proc.c：只新增 function3 块中的两个函数和布局断言，function2 留给 B。
- kern/schedule/sched.c：保留原 wakeup_proc，新增 schedule。
- kern/process/switch.S：实现 switch_to。

本地另存有相对于原始 ZIP 的 member-c.patch。组内目录可能为 code 或 lab4，应用前检查路径；在对应代码根目录执行 git apply --check 后再应用。若 B 已修改 proc.c，不要覆盖整个文件，只合并 function3 块。补丁之外的 tests、report、evidence 是辅助交付。

接口约定：B 将 proc->pgdir 设为页表根物理地址（通常 boot_pgdir_pa），不是内核虚拟地址，也不是完整 satp；idleproc 不挂入 proc_list；普通线程挂入 proc_list；context.sp 必须 16 字节对齐；首次运行 context.ra 指向 forkret。B/A 仍拥有线程创建和页表映射实现。

## 复现

在 Ubuntu 中切换到本目录，执行：

```bash
bash code/tools/member-c/tests/run.sh
bash code/tools/member-c/tests/gdb.sh
```

脚本使用本机已有工具链和 /home/mcz/riscv/qemu-4.1.1/bin/qemu-system-riscv64。更换电脑时修改脚本中的工具路径。GDB 脚本使用本机回环端口 12444，只结束它自己启动的 QEMU。

## 验证边界与剩余事项

独立测试链接的是实际提交的三个 C 部分源文件；测试用静态 PCB/线程栈代替 B 的创建过程，S 态测试用私有恒等映射代替 A 的页表初始化。它验证实际切换代码，但不验证 forkret/forkrets 首次运行链、真实时钟抢占或完整内核启动。未声称完成正式 make grade。

当前完整编译首先报 best_fit_pmm.h 缺失，这是包内旧实验继承内容尚未补齐；A/B 和 Lab3 继承位置也尚未完成。Makefile 调用 tools/inject.py，但 ZIP 内没有该文件。需补齐匹配的官方脚本后才能完成正式评分，不应直接打印 PASS 或替换评分逻辑。

下一轮合并后须运行 make、make qemu、make grade，并验证 idle -> init 的正常首次运行链。当前 do_exit 仍是原包的 panic 占位，不属于本次 C 的修改范围。
