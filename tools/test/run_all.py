#!/usr/bin/env python3
"""Run the complete host/build verification suite without Twister or west."""

from __future__ import annotations

import pathlib
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[2]

COMMANDS = (
    ["python3", "tools/build/run_host_tests.py"],
    ["python3", "-m", "unittest", "discover", "-s", "tools/kconfig/tests", "-p", "test_*.py", "-v"],
    ["python3", "-m", "unittest", "discover", "-s", "tools/dts/tests", "-p", "test_*.py", "-v"],
    ["python3", "-m", "unittest", "discover", "-s", "tools/build/tests", "-p", "test_*.py", "-v"],
    ["python3", "-m", "unittest", "discover", "-s", "tools/test/tests", "-p", "test_*.py", "-v"],
    ["python3", "-m", "unittest", "discover", "-s", "tools/ota", "-p", "test_*.py", "-v"],
    ["python3", "tools/test/run_config_matrix.py"],
    ["python3", "tools/lcd/render_ui_preview.py"],
)


def main() -> None:
    for command in COMMANDS:
        print("+", " ".join(command), flush=True)
        subprocess.run(command, cwd=ROOT, check=True)
    print("all host, schema, matrix and preview checks passed")


if __name__ == "__main__":
    main()
