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

    def test_power_sample_requires_fresh_valid_snapshot(self) -> None:
        source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/app/runtime/control_runtime.c"
        ).read_text(encoding="utf-8")
        self.assertIn("!power_sample.valid", source)
        self.assertIn("power_sample.sample_age_ms > MOTOR_CONTROL_MAX_SUPPLY_SAMPLE_AGE_MS", source)
        self.assertIn("power_sample.millivolts < MOTOR_CONTROL_MIN_SUPPLY_MV", source)

    def test_motion_start_requires_power_but_stopped_does_not_fault(self) -> None:
        source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/app/runtime/control_runtime.c"
        ).read_text(encoding="utf-8")
        self.assertIn("if (!PowerReady(time_uptime_ms()))", source)
        self.assertIn("if (!PowerReady(now_ms) || !SafetyManager_RequestOpenLoopTest())", source)
        self.assertIn("SafetyManager_IsRunning() || SafetyManager_IsOpenLoopTestRunning()", source)

    def test_feedback_watchdog_requires_output_threshold_and_timeout(self) -> None:
        runtime_source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/app/runtime/control_runtime.c"
        ).read_text(encoding="utf-8")
        watchdog_source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/app/chassis/feedback_watchdog.c"
        ).read_text(encoding="utf-8")
        self.assertIn("MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD", watchdog_source)
        self.assertIn("MOTOR_ENCODER_FEEDBACK_LOSS_TIMEOUT_MS", watchdog_source)
        self.assertIn("FeedbackWatchdog_Update", runtime_source)
        self.assertIn("CHASSIS_FAULT_ENCODER", runtime_source)

    def test_encoder_injection_requires_running_output(self) -> None:
        source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/app/runtime/control_runtime.c"
        ).read_text(encoding="utf-8")
        self.assertIn("ControlRuntime_ArmEncoderReadFailure", source)
        self.assertIn("SafetyManager_IsRunning()", source)
        self.assertIn("MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD", source)


if __name__ == "__main__":
    unittest.main()
