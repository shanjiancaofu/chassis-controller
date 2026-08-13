# Firmware artifacts

Current development versions and pending hardware verification are summarized in
[`docs/current_status.md`](../docs/current_status.md). Detailed evidence remains in
[`docs/verification.md`](../docs/verification.md); artifact files do not establish a hardware pass.

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
qspi:        factory/qspi-a<app-build>-confirmed.bin
```

Current artifact names omit timestamps. The version/build pair identifies release inputs;
timestamped and diagnostic files belong under `archive/`.

Most recent hardware-verified baseline artifacts (historical b6/build15):

```text
application/app-v0.1.0-b6.bin
application/app-v0.1.0-b6.ota
bootloader/boot-v0.1.0-b15.bin
factory/factory-a6-b15.bin
```

These files document the last hardware-verified factory/UART baseline. They are not a claim that
the current b11/build21 source has been burned. CAN FD OTA, interrupted installation recovery, and
trial rollback remain unverified; see
[`docs/verification.md`](../docs/verification.md).

Programming addresses:

```text
Application BIN: 0x08008000
Bootloader BIN:  0x08000000
Factory BIN:     0x08000000
QSPI raw image:  external loader / QSPI base `0x000000`
```

The factory QSPI image is generated from the same `.ota` package as the Application BIN. It
contains Slot A and `CONFIRMED` Metadata A, while Metadata B remains erased. Program it with an
External Loader or equivalent factory provisioning step before claiming first-install rollback.
The factory generator requires both `--ota` and `--qspi-output` by default. Internal-Flash-only
diagnostic images require the explicit `--internal-only` opt-out and are not production baselines.

CubeIDE currently updates ELF but does not reliably regenerate BIN. Recreate each BIN from the
matching final ELF with `arm-none-eabi-objcopy -O binary` before packaging or combining images.

Files under `archive/` are not release artifacts.
