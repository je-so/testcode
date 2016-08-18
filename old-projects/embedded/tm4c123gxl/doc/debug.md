### Debugging

Der Vorgang des Debuggens wird anhand des Beispielprojektes *Assembler01_RGBLED* dargestellt.
Zuerst wechselt man in das Projektverzeichnis. Dort werden mittels `make flash` das ARM-Programm *rgbled* und der zu flashende Binärcode *flash.rom* erstellt und dieser auch auf das Target geschrieben.

Zum Debuggen muss OpenOCD als Server gestartet werden, welcher das *gdbserver protocol* implementiert.
Der Übersichlichkeit wegen starten wir openocd in einem neuen Terminal mit `xterm -e "openocd -f ../tm4c123gxl.cfg" &`.
Der gestartete Server hört am TCP-Port 3333 als Defaulteinstellung.

Nachdem gdb gestartet ist mittels `arm-none-eabi-gdb rgbled` wird mit dem Kommando `target remote :3333` die Verbindung zum Gdb-Server hergestellt.
Das nächste Kommando `mon reset halt` setzt das Board nach einem Reset in den Debug-Modus. Im Folgenden ist der Ablauf einer Beispieldebugsitzung
dargestellt.

```Shell
$ xterm -e "openocd -f ../tm4c123gxl.cfg" &
$ arm-none-eabi-gdb rgbled
...
Copyright (C) 2013 Free Software Foundation, Inc.
...
Reading symbols from rgbled...(no debugging symbols found)...done.
(gdb) target remote :3333
Remote debugging using :3333
0x00000000 in g_NVIC_vectortable ()
(gdb) mon reset halt
target state: halted
target halted due to debug-request, current mode: Thread 
xPSR: 0x01000000 pc: 0x0000026c msp: 0x20000200
(gdb) break main
Breakpoint 1 at 0x2c0
(gdb)  cont
Continuing.
Note: automatically using hardware breakpoints for read-only addresses.

Breakpoint 1, 0x000002c0 in main ()
(gdb) disass
Dump of assembler code for function main:
=> 0x000002c0 <+0>:	bl	0x2e4 <init_portf>
   0x000002c4 <+4>:	ldr	r3, [pc, #88]	; (0x320 <init_portf+60>)
End of assembler dump.
(gdb)
```

Im Makefile ist das target `debug` eingetragen, das openocd und den Debugger automatisch startet!

