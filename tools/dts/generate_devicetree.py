#!/usr/bin/env python3
"""Generate deterministic C metadata from a standard flattened DTB."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import struct
from dataclasses import dataclass, field


FDT_MAGIC = 0xD00DFEED
FDT_BEGIN_NODE = 1
FDT_END_NODE = 2
FDT_PROP = 3
FDT_NOP = 4
FDT_END = 9


@dataclass
class Node:
    name: str
    path: str
    properties: dict[str, bytes] = field(default_factory=dict)


def align4(value: int) -> int:
    return (value + 3) & ~3


def c_string(data: bytes) -> str:
    return data.split(b"\0", 1)[0].decode("utf-8")


def u32(data: bytes) -> int:
    if len(data) != 4:
        raise ValueError(f"expected one DT cell, got {len(data)} bytes")
    return struct.unpack(">I", data)[0]


def parse_dtb(path: pathlib.Path) -> list[Node]:
    blob = path.read_bytes()
    if len(blob) < 40:
        raise ValueError("truncated DTB")
    header = struct.unpack(">10I", blob[:40])
    magic, _, off_struct, off_strings, _, _, _, _, size_strings, size_struct = header
    if magic != FDT_MAGIC:
        raise ValueError("invalid DTB magic")
    strings = blob[off_strings : off_strings + size_strings]
    cursor = off_struct
    end = off_struct + size_struct
    stack: list[str] = []
    nodes: list[Node] = []
    current: Node | None = None

    while cursor < end:
        token = struct.unpack_from(">I", blob, cursor)[0]
        cursor += 4
        if token == FDT_BEGIN_NODE:
            zero = blob.index(0, cursor)
            name = blob[cursor:zero].decode("utf-8")
            cursor = align4(zero + 1)
            stack.append(name)
            parts = [part for part in stack if part]
            current = Node(name, "/" + "/".join(parts))
            nodes.append(current)
        elif token == FDT_END_NODE:
            stack.pop()
            current = nodes[-1] if stack and nodes else None
            if stack:
                parent_path = "/" + "/".join(part for part in stack if part)
                current = next(node for node in reversed(nodes)
                               if node.path == parent_path)
        elif token == FDT_PROP:
            if current is None:
                raise ValueError("property outside node")
            length, name_offset = struct.unpack_from(">II", blob, cursor)
            cursor += 8
            name_end = strings.index(0, name_offset)
            name = strings[name_offset:name_end].decode("utf-8")
            current.properties[name] = blob[cursor:cursor + length]
            cursor = align4(cursor + length)
        elif token == FDT_NOP:
            continue
        elif token == FDT_END:
            break
        else:
            raise ValueError(f"unknown DTB token {token}")
    return nodes


def token(value: str) -> str:
    result = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not result or result[0].isdigit():
        result = "node_" + result
    return result


def property_value(name: str, data: bytes):
    numeric = {
        "phandle", "bitrate", "sample-point", "bitrate-data",
        "sample-point-data", "standard-filter-count", "frequency",
        "width", "height", "size", "erase-block-size",
        "write-block-size", "pwm-period",
    }
    if name in numeric:
        return u32(data)
    return c_string(data)


def write_if_changed(path: pathlib.Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dtb", type=pathlib.Path, required=True)
    parser.add_argument("--header", type=pathlib.Path, required=True)
    parser.add_argument("--json", type=pathlib.Path, required=True)
    args = parser.parse_args()

    nodes = parse_dtb(args.dtb)
    phandles: dict[int, str] = {}
    devices: dict[str, dict] = {}
    chosen: dict[str, str] = {}
    for node in nodes:
        device_id_raw = node.properties.get("chassis,device-id")
        if device_id_raw is None:
            continue
        device_id = token(c_string(device_id_raw))
        values = {name: property_value(name, data)
                  for name, data in node.properties.items()
                  if name != "chassis,device-id"}
        devices[device_id] = {"path": node.path, **values}
        if "phandle" in values:
            phandles[int(values["phandle"])] = device_id
    chosen_node = next((node for node in nodes if node.path == "/chosen"), None)
    if chosen_node:
        for name, data in chosen_node.properties.items():
            handle = u32(data)
            if handle not in phandles:
                raise ValueError(f"chosen {name} references unknown phandle {handle}")
            chosen[token(name)] = phandles[handle]

    lines = ["#ifndef CHASSIS_DEVICETREE_GENERATED_H",
             "#define CHASSIS_DEVICETREE_GENERATED_H", "",
             "struct device;", ""]
    for device_id, values in sorted(devices.items()):
        upper_id = device_id.upper()
        lines.append(f"#define DT_NODE_{upper_id} {device_id}")
        lines.append(f"#define DT_NODE_{upper_id}_EXISTS 1")
        if "phandle" in values:
            lines.append(f"extern const struct device device_{device_id};")
        for name, value in sorted(values.items()):
            if name in ("path", "phandle"):
                continue
            macro = token(name).upper()
            rendered = f'"{value}"' if isinstance(value, str) else f"{value}U"
            lines.append(f"#define DT_PROP_{upper_id}_{macro} {rendered}")
        lines.append("")
    for name, device_id in sorted(chosen.items()):
        lines.append(f"#define DT_CHOSEN_{name.upper()} {device_id}")
    lines.extend(["", "#endif", ""])
    write_if_changed(args.header, "\n".join(lines))
    write_if_changed(args.json,
                     json.dumps({"devices": devices, "chosen": chosen},
                                indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
