# Programmieren von LaunchPad Evaluation Kit EK-TM4C123GXL unter Ubuntu 14.04 LTS

Das Evaluierungskit von TI kommt mit einer 80MHz 32-bit ARM Cortex-M4 basierten MCU mit Floating-Point-Unterstützung.
Es hat 256KB Flash und 32KB SRAM Speicher.


Folgende Pakete sind zuerst zu installieren:
```Shell
$ sudo apt-get install gcc-arm-none-eabi
$ sudo apt-get install openocd
```
Danach ist über http://www.ti.com/tm4c123g-launchpad die device-driver Software `SW-EK-TM4C123GX` für das Board herunterzuladen
und unter `~/EK-TM4C123GXL` zu entpacken. Eine Registrierung mit Email-Adresse ist leider notwendig.

Dann kann das Beispielprojekt `project0` kompiliert werden:
```Shell
$ cd ~/EK-TM4C123GXL/examples/boards/ek-tm4c123gxl/project0
$ make
```

Im Unterverzeichnis `./gcc` is jetzt das auf den Microkontroller zu flashende Binary `project0.bin` erstellt worden. Dieses kopiert man ins aktuelle Verzechnis mit `cp gcc/project0.bin flash.rom`.

Jetzt müssen noch die Konfigurationsdateien für openocd, den Open-On-Chip-Debugger, von hier geladen und im
aktuellen Verzeichnis gespeichert werden ([tm4c123gxl.cfg](./tm4c123gxl.cfg) und [ocdflash.cfg](./ocdflash.cfg)).

Bevor die MCU programmiert werden kann, muss wie in der beiliegenden Kurzbeschreibung am USB Anschluss des Rechners angeschlossen werden.

Das Kommando `dmesg` sollte folgende Ausgabe erzeugen:
```Shell
$ dmesg
...] usb 4-2: new full-speed USB device number 2 using ohci-pci
...] usb 4-2: New USB device found, idVendor=1cbe, idProduct=00fd
...] usb 4-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
...] usb 4-2: Product: In-Circuit Debug Interface
...] usb 4-2: Manufacturer: Texas Instruments
...] usb 4-2: SerialNumber: XXXXXXXX
...] cdc_acm 4-2:1.0: This device cannot do calls on its own. It is not a modem.
...] cdc_acm 4-2:1.0: ttyACM0: USB ACM device
```

Der aktuelle Benutzer sollte noch Mitglied der `dialout` Gruppe sein, damit er Zugriffsrechte auf das ICDI des Boards hat.
```Shell
$ ll /dev/ttyACM0
crw-rw-r-- 1 root dialout 166, 0 Mai  5 17:19 /dev/ttyACM0
```


Das Testprojekt kann jetzt mit folgendem Kommando persistent in den Mikrocontroller geladen und gestartet werden
```Shell
$ openocd -f ocdflash.cfg
```

Die auf dem Board befindliche LED sollte jetzt zu blinken beginnen.

Viel Spaß!
