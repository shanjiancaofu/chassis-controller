from __future__ import annotations

import unittest
from pathlib import Path


class CrashContextSafetyTest(unittest.TestCase):
    def test_exception_handlers_capture_after_emergency_stop(self) -> None:
        source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/cubemx/Core/Src/stm32g4xx_it.c"
        ).read_text(encoding="utf-8")
        self.assertIn("ChassisApp_PanicStopFromException();", source)
        self.assertIn("CrashContext_CaptureFromException", source)
        self.assertIn("__attribute__((naked)) void HardFault_Handler", source)
        self.assertIn("__attribute__((naked)) void BusFault_Handler", source)


if __name__ == "__main__":
    unittest.main()
