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


if __name__ == "__main__":
    unittest.main()
