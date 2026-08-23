# 仓库规则

## 范围

- 以当前仓库和当前 `.ioc` 为准，不依赖旧对话结论。
- 新对话先读取 `docs/current_status.md`，再按 `docs/README.md` 加载任务相关的权威文档。
- 修改范围只覆盖用户要求的功能，不创建推测性的模块或文档。
- 除非用户明确要求，否则不得提交或推送。
- 冲突按以下顺序裁决：当前代码和 `.ioc`、线协议、职责文档、示例和外部参考。

## 代码结构

- Application 的 CubeMX/CubeIDE 工程统一保留在 `cubemx/`，其中包含 `.ioc`、工程元数据、
  `Core/`、`Drivers/`、`Middlewares/` 和 `Application/User/`。
- 手写代码放入现有的 `app`、`boards`、`drivers`、`lib`、`subsys`、`kernel`、`tests`、
  `config` 或 `dts` 边界；产品装配文件、模块和 UI 位于 `app/`，通信位于 `subsys/communication/`，
  FreeRTOS runtime 位于 `kernel/freertos/`。
- 只有存在真实的职责或依赖边界时才创建目录或目标。
- 少量且只使用一次的逻辑优先留在调用方，不为此拆出辅助函数。
- 必须防御用户输入、外部协议、持久化数据以及硬件/资源边界；对硬编码常量或本地已保证
  的值不添加重复检查。
- 不提交 `__pycache__/`、`*.pyc`、测试可执行文件、IDE 工作区或构建产物等宿主机生成
  文件。`_output/` 下只跟踪 `README.md`。
- `cubemx/Drivers/` 和 `cubemx/Middlewares/` 视为供应商代码；仅在明确审查过的供应商补丁中修改，
  普通应用修改必须放在手写目录。

## 固件安全

- 启动和所有失败路径必须让四路电机 PWM 保持为零。
- ISR 必须有界；不得在 ISR 内解析协议、打印、擦写 Flash 或运行 PID。
- `control_task` 不得执行 Console、LCD、QSPI 擦写、RTC 显示或文本遥测。
- 危险目标板测试必须停止控制、保持零 PWM、独占资源并要求显式确认。
- CubeMX 文件只能在 `USER CODE BEGIN/END` 区域内手工修改。

## 验证

- 对修改运行范围最小且相关的构建或检查。
- 只有实际完成目标板测试后才能记录硬件结果。
- 仅在有证据时使用 `PASS`；否则使用 `READY`、`IMPLEMENTED` 或 `NOT VERIFIED`。
- 只更新承载该信息的权威文档。
- 构建和测试证据写入 `docs/verification.md`；不要在协议文档中重复硬件状态，也不能从
  源码审查推断硬件通过。

## 文档

- `docs/current_status.md` 保持简洁，只记录当前基线、已验证事实、待验证项和直接下一步。
- 合并或重组时必须保留信息；某一侧缺少内容不代表允许删除另一侧已有内容。
- 只有确认文档已过期、与当前代码/配置冲突，或已完整迁移到有链接的权威位置时才可删除。
- 保留带日期和版本范围的硬件证据；新结果可以补充或取代当前状态，但不能抹去有价值的
  历史证据。
