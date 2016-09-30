PROGRAM:=term
CFLAGS:=-DKONFIG_STACKSIZE=512
SOURCE:=../startup.c main.c
include ../Makefile

# Use your own terminal program instead of screen (ctrl-a + '\' exits program)
# Change /dev/ttyUSB0 to the serial device the USB-serial converter uses
# The UART on µController uses 115200 Baud, 8 data, no parity, 1 stopbit
run2:
	screen /dev/ttyUSB0 115200
