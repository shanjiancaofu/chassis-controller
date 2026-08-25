#!/usr/bin/env python3
"""Run the lightweight chassis-controller target regression suite."""

from __future__ import annotations

import argparse
import pathlib
import sys

from canfd_smoke import run_canfd_smoke
from report import RegressionReport
from serial_smoke import run_serial_smoke


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--expected-build", type=int, required=True)
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--can-interface")
    parser.add_argument("--expect-bus-off", action="store_true")
    parser.add_argument("--json", type=pathlib.Path, default=pathlib.Path("_output/test/target-regression.json"))
    args = parser.parse_args()

    report = RegressionReport()
    serial_results, sections = run_serial_smoke(
        args.serial_port,
        args.baud,
        args.expected_version,
        args.expected_build,
        args.reset,
        3.0,
    )
    report.extend(serial_results)

    if args.can_interface:
        system = sections.get("system", {})
        motor = sections.get("motor", {})
        safe = system.get("fault") == "0x00000000" and motor.get("control") == "STOPPED"
        if safe:
            report.extend(run_canfd_smoke(args.can_interface, 2.0, args.expect_bus_off))
        else:
            report.add(
                "CAN safety precondition",
                False,
                f"control={motor.get('control')} fault={system.get('fault')}",
            )
    else:
        report.skip("CAN FD smoke", "provide --can-interface after physical bus setup")

    report.print()
    report.write_json(args.json)
    print(f"report: {args.json}")
    return 0 if report.passed else 1


if __name__ == "__main__":
    sys.exit(main())
