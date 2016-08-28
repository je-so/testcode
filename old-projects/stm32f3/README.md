# embedded
Testcode and config files for programming and flashing ARM based µController

_Descriptions are only in *German*_.

### TM4C123 Launchpads / STM32F3-Discovery
| How-To | Link |
| ------ | ----------- |
| Setup von TM4C123GXL launchpad evaluation kit | [Setup](tm4c123gxl/doc/setup.md), [Debugging](tm4c123gxl/doc/debug.md), [Schaltplan](tm4c123gxl/doc/board.md), [GPIO](tm4c123gxl/doc/gpio.md)|
| Verwendung von Linker Scripten | [Linker-Skript](linker_script.md) |
| Setup von STM32F3-Discovery | [TODO: Board](#)|, 

----------------------
### Nützliche Kommandos
| Problem | Lösung |
| ------- | ------ |
| Seriellen Output anzeigen für Debugging | Shellskript [showserial.sh](util/showserial.sh) bzw. C-Code [testterm.c](util/testterm.c) |
| Erstellen eines Bitmapfonts | [Makefile und Shellskript] (util/font/README.md) |

#### Definitionen

**_GPIO Pad_**
> Im TM4C123GH6P Datasheet werden die GPIO Pins oft als _"Pads"_ bezeichnet - eine Bezeichnung der Halbleiterbranche in der Bedeutung "µController-Verbindung zur Außenwelt".

**State / Zustand**
> Ist ein Programm in einem bestimmten Zustand, bezeichnet *Zustand*  "was wir wissen" oder "was wir glauben zu wissen".
Es ist wichtig Zustände klar und unzweideutig zu kodieren.
> * Ein Zustand kann in Form einer Variablen gespeichert werden. So können zeitliche Ereignisse aufgezeichnet werden.
> * Ein Zustand kann über einen GPIO-Eingabe-Pin gelesen werden. Mittels Interruptprogrammierung kann so mit sehr geringer Latenz auf äußere Eregnisse reagiert werden.
> * Ein Zustand kann auf einen GPIO-Ausgabe-Pin geschrieben werden. Die Software kann den zuletzt geschriebenen Wert abfragen und den aktuellen Ausgabewert wie eine 1 Bit große Variable benutzen. Der Vorteil für den Anwender ist, dass der vom System angenommene Zustand über den Wert des Ausgabepins beobachtet werden kann.
> * Ein Zustand wird häufig auf die aktuelle Ausführungsposition (ARM Register PC) abgebildet.  
    `if (In1==1) { /*Pos1:*/PrintWarning(); if (In2==1) { /*Pos2:*/PrintCritical(); }}`  
    An Position 1 gehen wir davon aus, dass Eingabepin 1 gesetzt ist und an Position 2 davon, dass sowohl Pin 1 und 2 gesetzt sind, obwohl sie beide zwischenzeitlich den Zustand gewechselt haben könnten.

**Atomare Speicherzugriffe auf ARM Architekturen**
> Falls zwischen einem `LDREX` (dem Laden einer exclusiven Speicheradresse) und dem darauffolgenden `STREX` (dem Beschreiben einer exclusiven Speicheradresse) ein Thread-Kontextwechel stattfindet oder ein Interrupt ausgeführt wird, der auch einen atomaren Speicherzugriff vornehmen will, muss der CPU-lokale Monitor, welcher den atomaren Zugriff überwacht, vorher mittels `CLREX` zurückgesetzt werden. Nähere Einzelheiten finden sich auf [Arm-Infocenter](http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dht0008a/CJAGCFAF.html).  
> **Warum** ? *Weil* ein `STREX` erfolgreich sein kann, auch wenn dessen Zielspeicheradresse nicht derjenigen des vorausgegangenen `LDREX` Befehls entspricht. Portabler Code darf keine Annahmen darüber machen, ob Hardware Speicheradressen vergleicht oder nicht, das erlaubt die Optimierung von Hardware.

**Modularisierung**
> Das Ziel der Zerlegung der Software in Module ist die *Erhöhung der Verständlichkeit*. Das Gesamtproblem wird in kleinere Teilprobleme zerlegt, die einfacher Verstanden werden können. Werden die Teile möglichst unabhängig voneinander entworfen, können diese auch unabhängig geändert und erweitert werden. Der Aufwand für Softwarewartung nimmt ab. Daher gilt:  
> * Maximiere Anzahl der Module. **Aber:** Kopplung/Kommunikationsaufwand zwischen Modulen nimmt zu und die Verständlichkeit nimmt ab.
> * Daher zweites Ziel: Minimierung der Kopplung zwischen Modulen. **Aber:** Integration zweier Module zu einem Modul setzt Kommunikationsaufwand zwischen ihnen auf 0, was ein Gegengewicht zur Maximierung der Anzahl der Module ist.

