### Testprogramm zur Interaktion per seriellem Interface (UART) bzw. Konsole

Das Kommando `make flash` erzeugt das Program und schreibt es auf das ROM des über den Debugport
mittels USB-Kabels verbundenen Microcontrollers.

Das Terminalprogram, das die Eingabe- und Ausgabe-Funktion übernimmt, startet man mit `make run`.
Sollte ein Fehler auftreten beim ersten Start, einfach den µController resetten und nötigenfalls
das Terminalprogramm nochmals starten.

Das µController-Testprogramm fragt zwei Zahlen ab und gibt als Ergebnis die Multiplikation beider Zahlen aus.

Beispielsitzung:

```shell
$ make run
./testterm /dev/ttyACM0 115200 8N1
Press <CTRL>-D to end program

== Rectangle area calculator ==

Enter length (0..20): 2

Enter width (0..20): 3

length*width = 6
```
