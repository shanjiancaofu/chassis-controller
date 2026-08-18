#!/usr/bin/env python3

import unittest

from send_uart import is_ota_ready_response


class UartMessageTest(unittest.TestCase):
    def test_accepts_formal_ota_ready_response(self):
        self.assertTrue(is_ota_ready_response(
            b"[RSP] v=1 ts_ms=42 result=OK command=ota_uart mode=BINARY\r"
        ))

    def test_rejects_error_response(self):
        self.assertFalse(is_ota_ready_response(
            b"[RSP] v=1 ts_ms=42 result=ERROR command=ota_uart code=BUSY\r"
        ))

    def test_rejects_tokens_from_multiple_messages(self):
        self.assertFalse(is_ota_ready_response(
            b"[RSP] result=ERROR command=ota_uart [RSP] result=OK command=ping"
        ))


if __name__ == "__main__":
    unittest.main()
