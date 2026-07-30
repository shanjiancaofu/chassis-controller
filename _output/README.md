# Firmware artifacts

Burn and update artifacts are grouped by type:

```text
application/  Application BIN and OTA package
bootloader/   Bootloader BIN
factory/      Combined Bootloader + Application BIN
archive/      Historical, candidate, diagnostic, readback, and host-test files
```

Naming rules:

```text
application: app-v<version>-b<N>.<bin|ota>
bootloader:  boot-v<version>-b<N>.bin
factory:     factory-a<app-build>-b<boot-build>.bin
```

Current artifact names omit timestamps. The version/build pair identifies release inputs;
timestamped and diagnostic files belong under `archive/`.

Current verified artifacts:

```text
application/app-v0.1.0-b6.bin
application/app-v0.1.0-b6.ota
bootloader/boot-v0.1.0-b15.bin
factory/factory-a6-b15.bin
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
