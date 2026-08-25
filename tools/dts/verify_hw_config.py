#!/usr/bin/env python3
"""Fail configuration when CubeMX and board Devicetree facts diverge."""

from __future__ import annotations

import argparse
import json
import pathlib


def parse_ioc(path: pathlib.Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        if "=" in raw:
            key, value = raw.split("=", 1)
            values[key] = value
    return values


def integer(values: dict[str, str], key: str) -> int:
    if key not in values:
        raise ValueError(f"missing CubeMX property {key}")
    return int(values[key], 0)


def require_equal(name: str, actual, expected) -> None:
    if actual != expected:
        raise ValueError(f"{name}: CubeMX={actual!r}, DTS={expected!r}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ioc", type=pathlib.Path, required=True)
    parser.add_argument("--manifest", type=pathlib.Path, required=True)
    args = parser.parse_args()

    ioc = parse_ioc(args.ioc)
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    devices = manifest["devices"]
    can = devices["can0"]
    fdcan_clock = integer(ioc, "RCC.FDCANFreq_Value")
    nominal_total = 1 + integer(ioc, "FDCAN2.NominalTimeSeg1") + integer(
        ioc, "FDCAN2.NominalTimeSeg2")
    data_total = 1 + integer(ioc, "FDCAN2.DataTimeSeg1") + integer(
        ioc, "FDCAN2.DataTimeSeg2")
    nominal_bitrate = fdcan_clock // integer(ioc, "FDCAN2.NominalPrescaler") // nominal_total
    data_bitrate = fdcan_clock // integer(ioc, "FDCAN2.DataPrescaler") // data_total
    nominal_sample = round(
        1000 * (1 + integer(ioc, "FDCAN2.NominalTimeSeg1")) / nominal_total)
    data_sample = round(
        1000 * (1 + integer(ioc, "FDCAN2.DataTimeSeg1")) / data_total)

    require_equal("MCU", ioc.get("Mcu.CPN"), "STM32G474VET6")
    require_equal("CAN compatible", can["compatible"], "st,stm32g4-fdcan")
    require_equal("CAN nominal bitrate", nominal_bitrate, can["bitrate"])
    require_equal("CAN nominal sample point", nominal_sample,
                  can["sample-point"])
    require_equal("CAN data bitrate", data_bitrate, can["bitrate-data"])
    require_equal("CAN data sample point", data_sample,
                  can["sample-point-data"])
    require_equal("CAN standard filters", integer(ioc, "FDCAN2.StdFiltersNbr"),
                  can["standard-filter-count"])
    for prefix in ("SPI2.", "SPI3.", "QUADSPI1.", "TIM2.", "TIM4.",
                   "TIM8.", "USART1.", "ADC1.", "RTC.", "IWDG."):
        if not any(key.startswith(prefix) for key in ioc):
            raise ValueError(f"CubeMX peripheral missing: {prefix[:-1]}")

    expected_compatibles = {
        "imu0": "invensense,icm45686",
        "display0": "sitronix,st7789v",
        "flash0": "winbond,w25q64",
        "drive0": "st,stm32-pwm-drive",
        "left_encoder": "st,stm32-tim-encoder",
        "right_encoder": "st,stm32-tim-encoder",
        "power0": "st,stm32-adc-power",
    }
    for device_id, compatible in expected_compatibles.items():
        require_equal(f"{device_id} compatible",
                      devices[device_id]["compatible"], compatible)
    require_equal("E-stop EXTI mode", ioc.get("PD2.GPIO_ModeDefaultEXTI"),
                  "GPIO_MODE_IT_FALLING")
    require_equal("E-stop pull", ioc.get("PD2.GPIO_PuPd"), "GPIO_PULLUP")
    estop_gpio = manifest["phandle_arrays"]["estop0"]["gpios"][0]
    require_equal("E-stop GPIO controller", estop_gpio["controller"], "gpiod")
    require_equal("E-stop GPIO pin/flags", estop_gpio["cells"], [2, 1])
    print("CubeMX/DTS hardware configuration verified")


if __name__ == "__main__":
    main()
