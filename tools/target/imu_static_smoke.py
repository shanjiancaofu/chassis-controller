#!/usr/bin/env python3
"""Measure target IMU continuity from two bounded UART status snapshots."""

from __future__ import annotations

import argparse
import pathlib
import sys
import time

import serial

from report import RegressionReport
from serial_smoke import request_status


def value(section: dict[str, str], name: str) -> int:
    return int(section.get(name, "-1"), 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=30.0)
    parser.add_argument("--rate-tolerance", type=float, default=5.0)
    parser.add_argument("--json", type=pathlib.Path)
    args = parser.parse_args()
    if args.duration <= 0 or args.rate_tolerance < 0:
        parser.error("duration must be positive and rate tolerance non-negative")

    report = RegressionReport()
    try:
        with serial.Serial(args.port, args.baud, timeout=0.05) as serial_port:
            _first_text, first = request_status(serial_port, 4.0)
            started = time.monotonic()
            time.sleep(args.duration)
            _last_text, last = request_status(serial_port, 4.0)
            elapsed = time.monotonic() - started
    except (OSError, ValueError) as error:
        report.add("IMU serial access", False, str(error))
        report.print()
        if args.json:
            report.write_json(args.json)
        return 1

    first_sensor = first.get("sensors", {})
    last_sensor = last.get("sensors", {})
    report.add("IMU status snapshots", bool(first_sensor) and bool(last_sensor))
    if first_sensor and last_sensor:
        samples = value(last_sensor, "imu_samples") - value(first_sensor, "imu_samples")
        fifo_frames = value(last_sensor, "imu_fifo_frames") - value(first_sensor, "imu_fifo_frames")
        rate_hz = samples / elapsed
        report.add("IMU READY", last_sensor.get("imu") == "READY", last_sensor.get("imu", "missing"))
        report.add(
            "IMU sample rate",
            abs(rate_hz - 100.0) <= args.rate_tolerance,
            f"{rate_hz:.3f} Hz samples={samples} duration={elapsed:.3f}s",
        )
        report.add("IMU FIFO continuity", fifo_frames == samples, f"frames={fifo_frames} samples={samples}")
        report.add(
            "IMU FIFO errors",
            value(last_sensor, "imu_fifo_errors") == value(first_sensor, "imu_fifo_errors"),
            last_sensor.get("imu_fifo_errors", "missing"),
        )
        report.add(
            "IMU timestamp errors",
            value(last_sensor, "imu_timestamp_errors")
            == value(first_sensor, "imu_timestamp_errors"),
            last_sensor.get("imu_timestamp_errors", "missing"),
        )
        report.add("IMU Kalman", last_sensor.get("imu_kalman") == "1")

    report.print()
    if args.json:
        report.write_json(args.json)
    return 0 if report.passed else 1


if __name__ == "__main__":
    sys.exit(main())
