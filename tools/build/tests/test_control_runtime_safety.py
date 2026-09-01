from __future__ import annotations

import re
import unittest
from pathlib import Path


class ControlRuntimeSafetyTest(unittest.TestCase):
    def test_encoder_read_failure_latches_fault_without_zero_fallback(self) -> None:
        source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/app/runtime/control_runtime.c"
        ).read_text(encoding="utf-8")
        failure_path = re.search(
            r"if \(left_encoder_result < 0 \|\| right_encoder_result < 0\) \{"
            r"(?P<body>.*?)\n  \}",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(failure_path)
        body = failure_path.group("body")
        self.assertIn(
            "ControlRuntime_LatchInternalFault(CHASSIS_FAULT_ENCODER);", body
        )
        self.assertIn("return;", body)
        self.assertNotIn("left_delta = 0", body)
        self.assertNotIn("right_delta = 0", body)


if __name__ == "__main__":
    unittest.main()
