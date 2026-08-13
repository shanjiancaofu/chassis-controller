#!/usr/bin/env python3

import argparse
import pathlib
import struct
import sys
import zlib

from package_firmware import verify_package


FLASH_BASE = 0x08000000
BOOTLOADER_START = 0x08000000
BOOTLOADER_SIZE = 0x00008000
APPLICATION_START = 0x08008000
APPLICATION_SIZE = 0x00078000
QSPI_CAPACITY = 0x00800000
QSPI_METADATA_A = 0x00400000
QSPI_SLOT_A = 0x00402000
OTA_METADATA_MAGIC = 0x314D544F
OTA_METADATA_FORMAT_VERSION = 1
OTA_STATE_CONFIRMED = 5
OTA_SLOT_NONE = 0
OTA_SLOT_A = 1
OTA_METADATA_FORMAT = "<IHH9I16sI"
SRAM_START = 0x20000000
SRAM_END = 0x20020000


def validate_image(name, image, start, size):
    if len(image) < 8:
        raise ValueError(f"{name} is smaller than its vector table")
    if len(image) > size:
        raise ValueError(f"{name} is {len(image)} bytes; maximum is {size}")
    stack_pointer, reset_handler = struct.unpack_from("<II", image)
    if not SRAM_START <= stack_pointer <= SRAM_END or stack_pointer % 8:
        raise ValueError(
            f"{name} has invalid initial stack pointer 0x{stack_pointer:08X}"
        )
    reset_address = reset_handler & ~1
    if reset_handler & 1 == 0 or not start <= reset_address < start + size:
        raise ValueError(
            f"{name} has invalid reset handler 0x{reset_handler:08X}"
        )


def main():
    parser = argparse.ArgumentParser(
        description="Create a first-install STM32 image containing Bootloader and Application."
    )
    parser.add_argument("bootloader", type=pathlib.Path)
    parser.add_argument("application", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument(
        "--ota",
        type=pathlib.Path,
        help="matching .ota package used to provision QSPI Slot A",
    )
    parser.add_argument(
        "--qspi-output",
        type=pathlib.Path,
        help="8 MiB raw QSPI image containing factory package and metadata",
    )
    parser.add_argument(
        "--internal-only",
        action="store_true",
        help="explicitly create only the internal Flash image for diagnostics",
    )
    args = parser.parse_args()

    try:
        bootloader = args.bootloader.read_bytes()
        application = args.application.read_bytes()
        validate_image(
            "bootloader", bootloader, BOOTLOADER_START, BOOTLOADER_SIZE
        )
        validate_image(
            "application", application, APPLICATION_START, APPLICATION_SIZE
        )
        if args.internal_only and (args.ota or args.qspi_output):
            raise ValueError("--internal-only cannot be combined with QSPI options")
        if not args.internal_only and not (args.ota and args.qspi_output):
            raise ValueError(
                "factory provisioning requires --ota and --qspi-output; "
                "use --internal-only only for diagnostics"
            )
        if bool(args.ota) != bool(args.qspi_output):
            raise ValueError("--ota and --qspi-output must be specified together")
        qspi_image = None
        if args.ota:
            package = args.ota.read_bytes()
            payload_size, payload_crc = verify_package(package)
            if package[64:] != application:
                raise ValueError("OTA package payload does not match application BIN")
            if payload_size != len(application) or (
                zlib.crc32(application) & 0xFFFFFFFF
            ) != payload_crc:
                raise ValueError("OTA package header does not match application BIN")
            metadata = struct.pack(
                OTA_METADATA_FORMAT,
                OTA_METADATA_MAGIC,
                OTA_METADATA_FORMAT_VERSION,
                64,
                1,
                OTA_STATE_CONFIRMED,
                OTA_SLOT_A,
                OTA_SLOT_NONE,
                payload_size,
                payload_crc,
                0,
                0,
                0,
                bytes(16),
                0,
            )
            metadata = metadata[:-4] + struct.pack(
                "<I", zlib.crc32(metadata[:-4]) & 0xFFFFFFFF
            )
            qspi_image = bytearray(b"\xFF" * QSPI_CAPACITY)
            qspi_image[QSPI_METADATA_A : QSPI_METADATA_A + len(metadata)] = metadata
            qspi_image[QSPI_SLOT_A : QSPI_SLOT_A + len(package)] = package
        application_offset = APPLICATION_START - FLASH_BASE
        combined = bytearray(b"\xFF" * (application_offset + len(application)))
        combined[: len(bootloader)] = bootloader
        combined[application_offset:] = application
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(combined)
        if qspi_image is not None:
            args.qspi_output.parent.mkdir(parents=True, exist_ok=True)
            args.qspi_output.write_bytes(qspi_image)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(
        f"factory image={args.output} bytes={len(combined)} "
        f"bootloader={len(bootloader)} application={len(application)}"
    )
    if args.qspi_output:
        print(
            f"qspi image={args.qspi_output} bytes={QSPI_CAPACITY} "
            "metadata=CONFIRMED slot=A"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
