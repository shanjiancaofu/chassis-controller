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

    def test_task_creation_rolls_back_partial_static_tasks(self) -> None:
        source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/kernel/freertos/rtos_app.c"
        ).read_text(encoding="utf-8")
        self.assertIn("if (control_task_handle == NULL ||", source)
        self.assertIn("vTaskDelete(display_task_handle);", source)
        self.assertIn("vTaskDelete(diagnostics_task_handle);", source)
        self.assertIn("vTaskDelete(control_task_handle);", source)

    def test_motor_pair_update_has_one_interrupt_mask(self) -> None:
        source = (
            Path(__file__).parents[3]
            / "firmware/application/stm32g474/drivers/motor/motor_stm32.c"
        ).read_text(encoding="utf-8")
        pair_body = source.split("static void SetBoth", 1)[1].split(
            "static void EmergencyStop", 1
        )[0]
        self.assertEqual(pair_body.count("__disable_irq();"), 1)
        self.assertIn("state->left_applied_duty", pair_body)
        self.assertIn("state->right_applied_duty", pair_body)


if __name__ == "__main__":
    unittest.main()
