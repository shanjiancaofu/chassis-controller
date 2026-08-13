#!/usr/bin/env python3

import pathlib
import re
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import create_factory_image
import package_firmware


def read_shared(name):
    return (ROOT / "firmware" / "shared" / name).read_text(encoding="utf-8")


def macro_value(source, name):
    match = re.search(
        rf"^#define\s+{re.escape(name)}\s+(0x[0-9A-Fa-f]+|\d+)[UuLl]*\s*$",
        source,
        re.MULTILINE,
    )
    if not match:
        raise AssertionError(f"missing numeric macro {name}")
    return int(match.group(1), 0)


def enum_members(source, type_name):
    match = re.search(
        rf"typedef\s+enum\s*\{{(.*?)\}}\s*{re.escape(type_name)}\s*;",
        source,
        re.DOTALL,
    )
    if not match:
        raise AssertionError(f"missing enum {type_name}")
    values = {}
    current = -1
    for item in match.group(1).split(","):
        item = item.strip()
        if not item:
            continue
        if "=" in item:
            name, raw = (part.strip() for part in item.split("=", 1))
            current = int(raw.rstrip("UuLl"), 0)
        else:
            name = item
            current += 1
        values[name] = current
    return values


def struct_fields(source, type_name):
    match = re.search(
        rf"typedef\s+struct\s*\{{(.*?)\}}\s*{re.escape(type_name)}\s*;",
        source,
        re.DOTALL,
    )
    if not match:
        raise AssertionError(f"missing struct {type_name}")
    fields = []
    for raw_type, name, raw_count in re.findall(
        r"\b(uint(?:8|16|32)_t)\s+(\w+)\s*(?:\[(\d+)\])?\s*;",
        match.group(1),
    ):
        fields.append((raw_type, name, int(raw_count or "1")))
    return fields


def field_layout(fields):
    sizes = {"uint8_t": 1, "uint16_t": 2, "uint32_t": 4}
    result = []
    offset = 0
    for raw_type, name, count in fields:
        size = sizes[raw_type]
        alignment = size
        offset = (offset + alignment - 1) // alignment * alignment
        result.append((name, offset, size * count))
        offset += size * count
    return result, offset


class SharedAbiTest(unittest.TestCase):
    def test_flash_layout_matches_python_tools(self):
        layout = read_shared("flash_layout.h")
        expected = {
            "OTA_BOOTLOADER_START": create_factory_image.BOOTLOADER_START,
            "OTA_BOOTLOADER_SIZE": create_factory_image.BOOTLOADER_SIZE,
            "OTA_APPLICATION_START": create_factory_image.APPLICATION_START,
            "OTA_APPLICATION_SIZE": create_factory_image.APPLICATION_SIZE,
            "OTA_QSPI_METADATA_A_START": create_factory_image.QSPI_METADATA_A,
            "OTA_QSPI_SLOT_A_START": create_factory_image.QSPI_SLOT_A,
        }
        for name, python_value in expected.items():
            self.assertEqual(macro_value(layout, name), python_value, name)
        self.assertEqual(
            macro_value(layout, "OTA_APPLICATION_START"),
            package_firmware.APPLICATION_START,
        )
        self.assertEqual(
            macro_value(layout, "OTA_APPLICATION_SIZE"),
            package_firmware.APPLICATION_SIZE,
        )

    def test_metadata_abi_matches_factory_tool(self):
        metadata = read_shared("ota_metadata.h")
        slots = enum_members(metadata, "OtaSlotId")
        states = enum_members(metadata, "OtaState")

        self.assertEqual(
            macro_value(metadata, "OTA_METADATA_MAGIC"),
            create_factory_image.OTA_METADATA_MAGIC,
        )
        self.assertEqual(
            macro_value(metadata, "OTA_METADATA_FORMAT_VERSION"),
            create_factory_image.OTA_METADATA_FORMAT_VERSION,
        )
        self.assertEqual(slots["OTA_SLOT_NONE"], create_factory_image.OTA_SLOT_NONE)
        self.assertEqual(slots["OTA_SLOT_A"], create_factory_image.OTA_SLOT_A)
        self.assertEqual(
            states["OTA_STATE_CONFIRMED"],
            create_factory_image.OTA_STATE_CONFIRMED,
        )
        expected_fields = [
            ("uint32_t", "magic", 1),
            ("uint16_t", "format_version", 1),
            ("uint16_t", "record_size", 1),
            ("uint32_t", "sequence", 1),
            ("uint32_t", "state", 1),
            ("uint32_t", "confirmed_slot", 1),
            ("uint32_t", "candidate_slot", 1),
            ("uint32_t", "image_size", 1),
            ("uint32_t", "image_crc32", 1),
            ("uint32_t", "install_attempts", 1),
            ("uint32_t", "trial_boot_count", 1),
            ("uint32_t", "last_error", 1),
            ("uint8_t", "reserved", 16),
            ("uint32_t", "record_crc32", 1),
        ]
        expected_layout = [
            ("magic", 0, 4), ("format_version", 4, 2),
            ("record_size", 6, 2), ("sequence", 8, 4),
            ("state", 12, 4), ("confirmed_slot", 16, 4),
            ("candidate_slot", 20, 4), ("image_size", 24, 4),
            ("image_crc32", 28, 4), ("install_attempts", 32, 4),
            ("trial_boot_count", 36, 4), ("last_error", 40, 4),
            ("reserved", 44, 16), ("record_crc32", 60, 4),
        ]
        fields = struct_fields(metadata, "OtaMetadata")
        layout, size = field_layout(fields)
        self.assertEqual(fields, expected_fields)
        self.assertEqual(layout, expected_layout)
        self.assertEqual(create_factory_image.OTA_METADATA_FORMAT, "<IHH9I16sI")
        self.assertEqual(struct.calcsize(create_factory_image.OTA_METADATA_FORMAT), size)
        self.assertRegex(metadata, r"_Static_assert\(sizeof\(OtaMetadata\) == 64U")

    def test_image_header_abi_matches_package_tool(self):
        image = read_shared("firmware_image.h")
        self.assertEqual(
            macro_value(image, "OTA_IMAGE_MAGIC"), package_firmware.IMAGE_MAGIC
        )
        self.assertEqual(
            macro_value(image, "OTA_IMAGE_FORMAT_VERSION"),
            package_firmware.FORMAT_VERSION,
        )
        expected_fields = [
            ("uint32_t", "magic", 1),
            ("uint16_t", "format_version", 1),
            ("uint16_t", "header_size", 1),
            ("uint32_t", "payload_size", 1),
            ("uint32_t", "payload_crc32", 1),
            ("uint32_t", "load_address", 1),
            ("uint32_t", "vector_address", 1),
            ("uint32_t", "firmware_version", 1),
            ("uint32_t", "build_number", 1),
            ("uint32_t", "rollback_counter", 1),
            ("uint32_t", "flags", 1),
            ("uint8_t", "reserved", 20),
            ("uint32_t", "header_crc32", 1),
        ]
        expected_layout = [
            ("magic", 0, 4), ("format_version", 4, 2),
            ("header_size", 6, 2), ("payload_size", 8, 4),
            ("payload_crc32", 12, 4), ("load_address", 16, 4),
            ("vector_address", 20, 4), ("firmware_version", 24, 4),
            ("build_number", 28, 4), ("rollback_counter", 32, 4),
            ("flags", 36, 4), ("reserved", 40, 20),
            ("header_crc32", 60, 4),
        ]
        fields = struct_fields(image, "OtaImageHeader")
        layout, size = field_layout(fields)
        self.assertEqual(fields, expected_fields)
        self.assertEqual(layout, expected_layout)
        self.assertEqual(package_firmware.HEADER_FORMAT, "<IHH8I20sI")
        self.assertEqual(struct.calcsize(package_firmware.HEADER_FORMAT), size)
        self.assertRegex(image, r"_Static_assert\(sizeof\(OtaImageHeader\) == 64U")


if __name__ == "__main__":
    unittest.main()
