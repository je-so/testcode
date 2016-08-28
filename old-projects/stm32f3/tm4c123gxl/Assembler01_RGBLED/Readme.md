### Testprogramm zur Ansteuerung der RGB-LED

Der [Schaltplan](../doc/board.md) zeigt die Verdrahtung der LED mit Port-F des Controllers.

Das Kommando `make flash` erzeugt das Program und schreibt es auf das ROM des über den Debugport
mittels USB-Kabels verbundenen Microcontrollers.

Der Druck auf Taster-1 setzt die RGB-LED auf die Farbe rot.  
Ein Druck auf Taster-2 setzt die RGB-LED auf die Farbe grün.  
Das Drücken beider Tasten gleichzeitig setzt die RGB-LED auf die Farbe blau.
