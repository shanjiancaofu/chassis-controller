# 文档索引

本目录按职责维护文档。开始新的开发对话时，不需要一次加载全部文档；先读取
[`current_status.md`](current_status.md)，再按任务范围读取对应的权威文档。

## 快速入口

| 任务 | 先读 | 需要时再读 |
| --- | --- | --- |
| 继续当前开发 | `current_status.md` | `roadmap.md`、相关设计文档 |
| 修改代码结构或任务边界 | `architecture.md` | `AGENTS.md` |
| 修改硬件、引脚或外设 | `hardware.md` | 当前 `.ioc`、`verification.md` |
| 修改 Bootloader、OTA 或 QSPI | `bootloader_and_ota.md` | `protocol/ota_canfd_protocol.md`、`verification.md` |
| 修改 CAN FD 线协议 | `protocol/canfd_protocol.md` | `protocol/ota_canfd_protocol.md` |
| 构建、烧录或板上验收 | `verification.md` | `_output/README.md` |
| CMake/Ninja 构建 | `cmake_build.md` | `verification.md` |
| 规划后续工作 | `roadmap.md` | `current_status.md` |

## 权威职责

| 文档 | 唯一职责 |
| --- | --- |
| `current_status.md` | 当前代码基线、已验证状态、未验证项和下一步；保持短小 |
| `architecture.md` | 软件分层、目录职责、依赖方向和运行模型 |
| `hardware.md` | 实际硬件、引脚、接线和外设资源 |
| `bootloader_and_ota.md` | Bootloader、Flash/QSPI 布局、OTA 状态机和恢复设计 |
| `roadmap.md` | 已完成阶段、后续阶段和明确不实施的范围 |
| `verification.md` | 构建尺寸、命令、实物证据和历史验收记录 |
| `../protocol/*.md` | 跨设备线协议和兼容规则 |
| `../_output/README.md` | 固件产物目录、命名和烧录地址 |
| `../AGENTS.md` | Codex/AI 修改、验证和文档维护规则 |

## 多对话工作方式

每个新对话建议只提供任务本身，并让执行者按以下顺序读取：

1. `AGENTS.md`。
2. `docs/current_status.md`。
3. 本索引中与任务直接相关的文档。
4. 当前代码和 `.ioc`，以代码和配置作为最终事实来源。

完成一轮工作后：

1. 更新受影响的权威文档，不把同一状态复制到多处。
2. 更新 `current_status.md` 的基线、验证状态和下一步。
3. 构建或实物证据只写入 `verification.md`。
4. 详细调试过程有长期价值时保留在 `verification.md`；临时聊天过程不写入文档。

## 合并与整理规则

- 合并分支或整理文档时，某一侧没有出现的内容不等于应删除。
- 删除段落前必须确认它已过期、与代码冲突，或已完整迁移到明确的新位置。
- 内容迁移后在原职责入口保留链接，避免信息失去可发现性。
- 冲突按“当前代码和 `.ioc`、线协议、职责文档、示例和历史记录”的顺序裁决。
- 不用最新结论覆盖历史实物证据；历史证据保留日期、版本和适用范围。
- `PASS` 只来自实际构建或板上证据，未执行的项目保持 `READY` 或 `NOT VERIFIED`。
