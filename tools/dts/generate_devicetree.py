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


def cells(data: bytes) -> list[int]:
    if len(data) % 4:
        raise ValueError(f"expected DT cells, got {len(data)} bytes")
    return list(struct.unpack(f">{len(data) // 4}I", data))


def string_list(data: bytes) -> list[str] | None:
    if not data or data[-1] != 0:
        return None
    parts = data.rstrip(b"\0").split(b"\0")
    if not parts or any(not part for part in parts):
        return None
    try:
        values = [part.decode("utf-8") for part in parts]
    except UnicodeDecodeError:
        return None
    if any(not value.isprintable() for value in values):
        return None
    return values


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
    if not data:
        return True
    if name in numeric:
        return u32(data)
    values = string_list(data)
    if values is not None:
        return values[0] if len(values) == 1 else values
    if len(data) % 4 == 0:
        values = cells(data)
        return values[0] if len(values) == 1 else values
    return c_string(data)


def build_manifest(nodes: list[Node]) -> dict:
    phandles: dict[int, str] = {}
    paths: dict[str, str] = {}
    devices: dict[str, dict] = {}
    labels: dict[str, str] = {}
    for node in nodes:
        device_id_raw = node.properties.get("chassis,device-id")
        if device_id_raw is None:
            continue
        device_id = token(c_string(device_id_raw))
        values = {name: property_value(name, data)
                  for name, data in node.properties.items()
                  if name != "chassis,device-id"}
        devices[device_id] = {"path": node.path, **values}
        paths[node.path] = device_id
        labels[token(device_id)] = device_id
        if "phandle" in values:
            phandles[int(values["phandle"])] = device_id

    symbols_node = next((node for node in nodes if node.path == "/__symbols__"), None)
    if symbols_node:
        for label, data in symbols_node.properties.items():
            path_value = c_string(data)
            if path_value in paths:
                labels[token(label)] = paths[path_value]

    def resolve_reference(value: int) -> str:
        if value not in phandles:
            raise ValueError(f"unknown phandle {value}")
        return phandles[value]

    def resolve_property_reference(data: bytes) -> str:
        strings = string_list(data)
        if strings is not None and len(strings) == 1 and strings[0] in paths:
            return paths[strings[0]]
        return resolve_reference(u32(data))

    chosen: dict[str, str | list[str]] = {}
    aliases: dict[str, str] = {}
    phandle_arrays: dict[str, dict[str, list[str | int]]] = {}
    chosen_node = next((node for node in nodes if node.path == "/chosen"), None)
    aliases_node = next((node for node in nodes if node.path == "/aliases"), None)
    if chosen_node:
        for name, data in chosen_node.properties.items():
            references = [resolve_reference(cell) for cell in cells(data)]
            for reference in references:
                if devices[reference].get("status", "okay") != "okay":
                    raise ValueError(
                        f"chosen {name} references disabled device {reference}")
            chosen[token(name)] = references[0] if len(references) == 1 else references
    if aliases_node:
        for name, data in aliases_node.properties.items():
            aliases[token(name)] = resolve_property_reference(data)

    nodes_by_phandle = {
        u32(node.properties["phandle"]): node
        for node in nodes if "phandle" in node.properties
    }
    for device_id, values in devices.items():
        for name, value in list(values.items()):
            if name in {"path", "phandle"} or not isinstance(value, list):
                continue
            if not value or not isinstance(value[0], int) or value[0] not in phandles:
                continue
            cell_property = "#gpio-cells" if name == "gpios" else None
            if cell_property is None:
                continue
            entries = []
            index = 0
            while index < len(value):
                provider = value[index]
                provider_node = nodes_by_phandle.get(provider)
                if provider_node is None or provider not in phandles:
                    raise ValueError(f"{device_id}.{name}: unknown phandle {provider}")
                if cell_property not in provider_node.properties:
                    raise ValueError(
                        f"{device_id}.{name}: provider lacks {cell_property}")
                cell_count = u32(provider_node.properties[cell_property])
                end = index + 1 + cell_count
                if end > len(value):
                    raise ValueError(f"{device_id}.{name}: truncated specifier")
                entries.append({"controller": phandles[provider],
                                "cells": value[index + 1:end]})
                index = end
            phandle_arrays.setdefault(device_id, {})[name] = entries

    return {"devices": devices, "chosen": chosen, "aliases": aliases,
            "labels": labels, "phandle_arrays": phandle_arrays}


def render(value) -> str:
    if isinstance(value, str):
        return f'"{value}"'
    if isinstance(value, bool):
        return "1" if value else "0"
    return f"{value}U"


def generate_header(manifest: dict) -> str:
    devices = manifest["devices"]
    lines = ["#ifndef CHASSIS_DEVICETREE_GENERATED_H",
             "#define CHASSIS_DEVICETREE_GENERATED_H", "",
             "struct device;", ""]
    for device_id, values in sorted(devices.items()):
        lines.append(f"#define DT_NODE_{device_id} {device_id}")
        lines.append(f"#define DT_NODE_{device_id}_EXISTS 1")
        status = token(str(values.get("status", "okay")))
        lines.append(
            f"#define DT_NODE_{device_id}_STATUS_okay "
            f"{1 if status == 'okay' else 0}")
        lines.append(
            f"#define DT_NODE_{device_id}_STATUS_disabled "
            f"{1 if status == 'disabled' else 0}")
        if status == "okay":
            lines.append(f"extern const struct device device_{device_id};")
        for name, value in sorted(values.items()):
            if name in ("path", "phandle"):
                continue
            property_name = token(name)
            lines.append(f"#define DT_PROP_{device_id}_{property_name}_EXISTS 1")
            if isinstance(value, list):
                lines.append(
                    f"#define DT_PROP_{device_id}_{property_name}_LEN {len(value)}U")
                for index, item in enumerate(value):
                    lines.append(
                        f"#define DT_PROP_{device_id}_{property_name}_IDX_{index} {render(item)}")
            else:
                lines.append(
                    f"#define DT_PROP_{device_id}_{property_name} {render(value)}")
        for name, entries in sorted(
                manifest.get("phandle_arrays", {}).get(device_id, {}).items()):
            property_name = token(name)
            lines.append(
                f"#define DT_PHA_{device_id}_{property_name}_LEN {len(entries)}U")
            for index, entry in enumerate(entries):
                lines.append(
                    f"#define DT_PHA_{device_id}_{property_name}_CONTROLLER_{index} "
                    f"{entry['controller']}")
                for cell_index, cell in enumerate(entry["cells"]):
                    lines.append(
                        f"#define DT_PHA_{device_id}_{property_name}_CELL_{index}_{cell_index} "
                        f"{render(cell)}")
                if name == "gpios" and len(entry["cells"]) == 2:
                    lines.append(
                        f"#define DT_PHA_{device_id}_{property_name}_PIN_{index} "
                        f"{render(entry['cells'][0])}")
                    lines.append(
                        f"#define DT_PHA_{device_id}_{property_name}_FLAGS_{index} "
                        f"{render(entry['cells'][1])}")
        lines.append("")
    for name, device_id in sorted(manifest["chosen"].items()):
        if isinstance(device_id, list):
            raise ValueError(f"chosen {name} must reference one device")
        lines.append(f"#define DT_CHOSEN_{name} {device_id}")
    for name, device_id in sorted(manifest["aliases"].items()):
        lines.append(f"#define DT_ALIAS_{name} {device_id}")
    for name, device_id in sorted(manifest["labels"].items()):
        lines.append(f"#define DT_NODELABEL_{name} {device_id}")
    lines.extend(["", "#endif", ""])
    return "\n".join(lines)


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
    manifest = build_manifest(nodes)
    write_if_changed(args.header, generate_header(manifest))
    write_if_changed(args.json,
                     json.dumps(manifest,
                                indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
