from __future__ import annotations

import copy
import json
import unittest
from pathlib import Path

from tools.test.validate_chassis_schema import validate


ROOT = Path(__file__).parents[3]


class ChassisSchemaTest(unittest.TestCase):
    def setUp(self) -> None:
        self.schema = json.loads(
            (ROOT / "protocol/chassis_canfd.yaml").read_text(encoding="utf-8")
        )

    def test_repository_schema(self) -> None:
        validate(self.schema)

    def test_overlap_is_rejected(self) -> None:
        broken = copy.deepcopy(self.schema)
        broken["messages"][0]["fields"][-1]["offset"] = 5
        with self.assertRaisesRegex(ValueError, "overlaps"):
            validate(broken)

    def test_invalid_dlc_is_rejected(self) -> None:
        broken = copy.deepcopy(self.schema)
        broken["messages"][0]["payload_size"] = 10
        with self.assertRaisesRegex(ValueError, "payload size"):
            validate(broken)

    def test_invalid_flag_bit_is_rejected(self) -> None:
        broken = copy.deepcopy(self.schema)
        broken["messages"][1]["fields"][1]["bits"] = {"8": "invalid"}
        with self.assertRaisesRegex(ValueError, "invalid flag bit"):
            validate(broken)

    def test_formal_velocity_cannot_require_development_handshake(self) -> None:
        broken = copy.deepcopy(self.schema)
        broken["messages"][1]["requires_development_handshake"] = True
        with self.assertRaisesRegex(ValueError, "handshake-independent"):
            validate(broken)


if __name__ == "__main__":
    unittest.main()
