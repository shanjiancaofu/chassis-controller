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
    injection_time = time.monotonic()
    port.write(command.encode())
    port.flush()
    lines: list[str] = []
    response = ""
    fault_system: dict[str, str] = {}
    fault_motor: dict[str, str] = {}
    observed_fault_at: float | None = None
    response_timestamp_ms: int | None = None
    systems_by_seq: dict[str, dict[str, str]] = {}
    motors_by_seq: dict[str, dict[str, str]] = {}
    receive_buffer = ""
    deadline = time.monotonic() + 0.75
    next_status = injection_time
    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_status:
            port.write(b"status\r\n")
            port.flush()
            next_status = now + 0.05
        chunk = port.read(port.in_waiting or 1)
        if not chunk:
            continue
        receive_buffer += chunk.decode("ascii", errors="replace")
        complete_lines = receive_buffer.split("\n")
        receive_buffer = complete_lines.pop()
        for line in complete_lines:
            line = line.rstrip("\r")
            lines.append(line)
            if "command=encoder_fail" in line:
                response = line
                fields = parse_fields(line)
                try:
                    response_timestamp_ms = int(fields["ts_ms"])
                except (KeyError, ValueError):
                    response_timestamp_ms = None
            if "section=system" in line:
                fields = parse_fields(line)
                systems_by_seq[fields.get("seq", "")] = fields
                if fields.get("fault") == "0x00000010":
                    candidate_motor = motors_by_seq.get(fields.get("seq", ""), {})
                    if candidate_motor.get("left_pwm") == "0" and candidate_motor.get("right_pwm") == "0":
                        fault_system = fields
                        fault_motor = candidate_motor
                    if observed_fault_at is None:
                        observed_fault_at = time.monotonic()
            if "section=motor" in line and fault_system:
                fields = parse_fields(line)
                motors_by_seq[fields.get("seq", "")] = fields
                if fields.get("seq") == fault_system.get("seq"):
                    fault_motor = fields
            elif "section=motor" in line:
                fields = parse_fields(line)
                motors_by_seq[fields.get("seq", "")] = fields
                matching_system = systems_by_seq.get(fields.get("seq", ""), {})
                if matching_system.get("fault") == "0x00000010" and fields.get("left_pwm") == "0" and fields.get("right_pwm") == "0":
                    fault_system = matching_system
                    fault_motor = fields
                    if observed_fault_at is None:
                        observed_fault_at = time.monotonic()
        if fault_system and fault_motor.get("seq") == fault_system.get("seq") and fault_motor.get("left_pwm") == "0" and fault_motor.get("right_pwm") == "0":
            break
    if not response:
        response = next((line for line in lines if "command=encoder_fail" in line), "")
    motor, system = snapshot(lines)
    if fault_system:
        system = fault_system
    if fault_motor:
        motor = fault_motor
    fault_time = observed_fault_at or time.monotonic()
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
        "device_fault_timestamp_ms": system.get("ts_ms", "missing"),
        "device_injection_timestamp_ms": response_timestamp_ms or "missing",
        "raw_response": response,
    }
    try:
        device_latency_ms = int(result["device_fault_timestamp_ms"]) - int(result["device_injection_timestamp_ms"])
    except (TypeError, ValueError):
        device_latency_ms = None
    result["device_fault_latency_ms"] = device_latency_ms if device_latency_ms is not None else "inconclusive"
    device_latency_pass = device_latency_ms is not None and 0 <= device_latency_ms <= 510
    result["passed"] = bool(
        result["armed"]
        and result["encoder_fault_latched"]
        and result["left_pwm"] == "0"
        and result["right_pwm"] == "0"
        and device_latency_pass
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
