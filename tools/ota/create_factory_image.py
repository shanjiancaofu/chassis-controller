#!/usr/bin/env python3

import argparse
import pathlib
import struct
import sys


FLASH_BASE = 0x08000000
BOOTLOADER_START = 0x08000000
BOOTLOADER_SIZE = 0x00008000
APPLICATION_START = 0x08008000
APPLICATION_SIZE = 0x00078000
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
        application_offset = APPLICATION_START - FLASH_BASE
        combined = bytearray(b"\xFF" * (application_offset + len(application)))
        combined[: len(bootloader)] = bootloader
        combined[application_offset:] = application
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(combined)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(
        f"factory image={args.output} bytes={len(combined)} "
        f"bootloader={len(bootloader)} application={len(application)}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
