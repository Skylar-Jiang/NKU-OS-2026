# Lab5 丙部分 SPEC 与提示词

以下为按四段式整理的实现与测试需求，不是逐字历史对话；只包含丙负责的部分。

## 用户进程复制

```text
[PROMPT]
以官方 2026 Lab5 为基线，实现 do_fork、copy_mm、copy_thread、dup_mmap 和 copy_range。
迁移小组已完成的内存实现，不覆盖 A 的 Trap 文件或 B 的加载和退出函数。
[RELY]
proc_struct、mm_struct、vma_struct、trapframe；alloc_proc、setup_kstack、setup_pgdir、
set_links、hash_proc、wakeup_proc、mm_create、mm_count、get_pte、page_insert、exit_mmap。
注意 set_links 已增加进程计数，Lab5 Trap 字段为 tval，所有定义以真实源码为准。
[GUARANTEE]
保持四个模块三官方函数原型。成功返回子PID，子现场a0为0；失败返回负错误码。
[SPECIFICATION]
Pre：父线程和用户 mm 有效，分配器及内核映射可用。
Post：子PCB、栈、现场、父子关系及PID登记正确；共享整个mm和独立mm路径明确。
Cases：内核线程、CLONE_VM、普通Fork、各阶段内存不足、部分复制后失败。
Requirements：在登记前完成可失败分配，按已获得资源逆序清理；PID与挂链在临界区完成。
Validation：用实际生产函数验证返回值、页号、引用、计数、列表和失败资源恢复。
```

## COW 与内核写入

```text
[PROMPT]
加入COW：普通Fork使用独立mm/页表共享物理页，写时再复制。保留普通立即复制配置。
[RELY]
PTE软件位、VM_WRITE、Page引用、页表权限、sfence.vma、copy_to_user和用户地址校验。
[GUARANTEE]
提供cow_handle_store_fault(mm,fault_addr)，成功0，非法或OOM返回负值；给A说明成功后EPC保持不变。
[SPECIFICATION]
Pre：只处理合法可写VMA上的用户COW映射，当前仅保证单hart非抢占内核。
Post：写者获得独占可写映射，其他进程数据保持原值；真正只读区域仍不可写。
Cases：父先写、子先写、单引用恢复、多次Fork、退出、部分复制失败、内存不足、内核copyout。
Requirements：父子都撤销写权限并刷新TLB；更新映射和引用必须一致；禁止直接写共享物理页。
失败后父项可合法保留单引用COW，不要求PTE位模式完全恢复，但要求数据及资源一致。
Validation：对照立即复制与COW，记录物理页号、权限和引用；真实写错误必须重试原指令。
```

## 专项测试和 Dirty COW

```text
[PROMPT]
用独立QEMU环境先验证C能完成的模块，不补写A/B实现。分析Dirty COW并做受控错误模拟。
[RELY]
真实pmm/vmm/proc代码，测试专用分配器及静态环境，现有GCC/QEMU/GDB；NVD漏洞记录。
[GUARANTEE]
提供普通Fork、COW、GDB及变异测试脚本和原始结果；明确替代组件及未覆盖范围。
[SPECIFICATION]
Pre：独立编译真实模块，测试接口与正式接口一致。
Post：正常实现通过断言；临时副本绕过copyout的COW检查时被同一隔离断言检出。
Cases：正常共享/分离、只读拒绝、引用归零释放、所有分配失败、store fault与EPC不变。
Requirements：变异代码仅保留于忽略目录；不宣称原Linux竞态已经复现，不虚构官方成绩或截图。
Validation：保留实际日志、GDB数值和COW状态图。完整13项及生产U/S切换待A/B合并后回归。
```
