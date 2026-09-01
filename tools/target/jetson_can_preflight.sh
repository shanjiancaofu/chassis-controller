#!/usr/bin/env bash
set -euo pipefail

# Recover the Jetson-side CAN/CH340 transport prerequisites without sending a
# CAN data frame. This intentionally does not run cansend or target regression.

original_args=("$@")
fix_brltty=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --fix-brltty) fix_brltty=true ;;
    -h|--help)
      echo "usage: $0 [--fix-brltty]"
      exit 0
      ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

if [[ "$(id -u)" -ne 0 ]]; then
  exec sudo --preserve-env=PATH bash "$0" "${original_args[@]}"
fi

interface="${CAN_INTERFACE:-can0}"
data_sample_point="${CAN_DATA_SAMPLE_POINT:-0.80}"

if [[ "${fix_brltty}" == true ]]; then
  systemctl mask --now brltty.service brltty-udev.service 2>/dev/null || true
else
  systemctl stop brltty.service brltty-udev.service 2>/dev/null || true
fi
pkill -9 -x brltty 2>/dev/null || true
modprobe ch341

ch341_interface="${CH341_INTERFACE:-}"
if [[ -z "${ch341_interface}" ]]; then
  mapfile -t ch341_interfaces < <(
    for device in /sys/bus/usb/devices/*; do
      [[ -r "${device}/idVendor" && -r "${device}/idProduct" ]] || continue
      [[ "$(<"${device}/idVendor")" == "1a86" && "$(<"${device}/idProduct")" == "7523" ]] || continue
      for usb_interface in "${device}":*; do
        [[ -e "${usb_interface}" ]] && basename "${usb_interface}"
      done
    done
  )
  if [[ "${#ch341_interfaces[@]}" -gt 1 ]]; then
    echo "multiple CH340 interfaces found; set CH341_INTERFACE explicitly" >&2
    exit 1
  fi
  ch341_interface="${ch341_interfaces[0]:-}"
fi

if [[ -n "${ch341_interface}" && -e "/sys/bus/usb/devices/${ch341_interface}" &&
      -e /sys/bus/usb/drivers/usb_ch341/bind ]]; then
  driver_link="/sys/bus/usb/devices/${ch341_interface}/driver"
  if [[ ! -L "${driver_link}" || "$(readlink "${driver_link}")" != *usb_ch341 ]]; then
    printf '%s\n' "${ch341_interface}" > /sys/bus/usb/drivers/usb_ch341/bind || true
  fi
fi

ip link set "${interface}" down
ip link set "${interface}" type can \
  bitrate 500000 sample-point 0.8 \
  dbitrate 2000000 dsample-point "${data_sample_point}" \
  fd on one-shot on berr-reporting on restart-ms 1000
ip link set "${interface}" up

printf '%s\n' '=== serial ==='
ls -l /dev/ttyCH341USB* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true
printf '%s\n' '=== can ==='
ip -details -statistics link show "${interface}"
printf '%s\n' '=== passive listen: no CAN frames transmitted ==='
timeout "${LISTEN_SECONDS:-5}" candump -tz -x "${interface}" || [[ "$?" -eq 124 ]]
