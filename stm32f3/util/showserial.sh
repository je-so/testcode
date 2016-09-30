#!/bin/sh
SERIAL_DEV=/dev/ttyACM0
SPEED=115200

# parenb  generate parity bit in output and expect parity bit in input
# -parenb no parity bit
# parodd  set odd parity
# -parodd set even parity
# cstopb  use two stop bits per character
# -cstopb use one stop bits per character
# csN     set character size to N bits, N in [5..8]
stty -F $SERIAL_DEV -raw ispeed $SPEED ospeed $SPEED -parenb -cstopb -istrip -icrnl cs8;

# read from device and show output
while [ true ]; do cat /dev/ttyACM0 ; done
