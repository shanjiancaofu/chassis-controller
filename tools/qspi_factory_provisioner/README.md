# QSPI factory provisioner

This maintenance target initializes the W25Q64 factory rollback baseline when
STM32 ROM DFU is available but no board-specific CubeProgrammer External Loader
is installed. It is not part of the production Bootloader or Application.

The current target is intentionally fixed to the b12 factory inputs:

```text
Internal 0x08000000: qspi-factory-provisioner.bin
Internal 0x08008000: app-v0.1.0-b12.ota, 183660 bytes
Internal 0x0807F000: Metadata A, 64 bytes
QSPI 0x00400000: confirmed Metadata A
QSPI 0x00401000: erased Metadata B
QSPI 0x00402000: b12 OTA package in Slot A
```

The provisioner validates the staged OTA header, payload CRC, vector table,
metadata CRC, state, slot roles, image size, and image CRC before erasing QSPI.
It erases and verifies both metadata sectors, writes and fully compares Slot A,
then commits Metadata A last. Success is reported only as:

```text
PROVISION: PASS
```

Build the maintained target from the repository root:

```text
cmake --preset arm-release
cmake --build --preset arm-release --target qspi_factory_provisioner
```

Artifacts are generated under `build/arm-release/`. Toolchain setup is documented
in [`docs/cmake_build.md`](../../docs/cmake_build.md).

`build.ps1` is retained only to reproduce the exact 2026-08-13 provisioning
artifact from the then-current CubeIDE Release objects. It is not the maintained
build path and must not be used after source or CubeMX-generated code changes.
After provisioning, erase internal Flash and restore the matching production
factory image before normal operation.
