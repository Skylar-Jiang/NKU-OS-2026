# 成员乙截图来源说明

这些 PNG 来自用户提供的 `实验二_乙.docx` 的 `word/media/`，没有生成或改写终端截图。
- 源文档 SHA-256：`e7e6146dceb3c277b16a7a0cba73e2ad6d7c024d7e1eb32253798d31480ebbf8`
- 2026-10-15 的重新构建、评分、QEMU 和 check_a.py 输出另存于 `report/evidence/member-b/`。
- 当前会话的计算机控制接口未列出原生 VS Code 窗口；因此未把日志渲染成伪终端图片。

| 报告文件 | DOCX 内原始媒体 | 实际内容 |
| --- | --- | --- |
| `cross-compile-success-history.png` | `image2.png` | 源报告中对应的终端或 GDB 画面 |
| `best-fit-alloc-breakpoint.png` | `image3.png` | 源报告中对应的终端或 GDB 画面 |
| `best-fit-stack-source.png` | `image4.png` | 源报告中对应的终端或 GDB 画面 |
| `slub-boundary-1025-gdb.png` | `image6.png` | 源报告中对应的终端或 GDB 画面 |
| `slub-8byte-gdb.png` | `image7.png` | 源报告中对应的终端或 GDB 画面 |
| `qemu-bestfit-slub-history.png` | `image8.png` | 源报告中对应的终端或 GDB 画面 |
| `tlb-satp-gdb.png` | `image9.png` | 源报告中对应的终端或 GDB 画面 |
| `tlb-fill-call-stack.png` | `image10.png` | 源报告中对应的终端或 GDB 画面 |
| `tlb-root-pte-values.png` | `image13.png` | 源报告中对应的终端或 GDB 画面 |
| `tlb-pte-physical-read.png` | `image14.png` | 源报告中对应的终端或 GDB 画面 |
| `tlb-set-page-arguments.png` | `image15.png` | 源报告中对应的终端或 GDB 画面 |
| `tlb-fill-return-true.png` | `image16.png` | 源报告中对应的终端或 GDB 画面 |

## 2026-10-16 补齐的 VS Code 截图

六张新截图及实际可见证据见 [本轮截图清单](本轮截图清单.md)。这些截图来自当前工程实际 VS Code 窗口的 CopyFromScreen，不是从日志生成的图片。完整构建、正式评分、QEMU、Best-Fit GDB、SLUB GDB、check_a.py 已在 VS Code 集成终端实际执行。原始日志在 `report/evidence/member-b/vscode-final/`。前文关于当前会话无法操作原生窗口的描述只适用于此前尝试，本轮已解决 DPI 坐标和焦点问题并取得有效截图。
