from __future__ import annotations

import json
import struct
import tempfile
import unittest
from pathlib import Path

from tools.dts.generate_devicetree import Node, build_manifest
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
                "links": struct.pack(">2I", 1, 7),
            }),
            Node("chosen", "/chosen", {"console": struct.pack(">I", 1)}),
            Node("aliases", "/aliases", {"can": b"/can\0"}),
            Node("__symbols__", "/__symbols__", {"can0": b"/can\0"}),
        ]
        manifest = build_manifest(nodes)
        self.assertEqual(manifest["chosen"]["console"], "can0")
        self.assertEqual(manifest["aliases"]["can"], "can0")
        self.assertEqual(manifest["labels"]["can0"], "can0")
        self.assertEqual(manifest["phandle_arrays"]["can0"]["links"], ["can0", 7])

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
