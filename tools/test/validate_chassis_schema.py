#!/usr/bin/env python3
"""Validate the authoritative chassis CAN FD schema."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


CAN_FD_LENGTHS = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64}


def validate(schema: dict) -> None:
    protocol = schema["protocol"]
    if protocol["byte_order"] != "little" or protocol["identifier_bits"] != 11:
        raise ValueError("protocol must use little-endian 11-bit identifiers")
    types = schema["types"]
    channels = {entry["name"]: entry for entry in schema["channels"]}
    ids: set[int] = set()
    names: set[str] = set()
    for message in schema["messages"]:
        identifier = message["id"]
        name = message["name"]
        size = message["payload_size"]
        if identifier in ids or name in names:
            raise ValueError(f"duplicate message id/name: {name}")
        ids.add(identifier)
        names.add(name)
        if size not in CAN_FD_LENGTHS:
            raise ValueError(f"{name}: invalid CAN FD payload size {size}")
        channel = channels.get(message["channel"])
        if channel is None or not channel["first_id"] <= identifier <= channel["last_id"]:
            raise ValueError(f"{name}: id outside channel allocation")
        occupied = [False] * size
        field_names: set[str] = set()
        for field in message["fields"]:
            field_name = field["name"]
            if field_name in field_names:
                raise ValueError(f"{name}: duplicate field {field_name}")
            field_names.add(field_name)
            field_size = field.get("size") if field["type"] == "bytes" else types.get(field["type"])
            if not isinstance(field_size, int) or field_size <= 0:
                raise ValueError(f"{name}.{field_name}: unknown/invalid type")
            start = field["offset"]
            end = start + field_size
            if start < 0 or end > size:
                raise ValueError(f"{name}.{field_name}: outside payload")
            if any(occupied[start:end]):
                raise ValueError(f"{name}.{field_name}: overlaps another field")
            occupied[start:end] = [True] * field_size
        if not all(occupied):
            raise ValueError(f"{name}: payload contains undefined bytes")
        if message["crc"] == "crc16_ccitt_false":
            crc = message["fields"][-1]
            if crc["name"] != "crc16" or crc["offset"] != size - 2:
                raise ValueError(f"{name}: CRC16 must be the final field")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("schema", type=Path)
    args = parser.parse_args()
    schema = json.loads(args.schema.read_text(encoding="utf-8"))
    validate(schema)
    print(f"validated {len(schema['messages'])} chassis CAN FD messages")


if __name__ == "__main__":
    main()
