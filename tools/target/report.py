#!/usr/bin/env python3
"""Small PASS/FAIL/SKIP report shared by target regression tools."""

from __future__ import annotations

import dataclasses
import json
import pathlib
from typing import Iterable


@dataclasses.dataclass(frozen=True)
class CheckResult:
    name: str
    status: str
    detail: str = ""

    def __post_init__(self) -> None:
        if self.status not in {"PASS", "FAIL", "SKIP"}:
            raise ValueError(f"invalid result status: {self.status}")


class RegressionReport:
    def __init__(self) -> None:
        self.results: list[CheckResult] = []

    def add(self, name: str, passed: bool, detail: str = "") -> None:
        self.results.append(CheckResult(name, "PASS" if passed else "FAIL", detail))

    def skip(self, name: str, detail: str) -> None:
        self.results.append(CheckResult(name, "SKIP", detail))

    def extend(self, results: Iterable[CheckResult]) -> None:
        self.results.extend(results)

    @property
    def passed(self) -> bool:
        return not any(result.status == "FAIL" for result in self.results)

    def print(self) -> None:
        for result in self.results:
            suffix = f" - {result.detail}" if result.detail else ""
            print(f"{result.status:4} {result.name}{suffix}")
        print("PASS target regression" if self.passed else "FAIL target regression")

    def write_json(self, path: pathlib.Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(
                {
                    "passed": self.passed,
                    "results": [dataclasses.asdict(result) for result in self.results],
                },
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
