#!/usr/bin/env bash
set -euo pipefail

# Recover the Jetson-side CAN/CH340 transport prerequisites without sending a
# CAN data frame. This intentionally does not run cansend or target regression.

if [[ "$(id -u)" -ne 0 ]]; then
  exec sudo --preserve-env=PATH bash "$0" "$@"
fi

interface="${CAN_INTERFACE:-can0}"
ch341_interface="${CH341_INTERFACE:-1-2.4.4:1.0}"
data_sample_point="${CAN_DATA_SAMPLE_POINT:-0.80}"

systemctl mask --now brltty.service brltty-udev.service 2>/dev/null || true
pkill -9 -x brltty 2>/dev/null || true
modprobe ch341

if [[ -e "/sys/bus/usb/devices/${ch341_interface}" &&
      -e /sys/bus/usb/drivers/usb_ch341/bind ]]; then
  driver_link="/sys/bus/usb/devices/${ch341_interface}/driver"
  if [[ ! -L "${driver_link}" || "$(readlink "${driver_link}")" != *usb_ch341 ]]; then
    printf '%s\n' "${ch341_interface}" > /sys/bus/usb/drivers/usb_ch341/bind || true
  fi
fi

udevadm control --reload-rules
udevadm trigger --subsystem-match=tty || true

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
