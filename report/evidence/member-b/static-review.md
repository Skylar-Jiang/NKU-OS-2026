# 成员乙静态核查记录（2026-10-15）

- 核查分支：`codex/lab2-integration`；核查前集成基线：`0d468f5bb8a07ce4da2404d3dcd63115e9507682`。
- 当前源代码改动只在 `code/kern/mm/slub_test.c`：补全交替释放/复用用例，并把内存耗尽用例改为先保留一个 1024 字节 slab、耗尽其余物理页、复用已有槽位、最终断言 NULL、清理全部对象和临时页后核对 `nr_free_pages()`。
- `best_fit_pmm.c` 相对集成提交 `19f9c7a795c77e7502322c0e34c0388b27309b2a` 无工作树差异；`basic_check()` 与 `best_fit_check()` 未被本次修改。
- `slub.c` 和 `slub.h` 无工作树差异。`slub_free()` 的源级可验证局限：一页 slab、七个固定大小类、线性空闲链表扫描、无锁、无错误码。
- `kern/init/init.c` 无工作树差异；当前调用顺序仍为 `pmm_init()`、`slub_init()`、`slub_test()`。
- `Makefile`、`tools/grade.sh`、`tools/inject.py` 与评分测试没有工作树差异。
- `git diff --check` 未报告空白错误。
- 构建生成的 `code/bin/`、`code/obj/` 均保持未跟踪，不纳入提交。

## `git diff --check` 输出

```text
（无空白错误。）
```

## 变更差异

详见 [slub-test-memory-exhaustion-fix.diff](slub-test-memory-exhaustion-fix.diff)。
