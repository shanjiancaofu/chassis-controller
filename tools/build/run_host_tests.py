#!/usr/bin/env python3
"""Compile and run the firmware's HAL-independent C unit tests."""

from __future__ import annotations

import pathlib
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[2]
APP = ROOT / "firmware/application/stm32g474"
STUBS = ROOT / "tools/build/host_stubs"

TESTS = {
    "device_init": (
        "DEVICE_INIT_HOST_TEST",
        ["tests/unit/test_device_init.c", "kernel/device.c", "kernel/init.c"],
    ),
    "can_transport": (
        "CAN_TRANSPORT_HOST_TEST",
        ["tests/unit/test_can_transport.c", "subsys/communication/can/can_transport.c",
         "drivers/can/can_common.c"],
    ),
    "chassis_protocol": (
        "CHASSIS_PROTOCOL_HOST_TEST",
        ["tests/unit/test_chassis_protocol.c",
         "subsys/communication/can/chassis_protocol.c"],
    ),
    "chassis_control_flow": (
        "CHASSIS_CONTROL_FLOW_HOST_TEST",
        ["tests/unit/test_chassis_control_flow.c",
         "subsys/communication/can/chassis_protocol.c",
         "app/chassis/command_manager.c",
         "app/chassis/differential_drive.c",
         "app/safety/fault_manager.c",
         "app/safety/safety_manager.c"],
    ),
    "command_manager": (
        "COMMAND_MANAGER_HOST_TEST",
        ["tests/unit/test_command_manager.c", "app/chassis/command_manager.c"],
    ),
    "fault_manager": (
        "FAULT_MANAGER_HOST_TEST",
        ["tests/unit/test_fault_manager.c", "app/safety/fault_manager.c"],
    ),
    "feedback_watchdog": (
        "FEEDBACK_WATCHDOG_HOST_TEST",
        ["tests/unit/test_feedback_watchdog.c", "app/chassis/feedback_watchdog.c"],
    ),
    "gpio_stm32": (
        "GPIO_STM32_HOST_TEST",
        ["tests/unit/test_gpio_stm32.c", "drivers/gpio/gpio_common.c",
         "drivers/gpio/gpio_stm32.c", "kernel/device.c"],
    ),
    "safety_manager": (
        "SAFETY_MANAGER_HOST_TEST",
        ["tests/unit/test_safety_manager.c", "app/safety/safety_manager.c",
         "app/safety/fault_manager.c"],
    ),
    "icm45686": (
        "ICM45686_HOST_TEST",
        ["tests/unit/test_icm45686.c", "lib/icm45686/icm45686.c"],
    ),
    "imu_fusion": (
        "IMU_FUSION_HOST_TEST",
        ["tests/unit/test_imu_fusion.c", "lib/imu_fusion/imu_fusion.c"],
    ),
    "odometry": (
        "ODOMETRY_HOST_TEST",
        ["tests/unit/test_odometry.c", "app/chassis/odometry.c",
         "app/chassis/differential_drive.c"],
    ),
    "ota_metadata": (
        "OTA_METADATA_HOST_TEST",
        ["tests/unit/test_ota_metadata.c", "subsys/communication/ota/ota_metadata.c",
         "lib/crc/crc32.c"],
    ),
    "ota_uart_arm_guard": (
        "OTA_UART_ARM_GUARD_HOST_TEST",
        ["tests/unit/test_ota_uart_arm_guard.c"],
    ),
    "parameter_record": (
        "PARAMETER_RECORD_HOST_TEST",
        ["tests/unit/test_parameter_record.c", "app/parameters/parameter_record.c",
         "lib/crc/crc32.c"],
    ),
    "uart_messages": (
        "UART_MESSAGES_HOST_TEST",
        ["tests/unit/test_uart_messages.c",
         "subsys/communication/uart/uart_messages.c"],
    ),
    "wheel_controller": (
        "WHEEL_CONTROLLER_HOST_TEST",
        ["tests/unit/test_wheel_controller.c", "app/chassis/wheel_controller.c",
         "lib/pid/speed_pid.c"],
    ),
    "uart_stm32": (
        "UART_STM32_HOST_TEST",
        ["tests/unit/test_uart_stm32.c", "drivers/uart/uart_stm32.c"],
    ),
    "flash_stm32_qspi": (
        "FLASH_STM32_QSPI_HOST_TEST",
        ["tests/unit/test_flash_stm32_qspi.c",
         "drivers/flash/flash_stm32_qspi.c"],
    ),
}


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="chassis-host-tests-") as directory:
        output = pathlib.Path(directory)
        for name, (definition, sources) in TESTS.items():
            executable = output / name
            command = [
                "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                f"-D{definition}", f"-I{STUBS}", f"-I{APP}",
                f"-I{APP / 'include'}",
                *(str(APP / source) for source in sources), "-lm", "-o", str(executable),
            ]
            subprocess.run(command, check=True)
            subprocess.run([str(executable)], check=True)
            print(f"PASS {name}")
    print(f"{len(TESTS)} C host tests passed")


if __name__ == "__main__":
    main()
