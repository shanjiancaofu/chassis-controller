#!/usr/bin/env python3

import struct
import sys
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ota_transfer as ota


class OtaProtocolTest(unittest.TestCase):
    def test_uart_request_layout_and_crc(self):
        frame = ota.encode_uart_request(ota.MessageType.DATA, 7, 123, b"abc")
        self.assertEqual(len(frame), 19)
        self.assertEqual(frame[0:2], ota.UART_MAGIC)
        self.assertEqual(frame[6:8], b"\0\0")
        self.assertEqual(struct.unpack_from("<I", frame, 8)[0], 123)
        self.assertEqual(
            struct.unpack_from("<I", frame, len(frame) - 4)[0],
            zlib.crc32(frame[:-4]) & 0xFFFFFFFF,
        )

    def test_can_response_layout(self):
        frame = bytes([1, 5, 7, 0, 2, 0, 0, 0])
        frame += struct.pack("<I", 123) + bytes(52)
        self.assertEqual(
            ota.decode_can_response(ota.CAN_RESPONSE_ID, frame),
            (7, ota.Result.OK, ota.TransferState.RECEIVING, 123),
        )

    def test_stop_and_wait_transfer(self):
        package = bytes(range(250)) * 3
        device = SimulatedDevice()
        response = ota.transfer_package(
            device.exchange, package, ota.CAN_DATA_SIZE, 9, lambda _text: None
        )
        self.assertEqual(response[2], ota.TransferState.STAGED)
        self.assertEqual(response[3], len(package))
        self.assertEqual(device.received, package)


class SimulatedDevice:
    def __init__(self):
        self.received = b""
        self.preparing = False

    def exchange(self, message_type, session_id, argument=0, data=b""):
        message_type = ota.MessageType(message_type)
        if message_type == ota.MessageType.BEGIN:
            self.preparing = True
            return (
                session_id,
                ota.Result.OK,
                ota.TransferState.PREPARING,
                0,
            )
        if message_type == ota.MessageType.STATUS and self.preparing:
            self.preparing = False
            return (
                session_id,
                ota.Result.OK,
                ota.TransferState.RECEIVING,
                len(self.received),
            )
        if message_type == ota.MessageType.DATA:
            self.assert_offset(argument)
            self.received += data
            return (
                session_id,
                ota.Result.OK,
                ota.TransferState.RECEIVING,
                len(self.received),
            )
        if message_type == ota.MessageType.END:
            self.assert_offset(argument)
            return (
                session_id,
                ota.Result.OK,
                ota.TransferState.STAGED,
                len(self.received),
            )
        raise AssertionError(f"unexpected message {message_type}")

    def assert_offset(self, offset):
        if offset != len(self.received):
            raise AssertionError(
                f"expected offset {len(self.received)}, received {offset}"
            )


if __name__ == "__main__":
    unittest.main()
