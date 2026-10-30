# Lab3 甲部分：原始请求、SPEC 与协作记录

## 本轮原始用户请求

以下两条按用户在本轮会话中的文本原样保留：

```text
我是甲，你开始直接帮我做吧，帮我做完美一点
```

```text
lab2的代码可以上git上看看有没有最新的
```

用户此前提供的三人分工中，甲的范围为 Lab2 继承、SAVE_ALL 检查、RESTORE_ALL、__alltraps、__trapret、GDB 验证和 do...while(0) 分析。原请求没有规定的实现细节由本轮依据源码和指导书整理为下列设计约束。下列内容是本轮整理的 SPEC，不声称是此前聊天中逐字发送的历史 Prompt。

## 设计约束一：继承 Lab2 内存管理

```text
[PROMPT]
核对小组 GitHub 的 lab2 分支最新版本，继承其 Page Init 与 Best-Fit 核心算法，
补齐本次提供的 Lab3 源码中的内存管理空缺，并验证初始化、分配、拆分、释放和合并。

[RELY]
来源为 Skylar-Jiang/NKU-OS-2026 的 origin/lab2@8b06753。
RV64、页大小4096、DTB提供RAM范围；Page数组覆盖[nbase,npage)物理页号。
分配释放调用者遵守合法区间契约；内核是当前单hart实验配置。

[GUARANTEE]
内核、固件、描述符数组及对齐占用不进入空闲页集合。
Best-Fit选择最小足够块，等长按低地址；空闲块按地址有序，计数准确。
保留Lab3已有的local_intr_save/local_intr_restore包装。

[SPECIFICATION]
验证DTB范围和整数加法边界，按PHYSICAL_MEMORY_END限制管理范围。
检查描述符所需空间，先全部标保留，再初始化合法空闲区域。
只将空闲块头挂链；正确拆分、合并和更新头页标记及长度。
不复制Lab2无中断保护的包装函数，不改输入框架自检和原评分规则。
用独立位图参考模型核对3000次操作，并测试不同RAM配置。
```

## 设计约束二：Trap 入口与现场恢复

```text
[PROMPT]
逐项检查输入源码已有的RESTORE_ALL、__alltraps和__trapret。
修正发现的问题，保留正确实现，补充布局约束及实际寄存器恢复验证。

[RELY]
沿用给定SAVE_ALL、struct trapframe和trap(struct trapframe *tf)接口。
RV64下36槽、每槽8字节；当前内核栈有效且16字节对齐。
C层负责中断/异常处理及必要的返回epc调整。

[GUARANTEE]
trap()的a0指向当前Trap frame；C层返回后进入统一恢复路径。
从对应槽恢复sstatus、sepc和通用寄存器；原sp最后恢复。
通过sret返回Trap frame指定的位置和应有状态。

[SPECIFICATION]
不改变SAVE_ALL、结构体字段和函数签名。
注意s1/s2在写回CSR后还要恢复原通用寄存器值。
x0不必回写；scause/stval用于诊断，不作为常规恢复目标。
增加编译期布局断言；对恢复前人为改写GPR进行实际QEMU/GDB验证。
记录QEMU4.1.1的SPIE历史行为，不将其与较新实现混为一谈。
```

## 验证和修复记录

实际问题与原始错误输出分别保存在报告第7节和 `../code/tools/member_a/evidence/`。
本轮没有捏造 AI 对话、运行结果或错误修复经历。报告中的运行截图是原始日志在记事本内的原生截图。
