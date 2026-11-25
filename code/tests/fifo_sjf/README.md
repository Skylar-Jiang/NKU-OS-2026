# FIFO / SJF 专项测试

## 运行

在 WSL / Linux 中进入本目录，执行：

```sh
bash run.sh
```

Windows PowerShell 可直接执行：

```powershell
wsl -d Ubuntu-22.04 -- bash /mnt/d/NKU-OS-2026/lab6/code/tests/fifo_sjf/run.sh
```

需要已有的 GCC、Make 及 GCC 的 AddressSanitizer / UndefinedBehaviorSanitizer。
脚本编译独立测试程序，逐项输出 `expected`、`actual`、`PASS/FAIL`，并保存
`results.txt`。程序全部通过时退出 0，值不匹配时退出 1，内核断言失败时退出 2。
脚本保留程序退出码；编译失败也返回非零。也可以 `make test`，但 Make 会将
测试程序的非零退出码转换为其自己的失败退出码。

## 使用真实实现与最小隔离

- `test_main.c` 包含 `static void test_fifo(void)`、`static void test_sjf(void)`
  及统一的 `main`；通过五个 `sched_class` 回调调用实际调度类。
- Makefile 直接编译项目中的两个调度源文件，使用真实的 `sched.h`、`proc.h`、
  `defs.h` 和 `libs/list.h`；没有复制、抽取或重新实现调度算法和数据结构。
- 宿主编译时仅定义 `__riscv_xlen=64` 供项目头文件选择 64 位类型。
  `host_io.c` 单独处理 libc 输出、检查计数和 `__panic` 退出，避免 libc 与内核
  类型定义冲突；没有替换链表或调度器。
- 测试夹具将真实 `proc_struct` 清零、设置 PID 并初始化链表节点，代替尚未完成的
  `alloc_proc`。不链接其他进程、内存、Trap、调度框架或官方评分代码。
- 这是对生产 C 实现的宿主专项测试，不是 RISC-V 中断、上下文切换或完整内核验收。

## 覆盖

- FIFO：队列初始化、空队列、ABC 顺序、数量和归属、选择不出队、节点复位、
  中间节点移除及重新入队、连续节拍不抢占、不清除已有重调度标记。
- SJF：零状态初始估计 5、8/2/5 选择顺序、同估计按 CAB 入队顺序选择、
  选择不重排、出队保留估计、实际 3 个节拍后由 5 更新为 4、计数清零、
  零节拍保留估计、奇数和向下取整、估计正值、节拍累计及非抢占行为。
- SJF 边界：夹具将计数置于 `INT_MAX-1` 后调用实际 tick，检查达到上限并饱和；
  将历史字段置于 `UINT32_MAX` 后，检查平均值计算提升为 64 位，没有溢出。
  这些边界初值由夹具设置，不通过循环执行数十亿个节拍。

构建产物位于已有 Git 忽略规则覆盖的 `build/` 下，不接入顶层内核构建。

## 本次实际结果

2026-11-25，在 WSL Ubuntu-22.04 / GCC 11.4.0 下运行 `bash run.sh`：

```text
SUMMARY: checks=95 passed=95 failed=0
EXIT_STATUS=0
```

逐项预期值与实际值见 `results.txt`，AddressSanitizer / UndefinedBehaviorSanitizer
没有报告错误。未发现需要修复的算法问题，两个调度源文件保持原样。

另外使用 `build/reporter_failure.c` 故意调用一次 `check_value(..., 1, 0)`，
验证报告器确实返回退出码 1；该负对照不是算法失败，输出留在
`build/reporter_failure.txt`，不混入正常的 95 项测试结果。

仅新增本目录的测试与结果文件；未修改其他成员文件或官方评分脚本，
未执行全量 `make grade`，未生成额外回滚脚本或重复进行哈希验证，未 commit、push。
