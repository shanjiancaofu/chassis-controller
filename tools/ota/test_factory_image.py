#!/usr/bin/env python3

import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import package_firmware


class FactoryImageTest(unittest.TestCase):
    @staticmethod
    def create_inputs(root):
        payload = bytearray(64)
        struct.pack_into("<II", payload, 0, 0x20001000, 0x08008001)
        package = package_firmware.create_header(
            payload, 0x00010000, 7, 0
        ) + payload
        boot = root / "boot.bin"
        app = root / "app.bin"
        ota = root / "app.ota"
        boot_payload = bytearray(64)
        struct.pack_into("<II", boot_payload, 0, 0x20001000, 0x08000001)
        boot.write_bytes(boot_payload)
        app.write_bytes(payload)
        ota.write_bytes(package)
        return boot, app, ota, payload, package

    def test_qspi_image_contains_confirmed_factory_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            boot, app, ota, payload, package = self.create_inputs(root)
            factory = root / "factory.bin"
            qspi = root / "qspi.bin"

            subprocess.run(
                [
                    sys.executable,
                    str(pathlib.Path(__file__).with_name("create_factory_image.py")),
                    str(boot),
                    str(app),
                    str(factory),
                    "--ota",
                    str(ota),
                    "--qspi-output",
                    str(qspi),
                ],
                check=True,
                capture_output=True,
                text=True,
            )

            image = qspi.read_bytes()
            self.assertEqual(len(image), 0x00800000)
            self.assertEqual(image[0x00402000 : 0x00402000 + len(package)], package)
            metadata = image[0x00400000 : 0x00400040]
            fields = struct.unpack("<IHH9I16sI", metadata)
            self.assertEqual(fields[0], 0x314D544F)
            self.assertEqual(fields[4], 5)
            self.assertEqual(fields[5], 1)
            self.assertEqual(fields[6], 0)
            self.assertEqual(fields[7], len(payload))
            self.assertEqual(fields[8], zlib.crc32(payload) & 0xFFFFFFFF)
            self.assertEqual(fields[-1], zlib.crc32(metadata[:-4]) & 0xFFFFFFFF)
            self.assertEqual(image[0x00401000 : 0x00401040], b"\xFF" * 64)

    def test_factory_requires_qspi_provisioning_by_default(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            boot, app, _, _, _ = self.create_inputs(root)
            result = subprocess.run(
                [
                    sys.executable,
                    str(pathlib.Path(__file__).with_name("create_factory_image.py")),
                    str(boot),
                    str(app),
                    str(root / "factory.bin"),
                ],
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("requires --ota and --qspi-output", result.stderr)

    def test_internal_only_requires_explicit_flag(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            boot, app, _, _, _ = self.create_inputs(root)
            factory = root / "factory.bin"
            subprocess.run(
                [
                    sys.executable,
                    str(pathlib.Path(__file__).with_name("create_factory_image.py")),
                    str(boot),
                    str(app),
                    str(factory),
                    "--internal-only",
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertTrue(factory.exists())


if __name__ == "__main__":
    unittest.main()
