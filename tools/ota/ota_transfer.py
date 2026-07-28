#!/usr/bin/env python3

import enum
import pathlib
import random
import struct
import time
import zlib


PROTOCOL_VERSION = 1
CAN_REQUEST_ID = 0x730
CAN_RESPONSE_ID = 0x731
CAN_FRAME_SIZE = 64
CAN_DATA_SIZE = 56
UART_MAGIC = b"\xA5\x5A"
UART_HEADER_SIZE = 12
UART_DATA_SIZE = 240


class MessageType(enum.IntEnum):
    BEGIN = 1
    DATA = 2
    END = 3
    ABORT = 4
    STATUS = 5


class Result(enum.IntEnum):
    OK = 0
    BUSY = 1
    INVALID_STATE = 2
    INVALID_HEADER = 3
    INVALID_SEQUENCE = 4
    INVALID_CRC = 5
    NO_SPACE = 6
    IO_ERROR = 7
    TIMEOUT = 8


class TransferState(enum.IntEnum):
    IDLE = 0
    PREPARING = 1
    RECEIVING = 2
    FINALIZING = 3
    STAGED = 4
    ABORTED = 5
    FAILED = 6


class OtaError(RuntimeError):
    pass


def load_package(path):
    package = pathlib.Path(path).read_bytes()
    if not package:
        raise OtaError("OTA package is empty")
    return package


def new_session_id():
    return random.SystemRandom().randint(1, 255)


def package_crc32(package):
    return zlib.crc32(package) & 0xFFFFFFFF


def encode_can_request(message_type, session_id, argument=0, data=b""):
    if len(data) > CAN_DATA_SIZE:
        raise OtaError("CAN OTA payload exceeds 56 bytes")
    header = struct.pack(
        "<BBBBI", PROTOCOL_VERSION, message_type, session_id, len(data), argument
    )
    return (header + data).ljust(CAN_FRAME_SIZE, b"\0")


def decode_can_response(can_id, payload):
    if can_id != CAN_RESPONSE_ID or len(payload) != CAN_FRAME_SIZE:
        return None
    version, message_type, session_id, result, state = struct.unpack_from(
        "<BBBBB", payload
    )
    if version != PROTOCOL_VERSION or message_type != MessageType.STATUS:
        return None
    if any(payload[5:8]) or any(payload[12:]):
        return None
    next_offset = struct.unpack_from("<I", payload, 8)[0]
    return session_id, Result(result), TransferState(state), next_offset


def encode_uart_request(message_type, session_id, argument=0, data=b""):
    if len(data) > UART_DATA_SIZE:
        raise OtaError("UART OTA payload exceeds 240 bytes")
    frame = UART_MAGIC + struct.pack(
        "<BBBB2xI",
        PROTOCOL_VERSION,
        message_type,
        session_id,
        len(data),
        argument,
    )
    frame += data
    return frame + struct.pack("<I", zlib.crc32(frame) & 0xFFFFFFFF)


def decode_uart_response(frame):
    if len(frame) < UART_HEADER_SIZE + 4 or frame[:2] != UART_MAGIC:
        return None
    version, message_type, session_id, data_length = struct.unpack_from(
        "<BBBB", frame, 2
    )
    if version != PROTOCOL_VERSION or message_type != MessageType.STATUS:
        return None
    if frame[6:8] != b"\0\0" or data_length != 2:
        return None
    expected_length = UART_HEADER_SIZE + data_length + 4
    if len(frame) != expected_length:
        return None
    received_crc = struct.unpack_from("<I", frame, expected_length - 4)[0]
    if zlib.crc32(frame[:-4]) & 0xFFFFFFFF != received_crc:
        return None
    next_offset = struct.unpack_from("<I", frame, 8)[0]
    return session_id, Result(frame[12]), TransferState(frame[13]), next_offset


def transfer_package(exchange, package, chunk_size, session_id, progress=print):
    crc = package_crc32(package)
    begin_data = struct.pack("<I", crc)
    response = exchange(MessageType.BEGIN, session_id, len(package), begin_data)
    _require_session(response, session_id)
    if response[1] not in (Result.OK, Result.BUSY):
        _raise_response("BEGIN rejected", response)

    while response[2] == TransferState.PREPARING:
        time.sleep(0.1)
        response = exchange(MessageType.STATUS, session_id)
        _require_ok("STATUS while preparing", response, session_id)
    if response[2] != TransferState.RECEIVING:
        _raise_response("device did not enter RECEIVING", response)

    offset = response[3]
    if offset > len(package):
        raise OtaError(f"device returned invalid offset {offset}")
    while offset < len(package):
        block = package[offset : offset + chunk_size]
        response = exchange(MessageType.DATA, session_id, offset, block)
        _require_session(response, session_id)
        expected_offset = offset + len(block)
        while response[1] == Result.BUSY:
            time.sleep(0.05)
            response = exchange(MessageType.STATUS, session_id)
            _require_session(response, session_id)
        if response[1] == Result.INVALID_SEQUENCE and response[3] == expected_offset:
            pass
        elif response[1] != Result.OK:
            _raise_response(f"DATA at offset {offset} rejected", response)
        if response[3] != expected_offset:
            _raise_response(f"unexpected DATA offset after {offset}", response)
        offset = response[3]
        progress(f"OTA {offset}/{len(package)} bytes ({offset * 100 // len(package)}%)")

    response = exchange(
        MessageType.END, session_id, len(package), struct.pack("<I", crc)
    )
    _require_session(response, session_id)
    while response[1] == Result.BUSY or response[2] == TransferState.FINALIZING:
        time.sleep(0.1)
        response = exchange(MessageType.STATUS, session_id)
        _require_session(response, session_id)
    if response[1] != Result.OK or response[2] != TransferState.STAGED:
        _raise_response("END did not produce STAGED", response)
    return response


def _require_session(response, session_id):
    if response is None:
        raise OtaError("missing OTA response")
    if response[0] != session_id:
        raise OtaError(
            f"response session mismatch: expected {session_id}, got {response[0]}"
        )


def _require_ok(operation, response, session_id):
    _require_session(response, session_id)
    if response[1] != Result.OK:
        _raise_response(f"{operation} failed", response)


def _raise_response(message, response):
    raise OtaError(
        f"{message}: result={response[1].name} state={response[2].name} "
        f"next_offset={response[3]}"
    )
