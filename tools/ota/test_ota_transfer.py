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

    def test_resume_from_reported_offset(self):
        package = bytes(range(100))
        device = SimulatedDevice()
        device.received = package[:37]
        ota.transfer_package(device.exchange, package, 16, 3, lambda _text: None)
        self.assertEqual(device.received, package)

    def test_busy_data_is_recovered_through_status(self):
        package = bytes(range(80))
        device = BusyDataDevice()
        response = ota.transfer_package(
            device.exchange, package, 20, 4, lambda _text: None
        )
        self.assertEqual(response[2], ota.TransferState.STAGED)
        self.assertEqual(device.received, package)

    def test_invalid_sequence_with_committed_offset_is_accepted(self):
        package = bytes(range(64))
        device = InvalidSequenceAckDevice()
        ota.transfer_package(device.exchange, package, 16, 5, lambda _text: None)
        self.assertEqual(device.received, package)

    def test_invalid_resume_offset_is_rejected(self):
        def exchange(message_type, session_id, argument=0, data=b""):
            del argument, data
            return (session_id, ota.Result.OK, ota.TransferState.RECEIVING, 999)

        with self.assertRaisesRegex(ota.OtaError, "invalid offset"):
            ota.transfer_package(exchange, b"abc", 2, 6, lambda _text: None)

    def test_session_mismatch_is_rejected(self):
        def exchange(message_type, session_id, argument=0, data=b""):
            del message_type, argument, data
            return (session_id + 1, ota.Result.OK, ota.TransferState.RECEIVING, 0)

        with self.assertRaisesRegex(ota.OtaError, "session mismatch"):
            ota.transfer_package(exchange, b"abc", 2, 7, lambda _text: None)


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


class BusyDataDevice(SimulatedDevice):
    def __init__(self):
        super().__init__()
        self.busy_once = True

    def exchange(self, message_type, session_id, argument=0, data=b""):
        if ota.MessageType(message_type) == ota.MessageType.DATA and self.busy_once:
            self.assert_offset(argument)
            self.received += data
            self.busy_once = False
            return (session_id, ota.Result.BUSY, ota.TransferState.RECEIVING,
                    len(self.received))
        if ota.MessageType(message_type) == ota.MessageType.STATUS and not self.preparing:
            return (session_id, ota.Result.OK, ota.TransferState.RECEIVING,
                    len(self.received))
        return super().exchange(message_type, session_id, argument, data)


class InvalidSequenceAckDevice(SimulatedDevice):
    def exchange(self, message_type, session_id, argument=0, data=b""):
        if ota.MessageType(message_type) == ota.MessageType.DATA:
            self.assert_offset(argument)
            self.received += data
            return (session_id, ota.Result.INVALID_SEQUENCE,
                    ota.TransferState.RECEIVING, len(self.received))
        return super().exchange(message_type, session_id, argument, data)


if __name__ == "__main__":
    unittest.main()
