PROGRAM:=uart
SOURCE:=../startup.s main.s uart.s
CLEAN_FILES:=testterm

all: flash.rom testterm

run: all
	./testterm /dev/ttyACM0 115200 8N1

testterm: ../../util/testterm.c
	gcc -std=gnu99 -otestterm $^

flash.rom: ${PROGRAM}
	arm-none-eabi-objcopy -O binary ${PROGRAM} flash.rom

${PROGRAM}: ../startup.ld ${SOURCE}
	arm-none-eabi-gcc -o${PROGRAM} -T ../startup.ld -nostdlib -mthumb -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=softfp -std=c99 -Wall ${SOURCE}

flash: flash.rom
	openocd -s ../ -f ../ocdflash.cfg

debug: all
	xterm -e "openocd -f ../tm4c123gxl.cfg" &
	arm-none-eabi-gdb --eval-command="target remote localhost:3333" --eval-command="mon reset halt" ${PROGRAM}

clean:
	rm -f ${PROGRAM} ${CLEAN_FILES} flash.rom
