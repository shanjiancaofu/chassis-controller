#!/usr/bin/env python3
"""UART startup and status smoke test for a chassis-controller target."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
import time

from report import CheckResult, RegressionReport


def parse_fields(line: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in line.strip().split()[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def read_until_idle(serial_port, timeout: float, idle: float = 0.25) -> str:
    deadline = time.monotonic() + timeout
    idle_deadline = deadline
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = serial_port.read(serial_port.in_waiting or 1)
        if chunk:
            data.extend(chunk)
            idle_deadline = time.monotonic() + idle
        elif data and time.monotonic() >= idle_deadline:
            break
    return data.decode("ascii", errors="replace")


def request_status(serial_port, timeout: float) -> tuple[str, dict[str, dict[str, str]]]:
    serial_port.reset_input_buffer()
    serial_port.write(b"telemetry off\r\nstatus\r\n")
    serial_port.flush()
    text = read_until_idle(serial_port, timeout)
    sections: dict[str, dict[str, str]] = {}
    for line in text.splitlines():
        if not line.startswith("[TEL]"):
            continue
        fields = parse_fields(line)
        section = fields.get("section")
        if section:
            sections[section] = fields
    return text, sections


def run_serial_smoke(
    port: str,
    baud: int,
    expected_version: str,
    expected_build: int,
    reset: bool,
    startup_wait: float,
    ready_timeout: float = 15.0,
) -> tuple[list[CheckResult], dict[str, dict[str, str]]]:
    try:
        import serial
    except ImportError as error:
        return [CheckResult("serial dependency", "FAIL", str(error))], {}

    report = RegressionReport()
    try:
        with serial.Serial(port, baud, timeout=0.05) as serial_port:
            startup_text = ""
            if reset:
                serial_port.reset_input_buffer()
                subprocess.run(
                    [
                        "openocd",
                        "-f",
                        "interface/stlink.cfg",
                        "-f",
                        "target/stm32g4x.cfg",
                        "-c",
                        "init; reset run; shutdown",
                    ],
                    check=True,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                # Bootloader and Application banners are separated by image validation and
                # handoff. Keep the full bounded capture window instead of treating that gap
                # as end-of-log.
                startup_text = read_until_idle(serial_port, startup_wait, idle=startup_wait)
                report.add("bootloader startup", "BOOT: START" in startup_text)
                report.add(
                    "application startup",
                    f"fw={expected_version} build={expected_build}" in startup_text,
                )
            else:
                report.skip("startup log", "run with --reset to capture boot logs")

            time.sleep(startup_wait)
            ready_deadline = time.monotonic() + ready_timeout
            sections: dict[str, dict[str, str]] = {}
            while time.monotonic() < ready_deadline:
                _text, sections = request_status(serial_port, 4.0)
                if (
                    sections.get("sensors", {}).get("imu") == "READY"
                    and sections.get("sensors", {}).get("adc_valid") == "1"
                    and sections.get("communication", {}).get("qspi_id") == "1"
                    and sections.get("communication", {}).get("lcd")
                    in {"READY", "DRAWING"}
                    and sections.get("communication", {}).get("ota_confirmation")
                    in {"CONFIRMED", "NOT_REQUIRED"}
                ):
                    break
                time.sleep(0.5)
    except (OSError, subprocess.CalledProcessError) as error:
        report.add("serial access", False, str(error))
        return report.results, {}

    required = {"system", "motor", "sensors", "communication"}
    report.add("UART status sections", required.issubset(sections), ",".join(sorted(sections)))
    if not required.issubset(sections):
        return report.results, sections

    system = sections["system"]
    motor = sections["motor"]
    sensors = sections["sensors"]
    communication = sections["communication"]
    report.add(
        "firmware version",
        system.get("fw") == expected_version and system.get("build") == str(expected_build),
        f"fw={system.get('fw')} build={system.get('build')}",
    )
    report.add(
        "safe control state",
        system.get("fault") == "0x00000000" and motor.get("control") == "STOPPED",
        f"control={motor.get('control')} fault={system.get('fault')}",
    )
    report.add(
        "RTOS tasks",
        all(system.get(f"{name}_task") == "RUNNING" for name in ("service", "control", "diagnostics", "display")),
    )
    report.add(
        "motor zero output",
        all(motor.get(name) == "0" for name in ("left_target", "left_pwm", "right_target", "right_pwm")),
        (
            f"control={motor.get('control')} left_pwm={motor.get('left_pwm')} "
            f"right_pwm={motor.get('right_pwm')}"
        ),
    )
    report.add("ADC", sensors.get("adc_valid") == "1", f"mv={sensors.get('adc_mv')}")
    report.add("IMU", sensors.get("imu") == "READY", sensors.get("imu" , "missing"))
    report.add("QSPI", communication.get("qspi_id") == "1", communication.get("qspi_jedec", "missing"))
    report.add("LCD", communication.get("lcd") in {"READY", "DRAWING"}, communication.get("lcd", "missing"))
    report.add(
        "OTA state",
        communication.get("ota_confirmation") in {"CONFIRMED", "NOT_REQUIRED"},
        communication.get("ota_confirmation", "missing"),
    )
    return report.results, sections


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--expected-build", type=int, required=True)
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--startup-wait", type=float, default=3.0)
    parser.add_argument("--ready-timeout", type=float, default=15.0)
    parser.add_argument("--json", type=pathlib.Path)
    args = parser.parse_args()
    results, _sections = run_serial_smoke(
        args.port,
        args.baud,
        args.expected_version,
        args.expected_build,
        args.reset,
        args.startup_wait,
        args.ready_timeout,
    )
    report = RegressionReport()
    report.extend(results)
    report.print()
    if args.json:
        report.write_json(args.json)
    return 0 if report.passed else 1


if __name__ == "__main__":
    sys.exit(main())
