# Firmware artifacts

Burn and update artifacts are grouped by type and build date:

```text
application/  Application BIN and OTA package
bootloader/   Bootloader BIN
factory/      Combined Bootloader + Application BIN
archive/      Historical, candidate, diagnostic, readback, and host-test files
```

Naming rules:

```text
application: chassis-controller-<version>-build<N>-<YYYYMMDD-HHMMSS>.<bin|ota>
bootloader:  bootloader-<version>-build<N>-<YYYYMMDD-HHMMSS>.bin
factory:     chassis-controller-app-<version>-build<N>_bootloader-<version>-build<N>-<YYYYMMDD-HHMMSS>-factory.bin
```

Current verified artifacts:

```text
application/chassis-controller-0.1.0-build6-20260730-201215.bin
application/chassis-controller-0.1.0-build6-20260730-201234.ota
bootloader/bootloader-0.1.0-build15-20260730-213118.bin
factory/chassis-controller-app-0.1.0-build6_bootloader-0.1.0-build15-20260730-213129-factory.bin
```

The factory image and UART OTA chain have passed hardware verification. CAN FD OTA, interrupted
installation recovery, and trial rollback remain unverified; see
[`docs/verification.md`](../docs/verification.md).

Programming addresses:

```text
Application BIN: 0x08008000
Bootloader BIN:  0x08000000
Factory BIN:     0x08000000
```

CubeIDE currently updates ELF but does not reliably regenerate BIN. Recreate each BIN from the
matching final ELF with `arm-none-eabi-objcopy -O binary` before packaging or combining images.

Files under `archive/` are not release artifacts.
