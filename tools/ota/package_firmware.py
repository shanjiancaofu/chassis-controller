#!/usr/bin/env python3

import argparse
import pathlib
import struct
import sys
import zlib


IMAGE_MAGIC = 0x3141544F
FORMAT_VERSION = 1
HEADER_FORMAT = "<IHH8I20sI"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
APPLICATION_START = 0x08008000
APPLICATION_SIZE = 0x00078000
SRAM_START = 0x20000000
SRAM_END = 0x20020000


def parse_version(value):
    parts = value.split(".")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("version must be MAJOR.MINOR.PATCH")
    try:
        major, minor, patch = (int(part, 10) for part in parts)
    except ValueError as error:
        raise argparse.ArgumentTypeError("version contains a non-number") from error
    if not 0 <= major <= 255 or not 0 <= minor <= 255 or not 0 <= patch <= 65535:
        raise argparse.ArgumentTypeError("version component is out of range")
    return (major << 24) | (minor << 16) | patch


def validate_vector_table(payload):
    if len(payload) < 8:
        raise ValueError("firmware is smaller than its vector table")

    stack_pointer, reset_handler = struct.unpack_from("<II", payload)
    if stack_pointer < SRAM_START or stack_pointer > SRAM_END or stack_pointer % 8:
        raise ValueError(f"invalid initial stack pointer: 0x{stack_pointer:08X}")
    if reset_handler & 1 == 0:
        raise ValueError(f"reset handler is not Thumb code: 0x{reset_handler:08X}")

    reset_address = reset_handler & ~1
    application_end = APPLICATION_START + APPLICATION_SIZE
    if not APPLICATION_START <= reset_address < application_end:
        raise ValueError(
            f"reset handler 0x{reset_handler:08X} is outside the OTA application "
            f"region 0x{APPLICATION_START:08X}-0x{application_end - 1:08X}"
        )


def create_header(payload, firmware_version, build_number, rollback_counter):
    payload_crc = zlib.crc32(payload) & 0xFFFFFFFF
    fields = (
        IMAGE_MAGIC,
        FORMAT_VERSION,
        HEADER_SIZE,
        len(payload),
        payload_crc,
        APPLICATION_START,
        APPLICATION_START,
        firmware_version,
        build_number,
        rollback_counter,
        0,
        bytes(20),
        0,
    )
    header = struct.pack(HEADER_FORMAT, *fields)
    header_crc = zlib.crc32(header[:-4]) & 0xFFFFFFFF
    return header[:-4] + struct.pack("<I", header_crc)


def verify_package(package):
    if len(package) < HEADER_SIZE:
        raise ValueError("package is smaller than its header")

    fields = struct.unpack_from(HEADER_FORMAT, package)
    (
        magic,
        format_version,
        header_size,
        payload_size,
        payload_crc,
        load_address,
        vector_address,
        _firmware_version,
        _build_number,
        _rollback_counter,
        flags,
        _reserved,
        header_crc,
    ) = fields

    if magic != IMAGE_MAGIC or format_version != FORMAT_VERSION:
        raise ValueError("unsupported OTA package format")
    if header_size != HEADER_SIZE:
        raise ValueError(f"unexpected header size: {header_size}")
    if load_address != APPLICATION_START or vector_address != APPLICATION_START:
        raise ValueError("package targets the wrong application address")
    if flags != 0:
        raise ValueError("encrypted or signed package flags are not supported in v1")
    if len(package) != HEADER_SIZE + payload_size:
        raise ValueError("package length does not match its header")
    if zlib.crc32(package[: HEADER_SIZE - 4]) & 0xFFFFFFFF != header_crc:
        raise ValueError("header CRC32 mismatch")

    payload = package[HEADER_SIZE:]
    if zlib.crc32(payload) & 0xFFFFFFFF != payload_crc:
        raise ValueError("payload CRC32 mismatch")
    validate_vector_table(payload)
    return payload_size, payload_crc


def main():
    parser = argparse.ArgumentParser(
        description="Package a linked STM32G474 application for staged QSPI OTA."
    )
    parser.add_argument("input", type=pathlib.Path, help="application .bin")
    parser.add_argument("output", type=pathlib.Path, help="output .ota package")
    parser.add_argument("--version", required=True, type=parse_version)
    parser.add_argument("--build", required=True, type=int)
    parser.add_argument("--rollback-counter", type=int, default=0)
    args = parser.parse_args()

    try:
        payload = args.input.read_bytes()
        if len(payload) > APPLICATION_SIZE:
            raise ValueError(
                f"firmware is {len(payload)} bytes; maximum is {APPLICATION_SIZE}"
            )
        if args.build < 0 or args.build > 0xFFFFFFFF:
            raise ValueError("build number is out of range")
        if args.rollback_counter < 0 or args.rollback_counter > 0xFFFFFFFF:
            raise ValueError("rollback counter is out of range")

        validate_vector_table(payload)
        package = create_header(
            payload, args.version, args.build, args.rollback_counter
        ) + payload
        payload_size, payload_crc = verify_package(package)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(package)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(
        f"packed {payload_size} bytes, crc32=0x{payload_crc:08X}, "
        f"target=0x{APPLICATION_START:08X}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
