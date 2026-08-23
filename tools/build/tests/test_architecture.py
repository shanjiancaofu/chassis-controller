from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from tools.build.check_architecture import scan


class ArchitectureCheckTest(unittest.TestCase):
    def test_current_application_layers_are_clean(self) -> None:
        root = Path(__file__).parents[3] / "firmware/application/stm32g474"
        self.assertEqual(scan(root, ["app", "lib", "subsys"]), [])

    def test_direct_hal_include_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "app" / "bad.c"
            source.parent.mkdir(parents=True)
            source.write_text('#include "fdcan.h"\nvoid f(void) { HAL_FDCAN_Start(0); }\n', encoding="utf-8")
            violations = scan(root, ["app"])
            self.assertEqual(len(violations), 2)
            self.assertIn("direct CubeMX/HAL include", violations[0].message)
            self.assertIn("HAL_FDCAN_Start", violations[1].message)

    def test_generated_devicetree_include_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "app" / "modules" / "bad.c"
            source.parent.mkdir(parents=True)
            source.write_text('#include "devicetree_generated.h"\n',
                              encoding="utf-8")
            violations = scan(root, ["app"])
            self.assertEqual(len(violations), 1)
            self.assertIn("generated Devicetree include", violations[0].message)

    def test_freertos_include_outside_runtime_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "app" / "bad.c"
            source.parent.mkdir()
            source.write_text('#include "FreeRTOS.h"\n', encoding="utf-8")
            violations = scan(root, ["app"])
            self.assertEqual(len(violations), 1)
            self.assertIn("outside runtime", violations[0].message)


if __name__ == "__main__":
    unittest.main()
