#!/usr/bin/env python3
"""Build the Application under representative Kconfig/DTS combinations."""

from __future__ import annotations

import json
import pathlib
import re
import subprocess
import time
import xml.etree.ElementTree as ET


ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = ROOT / "build/test-matrix"
BASE_CONFIG = ROOT / "firmware/application/stm32g474/config/prj.conf"
BASE_DTS = ROOT / "firmware/application/stm32g474/boards/chassis_g474/chassis_g474.dts"
MATRIX = ROOT / "tools/test/test_matrix.yaml"


def set_config(text: str, symbol: str, value: str) -> str:
    pattern = rf"^CONFIG_{re.escape(symbol)}=.*$"
    replacement = f"CONFIG_{symbol}={value}"
    updated, count = re.subn(pattern, replacement, text, flags=re.MULTILINE)
    if count != 1:
        raise RuntimeError(f"missing CONFIG_{symbol}")
    return updated


def main() -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    results = []
    base_config = BASE_CONFIG.read_text(encoding="utf-8")
    base_dts = BASE_DTS.read_text(encoding="utf-8")
    scenarios = json.loads(MATRIX.read_text(encoding="utf-8"))["scenarios"]
    for scenario in scenarios:
        name = scenario["name"]
        preset = scenario["preset"]
        options = scenario.get("config", {})
        started = time.monotonic()
        config = base_config
        dts = base_dts
        for symbol in ("ICM45686", "SELF_TEST"):
            if symbol in options:
                config = set_config(config, symbol, options[symbol])
        if "imu0" in scenario.get("disable_nodes", []):
            dts, count = re.subn(
                r"(imu0:\s*sensor@0\s*\{.*?status\s*=\s*)\"okay\"",
                r'\1"disabled"', dts, count=1, flags=re.DOTALL)
            if count != 1:
                raise RuntimeError("failed to disable imu0")
            dts = re.sub(r"^\s*chassis,imu\s*=.*\n", "", dts,
                         count=1, flags=re.MULTILINE)
            dts = re.sub(r"^\s*imu\s*=.*\n", "", dts,
                         count=1, flags=re.MULTILINE)
        config_path = BUILD / f"{name}.conf"
        dts_path = BUILD / f"{name}.dts"
        build_path = BUILD / name
        config_path.write_text(config, encoding="utf-8")
        dts_path.write_text(dts, encoding="utf-8")
        try:
            subprocess.run([
                "cmake", "--preset", preset, "-B", str(build_path),
                f"-DAPP_PRJ_CONF={config_path}", f"-DAPP_DTS={dts_path}",
            ], cwd=ROOT, check=True)
            subprocess.run([
                "cmake", "--build", str(build_path), "--target", "application",
                "--parallel",
            ], cwd=ROOT, check=True)
            status = "PASS"
            error = ""
        except subprocess.CalledProcessError as exc:
            status = "FAIL"
            error = str(exc)
        result = {"name": name, "status": status,
                  "duration_s": round(time.monotonic() - started, 3),
                  "error": error}
        results.append(result)
        print(f"{status} {name} ({result['duration_s']}s)")
        if status == "FAIL":
            break
    report = {"scenarios": results}
    (BUILD / "test-results.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    suite = ET.Element("testsuite", name="chassis-config-matrix",
                       tests=str(len(results)),
                       failures=str(sum(item["status"] == "FAIL" for item in results)))
    for item in results:
        case = ET.SubElement(suite, "testcase", name=item["name"],
                             time=str(item["duration_s"]))
        if item["status"] == "FAIL":
            ET.SubElement(case, "failure", message=item["error"])
    ET.ElementTree(suite).write(BUILD / "junit.xml", encoding="utf-8",
                                xml_declaration=True)
    if any(item["status"] == "FAIL" for item in results):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
