# 构建与实物验证

本文件只记录可复现的构建、联调步骤和实物结果。设计目标不在这里标记为 `PASS`。
当前开发摘要见 [`current_status.md`](current_status.md)；本文件保留带日期、版本和适用范围的
历史证据，整理或合并时不得因为最新摘要未重复这些内容而删除。

## 状态定义

| 状态 | 含义 |
| --- | --- |
| `BUILD PASS` | 指定构建配置成功 |
| `READY` | 初始化或测试入口存在，尚未完成实物验收 |
| `HARDWARE PASS` | 已按明确步骤完成实物验收 |
| `NOT VERIFIED` | 尚未验证或固件变化后需要回归 |
| `DEFERRED` | 已知尚未完成，按当前计划后置且不阻塞主线 |

## 已有基线

以下结果来自 2026-07 的开发板联调，后续修改相关驱动、安全逻辑或链接布局后需要回归。

| 项目 | 结果 | 依据 |
| --- | --- | --- |
| USART1 TX/RX DMA | `HARDWARE PASS` | 串口命令、响应和遥测双向工作 |
| RTC 读取 | `HARDWARE PASS` | LSE 计时连续 |
| RTC 掉电保持 | `HARDWARE PASS` | 仅保留备用电池约 5 分钟后时间连续 |
| QSPI JEDEC | `HARDWARE PASS` | `EF4017`，识别 8 MiB |
| QSPI 擦写 | `HARDWARE PASS` | 保留扇区 1 KiB DMA 写入和回读 |
| LCD SPI DMA | `HARDWARE PASS` | 封面和动态状态页显示正常 |
| LCD 四页状态显示 | `HARDWARE PASS` | 0.12.0 build1；四页切换、文字、Logo 和电量显示已由用户确认正常 |
| LCD UI 0.13.0 | `NOT VERIFIED` | 已随 0.13.0 OTA 上板并由串口确认 `lcd=DRAWING`；页眉页码指示、分组标签、内容区层次和配色仍待人工目视确认 |
| LCD UI 0.14.0 | `HARDWARE PASS` | UART OTA confirmed；LCD 驱动 `READY`，暗色工业仪表四页、颜色、文字和切页已由用户人工确认正常 |
| LCD/UI 分层 0.15.0 | `NOT VERIFIED` | Debug/Release、主机测试和同源 C 预览通过；尚未烧录目标板 |
| KEY 消抖 | `HARDWARE PASS` | 历史按键消抖已验证；PB8 四页循环已人工确认正常，PD3/PD4 尚未接线 |
| ADC 电压 | `HARDWARE PASS` | 11.96 V 电池，PA2 约 1.086 V |
| ICM45686 SPI/FIFO | `HARDWARE PASS` | 0.11.1；WHO_AM_I=0xE9，普通复位后 224 帧零解析/时间戳错误，100 Hz 周期 10 ms |
| 双编码器 | `HARDWARE PASS` | 前进同为正、后退同为负 |
| 轮式里程计 | `NOT VERIFIED` | 0.12.0 已上板启动并报告静态零位；车轮运动、时间对齐和落地几何校准尚未验证 |
| 双电机开环 | `HARDWARE PASS` | 方向沿用历史验收；0.9.1 运行期占空比测试确认自动停止 |
| 开环启动下限 | `HARDWARE PASS` | 约 12.22 V、架空轮；左右可靠下限约 3500/8499（41.2%） |
| 目标加减速限制 | `NOT VERIFIED` | 0.10.0 已实现每 10 ms 最多 5 counts/tick；尚未完成带负载阶跃验收 |
| 编码器异常保护 | `NOT VERIFIED` | 0.10.0 已实现异常增量 critical fault 急停；尚未注入异常脉冲 |
| 欠压保护 | `NOT VERIFIED` | 0.10.0 已实现低于 9000 mV 锁存并急停；尚未做可控欠压注入 |
| 急停 | `HARDWARE PASS` | PD2 触发故障并清零 PWM |
| IWDG 受控复位 | `HARDWARE PASS` | 复位后报告测试通过 |
| 外部 CAN FD | `HARDWARE PASS` | Jetson 与 STM32 三步握手双向通过 |
| 低速 PID 闭环 | `NOT VERIFIED` | 参数持久化和 6500 开环启动已验证；低速闭环、停车和稳定性重新纳入当前验收 |
| PID 参数持久化 | `HARDWARE PASS` | 0.9.0 build1，`pid left 210 310 1` 返回 SAVED/STORED sequence=1 |
| FreeRTOS 四任务运行字段 | `HARDWARE PASS` | 2026-08-18，0.3.0 build1 四任务均为 `RUNNING`，周期、栈余量、heartbeat 和运行次数可读 |
| 正式 UART 文本协议 | `HARDWARE PASS` | 2026-08-18，0.3.0 build1 的 `[LOG]`、`[RSP]`、四分区 `[TEL]`、错误响应、遥测和 CRLF 已实测 |
| HC-SR501 输入 | `NOT VERIFIED` | b16 已验证 60 秒预热和低电平零误计数；高电平、事件计数和不重复计数重新纳入当前验收 |
| Bootloader factory 启动 | `HARDWARE PASS` | DFU 烧录并校验组合镜像，普通复位后 LCD 进入 Application |
| UART OTA 主升级链路 | `HARDWARE PASS` | 2026-08-19，build22 已完成到 0.3.0、0.6.0、0.7.0、0.7.1、0.8.0、0.11.1、0.12.0、0.13.0 和 0.14.0 的真实 UART 传输、安装、TRIAL 和 CONFIRMED 确认 |
| OTA 回滚、断电恢复与 CAN FD 传输 | `NOT VERIFIED` | 尚未完成故障注入、回滚和真实 CAN FD OTA 验收 |

## 构建基线

### 原生平台化迁移完成度审计（2026-08-23）

按当前源码与 `docs/重构.md` Phase 逐项核对：Phase 1/4/5/6/9 已完成；Phase 2 部分完成；
Phase 3 已在本批完成，Phase 7/8 尚未完成。当前主要设备均已有 device/API，公共 DT API、
status-aware instance、controller/child 拓扑和 GPIO phandle-array 已构建验证；仍存在 UART/QSPI/IMU
静态全局状态和 app FreeRTOS critical section。
这些属于软件迁移剩余项，不改变既有硬件证据，也不构成新的目标板 `PASS`。

预计还需 1 个代码收口大批次；完成后另行执行 1～2 个目标板回归批次。

### DT/device data 第一批收口（2026-08-23）

公共 `devicetree.h` 已替代上层生成头依赖；生成器支持 chosen/alias/nodelabel/property、
status-aware instance 和 GPIO phandle-array。DTS 已移除 C handle 字符串与手工 phandle，并将
SPI2/LCD、SPI3/IMU、QSPI/Flash、ADC1/电源采样改为 controller/child 拓扑。power、button、
SR501、display 运行态已迁入各实例 `device->data`。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 120256 | 96 | 55368 | `BUILD PASS` |
| Release | 106544 | 96 | 55360 | `BUILD PASS` |

Kconfig 3 项、DTS 3 项、架构边界 3 项、OTA Python 13 项和真实 C LCD 预览均通过；
`git diff --check` 通过。Release 产物：

```text
application.bin=106648 bytes
payload_crc32=0xE60C022C
application.bin sha256=ea00ca5a6e0456cf0cc64912e3dfee173e5df45539e46d14317785d9dc4d27d0
app-v0.15.0-b1.ota=106712 bytes
ota sha256=686d4146f28065f8825a7c423182822498567d90bbbd12065211bcd11d19eac1
```

本批未烧录；电机、编码器、LCD、IMU、GPIO 和 OTA 目标板状态不因构建结果升级为 `PASS`。

### Kconfiglib engine migration（2026-08-23）

Kconfig 语义引擎已由仓库自研 parser/evaluator 切换为 vendored Kconfiglib `14.1.0`。现有
`Kconfig`、`prj.conf`、`CONFIG_*` 和生成文件格式保持不变。切换前后输出哈希一致：

```text
.config       af4c104c204c31dae6bc4bda06b69becea8d1caf2a3153655aff5891cb586a64
autoconf.h    bc146b1b1f2f7dac17750b31496fba1a491a822887381c6a29639816b03e6c7b
config.cmake  209a6fa4aa13af025a4a9ca8ed4de02cef9464c3b675c168f7061f9fd9f3faec
Release BIN   4ba6193c9ab28bf59da732cbccf313dc0321833ba2372f22a20d8423a3d369f2
```

新增测试覆盖当前配置、choice/条件 default、select/depends；Debug/Release、DTS、架构、OTA
测试和 LCD 预览均通过。本批不改变固件字节，不构成新的目标板硬件证据。

### CubeMX 初始化入口恢复（2026-08-20）

按仓库规则恢复 CubeMX 生成的 `Core/Src/main.c`、`gpio.c`、`dma.c` 及对应头文件，并重新纳入
Application CMake。Device Model 的 `EARLY/PRE_KERNEL_1/PRE_KERNEL_2` 初始化调用保留在
`main.c` 用户代码区，`POST_KERNEL/APPLICATION` 仍由 FreeRTOS 生成入口调用；CubeMX `.ioc`
继续作为时钟、GPIO、DMA、NVIC 和外设初始化的硬件配置源。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 115132 | 120 | 55144 | `BUILD PASS` |
| Release | 102584 | 120 | 55136 | `BUILD PASS` |

Release 镜像门禁报告 Flash `102704 / 491520`、RAM `55256 / 131072`；CubeMX/DTS 一致性检查和
`git diff --check` 通过。本轮没有烧录或目标板验证，硬件状态保持 `NOT VERIFIED`。

### Native Kconfig pipeline（2026-08-20）

配置生成器已拆分为 `tools/kconfig/{lexer,parser,model,evaluator,config,genconfig}.py`，
`tools/config/generate_config.py` 保留为 CMake 兼容入口。当前支持 `config/menuconfig`、
`menu/endmenu`、`if/endif`、`choice/endchoice`、`source`、`bool/int/hex/string`、
`default`、`range`、`depends on`、`select` 以及基本逻辑/比较表达式；生成结果位于
`build/.../generated/.config`、`autoconf.h` 和 `config.cmake`。

Kconfig 单元测试 4 项通过；当前 `config/Kconfig` + `config/prj.conf` 生成的配置符号与 ARM
Debug/Release 构建通过，13 项 OTA Python 测试通过。此项只验证主机解析和构建集成，没有目标板
硬件结论。

### Architecture boundary checker（2026-08-20）

新增 `tools/build/check_architecture.py`，在 CMake configure 阶段扫描 `app/`、`modules/`、
`communication/`、`subsys/`、`ui/` 和 `rtos/`，禁止直接包含 CubeMX/HAL 外设头、调用
`HAL_*`、使用 `*_HandleTypeDef` 或暴露 `hfdcan*`、`hspi*`、`htim*` 等硬件句柄。当前源码扫描
通过；架构检查单元测试 2 项、Debug/Release 构建和 OTA Python 测试 13 项通过。本轮没有目标板
硬件结论。

### UART / QSPI / watchdog / RTC-time driver boundary（2026-08-20）

新增通用 driver API：

```text
include/drivers/uart.h
include/drivers/flash.h
include/drivers/watchdog.h
include/drivers/time.h
include/drivers/rtc.h
```

UART 已通过 `struct device`、DTS chosen 节点和 STM32 adapter 接入；CubeMX `huart1` 只在
`boards/chassis_g474/board_devices.c` 与 UART STM32 driver 实现可见。UART protocol、Console、
Telemetry、OTA UART transport 已移除对旧 BSP 的直接依赖。QSPI、watchdog、RTC/time 的上层
调用也已改为通用 driver API，原 BSP 实现已移动到 `drivers/*stm32*`，协议和安全行为未改写。

Debug/Release 构建、架构检查、Kconfig/DTS/architecture 单元测试和 OTA Python 测试均通过；
本轮没有烧录或目标板硬件结论。

### STM32 driver relocation and device readiness（2026-08-20）

UART、QSPI、watchdog、RTC、time 的 HAL 适配源码已从旧 `bsp/*` 目录移动到
`drivers/*stm32*`；QSPI、watchdog、RTC、time 增加 DTS 节点和
`DEVICE_DT_DEFINE` 实例，通用 API 在访问前检查 `device_is_ready()`。旧 Application CMake
旧 Application CMake 的 BSP 源文件引用已移除。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 115940 | 120 | 55184 | `BUILD PASS` |
| Release | 103280 | 120 | 55176 | `BUILD PASS` |

镜像门禁报告 Debug Flash `116060 / 491520`、RAM `55304 / 131072`；Release Flash
`103400 / 491520`、RAM `55296 / 131072`。架构、Kconfig、DTS 和 OTA Python 测试继续通过。
本轮没有目标板硬件结论。

### Hardware Device Model migration（2026-08-23）

新增真实设备实例：

```text
drive0
left_encoder
right_encoder
power0
imu0
display0
flash0
watchdog0
rtc0
time0
gpioa/gpiob/gpioc/gpiod
buttons0
leds0
sr5010
estop0
```

motor/encoder 的公共操作现在通过 `motor_*`、`encoder_*` API 和 `const struct device *` 进入，
CubeMX `htim8/htim2/htim4` 只在 board device config 和 STM32 driver 内可见。WheelController
仍保留业务 port，但 port 内部只调用 generic motor API；编码器读取改为分别读取左右两个 device。
power sample 的 API 现在通过 `power0` device 访问；ICM45686 通过 `imu0` sensor device 访问，LCD 通过 `display0` display device 访问。`flash0/watchdog0/rtc0/time0` 已移除 dummy init，改为真实 API/vtable/init；`board_config.h` 及全部 `BOARD_*` 引用已删除。GPIO port 和 button/LED/SR501/E-STOP consumer 已使用真实 device API；DTS 使用 `gpios` phandle-array，中断路由通过 consumer/sensor/display API 分发。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 120708 | 96 | 55384 | `BUILD PASS` |
| Release | 107016 | 96 | 55376 | `BUILD PASS` |

架构、Kconfig、DTS、OTA Python 测试均通过；电机安全和编码器方向未因源码构建结果推断硬件
`PASS`，仍需目标板回归。

Release ELF 已重新生成 Application BIN 和 OTA 包：

```text
application.bin=107120 bytes
payload_crc32=0x191ECC45
application.bin sha256=4ba6193c9ab28bf59da732cbccf313dc0321833ba2372f22a20d8423a3d369f2
app-v0.15.0-b1.ota=107184 bytes
ota sha256=3de2c8e3faddb734c3d9232dc084b13f4209be754530d385f246627c0075f80f
```

该 OTA 包只完成主机打包和格式校验，尚未烧录，不能记录硬件 `PASS`。

本轮同时将 Application CMake 拆为 `chassis_vendor`、`chassis_drivers`、`chassis_components`、
`chassis_communication`、`chassis_subsys`、`chassis_modules`、`chassis_ui`、`chassis_kernel`、
`chassis_app`、`chassis_rtos` 和 `chassis_target_tests` 静态目标；顶层只负责依赖聚合和最终链接。

### Native Devicetree metadata and bindings（2026-08-20）

`tools/dts/generate_devicetree.py` 现已生成 `chosen`、`aliases`、`__symbols__` 节点标签和
phandle-array 元数据；`tools/dts/verify_bindings.py` 根据
`firmware/application/stm32g474/dts/bindings/*.yaml` 校验每个启用设备的 `compatible`、必需
属性和基础类型。CMake 配置阶段已接入 `dtc -@`、CubeMX/DTS 一致性检查和 bindings 检查。

DTS 单元测试 2 项、Kconfig 单元测试 4 项、Debug/Release 构建和 OTA Python 测试 13 项均通过。
本轮只验证生成、解析和构建集成，没有目标板硬件结论。

### 0.15.0 全仓库依赖边界收敛（2026-08-20）

本轮在 0.15.0 首版基础上继续移除 CAN ISR 协议解析和上层 HAL/CubeMX 依赖，拆分 Console、
OTA 维护、IMU orientation 与 LCD presenter。FDCAN ISR 仅执行一次有界 HAL 收帧、固定队列写入
和原子计数；握手、运动命令与 OTA 解码均由 `service_task` 推进。依赖扫描确认
Application 的 `app/communication/components/subsys/modules/rtos/ui` 不再直接包含
`main.h`/CubeMX 外设头、调用 HAL 或定义 HAL 回调。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 114020 | 120 | 55128 | `BUILD PASS` |
| Release | 101636 | 120 | 55120 | `BUILD PASS` |

```text
application.bin=101764 bytes
payload_crc32=0x447F9AC9
bin_sha256=ec730a156fe12b23a46a0e5b81d872cd2d355a4d718c8f473f8c255c9ac83a2f
app-v0.15.0-b1.ota=101828 bytes
ota_sha256=a7553b9b928de830412cd92b9b462acfdc88e6bb1f8938f5f0439d6c660bea41
```

Application C 主机测试增加 CanTransport 后共 10 个，全部使用
`-std=c11 -Wall -Wextra -Werror` 编译并运行通过。新增测试覆盖 PING/PASS 握手、运动控制帧、
OTA 帧分发、bus-off 会话撤销和延迟恢复；既有 CommandManager、ICM45686、IMU fusion、odometry、
OTA metadata、UART arm guard、parameter record、UART protocol 和 wheel controller 测试继续通过。
Bootloader core 主机测试通过，OTA Python 13 项通过，真实 `lcd_ui.c` 预览重新生成，
`git diff --check` 通过。

本轮未烧录，不从构建、主机测试、依赖扫描或 PNG 预览推断 FDCAN、IMU、四任务、LCD、OTA 或
异常零 PWM 已在 0.15.0 目标板通过；上述项目保持 `NOT VERIFIED`。

### 0.15.0 分层重构构建与产物（2026-08-19）

本节保留 2026-08-19 首轮分层产物证据；同版本当前源码和 OTA 已被上方 2026-08-20 边界收敛
结果取代，不得再使用本节哈希烧录。

本轮拆分 LCD UI/BSP、Application 状态采集、RTOS 回调和轮控电机端口，并加入异常入口零 PWM。
预览器以主机 `cc -Wall -Wextra -Werror` 编译真实 `lcd_ui.c`，生成四页和汇总 PNG。执行
Debug/Release 配置与构建、从最终 Release ELF 重新生成 BIN 和 OTA：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 112988 | 120 | 54544 | `BUILD PASS` |
| Release | 100712 | 120 | 54536 | `BUILD PASS` |

```text
application.bin=100840 bytes
payload_crc32=0x72FEABE4
bin_sha256=fa9d47b50557b7dee7e5dffcddf7bf00d0feb9c5c3be4b41f3dca16b55591ef1
app-v0.15.0-b1.ota=100904 bytes
ota_sha256=29bbce931676256c6f174b7cb03dd2f68ce7155c68d89c8583461a49db23bbd9
```

Application C 主机测试共 9 个，均以 `-std=c11 -Wall -Wextra -Werror` 编译运行通过；覆盖
CommandManager、ICM45686、IMU fusion、odometry、OTA metadata、UART arm guard、parameter
record、UART protocol 和新增的 wheel controller 假电机端口。OTA Python 13 项通过。
`git diff --check` 通过，未生成需跟踪的 Python 缓存或宿主测试可执行文件。

该版本尚未烧录；不能从构建、主机测试或 PNG 预览推断 LCD、四任务、OTA 安装或异常零 PWM
已在目标板通过。

### 0.14.0 LCD UI 构建与产物（2026-08-19）

暗色工业仪表四页、32 px Logo、状态色语义和弱化 Footer 已在 LCD BSP 实现；预览脚本使用同一
320x240 坐标、5x7 字模、RGB565 常量和 Logo 数据重新生成五张 PNG。执行 CMake Debug/Release
配置与构建、从最终 Release ELF 重新生成 BIN、OTA 打包和 `git diff --check`：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 111592 | 120 | 54736 | `BUILD PASS` |
| Release | 99500 | 120 | 54728 | `BUILD PASS` |

```text
application.bin=99628 bytes
payload_crc32=0x5BEC2E71
bin_sha256=a5fedd2be79befb12029050f8c33c85cda7590bb4558ca3b151f732a8e40df02
app-v0.14.0-b1.ota=99692 bytes
ota_sha256=b94b573309504fc1bd0c6304a0aaa68e22886cc116aa20cdd5c335b4c74daaaa
```

随后已执行 UART OTA，结果见下节；本节构建和产物校验值保持不变。

### 0.14.0 build1 UART OTA 与启动复核（2026-08-19）

使用 `/dev/ttyUSB0`、115200 8N1 发送 `build/arm-release/app-v0.14.0-b1.ota`。发送工具完成
99692 字节传输并收到 `STAGED`，Bootloader 和 Application 依次报告：

```text
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
fw=0.14.0 build=1 ota_confirmation=CONFIRMED
```

健康窗口后执行 `status`，四任务均为 `RUNNING`，`fault=0`、`control=STOPPED`，左右目标、
速度和 PWM 均为零；LCD 为 `READY`，IMU 为 `READY`、`WHO_AM_I=0xE9`，FIFO 和 timestamp
错误均为零。未执行电机命令。随后用户人工确认 0.14.0 LCD 暗色工业仪表四页的配色、文字排版、
Logo、电量条和页面切换正常，因此 LCD UI 0.14.0 记录为 `HARDWARE PASS`。

Application Release 于 2026-08-13 使用 CubeIDE 2.2.0 GCC clean build；Debug 行保留
2026-07-28 的最近结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 222948 | 96 | 34072 | `BUILD PASS`，2026-07-28，0 errors，0 warnings |
| Release | 183492 | 96 | 34224 | `BUILD PASS`，2026-08-13，b12，0 errors，0 warnings |

Debug/Release 的 `.isr_vector` 均位于 `0x08008000`；反汇编确认 `SystemInit()` 将
VTOR 写入 `0x08008000`。最高 Flash load 地址未超出 `0x0807FFFF`。构建结果只说明
当前 Application b12 源码可编译和链接，不代表本轮固件已通过实物启动；b6 仍是最近一次
完成 UART OTA 实物闭环的 Application。

Bootloader Release 于 2026-08-13 使用 CubeIDE 2.2.0 GCC clean build；Debug 行保留
2026-07-28 的最近结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 17652 | 44 | 1668 | `BUILD PASS`，2026-07-28，0 errors，0 warnings |
| Release | 13400 | 48 | 1656 | `BUILD PASS`，2026-08-13，build22，0 errors，0 warnings |

Bootloader 链接脚本只提供 `0x08000000` 起始的 32 KiB Flash。该结果仅证明编译和
链接通过，不证明 QSPI 安装、掉电恢复、Application 跳转或回滚已通过实物验证。
本次 build22 已于 2026-08-13 从当时的 Release ELF 生成正式配套产物。本段记录的是烧录前
状态；后续同日已完成匹配 factory/QSPI 基线写入并确认 build22 启动 b12，详见下一节。
build22 已于 2026-08-17 完成 b12 到 b13 的 UART OTA 全闭环验证。

### b12/build22 上板准备（2026-08-13）

从当前 Release ELF 重新执行 `arm-none-eabi-objcopy -O binary`，随后生成 OTA、内部 factory
组合镜像和 QSPI confirmed raw。工具输出确认 Application payload CRC32 为 `0x3A5DFAD0`，
QSPI metadata 为 `CONFIRMED/SLOT_A`。产物如下：

| 产物 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `app-v0.1.0-b12.bin` | 183596 | `1A881962E44E298A8BE064834E5FF3CA81366B5CFB925004FF942FF03FD716FF` |
| `app-v0.1.0-b12.ota` | 183660 | `F7FF629B1E02C45CF582ABF96904E0E3500C00EBB7F6BB6F7358D8320505F6FF` |
| `boot-v0.1.0-b22.bin` | 13448 | `B56E787611A0FBA816E11801EE4BAF62DEB9D61A965B369D58A4A276B2A25358` |
| `factory-a12-b22.bin` | 216364 | `A7FB68D07022D7C12D88F2EA9D744B7E9EE249947B5E9E35F60A4ADAA9FBE243` |
| `qspi-a12-confirmed.bin` | 8388608 | `6A86EC1E297D525023CE8D024971E359B13939302C6B3D8A5EA439CD39A26F81` |

同轮重新运行 OTA Python 9 项测试，全部通过。COM9 可打开并返回 `PONG` 和完整 `status`；
在线固件报告 QSPI `EF4017`、`MOTOR: DISABLED`、`OTA_CONFIRM: NOT_REQUIRED`，但串口命令
不输出 Application/Bootloader 版本，不能据此认定板上已是 b12/build22。

STM32CubeProgrammer 2.23.0 未检测到 ST-Link 或 DFU 设备，安装目录也没有匹配本板
STM32G474 + W25Q64 自定义引脚的 External Loader。因此本轮未执行内部 Flash/QSPI 烧录，
当时冻结清单七项继续保持 `NOT VERIFIED`。

后续同日通过 STM32 ROM DFU 和独立
`tools/qspi_factory_provisioner/` 完成匹配基线写入。Provisioner 固定校验内部暂存的 b12
OTA package 和 `CONFIRMED/SLOT_A` Metadata A，擦除并读回确认 Metadata A/B，擦写 Slot A，
逐字节读回比较完整 183660 字节 package，最后提交并读回 Metadata A。板上日志为：

```text
PROVISION: INPUTS VALID
PROVISION: METADATA ERASED
PROVISION: SLOT A ERASED
PROVISION: SLOT A WRITTEN
PROVISION: SLOT A VERIFIED
PROVISION: METADATA A VERIFIED
PROVISION: METADATA B ERASED
PROVISION: PASS
```

随后通过 DFU 全擦内部临时工具和暂存数据，将 `factory-a12-b22.bin` 下载到
`0x08000000` 并由 CubeProgrammer 校验成功。启动日志确认：

```text
BOOT: VERSION=0.1.0 BUILD=22
BOOT: QSPI READY
BOOT: METADATA READY
BOOT: STATE=0x00000005
chassis-controller started
```

Application 健康窗口后 `status` 报告 QSPI `EF4017`、`OTA_CONFIRM: NOT_REQUIRED`、
`MOTOR: DISABLED`、`CONTROL_OVERRUN: count=0 missed=0`。因此匹配的 QSPI factory confirmed
基线写入和 b12/build22 confirmed 启动已完成实物验证。该启动由 DFU `go 0x08000000`
触发；仍需单独执行普通按键/断电复位回归。`MOTOR: DISABLED` 是软件状态证据，四路 PWM
上电电气零输出仍需示波器或逻辑分析仪测量。

2026-08-13 在当前工作树完成宿主机回归：

- `tools/ota/test_ota_transfer.py`：3 项 Python 测试通过，覆盖 UART 帧 CRC、CAN 响应
  布局和 stop-and-wait 分块状态流程。
- `tools/ota/test_factory_image.py`：3 项测试通过，确认 Slot A package、`CONFIRMED` Metadata A、
  CRC、擦除态 Metadata B、默认缺少 QSPI 参数时失败，以及显式 `--internal-only` 调试路径。
- `tools/ota/test_shared_abi.py`：3 项测试通过，直接解析共享 C 头文件并核对两个 Python
  打包工具的 Flash 地址、大小、magic、格式版本、枚举值，以及镜像头/metadata 的字段类型、
  声明顺序、数组长度、逐字段 offset、C `_Static_assert(offsetof(...))` 和 64 字节总大小。
- `test_command_manager.c`：WSL GCC 使用 `-std=c11 -Wall -Wextra -Werror` 编译运行通过，
  覆盖 OTA owner、运动命令清除、非法 source、Console 持续目标和 CAN 200 ms 超时。
- `test_ota_metadata.c`：同参数编译运行通过，覆盖 metadata 状态和严格槽约束。
- `test_ota_uart_arm_guard.c`：同参数编译运行通过，覆盖等待 BEGIN 的 30 秒超时、
  active session 和 tracked response 未完成时禁止禁用 UART transport。
- `bootloader_core_test.c`：此前使用同参数编译运行通过，覆盖 CRC32 标准向量、镜像校验、
  metadata 双副本选择、factory 擦除态识别、candidate/confirmed 安装三次上限、attempt 1/2/3
  中断后的 `TRIAL/CONFIRMED` salvage、无效镜像重装边界和 confirmed size/CRC 回填。
- 本轮继续新增恢复 source 状态和“仅 internal mismatch 允许重装”的纯策略断言。
  ARM GCC 使用 `-std=c11 -Wall -Wextra -Werror` 编译通过；Windows 环境无桌面 GCC，WSL
  启动返回 `E_ACCESSDENIED`，因此新增断言尚未在宿主机执行。目标 build22 已编译链接通过。

本轮新增的可诊断 confirmed 恢复、I/O 重试、confirmed fatal 处理和编译器 ABI 断言均只完成
代码审查、主机测试或目标构建，尚未完成目标板故障注入/烧录回归。

主机测试和目标构建均不替代 STM32 实物验收。

### Bootloader 启动对照

2026-07-28 使用同一 relocated Application 进行了单变量对照：

| 组合 | Bootloader 时钟 | Bootloader IWDG | 启动结果 |
| --- | --- | --- | --- |
| 候选 I | 16 MHz HSI | 禁用，诊断直跳 | Application LCD 和串口正常 |
| 候选 K | 16 MHz HSI | 启动，约 32 秒 | 黑屏，串口无响应 |
| 候选 L | 16 MHz HSI | 普通路径不启动，完整状态机 | LCD 正常，`status` 完整返回 |
| 候选 N | 170 MHz HSE/PLL | 普通路径不启动，完整状态机 | 黑屏，串口无响应 |
| 正式 factory | 16 MHz HSI | 不启动，仅刷新继承实例 | DFU 校验成功，普通复位后 LCD 正常 |

因此正式 Bootloader 使用 16 MHz HSI，任何路径都不主动启动或重配置 IWDG，只刷新
Application 在软件复位前已启动的实例。Recovery 不刷新，让继承的 IWDG 触发复位；
冷启动且 IWDG 未运行时，直接写 reload key 不会启动它。Application 启动后重新配置
正常周期并负责持续刷新。

### UART OTA 实物闭环（2026-07-30）

使用 `_output/application/app-v0.1.0-b6.ota`
在 COM8、115200 8N1 下完成真实传输：

```text
OTA staged over UART: next_offset=182856
BOOT: STATE=0x00000002
BOOT: INSTALL CANDIDATE
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: STATE=0x00000004
BOOT: TRIAL VERIFIED
BOOT: JUMP APPLICATION
OTA_CONFIRM: CONFIRMED
```

最终 Bootloader build15 经 DFU 烧录和校验后，普通复位得到：

```text
BOOT: STATE=0x00000005
QSPI_ID: PASS jedec=EF4017 capacity=8MiB
MOTOR: DISABLED
CONTROL_OVERRUN: count=0 missed=0
```

本次同时确认 Bootloader 必须提供 `SysTick_Handler()` 并将 VTOR 显式设置为
`0x08000000`；缺少 SysTick ISR 时首个 1 ms 中断会进入默认处理器，表现为启动日志被截断。
最终产物为：

```text
_output/bootloader/boot-v0.1.0-b15.bin 11684 bytes
_output/application/app-v0.1.0-b6.bin 182792 bytes
_output/application/app-v0.1.0-b6.ota 182856 bytes
_output/factory/factory-a6-b15.bin 215560 bytes
```

UART OTA 主链已达到 `HARDWARE PASS`。CAN FD OTA、安装中断电、TRIAL 失败回滚和无有效
Application 的 Recovery 行为仍为 `NOT VERIFIED`。

### b13 无 IMU启动与 UART OTA 闭环（2026-08-17）

使用 CMake 3.22、Ninja 和 GNU Arm Embedded 14.3.rel1 构建 b13 Release。Application 结果为
`text=185388 data=120 bss=34944`，生成 payload 185516 字节、CRC32 `0x6FD23D35` 的
`app-v0.1.0-b13.ota`；OTA Python 9 项通过。

目标板起点为 build22 + confirmed b12。`send_uart.py` 通过 `/dev/ttyUSB0`、115200 8N1
完成 185580 字节 package 传输并收到：

```text
OTA staged over UART: session=193 next_offset=185580; device reset expected
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: STATE=0x00000004
BOOT: TRIAL VERIFIED
```

b13 在未连接 ICM45686 时完成启动和 5 秒健康确认：

```text
ICM45686: NOT_INITIALIZED whoami=0x00 samples=0
LCD: READY
MOTOR: DISABLED
CONTROL_OVERRUN: count=0 missed=0
OTA_CONFIRM: CONFIRMED
```

随后通过 ST-Link 执行普通复位，Bootloader 报告 `STATE=0x00000005`，Application 再次正常
启动；等待健康窗口后 `status` 报告 `OTA_CONFIRM: NOT_REQUIRED`，屏幕保持点亮，未观察到
critical fault 或 IWDG 复位循环。因此 b13 无 ICM45686 启动和 build22 UART OTA 主链均为
`HARDWARE PASS`。该结果不包含四路 PWM 电气测量、断电启动、CAN FD OTA 或三个故障注入测试。

## CMake/Ninja 等价构建（2026-08-13）

使用 GNU Arm Embedded 14.3.rel1（GCC 14.3.1）、CMake 和 Ninja clean build 三个目标。
`--specs=nano.specs` 同时用于 C 编译和链接，以保持 newlib-nano 头文件 ABI 与 CubeIDE
一致。结果如下：

| 目标 | text | data | bss | BIN SHA-256 | 结果 |
| --- | ---: | ---: | ---: | --- | --- |
| Application | 183492 | 96 | 34224 | `1A881962E44E298A8BE064834E5FF3CA81366B5CFB925004FF942FF03FD716FF` | `BUILD PASS`，与 b12 基线一致 |
| Bootloader | 13400 | 48 | 1656 | `B56E787611A0FBA816E11801EE4BAF62DEB9D61A965B369D58A4A276B2A25358` | `BUILD PASS`，与 build22 基线一致 |
| QSPI provisioner | 9192 | 12 | 1644 | `01EF25D46787D45A91B66C7BAA481990DADA5E1C5C6C789B4375A42B8CF70E30` | `BUILD PASS`，与已上板工具一致 |

Application `.isr_vector` 位于 `0x08008000`，Bootloader 和 provisioner `.isr_vector` 位于
`0x08000000`。同轮 OTA Python 9 项测试全部通过。三个 BIN 均与已经生成或上板验证的基线
逐哈希一致，因此不需要为 CMake 构建重复烧录同一字节产物。

## 分阶段路线阶段 1a：FreeRTOS 运行基础（四任务迁移前，2026-08-17）

在保留两任务基线和既有电机/安全路径的前提下，新增双缓冲 `SystemStatusSnapshot`，统一记录
板级、传感器、通信、供电、RTC、OTA 基础状态，以及任务 uptime、实际/期望周期、超时、运行
次数、运行状态、heartbeat age、栈余量和 RCC 复位原因；`status` 与 LCD 当前页读取同一快照。
四任务目标模型已写入 `architecture.md`，该轮记录的是迁移前快照基础。该结果只证明代码构建和链接，
不代表新增运行字段已完成目标板实物验收。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Application Debug | 196660 | 120 | 43112 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |
| Application Release | 187948 | 120 | 43104 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |

执行命令：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

本阶段未执行目标板烧录或 FreeRTOS 运行字段实物验证，保持 `NOT VERIFIED`。在该轮记录时四任务
拆分、正式 UART `OK/ERROR`、`LOG`/`TEL` 和 LCD 四页仍未实施；不得用该轮构建结果替代后续阶段
的验证。

## 分阶段路线阶段 1b：四任务调度迁移（2026-08-17）

在保留控制任务 100 Hz 通知、OTA 维护锁和电机安全路径的前提下，完成四任务实际迁移：
`service_task` 负责 UART/CAN/Console/OTA/遥测，`diagnostics_task` 负责 RTC、ADC、电源、
IMU、SR501 和双缓冲快照，`display_task` 负责按键和 LCD。四个任务均纳入周期、超时、运行
状态、运行次数、栈余量和 heartbeat 诊断；关键任务健康仍是 IWDG 刷新条件。该结果只证明代码
构建和链接，不代表目标板运行字段或显示行为已完成实物验收。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Application Debug | 198048 | 120 | 48736 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |
| Application Release | 189216 | 120 | 48728 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |

执行命令：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

本阶段未执行目标板烧录。正式 UART `OK/ERROR`、`LOG`/`TEL` emitter 和 LCD 四页仍未实施，
ICM45686 多传感器时间轴、Kalman 以及 SR501 高电平事件验证继续按路线后置。

## CubeIDE 构建与烧录

1. 以仓库 `firmware/` 为工作区，分别导入 `application/stm32g474/` 和
   `bootloader/stm32g474/`。
2. 选择 `Debug` 或 `Release`，执行 `Project > Clean` 后再执行 `Build Project`。
3. 确认生成的 `Debug/chassis_controller.elf` 或
   `Release/chassis_controller.elf` 时间晚于本次源码修改。CubeIDE 当前不会自动更新
   BIN，OTA 打包前必须从最新 ELF 重新运行 `arm-none-eabi-objcopy -O binary`。
4. 日常调试使用 ST-Link；使用 USB Type-C DFU 时，通过 STM32CubeProgrammer
   选择正确的 DFU 设备和 ELF/HEX/BIN，再烧录并复位。
5. 烧录后的串口启动信息和固件版本必须与本次产物一致。

CubeMX 重新生成后必须重新检查自定义 source folder、include path 和 FreeRTOS 配置，
不能用旧 ELF 判断当前代码。

## VS Code Cortex-Debug

仓库共享 `.vscode/tasks.json` 和 `.vscode/launch.json`。F5 依次配置、构建 `arm-debug`，
启动 OpenOCD，加载并烧写 `build/arm-debug/application.elf`，复位后停在 `main`。所需命令
`cmake`、`openocd` 和 `arm-none-eabi-gdb` 必须在 `PATH`，VS Code 需安装 Cortex-Debug。

2026-08-14 软件侧检查：Debug ELF 含 DWARF，GDB 可定位 `main.c:77`，并可解析
`pxCurrentTCB`；Debug Application 含 `APP_DEBUG_IWDG_FREEZE`，Release compile commands
确认不含该宏。

2026-08-15 目标板检查：ST-Link `0483:3748` 和 CH340 已在 Ubuntu 虚拟机枚举；OpenOCD
不使用 `sudo` 启动，通过 ST-Link SWD 识别 STM32G47/G48、目标电压约 3.24 V，并在
3333 端口提供 GDB server。`arm-none-eabi-gdb build/arm-debug/application.elf` 已完成
`load` 烧写 Debug ELF，命中 `main()` 源码断点 `Core/Src/main.c:77`。

同一会话继续验证源码断点和运行状态：`ChassisApp_Init()` 在 `app/chassis_app.c:80` 命中，
Call Stack 回到 `main.c:121`；`ControlTaskMain(argument=0x0)` 在 `rtos/rtos_app.c:72`
命中，`pxCurrentTCB = 0x20007a0c <control_task_buffer>`。断点暂停时读取
`DBGMCU->APB1FZR1 = 0x1800`，包含 `DBG_IWDG_STOP` 位 `0x1000`；保持暂停 12 秒后仍停在
同一 FreeRTOS 任务栈，未发生 IWDG 复位。因此底层 OpenOCD/GDB 自动烧写、源码断点、
调用栈、FreeRTOS 符号和 Debug IWDG 冻结为 `HARDWARE PASS`。

2026-08-15 用户在 VS Code 图形界面按 F5 人工确认通过：Cortex-Debug 自动执行
`CMake: build arm-debug`、启动 OpenOCD、烧写 `build/arm-debug/application.elf`，并进入源码调试。
因此 VS Code F5 前端流程同样记为 `HARDWARE PASS`。

CMake/Ninja 命令和工具链要求见 [`cmake_build.md`](cmake_build.md)。

## 串口

CH340 参数：

```text
115200 baud, 8 data bits, no parity, 1 stop bit
```

常用命令以固件输出的 `COMMANDS` 为准。危险测试必须保持车轮架空并显式输入确认命令。

## PID 调参

使用 Windows、USB 串口和 VOFA+，不另建专用 GUI。车轮架空，每次只调一侧，
遥测默认关闭，控制仍由 STM32 的 100 Hz `control_task` 执行。

建议 FireWater 文本：

```text
pid:time,target,measured,error,p,i,d,output,pwm
```

当前实际命令接口：

```text
pid show
pid left <kp> <ki> <kd>
pid right <kp> <ki> <kd>
pid target <left_counts_per_tick> <right_counts_per_tick>
pid stop
telemetry text
telemetry vofa
telemetry off
```

规则：

- `pid left/right` 立即更新 RAM 参数，并由 `service_task` 异步写入 QSPI 双副本；响应中的
  `persistence=QUEUED/SAVING/STORED/ERROR` 表示持久化状态。
- 启动时选择最新有效的 QSPI 副本；当前仍没有单独的 `pid save/apply/reset` 命令。
- `pid target` 持续保持到 `pid stop`，不使用 CAN 的 200 ms heartbeat timeout。
- 停止、急停、超时、故障和换向时重置积分。
- 先调 `kp`，再增加少量 `ki`，最后仅在确有必要时加入 `kd`。
- `telemetry vofa` 以 10 ms 周期输出左右目标、增量、RPM x10、PWM、电压、状态和故障；
  出现异常立即执行 `pid stop`。
- 方向沿用已有实物验收结论，不重复测试；低速 PID 闭环、停车和稳定性仍标记为 `DEFERRED`。

## CAN FD 联调

Jetson 配置：

```bash
sudo modprobe can
sudo modprobe can_raw
sudo modprobe mttcan

sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 sample-point 0.8 \
  dbitrate 2000000 dsample-point 0.8 fd on one-shot off \
  berr-reporting on restart-ms 1000
sudo ip link set can0 up
ip -details -statistics link show can0
```

正常基线应为 `ERROR-ACTIVE`，当前 `tx/rx` 错误计数应为零或稳定不增长。

终端 1：

```bash
candump can0
```

终端 2发送握手请求：

```bash
cansend can0 720##150494E4701000000
```

收到 STM32 `0x721` 后发送确认：

```bash
cansend can0 720##15041535301000000
```

通过条件：

- Jetson 收到 `0x721`
- STM32 `status` 显示 `FDCAN_EXTERNAL: PASS`
- `can status` 中没有持续增长的 TEC、REC、bus-off 或 protocol error

排障原则：

- FDCAN TX/RX 与收发器同名连接，不像 UART 那样交叉。
- 断电检查 CANH-CANL 约 60 Ω，并检查 CANH、CANL、GND 端到端连通。
- 只有看到 STM32 返回帧才证明双向链路；`candump` 中本地 `0x720` 回显不算。
- Error-Passive 或 Bus-Off 后先停止发送并修复接线、终端和时序。

## 电机与安全回归

涉及电机、控制任务、安全管理或配置变化时：

1. 架空车轮，保持电机电源断开。
2. 上电确认四路 PWM 为零且电机测试默认关闭。
3. 接通电机电源，分别验证左右轮正反转和编码器符号。
4. 运行闭环时确认目标、测量和输出方向一致。
5. 触发急停，确认 PWM 立即归零且故障锁存。
6. 停止 CAN 控制帧，确认 200 ms 后停车且旧命令不会恢复。
7. 检查控制 overrun、关键任务健康和 IWDG 行为。

## OTA 首次烧录与验收

首次操作必须同时写内部组合镜像和 QSPI confirmed 基线：

1. 断开电机主电源，保持急停可用，确认 CAN 不发送运动命令。
2. 在 STM32CubeProgrammer 连接 DFU 或 ST-Link，执行 Full chip erase。
3. 选择匹配版本的 `factory-a*-b*.bin`，下载地址填写 `0x08000000`，启用下载后校验。
4. 使用 W25Q64 External Loader 或等价工装，将匹配的 `qspi-a*-confirmed.bin` 从 QSPI
   地址 `0x000000` 写入并校验。
5. 复位，确认串口出现 Application 启动信息，电机保持零输出。
6. 执行 `status`，记录 `OTA_CONFIRM` 和 `OTA_TRANSFER`；首次无 TRIAL 元数据时不得
   将 READY 当成 OTA 硬件通过。

当前 OTA V1 按以下清单验收：

匹配的内部 factory 镜像和 QSPI `CONFIRMED/SLOT_A` 基线已通过 DFU provisioner 写入、
完整读回和启动校验；该 factory provisioning 前置项不替代下列项目各自的验收。

| 顺序 | 项目 | 当前状态 | 通过条件 |
| ---: | --- | --- | --- |
| 1 | 普通启动 | `HARDWARE PASS` | 2026-08-17，confirmed b13 经 ST-Link 普通复位后正常运行；断电启动仍需单独验收 |
| 2 | 上电零 PWM | `NOT VERIFIED` | 四路 PWM 在进入控制前保持为零 |
| 3 | UART OTA | `HARDWARE PASS` | 2026-08-17，b12 -> b13 完整 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED` |
| 4 | CAN FD OTA | `NOT VERIFIED` | 用 SocketCAN 完成与 UART 对等的 OTA 闭环 |
| 5 | Application 安装中断电 | `NOT VERIFIED` | 重启后继续或安全恢复，不跳入半写镜像 |
| 6 | TRIAL 不确认 | `NOT VERIFIED` | 超过试运行限制后自动恢复 confirmed |
| 7 | rollback 安装中断电 | `NOT VERIFIED` | 重启后继续恢复 confirmed，不无限破坏性重装 |

2026-08-17 的冻结决定作为历史记录保留。2026-08-20 已解除该冻结：CAN FD OTA、断电启动、
四路 PWM 电气零输出和三个故障注入项目重新纳入当前验收；它们仍不构成已通过证据，必须在
实际测试后才能更新为 `HARDWARE PASS`。固件签名、防回滚和 Bootloader CAN Recovery 仍属于
OTA V2 设计范围。

每次传输都应保存发送工具输出和复位后的 `status`。只有看到完整
`STAGED -> TRIAL -> CONFIRMED` 且新 Application 正常启动，才可记录
`HARDWARE PASS`。当前 factory 普通启动和 UART OTA 的传输、安装、TRIAL、确认已通过；
CAN FD OTA、回滚、断电恢复和零 PWM 电气验收当前均为 `NOT VERIFIED`，已重新进入活动清单。

## Bootloader 单元测试

当前已有 PC 单元测试入口：

```text
firmware/bootloader/stm32g474/tests/unit/bootloader_core_test.c
```

覆盖内容：

- CRC32 标准向量 `123456789 -> 0xCBF43926`
- OTA 镜像头、payload CRC 和向量表校验
- OTA 元数据双副本 CRC 校验和最新 sequence 选择

Bootloader 的 Debug 与 Release 已使用 STM32CubeIDE 自带 `arm-none-eabi-gcc`
完成构建。该 PC 测试已于 2026-07-30 使用 WSL GCC 和
`-std=c11 -Wall -Wextra -Werror` 编译运行通过；结果仅覆盖纯逻辑，不覆盖 QSPI、内部
Flash、IWDG、复位和跳转的目标板行为。

## 2026-08-14 ICM45686 与预留按钮构建

- 独立 CubeMX 生成器已使用带目标目录的 CLI 脚本完成生成：
  `E:\STM32CubeMX\STM32CubeMX.exe -q .cubemx-generate.script`。
- CMake 3.22 + Ninja + GNU Arm Embedded 14.3.rel1 clean build：Application、Bootloader、QSPI provisioner 均通过。
- ICM45686、VS Code Debug 配置和 IWDG 调试冻结接入后，当前 Release clean build 通过：
  Application `text=198060 data=120 bss=35232`、Bootloader `text=13480 data=48 bss=1656`、
  QSPI provisioner `text=9272 data=12 bss=1644`。
- `test_icm45686.c` 使用 Visual Studio 2022 MSVC `/std:c11 /W4 /WX` 编译并实际运行通过，
  覆盖软复位恢复、MREG 字节序、ODR/4低通、FIFO配置/计数/状态/flush、16字节大端帧解析、
  timestamp正常差值与16位回绕，以及SI单位换算。
- `test_imu_fusion.c` 使用相同 MSVC 参数编译并实际运行通过，覆盖 200 样本静止零偏收敛、
  姿态初始化、四元数归一化和 1 秒陀螺旋转积分。
- SPI3 DMA1 CH5/CH6 已写入 `.ioc`；CubeMX 6.18 CLI 已生成 `hdma_spi3_rx/tx`、DMA NVIC
  配置和 CH5/CH6 IRQ handler，原临时 BSP DMA glue 已移除。
- 本轮仅有源码、宿主测试和编译/链接证据；ICM45686、FIFO/DMA、零偏、姿态和 PD3/PD4
  按钮尚未进行实物验收。

## 2026-08-17 SR501 代码接入构建

SR501 使用 `.ioc` 中的 PD5 普通输入、内部下拉配置。已使用 CubeMX 6.18.1 和
STM32Cube FW_G4 V1.6.3 生成 `SR501_OUT` 引脚定义及 GPIO 初始化；`drivers/sensor/sr501_stm32.c` 负责 60 秒
预热、50 ms 稳定滤波、READY 后稳定低到高单次计数，以及 `status` 诊断输出。当前未绑定
电机、安全、LCD 或业务。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
```

结果：

| 配置 | text | data | bss | 状态 |
| --- | ---: | ---: | ---: | --- |
| Debug | 194364 | 120 | 35104 | `BUILD PASS` |
| Release | 185948 | 120 | 35096 | `BUILD PASS` |

本轮只证明源码编译和链接通过。SR501 的 5 V 供电、OUT 高低电平、60 秒预热、50 ms 滤波、
持续高电平不重复计数和断开模块时 PD5 下拉行为均为 `NOT VERIFIED`。

同日将包含 SR501 的 Application 提升为 b14，重新执行上述 Debug/Release 构建并生成：

| 产物 | 字节数 | CRC32 / SHA-256 |
| --- | ---: | --- |
| `app-v0.1.0-b14.bin` | 186076 | CRC32 `0x1E5B771D`；SHA-256 `85755554e0e2f0234c7071fcbd7867309048933247d8e317260930f268028ec4` |
| `app-v0.1.0-b14.ota` | 186140 | SHA-256 `20548218f4db0ba3b5721c8d8f9c6c7b97b17d2d6fcb18c85a2c9214f397de66` |

打包器确认目标地址为 `0x08008000`，OTA Python 9 项执行通过。随后 CH340 和 ST-Link 已透传
到 Ubuntu，b13 在线状态为 `MOTOR: DISABLED`，因此通过 UART 发送 b14。Bootloader 报告
`INSTALL VERIFIED`、`TRIAL COMMITTED`、`TRIAL VERIFIED`，Application 正常响应 `PONG`，
但 `status` 无输出。源码检查确认完整报告加 SR501 行后超过 UART 单条消息 1200 字节上限。

b15 将诊断格式缓冲从 1664 增至 2048，但 UART 上限未同步，复测仍无 `status`。b16 将
`BSP_UART_MAX_WRITE_SIZE` 同步为 2048，并用 `_Static_assert` 约束诊断缓冲不得超过该上限。
b16 重新构建结果和正式待测产物为：

| 配置/产物 | text/字节数 | data | bss | CRC32 / SHA-256 |
| --- | ---: | ---: | ---: | --- |
| Debug ELF | 194364 | 120 | 42272 | `BUILD PASS` |
| Release ELF | 185948 | 120 | 42264 | `BUILD PASS` |
| `app-v0.1.0-b16.bin` | 186076 | - | - | CRC32 `0x995BF0B5`；SHA-256 `ca963794d3dc912b32c671061ea68c07acbc54b68975e80985b884b0ea1ca563` |
| `app-v0.1.0-b16.ota` | 186140 | - | - | SHA-256 `4999ec00755b372482ea2ea370db20389f43ee0271f1fd479d3e85c268e9c2ba` |

b16 经 UART 收到 `STAGED` 后，Bootloader 再次报告 `INSTALL VERIFIED`、`TRIAL COMMITTED`、
`TRIAL VERIFIED`；随后 `status` 返回 1251 字节完整报告并显示 `OTA_CONFIRM: CONFIRMED`、
`MOTOR: DISABLED`、`CONTROL_OVERRUN: count=0 missed=0`。这同时实证旧 1200 字节限制不足。

SR501 首次状态为：

```text
SR501: WARMING_UP motion=0 raw=0 count=0 last_ms=0 warmup_ms=24831
SR501: WARMING_UP motion=0 raw=0 count=0 last_ms=0 warmup_ms=3328
SR501: READY motion=0 raw=0 count=0 last_ms=0 warmup_ms=0
```

随后连续监听 60 秒，始终为 `READY motion=0 raw=0 count=0`。因此 60 秒预热、预热期间不计数、
READY 后低电平零误计数已有实物证据；模块指示灯未亮，传感器 OUT 未观察到高电平。
2026-08-17 决定将模块供电/设置排查、50 ms 稳定滤波、稳定低到高单次计数和持续高电平
不重复计数统一标记为 `DEFERRED`，不阻塞 PID 主线。

## 2026-08-18 正式 UART 消息构建与实物验证

Application 文本输出已迁移到统一 emitter：命令响应使用 `[RSP]`，异步事件使用 `[LOG]`，
周期状态使用 `[TEL]`。`status` 现在以同一 `seq` 输出 `system`、`motor`、`sensors`、
`communication` 四个分区；VOFA 数字流保留为显式兼容模式。UART OTA 进入二进制模式后，
Application 跳过所有文本诊断和遥测输出；`send_uart.py` 已同步等待 `[RSP] command=ota_uart`。
`test_uart_message.py` 的 4 项主机测试覆盖正式/旧版 OTA ready 响应、错误响应和重复/混合字段拒绝；
旧版准备行只用于主机工具从旧固件单向升级，不放宽新 Application 的输出格式。
`test_command_manager.c` 重新通过宿主机 `-Werror` 回归，覆盖明确的 `ACCEPTED`、`NOT_OWNER` 和
`INVALID_ARGUMENT` 提交结果。目标板首次验证发现 nano printf 不正确支持 `%lld`，导致编码器
显示为 `ld` 并让后续变参错位；现已用 `UartProtocol_FormatSigned64()` 替代 Application 内全部
`%lld`。新增 C 测试覆盖 `0`、`INT64_MAX`、`INT64_MIN` 和容量不足。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --clean-first --parallel
cmake --preset arm-release
cmake --build --preset arm-release --clean-first --parallel
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools/ota -p 'test_*.py' -v
gcc -std=c11 -Wall -Wextra -Werror -DUART_PROTOCOL_HOST_TEST ...
gcc -std=c11 -Wall -Wextra -Werror -DCOMMAND_MANAGER_HOST_TEST ...
git diff --check
```

结果：

| 配置 | Application text | data | bss | 状态 |
| --- | ---: | ---: | ---: | --- |
| Debug | 199868 | 120 | 52864 | `BUILD PASS` |
| Release | 190404 | 120 | 52856 | `BUILD PASS` |

同一 preset 的 Bootloader 和 QSPI provisioner 也完成构建：Debug Bootloader `text=15596 data=48
bss=1656`、Debug provisioner `text=10556 data=12 bss=1644`；Release Bootloader
`text=13480 data=48 bss=1656`、Release provisioner `text=9272 data=12 bss=1644`。
Python OTA 测试共 13 项通过；UART 64 位格式化和 CommandManager 两个 C 宿主测试均以
`-Wall -Wextra -Werror` 编译并执行通过。最终 b17 产物为：

| 产物 | 字节数 | CRC32 / SHA-256 |
| --- | ---: | --- |
| b17 Release BIN | 190532 | CRC32 `0xA68BC9CE`；SHA-256 `c007a8b794ef3dfdf868f8a6fbfec09a181fc79e9b836491f3343245bdc04a04` |
| b17 OTA package | 190596 | SHA-256 `b4747c5df5268565a8eb2aae30f837ca1c53f926f1270795e90a060353217341` |

通过 `/dev/ttyUSB0`、115200 8N1 将 b17 OTA 包发送到 confirmed b16，发送工具收到：

```text
OTA staged over UART: session=120 next_offset=190596; device reset expected
```

b17 进入试运行并完成确认后，`ping`、`status` 和 `encoder result` 实测确认：

```text
[RSP] v=1 ts_ms=59862 result=OK command=ping
[TEL] v=1 ts_ms=60064 seq=1 section=system fw=0.1.0-b17 ... critical_tasks=1 ...
[TEL] v=1 ts_ms=60064 seq=1 section=motor ... left_encoder=0 right_encoder=0 ... overrun=0 missed=0
[TEL] v=1 ts_ms=60064 seq=1 section=communication ... ota_confirmation=CONFIRMED ...
[RSP] v=1 ts_ms=60266 result=OK command=encoder_result left_total=0 right_total=0
```

同轮已验证未知命令返回 `INVALID_ARGUMENT`、`pid show` 字段正确、`telemetry text` 以 100 ms
周期发送、采集的 30 行全部以 CRLF 结束，随后用 `telemetry off` 恢复。未执行电机命令。

最后通过 ST-Link/OpenOCD 执行普通软件复位，Bootloader 和 Application 报告：

```text
BOOT: VERSION=0.1.0 BUILD=22
BOOT: STATE=0x00000005
[LOG] v=1 ts_ms=47 level=INFO module=boot event=STARTED fw=0.1.0-b17
[LOG] v=1 ts_ms=310 level=INFO module=application event=READY tasks=4
[TEL] v=1 ts_ms=11219 seq=1 section=communication ... ota_confirmation=NOT_REQUIRED ...
```

稳定状态下四任务均为 `RUNNING`，`critical_tasks=1`、`control=STOPPED`、`fault=0`、
`overrun=0`、`missed=0`，QSPI 为 `EF4017`，LCD 为 `READY`。该证据不覆盖按键/断电启动、
四路 PWM 电气测量、SR501 高电平、事件计数、CAN FD OTA、断电恢复或回滚；这些项目继续保持
`DEFERRED` 或 `NOT VERIFIED`。

## 2026-08-18 Application 版本语义拆分过渡产物（0.2.0 build18）

本轮将 Application 产品版本从混合字符串 `0.1.0-b17` 拆为独立字段，曾暂用
`0.2.0 build18` 验证格式。随后按历史版本序列重新编号为当前的 `0.3.0 build1`；本节产物
保留为真实过渡证据，不作为当前版本基线。版本用于表示功能、协议或兼容行为变化；build 只
标识同一版本下的具体产物，普通本地重编译不递增。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --clean-first --parallel
cmake --preset arm-release
cmake --build --preset arm-release --clean-first --parallel
python3 tools/ota/package_firmware.py build/arm-release/application.bin \
  /tmp/chassis-controller-app-v0.2.0-b18.ota --version 0.2.0 --build 18
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools/ota -p 'test_*.py'
git diff --check
```

结果：

| 配置/产物 | text/字节数 | data | bss | CRC32 / SHA-256 |
| --- | ---: | ---: | ---: | --- |
| Debug ELF | 199896 | 120 | 52864 | `BUILD PASS` |
| Release ELF | 190444 | 120 | 52856 | `BUILD PASS` |
| build18 Release BIN | 190572 | - | - | CRC32 `0x6FCF40CC`；SHA-256 `1e14c0dd4076d228d737b5de58bc58ee9473fe29a08a68a7b816015b08b8c907` |
| build18 OTA package | 190636 | - | - | SHA-256 `cfcf7ff038d661a0278551667827f81cfb077daf77a27d43be53eb8bc4f519bb` |

OTA 工具完成 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED`。目标板状态实测为：

```text
[TEL] v=1 ts_ms=17001 seq=1 section=system fw=0.2.0 build=18 ... critical_tasks=1 ...
[TEL] v=1 ts_ms=17001 seq=1 section=motor ... left_encoder=0 right_encoder=0 ... overrun=0 missed=0
[TEL] v=1 ts_ms=17001 seq=1 section=communication ... ota_confirmation=CONFIRMED ... lcd=READY
[RSP] v=1 ts_ms=17203 result=OK command=encoder_result left_total=0 right_total=0
```

四任务均为 `RUNNING`，控制保持 `STOPPED`、故障为零，未执行任何电机命令。SR501 高电平、
按键/断电启动、四路 PWM 电气测量、CAN FD OTA、断电恢复和回滚仍按计划保持 `DEFERRED`
或 `NOT VERIFIED`。

## 2026-08-18 Application 版本序列最终基线（0.3.0 build1）

按路线文档中的历史映射，b17 的 FreeRTOS、统一快照和正式 UART 协议变化进入 `0.3.0`，
版本切换后 build 从 1 开始。重新生成并发送的最终包为：

| 配置/产物 | text/字节数 | data | bss | CRC32 / SHA-256 |
| --- | ---: | ---: | ---: | --- |
| Debug ELF | 199892 | 120 | 52864 | `BUILD PASS` |
| Release ELF | 190444 | 120 | 52856 | `BUILD PASS` |
| `app-v0.3.0-b1.bin` | 190572 | - | - | CRC32 `0x5866D20E`；SHA-256 `117c4a9cf45276f2c3722c9a67cf0de739cfe960a31e609de2970537383d5e1c` |
| `app-v0.3.0-b1.ota` | 190636 | - | - | SHA-256 `d5560e6710706f1560c2f28d00ab8b117ae71f8405acd1ca7cf515f3709f29df` |

目标板通过 UART OTA 完成 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED`，实测：

```text
[TEL] v=1 ts_ms=14349 seq=1 section=system fw=0.3.0 build=1 ... critical_tasks=1 ...
[TEL] v=1 ts_ms=14349 seq=1 section=motor ... left_encoder=0 right_encoder=0 ... overrun=0 missed=0
[TEL] v=1 ts_ms=14349 seq=1 section=communication ... ota_confirmation=CONFIRMED ... lcd=READY
[RSP] v=1 ts_ms=14552 result=OK command=encoder_result left_total=0 right_total=0
```

四任务均为 `RUNNING`，控制保持 `STOPPED`，未执行电机命令。后续同一 `0.3.0` 源码的重复
可部署产物才使用 build2、build3；功能或协议变化时进入下一个语义版本并将 build 重置为 1。

## 2026-08-18 LCD 小 Logo 与单页结构构建（0.4.0 build1）

本轮保留原始 `assets/tafei/picture_tafei.h`，从原始 JPG 复制生成 48x47 RGB565
`picture_tafei_logo.h`。LCD 不再使用独立全屏封面页，小 Logo 改为状态页右上角固定元素；当前
继续使用已接线的 PB8/BOOT0 按键，PD3/PD4 尚未接线且不参与 LCD 操作。四个功能页和 PB8
循环切页仍是后续工作。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 85712 | 120 | 52864 | `BUILD PASS` |
| Release | 76244 | 120 | 52856 | `BUILD PASS` |

原全屏 RGB565 资源不再链接到 Application，改为链接 4512 字节的小 Logo 像素数据，因此
Flash 占用明显下降。本轮未执行烧录或 LCD 实物观察，Logo 尺寸、颜色、位置和 PB8 页面操作均
保持 `NOT VERIFIED`。

## 2026-08-18 LCD 四页状态显示构建（0.5.0 build1）

LCD 已实现 `OVERVIEW`、`MOTOR`、`SENSORS`、`SYSTEM` 四页，固定显示 48x47 taifei Logo。
四页均由 `status_display` 一次读取 `SystemStatusSnapshot` 后转换数据；PB8/BOOT0 每次有效按下
循环到下一页，`SYSTEM` 后返回 `OVERVIEW`，不再触发诊断自检。PD3/PD4 未接线且不参与本轮操作。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 87896 | 120 | 52944 | `BUILD PASS` |
| Release | 78056 | 120 | 52936 | `BUILD PASS` |

构建未产生警告。四页实际文字、颜色、Logo、1 秒刷新以及
`OVERVIEW -> MOTOR -> SENSORS -> SYSTEM -> OVERVIEW` 的 PB8 循环尚未烧录观察，均保持
`NOT VERIFIED`。本轮未执行电机命令，也未改变 SR501、ICM45686 或 OTA 的既有硬件结论。

## 2026-08-18 LCD DMA 调度修复与上板观察（0.5.1 build1）

### 观察与修复

使用 ST-Link 直接写入 Application 区 `0x08008000` 后执行普通复位，Bootloader 检测到内部
镜像与 QSPI confirmed `0.3.0 build1` 不匹配，按 confirmed repair 路径恢复旧镜像；Bootloader
和 QSPI 未被修改。随后使用 GDB 从 Application 向量直接启动 `0.5.0 build1`，UART 确认：

```text
[TEL] ... fw=0.5.0 build=1 ... control=STOPPED ... left_pwm=0 right_pwm=0 ...
[TEL] ... display_task=RUNNING ... lcd=DRAWING ...
```

调试读取表明 LCD DMA 已完成但显示任务每 20 ms 只推进一行，约 4.8 秒才能完成一帧；1 秒
刷新会在完成后立即再次请求绘制，导致状态长期停留在 `DRAWING`。因此将
`DISPLAY_TASK_EXPECTED_PERIOD_MS` 改为 1 ms，并将补丁版本提升为 `0.5.1`。

### 0.5.1 构建

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
```

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 87896 | 120 | 52944 | `BUILD PASS` |
| Release | 78056 | 120 | 52936 | `BUILD PASS` |

`0.5.1` 调试启动已观察到 `lcd=READY`、四任务 `RUNNING`、`control=STOPPED` 和四路 PWM 为零；
四页实际内容、Logo 颜色和 PB8 循环仍未人工观察，保持 `NOT VERIFIED`。同一输出暴露
`display_period_ms=1` 但 `display_expected_ms=20` 的快照字段不一致，已在 `0.5.2` 修复。

## 2026-08-18 LCD 任务诊断周期统一（0.5.2 build1）

将 `SystemRuntimeSnapshot.display_expected_period_ms` 改为复用
`DISPLAY_TASK_EXPECTED_PERIOD_MS`，使实际 1 ms 调度周期、期望周期和 UART/LCD 诊断一致。
本修正不改变 LCD 页面、Logo、按键或电机控制逻辑。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 87892 | 120 | 52944 | `BUILD PASS` |
| Release | 78052 | 120 | 52936 | `BUILD PASS` |

`0.5.2` 已通过 ST-Link 写入 Application 区并由 GDB 从 Application 向量直接启动。串口状态为：

```text
[TEL] ... fw=0.5.2 build=1 ... control=STOPPED ... critical_tasks=1
[TEL] ... service_task=RUNNING control_task=RUNNING diagnostics_task=RUNNING display_task=RUNNING
[TEL] ... display_period_ms=1 display_expected_ms=1 ... left_pwm=0 right_pwm=0
[TEL] ... lcd=READY ...
```

停止 OpenOCD 后 `ping` 仍返回正式 `[RSP]`，Application 保持运行。该验证证明 LCD DMA 状态机
能够完成一帧并进入 `READY`，不代表四页文字、颜色、Logo 或 PB8 循环已完成人工目视验收；
这些项目继续保持 `NOT VERIFIED`。普通复位仍会由 Bootloader 恢复 QSPI confirmed
`0.3.0 build1`，因此本节也不构成 `0.5.2` 正常上电启动或 OTA 确认证据。

## 2026-08-18 LCD UI 美化与电量估算构建（0.6.0 build1）

本轮将 taifei Logo 裁剪为 40x40 RGB565，并增加透明掩码去除白色背景；LCD 四页增加深色
标题栏、分隔面板、状态色和总览页电量条。百分比使用 9.0--12.6 V 电压窗口估算，实际电池
化学体系、串数和放电曲线确认后仍需校准。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 88612 | 120 | 52944 | `BUILD PASS` |
| Release | 78660 | 120 | 52944 | `BUILD PASS` |

`0.6.0` 已通过 ST-Link 写入 Application 区并由 GDB 从 Application 向量直接启动。`status`
确认 `fw=0.6.0 build=1`、四任务 `RUNNING`、`control=STOPPED`、左右 PWM 为零，LCD 在 1 s
刷新周期中可观察到 `DRAWING` 并回到 `READY`；停止 OpenOCD 后 `ping` 仍正常响应。当前电机
电源未接入，ADC 报告 `supply_mv=0`，因此百分比实际电压效果仍需带电池观察。

透明 Logo、百分比和电量条、四页颜色布局以及 PB8 循环尚待人工目视，均保持
`NOT VERIFIED`。本轮未执行电机命令，也未改变 SR501 或 ICM45686 的既有硬件结论；
初次直接写内部 Application 后普通复位仍会恢复当时 QSPI confirmed `0.3.0 build1`。

为使本版断电后可由 Bootloader 正常启动，随后从 `build/arm-release/application.bin` 生成
`0.6.0 build1` OTA 包并通过 `/dev/ttyUSB0`、115200 8N1 发送：

```text
payload=78788 bytes
payload_crc32=0x8DE78C48
bin_sha256=93725db9139c330f4b25b29254ecf475b4825329ae39694804b4a1af11d675a1
ota_sha256=fc1e70013396d729f853253011b35ba0923aca4b3eacc63d98e775aaea7203a8
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
```

健康窗口后 `status` 报告 `fw=0.6.0 build=1`、`ota_confirmation=CONFIRMED`、四任务
`RUNNING`、`control=STOPPED` 和左右 PWM 为零。随后通过 ST-Link 执行普通复位，目标板继续
报告 `fw=0.6.0 build=1`、`ota_confirmation=NOT_REQUIRED`、`lcd=READY`，证明内部镜像与
QSPI confirmed 基线已经一致。真实断电重上电未执行，不能据此记录断电启动 `PASS`。

## 2026-08-18 LCD 信息层级与大号电量显示（0.7.0 build1）

本轮将 LCD 文本布局扩展为 10 个可独立设置坐标、字号和颜色的区域。总览页使用 3 倍字号
显示电压和百分比，并将电量条扩大到 288x16；电机页改为左右双栏，传感器页和系统页重新
按信息层级排版。四页继续使用统一标题栏、内容面板、分隔线、底部状态栏和 40x40 透明 Logo。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
```

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 89204 | 120 | 53088 | `BUILD PASS` |
| Release | 79264 | 120 | 53080 | `BUILD PASS` |

Release `application.bin` 和 OTA 包信息：

```text
payload=79392 bytes
payload_crc32=0x7B2868B2
bin_sha256=c3bcc39853b10a530853e445c1c2e9aabf2dae073c849a3d1d7e811ca029beac
ota_sha256=e012ab69122be5dc46f0a0c5cdce72e727f556f9573ebf04d65c7d58d7794202
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
```

健康窗口后 `status` 报告 `fw=0.7.0 build=1` 和 `ota_confirmation=CONFIRMED`。随后通过
ST-Link 执行普通复位，复位后再次读取 `status`：

```text
fw=0.7.0 build=1 reset=SOFTWARE critical_tasks=1
service_task=RUNNING control_task=RUNNING diagnostics_task=RUNNING display_task=RUNNING
control=STOPPED left_pwm=0 right_pwm=0
lcd=READY ota_confirmation=NOT_REQUIRED
```

当前 ADC 报告 0 mV，所以页面应显示 0% 和空进度条。该串口结果只证明软件状态、普通复位
启动和零 PWM；大号电量显示、透明 Logo、双栏/分组布局无重叠以及 PB8 四页循环仍需人工
目视确认，保持 `NOT VERIFIED`。真实断电重上电和四路 PWM 电气测量未执行，继续 `DEFERRED`。

## 2026-08-18 LCD 中性配色修正（0.7.1 build1）

本轮保留 `0.7.0` 的页面信息、坐标、字号和透明 Logo，将标题栏、内容面板和背景调整为中性
深灰，移除贯穿全屏的青色横线；标题下方仅保留短青色强调线，页脚和电机双栏分隔改为灰色。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 89216 | 120 | 53088 | `BUILD PASS` |
| Release | 79272 | 120 | 53080 | `BUILD PASS` |

Release 产物：

```text
payload=79400 bytes
payload_crc32=0xAABB8733
bin_sha256=6c828adf6845add29a1be8b23476c8363b9a7beec478056604e889ee0fbe9738
ota_sha256=febdea7b478d970a3d26259b5d0d01f6a5946196fc8498e83cba845c17b902e9
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
```

健康窗口后报告 `fw=0.7.1 build=1`、`ota_confirmation=CONFIRMED`。ST-Link 普通复位后再次
读取状态：四任务均为 `RUNNING`，`control=STOPPED`，左右 PWM 为零，`lcd=READY`，
`ota_confirmation=NOT_REQUIRED`。串口证据不等于配色人工验收，中性背景和短标题强调线的
实际观感仍保持 `NOT VERIFIED`。

用户随后确认当前可见页面的中性配色“看着还行”。该反馈只覆盖配色观感，不自动覆盖四页
内容、Logo、文字重叠和 PB8 循环，因此 LCD 四页整体仍保持 `NOT VERIFIED`。

## 2026-08-18 ICM45686 正式启用与首次识别（0.8.0 build1）

打开 `ENABLE_ICM45686` 后，Application 正式执行 WHO_AM_I、寄存器配置、FIFO/DMA、静止零偏
和 Mahony 融合路径。启动日志根据快照输出真实状态，不再固定报告 `INITIALIZED`。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 103160 | 120 | 53384 | `BUILD PASS` |
| Release | 92076 | 120 | 53376 | `BUILD PASS` |

Release 产物：

```text
payload=92204 bytes
payload_crc32=0x1484C456
bin_sha256=80f6c6732bf342c40112ded6d3a35e2148bbd968ad4be9b1ecc21970960b48b1
ota_sha256=2c680954a4e431aa576f94729ce83f353f8e54f3e7053f99c9742e2afb298ed2
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
```

首次启动日志：

```text
[LOG] ... module=imu event=NOT_FOUND device=ICM45686 who_am_i=0xFF
[TEL] ... fw=0.8.0 build=1 ... control=STOPPED ... left_pwm=0 right_pwm=0
[TEL] ... imu=NOT_FOUND imu_whoami=0xFF imu_samples=0 imu_fifo_frames=0
```

健康窗口后 `ota_confirmation=CONFIRMED`。ST-Link 普通复位后仍报告 `fw=0.8.0 build=1`、
`ota_confirmation=NOT_REQUIRED`、四任务 `RUNNING`、控制停止和左右 PWM 为零。ADC 同时报告
约 12.22 V。`WHO_AM_I=0xFF` 通常表示 MISO 保持高电平，应优先检查模块 3.3 V、共地、PD0 CS、
PC10 SCK、PC11 MISO 和 PC12 MOSI；在读到规定值 `0xE9` 前不得记录硬件通过，也不进入
FIFO 连续性、零偏和安装方向结论。

## 2026-08-18 ICM45686/SR501 接线整理后复核（0.8.0 build1）

用户将模块电源和信号线重新整理到面包板后，未重新烧录，直接读取运行中的 Application：

```text
fw=0.8.0 build=1 reset=PIN
control=STOPPED left_pwm=0 right_pwm=0
imu=NOT_FOUND imu_whoami=0x00 imu_samples=0 imu_fifo_frames=0
sr501=READY sr501_raw=0 sr501_motion=0 sr501_count=14 sr501_warmup_ms=0
lcd=READY ota_confirmation=NOT_REQUIRED
```

ICM45686 的返回值从整理前 `0xFF` 变为 `0x00`，仍不是规定的 `0xE9`，因此目前不能判断
WHO_AM_I、FIFO/DMA 或姿态数据通过。`0x00` 优先指向模块端未供电、CS 未选中、MISO 被拉低或
模块针脚定义与当前接线不一致；应在模块插针处测量 3.3 V 和 GND，并用万用表逐根确认
`PD0-CS`、`PC10-SCK`、`PC11-MISO`、`PC12-MOSI` 的连续性。

SR501 已完成预热并处于 `READY`，本次运行累计 `14` 个事件，但没有进行受控的人体移动、
静止和持续高电平重复计数测试，仍保持 `NOT VERIFIED`。本次复核未执行任何电机命令，四路
PWM 保持零。

## 2026-08-18 其他硬件在线复核（0.8.0 build1）

跳过 ICM45686 后，通过 `/dev/ttyUSB0`、115200 8N1 对当前板上运行中的 Application
执行只读状态、CAN 状态和编码器查询，并运行已有的 QSPI 目标读写自检。整个过程没有发送
电机、PID 或 OTA 命令，控制保持 `STOPPED`，左右 PWM 保持零。

`status` 实测摘要：

```text
fw=0.8.0 build=1
supply_valid=1 supply_mv=12206..12215
rtc_valid=1
service_task=RUNNING control_task=RUNNING diagnostics_task=RUNNING display_task=RUNNING
control=STOPPED left_pwm=0 right_pwm=0 overrun=0 missed=0
qspi_read=1 qspi_id=1 qspi_jedec=EF4017 qspi_capacity_bytes=8388608
lcd=READY
imu=NOT_FOUND imu_whoami=0x00 imu_samples=0 imu_fifo_frames=0
```

CAN 查询返回：

```text
activity=8 lec=7 dlec=7 tec=0 rec=0 passive=0 warning=0
busoff=0 restricted=0 rxfill=0 txfree=3
warning_count=0 passive_count=0 busoff_count=0 protocol_error_count=0
rx_fifo_full_count=0 rx_fifo_lost_count=0 recovery_count=0 recovery_failure_count=0
```

编码器查询返回 `left_total=1 right_total=-2`，表示当前静止状态下没有持续计数异常。

QSPI 目标自检使用保留地址 `0x007FF000` 的 1024 字节区域，完成擦除、写入、回读和比较：

```text
[LOG] ... module=qspi event=RW_TEST state=PASS address=0x007FF000 size=1024
qspi_test=2
```

本轮可记录为 QSPI 目标读写 `HARDWARE PASS` 的新增证据；LCD 仅确认驱动状态为 `READY`，
四页排版和 PB8 循环仍需人工目视验收。ICM45686 和 SR501 高电平事件结论不变，继续后置。

## 2026-08-18 PID 持久化与架空轮启动复核（0.9.0 build1）

当前目标板运行 `0.9.0 build1`，测试前通过 `status` 确认 `control=STOPPED`、左右 PWM 为零，
车轮保持架空，遥测关闭。方向不在本轮重复验收，沿用已有实物结论。

PID 参数测试：

```text
pid left 210 310 1
[RSP] ... result=OK ... persistence=QUEUED sequence=0
[LOG] ... module=parameters event=SAVED sequence=1 error_count=0
pid show
[RSP] ... result=OK ... left_kp=210 left_ki=310 left_kd=1 ... persistence=STORED sequence=1
```

电机启动测试使用现有受控测试命令，左右各运行约 1 秒，测试结束自动停止：

```text
left:  encoder result -> left_total=5525; status -> left_encoder=5837 left_pwm=0 control=STOPPED
right: encoder result -> right_total=4556; status -> right_encoder=4598 right_pwm=0 control=STOPPED
```

两侧均能启动且编码器有明显变化，6500 占空比满足架空轮启动门槛；本结果不重新定义或覆盖
既有电机方向结论。低速闭环、停车稳定性和普通复位后的 QSPI 恢复仍待后续验证。

## 2026-08-18 开环 PWM 启动下限复核（0.9.1 build1）

为避免反复重编译，新增运行期命令 `motor duty <0..8499>`；命令只在测试未运行时修改
本次开环测试占空比，复位后回到默认 `6500`，不改变 PID 参数。每次测试前执行
`encoder zero`，使用显式 `motor <left|right> forward confirm`，运行约 1.25 秒后自动停止，
再读取 `encoder result`。

条件：目标板 `0.9.1 build1`、供电约 `12.22 V`、车轮架空、控制初始为 `STOPPED`，方向沿用
已有实物验收结论。结果如下：

| 侧别 | 临界测试 | 结果 |
| --- | --- | --- |
| 左 | `2900` 无计数；`3000` 偶发；`3200` 仍有一次未启动；`3500` 连续两次约 `1144/1173` | 可靠下限取 `3500` |
| 右 | `3200` 连续三次无计数；`3500` 连续三次约 `938/998/1004` | 可靠下限取 `3500` |

因此当前工程建议的架空轮开环启动值为 `3500/8499`，占满量程约 `41.2%`。更低档位可能在
某次测试中偶尔转动，但受静摩擦、供电电压、负载和启动时序影响，不作为可靠启动下限，也不
直接用于闭环 PID 输出限制。测试结束后已发送 `motor stop` 和 `motor duty 6500`，最终
`control=STOPPED`、左右 PWM 为零。

## 2026-08-18 控制收尾与低速 PID 响应（0.10.0 build1）

`0.10.0 build1` 通过 Debug/Release 构建和 UART OTA，目标板报告 `ota_confirmation=CONFIRMED`、
四任务 `RUNNING`、供电约 `12.206 V`、故障为零。

低速 PID 架空轮测试：

```text
pid target 5 5
left_speed=3..7   right_speed=5..6
left_pwm=1935..2043  right_pwm=2238..2408
left_encoder=173..1908  right_encoder=490..1951
pid stop -> control=STOPPED left_pwm=0 right_pwm=0
```

这证明当前 PID、编码器反馈和停止路径能在短时架空轮测试中工作；不等同于带负载稳定性、
停车距离或长时间运行通过。

控制保护代码已构建并上板：目标加减速限制为每个 10 ms tick 最多 5 counts/tick；控制任务
读取 diagnostics 任务缓存的 ADC 电压，低于 `9000 mV` 锁存 `CHASSIS_FAULT_UNDERVOLTAGE`；
单周期编码器增量超过 `500` counts/tick 锁存 `CHASSIS_FAULT_ENCODER`。本轮只观察正常供电和
正常编码器，无故障注入，因此两项保持 `NOT VERIFIED`。

ICM45686 软件侧寄存器复核：PC10/PC11/PC12 为 SPI3 AF6，SPI3 时钟使能，PD0 CS 为输出高；
目标板仍返回 `WHO_AM_I=0x00`、`imu_samples=0`。软件配置无异常证据，模块供电、CS/SPI 线序
和模块型号仍需实测确认。

## 2026-08-18 当前工作树收尾构建（0.10.0 build1）

修正安全故障位宏格式和控制周期异常路径缩进后，重新执行 Debug/Release 配置和构建：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 106492 | 120 | 53520 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |
| Release | 94852 | 120 | 53512 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |

从最新 Release ELF 重新生成 `application.bin` 和 `app-v0.10.0-b1.ota`；OTA payload 为
`94980` 字节，CRC32 为 `0x9BAB75E6`。本次只重建和校验产物，未再次烧录目标板，因此不改变
已有 `0.10.0 build1` 上板记录。

宿主机回归：`tools/ota` unittest 全部 13 项通过；参数记录和 UART 64 位格式化 C 宿主机测试
使用 `-std=c11 -Wall -Wextra -Werror` 编译运行通过。`git diff --check` 通过。

## 2026-08-19 roll/pitch Kalman 组件实现（0.11.1 build1）

新增独立的 roll/pitch 两状态 Kalman（角度 + 陀螺零偏）输出。Mahony 输出保持不变，Kalman
结果进入 `BspIcm45686Snapshot` 和 `sensors` 诊断字段；LCD 继续显示原 Mahony 姿态，避免
在硬件数据尚未确认前改变已验收页面。

宿主机测试覆盖静止零偏初始化、roll/pitch 初始姿态、陀螺 yaw 对照、四元数归一化，以及线性
加速度导致重力幅值无效时只做 Kalman 预测。测试使用 `-std=c11 -Wall -Wextra -Werror` 编译
运行通过。

参考本地 ICM45686 数据手册和官方驱动后确认 `SREG_CTRL.SREG_DATA_ENDIAN_SEL` 位于 bit 1；原代码
误写 bit 0，器件保持默认小端而解析器按大端解释。修正为 bit 1 并增加初始化回读校验后，目标板
读取 `WHO_AM_I=0xE9`。调试快照连续 588 帧，`fifo_parse_error_count=0`、
`timestamp_error_count=0`、`dma_timeout_count=0`、`transfer_error_count=0`，采样周期稳定为
`0.01 s`。调试暂停引发 2 次 FIFO full，两次 flush 均成功恢复。

静止 200 样本后 `calibrated=1`、Mahony 和 Kalman 均有效；当时加速度约
`[-0.075, -0.682, 9.823] m/s²`，陀螺零偏约
`[0.00219, -0.00109, 0.00207] rad/s`，Kalman roll/pitch 约 `[-69, 7] mrad`。

全量 diff 审阅后修正 Kalman 协方差观测更新，使四项均使用预测矩阵，并对 ±π 跨界创新做角度
归一化；宿主机测试增加协方差对称/非负与跨界回归。该修复按版本规则从中间 `0.11.0 build1`
提升为 `0.11.1 build1`，不以相同版本号覆盖不同代码。

`0.11.1` Release 已通过 UART OTA 完成 `STAGED -> INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL
VERIFIED -> CONFIRMED`。普通复位后 Bootloader 报告 metadata state `0x5`；5.6 秒后的完整
`status` 报告 `fw=0.11.1 build=1`、`ota_confirmation=NOT_REQUIRED`、供电 `12.206 V`、
四任务 `RUNNING`、`fault=0`、`control=STOPPED`、左右 PWM 为零，并再次观察到 224 个 IMU 帧、
零 FIFO/时间戳错误和 `imu_kalman=1`。模块安装位置和方向未固定，正负轴向动作、动态姿态和
长期漂移验证已 `DEFERRED`。

同日通过 `/dev/ttyUSB0` 在架空、零 PWM 状态下进行一次临时安装方向观察。近水平状态的 Kalman
roll/pitch 为 `[-69, 8] mrad`；用户保持车体左侧抬高后读数为 `[-52, -212] mrad`，IMU 样本数
从 `169408` 增至 `178212`，FIFO 解析和 timestamp 错误仍为零，控制保持 `STOPPED`、左右 PWM
为零。由于模块安装位置和方向尚未固定，该现象只表明临时摆放时左右倾斜主要进入当前 pitch
通道，不能据此确定最终轴交换、符号或安装矩阵。用户决定将安装轴向、动态响应和静止回归统一
后置，状态为 `DEFERRED`，本轮不修改融合算法或轴映射。

最新 CMake/Ninja 构建结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 107576 | 120 | 53624 | `BUILD PASS` |
| Release | 95748 | 120 | 53616 | `BUILD PASS` |

Release BIN 为 `95876` 字节，CRC32 `0x88BB01DD`；OTA 包为 `95940` 字节。ICM45686 与
IMU fusion C 宿主机测试均使用 `-std=c11 -Wall -Wextra -Werror` 编译运行通过，OTA Python
13 项通过，`git diff --check` 通过。

## 2026-08-19 统一采样时间戳与轮式里程计（0.12.0 build1）

编码器控制采样、ADC 转换完成和 ICM45686 FIFO 数据均增加本地单调采样时刻与数据年龄。
轮式里程计使用 `1320 counts/rev`、`65 mm` 轮径和 `220 mm` 轮距，采用差速圆弧积分输出
`x/y/heading`、左右累计距离、线速度和角速度；UART/LCD 接入和 `odometry reset` 已实现。

宿主机里程计测试使用 `-std=c11 -Wall -Wextra -Werror` 编译执行，覆盖初始化参数、直行、
原地旋转、圆弧积分、航向归一化、时间戳/年龄和复位，结果通过。CMake/Ninja
构建结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 110024 | 120 | 54144 | `BUILD PASS` |
| Release | 97960 | 120 | 54136 | `BUILD PASS` |

Release BIN/payload 为 `98088` 字节，CRC32 `0x124D4C30`，SHA-256
`e91540cf5cb93cce6c6876d66c1553aee0bc6ade7ad6b1f3227949a557e8ad20`；OTA 包为 `98152`
字节，SHA-256 `d7c8a0cf85f085bf1d9d82c44745942818f6b1d1f22d3c8aa7a98f66255df993`。OTA Python
13 项通过，`git diff --check` 通过。该段记录构建阶段证据；随后 OTA 启动复核见下一节。时间
对齐、里程计符号、直线距离和原地旋转角度仍保持 `NOT VERIFIED`。

## 2026-08-19 0.12.0 build1 UART OTA 与启动复核

使用 `/dev/ttyUSB0`、115200 8N1，将 `build/arm-release/app-v0.12.0-b1.ota`（98152 字节，
CRC32 `0x124D4C30`）发送到板上 confirmed `0.11.1 build1`。发送工具完成 98152/98152 字节
传输，Bootloader 日志依次报告：

```text
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
```

随后 Application 启动日志报告 `fw=0.12.0 build=1`，健康窗口结束后状态为
`ota_confirmation=CONFIRMED`。复核 `status` 报告：

```text
fw=0.12.0 build=1 ota_confirmation=CONFIRMED
control=STOPPED fault=0x00000000 left_pwm=0 right_pwm=0
service_task=RUNNING control_task=RUNNING diagnostics_task=RUNNING display_task=RUNNING
odom_valid=1 odom_x_mm=0 odom_y_mm=0 odom_heading_mrad=0
imu=READY imu_whoami=0xE9 imu_fifo_errors=0 imu_timestamp_errors=0 lcd=READY
```

供电约 `12.188 V`。本轮只做 OTA、启动和静态安全状态复核，没有驱动车轮；断电恢复、里程计
方向/距离/旋转角度和 LCD 动态里程计显示仍保持 `NOT VERIFIED`。

## 2026-08-19 0.13.0 build1 LCD UI 构建

本轮仅调整 LCD 绘制层和产品版本：增加四页页眉位置指示、上下内容区对比、暖色电量强调，
并将电机、传感器、系统页的长串缩写改为分组标签；保留四页、PB8 单键切换、透明 Logo、
电量条和 `SystemStatusSnapshot` 数据来源。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 110292 | 120 | 54144 | `BUILD PASS` |
| Release | 98136 | 120 | 54136 | `BUILD PASS` |

Release BIN/payload 为 `98264` 字节，CRC32 `0x4659F611`，SHA-256
`ccf30f044accaf61975bf12217df38dc8932d795d35d84561ae43e135cc2367b`；OTA 包为 `98328`
字节，SHA-256 `c894baa29d541d1ecd711f7afffd66c48521d837b818ba45adb0418d5676946e`。OTA Python
13 项和 `git diff --check` 通过。随后已完成 UART OTA，UI 视觉、文字不重叠和四页切换仍需
目标板人工目视确认。

## 2026-08-19 0.13.0 build1 UART OTA 与启动复核

使用 `/dev/ttyUSB0`、115200 8N1 发送
`build/arm-release/app-v0.13.0-b1.ota`（98328 字节，payload CRC32 `0x4659F611`）。OTA 链路完成：

```text
STAGED
INSTALL VERIFIED
TRIAL COMMITTED
TRIAL VERIFIED
CONFIRMED
```

烧录后的在线 `status` 复核报告：

```text
fw=0.13.0 build=1 ota_confirmation=CONFIRMED
control=STOPPED fault=0x00000000 left_pwm=0 right_pwm=0
service_task=RUNNING control_task=RUNNING diagnostics_task=RUNNING display_task=RUNNING
lcd=DRAWING imu=READY imu_whoami=0xE9 imu_fifo_errors=0 imu_timestamp_errors=0
odom_valid=1 odom_x_mm=0 odom_y_mm=0 odom_heading_mrad=0
```

`lcd=DRAWING` 是在线读取时的逐行 DMA 刷新状态，证明显示任务在运行，不证明画面布局正确。
本轮没有发送电机命令，控制保持停止且左右 PWM 软件状态为零；四页切换、文字、Logo、电量条
和新配色仍为 `NOT VERIFIED`，等待人工目视确认。
