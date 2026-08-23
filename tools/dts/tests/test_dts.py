from __future__ import annotations

import json
import struct
import tempfile
import unittest
from pathlib import Path

from tools.dts.generate_devicetree import Node, build_manifest, generate_header
from tools.dts.verify_bindings import load_bindings, validate


class DevicetreeTest(unittest.TestCase):
    def test_chosen_alias_label_and_phandle_array(self) -> None:
        nodes = [
            Node("", "/", {}),
            Node("can", "/can", {
                "phandle": struct.pack(">I", 1),
                "chassis,device-id": b"can0\0",
                "compatible": b"test,can\0",
                "status": b"okay\0",
                "#gpio-cells": struct.pack(">I", 2),
                "gpios": struct.pack(">3I", 1, 7, 1),
            }),
            Node("chosen", "/chosen", {"console": struct.pack(">I", 1)}),
            Node("aliases", "/aliases", {"can": b"/can\0"}),
            Node("__symbols__", "/__symbols__", {"can0": b"/can\0"}),
        ]
        manifest = build_manifest(nodes)
        self.assertEqual(manifest["chosen"]["console"], "can0")
        self.assertEqual(manifest["aliases"]["can"], "can0")
        self.assertEqual(manifest["labels"]["can0"], "can0")
        self.assertEqual(manifest["phandle_arrays"]["can0"]["gpios"], [
            {"controller": "can0", "cells": [7, 1]},
        ])

    def test_disabled_node_has_status_but_no_device_declaration(self) -> None:
        manifest = build_manifest([
            Node("", "/", {}),
            Node("sensor", "/sensor", {
                "chassis,device-id": b"sensor0\0",
                "compatible": b"test,sensor\0",
                "status": b"disabled\0",
            }),
        ])
        header = generate_header(manifest)
        self.assertIn("DT_NODE_sensor0_STATUS_okay 0", header)
        self.assertIn("DT_NODE_sensor0_STATUS_disabled 1", header)
        self.assertNotIn("extern const struct device device_sensor0", header)

    def test_binding_validation_rejects_missing_required_property(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binding = root / "test.yaml"
            binding.write_text(
                "compatible: test,can\n"
                "required:\n"
                "  - status\n"
                "  - bitrate\n"
                "properties:\n"
                "  status:\n"
                "    type: string\n"
                "  bitrate:\n"
                "    type: integer\n", encoding="utf-8")
            bindings = load_bindings(root)
            manifest = {"devices": {"can0": {"compatible": "test,can", "status": "okay"}}}
            with self.assertRaises(ValueError):
                validate(manifest, bindings)


if __name__ == "__main__":
    unittest.main()
