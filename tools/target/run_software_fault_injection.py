#!/usr/bin/env python3
"""Validate encoder read-failure fail-close using software injection only."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import time


MOTION_RE = re.compile(r"\[TEL\].*section=motor (.*)")
SYSTEM_RE = re.compile(r"\[TEL\].*section=system (.*)")


def parse_fields(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in text.split():
        if "=" in token:
            key, value = token.split("=", 1)
            fields[key] = value
    return fields


def read_lines(port, duration: float) -> list[str]:
    deadline = time.monotonic() + duration
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            data.extend(chunk)
    return data.decode("ascii", errors="replace").splitlines()


def snapshot(lines: list[str]) -> tuple[dict[str, str], dict[str, str]]:
    motor: dict[str, str] = {}
    system: dict[str, str] = {}
    for line in lines:
        motor_match = MOTION_RE.match(line)
        system_match = SYSTEM_RE.match(line)
        if motor_match:
            motor = parse_fields(motor_match.group(1))
        if system_match:
            system = parse_fields(system_match.group(1))
    return motor, system


def run_case(port, target: int, side: str) -> dict[str, object]:
    command = f"encoder fail {side} confirm\r\n"
    target_command = f"pid target {target} {target}\r\n".encode()
    port.reset_input_buffer()
    start = time.monotonic()
    while time.monotonic() - start < 0.60:
        port.write(target_command)
        port.flush()
        time.sleep(0.04)
    port.reset_input_buffer()
    injection_time = time.monotonic()
    port.write(command.encode())
    port.flush()
    lines = read_lines(port, 0.55)
    port.write(b"status\r\n")
    port.flush()
    lines.extend(read_lines(port, 0.45))
    motor, system = snapshot(lines)
    fault_time = time.monotonic()
    port.write(b"pid stop\r\n")
    port.flush()
    response = next((line for line in lines if "command=encoder_fail" in line), "")
    fault = int(system.get("fault", "0"), 16) if system.get("fault") else 0
    result = {
        "target": target,
        "side": side.upper(),
        "armed": "result=OK command=encoder_fail" in response,
        "fault": system.get("fault", "missing"),
        "encoder_fault_latched": bool(fault & (1 << 4)),
        "control": motor.get("control", "missing"),
        "left_pwm": motor.get("left_pwm", "missing"),
        "right_pwm": motor.get("right_pwm", "missing"),
        "fault_observation_latency_ms": round((fault_time - injection_time) * 1000.0, 1),
        "raw_response": response,
    }
    result["passed"] = bool(
        result["armed"]
        and result["encoder_fault_latched"]
        and result["left_pwm"] == "0"
        and result["right_pwm"] == "0"
    )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--target", type=int, choices=(100, -100), required=True)
    parser.add_argument("--side", choices=("left", "right"), required=True)
    parser.add_argument("--json", type=pathlib.Path)
    args = parser.parse_args()

    import serial

    with serial.Serial(args.serial_port, args.baud, timeout=0.05) as port:
        port.write(b"telemetry off\r\npid stop\r\n")
        port.flush()
        time.sleep(0.3)
        port.write(b"status\r\n")
        port.flush()
        initial_lines = read_lines(port, 0.4)
        _initial_motor, initial_system = snapshot(initial_lines)
        if initial_system.get("fault", "0x00000000") != "0x00000000":
            result = {
                "target": args.target,
                "side": args.side.upper(),
                "passed": False,
                "precondition": "fault must be cleared by reset before each case",
                "fault": initial_system.get("fault", "missing"),
            }
            report = {"passed": False, "results": [result]}
            print(json.dumps(report, indent=2, ensure_ascii=False))
            return 1
        results = [run_case(port, args.target, args.side)]
    report = {"passed": all(result["passed"] for result in results), "results": results}
    print(json.dumps(report, indent=2, ensure_ascii=False))
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
