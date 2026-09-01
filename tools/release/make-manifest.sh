#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${BUILD_DIR:-${root_dir}/build/arm-release}"
output_file="${1:-${build_dir}/release-manifest.txt}"
build_info="${root_dir}/firmware/application/stm32g474/config/build_info.h"

version="$(sed -n 's/^#define CHASSIS_FIRMWARE_VERSION "\([^"]*\)"$/\1/p' "${build_info}")"
build_number="$(sed -n 's/^#define CHASSIS_FIRMWARE_BUILD \([0-9][0-9]*\)$/\1/p' "${build_info}")"
toolchain="${ARM_NONE_EABI_GCC:-arm-none-eabi-gcc}"

[[ -n "${version}" && -n "${build_number}" ]] || {
  echo "firmware version/build could not be read from ${build_info}" >&2
  exit 1
}
command -v "${toolchain}" >/dev/null 2>&1 || {
  echo "toolchain executable not found: ${toolchain}" >&2
  exit 1
}

sha256() {
  sha256sum "$1" | awk '{print $1}'
}

size_bytes() {
  stat -c '%s' "$1"
}

artifact_line() {
  local label="$1"
  local path="$2"

  [[ -f "${path}" ]] || {
    echo "missing artifact: ${path}" >&2
    exit 1
  }
  printf '%s: %s\n' "${label}" "${path}"
  printf '  size_bytes: %s\n' "$(size_bytes "${path}")"
  printf '  sha256: %s\n' "$(sha256 "${path}")"
}

mkdir -p "$(dirname -- "${output_file}")"
{
  printf 'project: chassis-controller\n'
  printf 'firmware_version: %s\n' "${version}"
  printf 'firmware_build: %s\n' "${build_number}"
  printf 'source_commit: %s\n' "$(git -C "${root_dir}" rev-parse HEAD)"
  printf 'source_dirty: %s\n' "$(git -C "${root_dir}" status --porcelain | if read -r _; then echo true; else echo false; fi)"
  printf 'toolchain: %s\n' "${toolchain}"
  printf 'toolchain_version: %s\n' "$("${toolchain}" -dumpfullversion)"
  printf 'build_timestamp_utc: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  artifact_line application_elf "${build_dir}/application.elf"
  artifact_line application_bin "${build_dir}/application.bin"
  artifact_line bootloader_elf "${build_dir}/bootloader.elf"
  artifact_line bootloader_bin "${build_dir}/bootloader.bin"
  artifact_line provisioner_elf "${build_dir}/qspi_factory_provisioner.elf"
  artifact_line provisioner_bin "${build_dir}/qspi_factory_provisioner.bin"
  if [[ -f "${build_dir}/app-v${version}-b${build_number}.ota" ]]; then
    artifact_line application_ota "${build_dir}/app-v${version}-b${build_number}.ota"
  else
    printf 'application_ota: NOT_BUILT\n'
  fi
  printf 'hardware_validation: PENDING\n'
} >"${output_file}"

printf 'wrote %s\n' "${output_file}"
