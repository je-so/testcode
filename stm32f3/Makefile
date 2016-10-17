# Eingabe Parameter
# PROGRAM: Name des Programms
# SOURCE:  Liste der zu compilierenden Dateien
# CFLAGS:  Flags für den Compiler
# LIBS:    Bibliotheken, z.B -lm für die Mathebibliothek

# Beispiel Makefile
# PROGRAM:=uart
# SOURCE:=../startup.c main.s uart.s
# CFLAGS:=-DKONFIG_STACKSIZE=512 -Os
# include ../Makefile

WARNINGS:=-Wall

CLEAN_FILES:=testterm

all: flash.rom

run: testterm
	./testterm /dev/ttyUSB0 115200 8N1

testterm: ../util/testterm.c
	gcc -std=gnu99 -otestterm $^

flash.rom: ${PROGRAM}
	arm-none-eabi-objcopy -O binary ${PROGRAM} flash.rom

${PROGRAM}: ../startup.ld ${SOURCE}
	arm-none-eabi-gcc -o${PROGRAM} -T ../startup.ld -nostdlib -mthumb -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=softfp -std=c99 ${WARNINGS} -I../ ${CFLAGS} ${SOURCE} ${LIBS}

flash: flash.rom
	openocd -s ../ -f ../ocdflash.cfg

debug: all
	xterm -e "openocd -f ../stm32f303vc.cfg" &
	arm-none-eabi-gdb --eval-command="target remote localhost:3333" --eval-command="mon reset halt" ${PROGRAM}

# gdb commamds: "mon resume" continues, "info reg" shows register ...
attach: all
	xterm -e "openocd -f ../stm32f303vc.cfg" &
	arm-none-eabi-gdb --eval-command="target remote localhost:3333" --eval-command="mon halt" ${PROGRAM}

jattach: all
	xterm -e "openocd -f ../stm32f303vc.cfg" &
	arm-none-eabi-gdb --eval-command="target remote localhost:3333" --eval-command="mon halt" --eval-command="print s_10ms" --eval-command="print s_cycles1" ${PROGRAM}

clean:
	rm -f ${PROGRAM} ${CLEAN_FILES} flash.rom

build_projects:
	for i in P[0-9][0-9]_* test/ jrtos/; do\
		cd $$i; make || exit 1; cd ..;\
	done;

clean_projects:
	for i in P[0-9][0-9]_* test/ jrtos/; do\
		cd $$i; make clean; cd ..;\
	done;
