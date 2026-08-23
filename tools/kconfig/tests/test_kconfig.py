from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from tools.kconfig.engine import evaluate


class KconfigTest(unittest.TestCase):
    def test_current_configuration(self) -> None:
        root = Path(__file__).parents[3]
        values, kinds = evaluate(
            root / "firmware/application/stm32g474/config/Kconfig",
            root / "firmware/application/stm32g474/config/prj.conf",
        )
        self.assertEqual(values["CAN"], "y")
        self.assertEqual(values["CAN_RX_QUEUE_SIZE"], "8")
        self.assertEqual(values["MOTOR_DEMO"], "n")
        self.assertEqual(kinds["CAN"], "bool")

    def test_choice_and_conditional_default(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Kconfig").write_text(
                'mainmenu "test"\nconfig ENABLE\n    bool "enable"\n    default y\n'
                'choice\n    prompt "mode"\nconfig MODE_A\n    bool "A"\n'
                'config MODE_B\n    bool "B"\nendchoice\n'
                'config VALUE\n    int\n    default 4 if MODE_A\n    default 8\n',
                encoding="utf-8",
            )
            (root / ".config").write_text("CONFIG_MODE_A=y\n", encoding="utf-8")
            values, _ = evaluate(root / "Kconfig", root / ".config")
            self.assertEqual(values["MODE_A"], "y")
            self.assertEqual(values["MODE_B"], "n")
            self.assertEqual(values["VALUE"], "4")

    def test_select_and_dependency(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Kconfig").write_text(
                'config BASE\n    bool "base"\nconfig FEATURE\n    bool\n'
                'config SELECTOR\n    bool "selector"\n    select FEATURE if BASE\n',
                encoding="utf-8",
            )
            (root / ".config").write_text(
                "CONFIG_BASE=y\nCONFIG_SELECTOR=y\n", encoding="utf-8"
            )
            values, _ = evaluate(root / "Kconfig", root / ".config")
            self.assertEqual(values["FEATURE"], "y")


if __name__ == "__main__":
    unittest.main()
