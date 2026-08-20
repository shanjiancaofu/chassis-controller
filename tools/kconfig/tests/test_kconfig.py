from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from tools.kconfig.config import parse_config
from tools.kconfig.evaluator import evaluate
from tools.kconfig.parser import parse


class KconfigTest(unittest.TestCase):
    def test_current_configuration(self) -> None:
        root = Path(__file__).parents[3]
        model = parse(root / "firmware/application/stm32g474/config/Kconfig")
        values = evaluate(model, parse_config(root / "firmware/application/stm32g474/config/prj.conf"))
        self.assertEqual(values["CAN"], "y")
        self.assertEqual(values["CAN_RX_QUEUE_SIZE"], "8")
        self.assertEqual(values["MOTOR_DEMO"], "n")

    def test_dependencies_select_and_choice_syntax(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Kconfig"
            path.write_text(
                'mainmenu "test"\n'
                'config ENABLE\n    bool\n    default y\n'
                'config FEATURE\n    bool\n    depends on ENABLE\n    default n\n'
                'config SELECTOR\n    bool\n    default y\n    select FEATURE\n'
                'choice MODE\n'
                'config MODE_A\n    bool\n    default y\n'
                'endchoice\n', encoding="utf-8")
            model = parse(path)
            self.assertEqual(evaluate(model, {})["FEATURE"], "y")

    def test_if_context_disables_symbols(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Kconfig"
            path.write_text(
                "config ENABLE\n    bool\n    default n\n"
                "if ENABLE\n"
                "config HIDDEN\n    int\n    default 4\n"
                "endif\n", encoding="utf-8")
            model = parse(path)
            self.assertEqual(evaluate(model, {})["HIDDEN"], "0")

    def test_invalid_value_and_duplicate_config(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Kconfig"
            path.write_text("config VALUE\n    int\n    range 1 3\n    default 1\n", encoding="utf-8")
            model = parse(path)
            with self.assertRaises(ValueError):
                evaluate(model, {"VALUE": "4"})
            config = Path(directory) / "prj.conf"
            config.write_text("CONFIG_VALUE=1\nCONFIG_VALUE=2\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                parse_config(config)


if __name__ == "__main__":
    unittest.main()
