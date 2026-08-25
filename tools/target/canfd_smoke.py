#!/usr/bin/env python3
"""Safe zero-velocity CAN FD smoke test for Chassis CAN FD V1."""

from __future__ import annotations

import argparse
import pathlib
import socket
import struct
import sys
import time

from report import CheckResult, RegressionReport


CAN_EFF_MASK = 0x1FFFFFFF
CAN_ERR_FLAG = 0x20000000
CAN_ERR_BUSOFF = 0x00000040
CAN_RAW_ERR_FILTER = 2
CAN_RAW_FD_FRAMES = getattr(socket, "CAN_RAW_FD_FRAMES", 5)
CANFD_BRS = 0x01
FRAME = struct.Struct("=IBBBB64s")


def crc16(identifier: int, payload: bytes) -> int:
    crc = 0xFFFF
    for value in struct.pack("<H", identifier) + payload:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def heartbeat(sequence: int, uptime_ms: int) -> bytes:
    payload = bytearray(12)
    payload[0:4] = bytes((1, 1, sequence & 0xFF, 1))
    struct.pack_into("<IH", payload, 4, uptime_ms & 0xFFFFFFFF, 0)
    struct.pack_into("<H", payload, 10, crc16(0x200, payload[:10]))
    return bytes(payload)


def velocity(sequence: int, enabled: bool) -> bytes:
    payload = bytearray(12)
    payload[0:4] = bytes((1, 1 if enabled else 0, sequence & 0xFF, 0))
    struct.pack_into("<hhH", payload, 4, 0, 0, 0)
    struct.pack_into("<H", payload, 10, crc16(0x101, payload[:10]))
    return bytes(payload)


def send_frame(sock: socket.socket, identifier: int, payload: bytes) -> None:
    sock.send(FRAME.pack(identifier, len(payload), CANFD_BRS, 0, 0, payload.ljust(64, b"\0")))


def receive_until(sock: socket.socket, deadline: float, wanted: set[int]) -> dict[int, bytes]:
    received: dict[int, bytes] = {}
    while time.monotonic() < deadline and not wanted.issubset(received):
        try:
            frame = sock.recv(FRAME.size)
        except TimeoutError:
            continue
        can_id, length, _flags, _r0, _r1, data = FRAME.unpack(frame)
        identifier = can_id & CAN_EFF_MASK
        if can_id & CAN_ERR_FLAG and identifier & CAN_ERR_BUSOFF:
            received[CAN_ERR_BUSOFF] = data[:length]
        elif identifier in wanted:
            received[identifier] = data[:length]
    return received


def run_canfd_smoke(interface: str, timeout: float, expect_bus_off: bool) -> list[CheckResult]:
    report = RegressionReport()
    try:
        with socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW) as sock:
            sock.setsockopt(socket.SOL_CAN_RAW, CAN_RAW_FD_FRAMES, 1)
            sock.setsockopt(socket.SOL_CAN_RAW, CAN_RAW_ERR_FILTER, struct.pack("=I", CAN_ERR_BUSOFF))
            sock.settimeout(0.1)
            sock.bind((interface,))

            initial = receive_until(sock, time.monotonic() + timeout, {0x200})
            report.add("STM32 heartbeat", 0x200 in initial)

            sequence = 0
            start = time.monotonic()
            for _ in range(4):
                send_frame(sock, 0x200, heartbeat(sequence, int((time.monotonic() - start) * 1000)))
                send_frame(sock, 0x101, velocity(sequence, True))
                sequence = (sequence + 1) & 0xFF
                time.sleep(0.05)
            active = receive_until(sock, time.monotonic() + timeout, {0x180, 0x181, 0x200, 0x240})
            report.add("0x101 zero-velocity path", 0x180 in active, "no non-zero target sent")
            report.add("status channels", {0x180, 0x181, 0x200, 0x240}.issubset(active), ",".join(hex(item) for item in sorted(active)))

            timeout_deadline = time.monotonic() + 0.45
            while time.monotonic() < timeout_deadline:
                send_frame(sock, 0x200, heartbeat(sequence, int((time.monotonic() - start) * 1000)))
                sequence = (sequence + 1) & 0xFF
                time.sleep(0.08)
            timed_out = receive_until(sock, time.monotonic() + timeout, {0x180})
            motion = timed_out.get(0x180, b"")
            report.add(
                "200 ms control timeout",
                len(motion) == 16 and motion[3] == 2,
                f"control_state={motion[3] if len(motion) == 16 else 'missing'}",
            )
            send_frame(sock, 0x101, velocity(sequence, False))

            if expect_bus_off:
                bus_off = receive_until(sock, time.monotonic() + timeout, {CAN_ERR_BUSOFF})
                report.add("CAN bus-off event", CAN_ERR_BUSOFF in bus_off, "external fault injection required")
            else:
                report.skip("CAN bus-off event", "rerun with --expect-bus-off during external fault injection")
    except OSError as error:
        report.add("SocketCAN access", False, str(error))
    return report.results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interface", default="can0")
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--expect-bus-off", action="store_true")
    parser.add_argument("--json", type=pathlib.Path)
    args = parser.parse_args()
    report = RegressionReport()
    report.extend(run_canfd_smoke(args.interface, args.timeout, args.expect_bus_off))
    report.print()
    if args.json:
        report.write_json(args.json)
    return 0 if report.passed else 1


if __name__ == "__main__":
    sys.exit(main())
