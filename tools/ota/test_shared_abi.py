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
        self.assertEqual(struct.calcsize(create_factory_image.OTA_METADATA_FORMAT), 64)
        self.assertRegex(metadata, r"_Static_assert\(sizeof\(OtaMetadata\) == 64U")
        for field in (
            "magic", "format_version", "record_size", "sequence", "state",
            "confirmed_slot", "candidate_slot", "image_size", "image_crc32",
            "install_attempts", "trial_boot_count", "last_error", "reserved",
            "record_crc32",
        ):
            self.assertRegex(metadata, rf"\b{field}\b")

    def test_image_header_abi_matches_package_tool(self):
        image = read_shared("firmware_image.h")
        self.assertEqual(
            macro_value(image, "OTA_IMAGE_MAGIC"), package_firmware.IMAGE_MAGIC
        )
        self.assertEqual(
            macro_value(image, "OTA_IMAGE_FORMAT_VERSION"),
            package_firmware.FORMAT_VERSION,
        )
        self.assertEqual(struct.calcsize(package_firmware.HEADER_FORMAT), 64)
        self.assertRegex(image, r"_Static_assert\(sizeof\(OtaImageHeader\) == 64U")


if __name__ == "__main__":
    unittest.main()
