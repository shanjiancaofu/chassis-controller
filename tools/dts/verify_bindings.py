#!/usr/bin/env python3
"""Validate the generated device manifest against local YAML bindings."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def parse_binding(path: Path) -> dict:
    binding: dict = {"required": [], "properties": {}}
    section = ""
    property_name = ""
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("compatible:"):
            binding["compatible"] = line.split(":", 1)[1].strip().strip('"')
        elif line == "required:":
            section = "required"
        elif line == "properties:":
            section = "properties"
        elif section == "required" and line.startswith("-"):
            binding["required"].append(line[1:].strip().strip('"'))
        elif section == "properties" and line.endswith(":") and not line.startswith("-"):
            property_name = line[:-1].strip()
            binding["properties"][property_name] = {}
        elif section == "properties" and ":" in line and property_name:
            key, value = line.split(":", 1)
            binding["properties"][property_name][key.strip()] = value.strip().strip('"')
        else:
            raise ValueError(f"{path}:{number}: unsupported binding syntax: {raw}")
    if "compatible" not in binding:
        raise ValueError(f"{path}: missing compatible")
    return binding


def load_bindings(directory: Path) -> dict[str, dict]:
    bindings = {}
    for path in sorted(directory.glob("*.yaml")):
        binding = parse_binding(path)
        compatible = binding["compatible"]
        if compatible in bindings:
            raise ValueError(f"duplicate binding for {compatible}")
        bindings[compatible] = binding
    return bindings


def validate(manifest: dict, bindings: dict[str, dict]) -> None:
    for device_id, device in manifest.get("devices", {}).items():
        compatible = device.get("compatible")
        if not isinstance(compatible, str):
            raise ValueError(f"{device_id}: missing compatible")
        binding = bindings.get(compatible)
        if binding is None:
            raise ValueError(f"{device_id}: no binding for {compatible}")
        for name in binding["required"]:
            if name not in device:
                raise ValueError(f"{device_id}: required property {name!r} missing")
        for name, schema in binding["properties"].items():
            if name not in device or "type" not in schema:
                continue
            value = device[name]
            expected = schema["type"]
            if expected == "string" and not isinstance(value, str):
                raise ValueError(f"{device_id}: {name} must be string")
            if expected == "integer" and not isinstance(value, int):
                raise ValueError(f"{device_id}: {name} must be integer")
            if expected == "array" and not isinstance(value, list):
                raise ValueError(f"{device_id}: {name} must be array")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--bindings", type=Path, required=True)
    args = parser.parse_args()
    validate(json.loads(args.manifest.read_text(encoding="utf-8")), load_bindings(args.bindings))
    print("Devicetree bindings verified")


if __name__ == "__main__":
    main()
