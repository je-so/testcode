### Port

Ein Port umfasst mehrere, typisch 8, Ein-/Ausgabe Anschlüsse (I/O Pins).
Diese können zu einem einzigen parallelen Port zusammengefasst werden.
Jeder Pin kann aber auch einzeln verwendet und konfiguriert werden.

Der Zugriff auf die Ports erfolgt über Register, die in den 32-Bit Addressbereich
des Prozessors abgebildet werden (Memory Mapped Register). Ein Zugriff auf ein Register ist deshalb mit einem
Schreib-/Lesezugriff wie auf eine Adresse verbunden
`*((volatile uint32_t*)0x40004528) = 0x123`.

Jeder Port hat seinen eigenen Satz von Konfigurationsregistern.

#### GPIOAMSEL
Um die Analogschaltkreise vor 5V Eingaben zu schützen, werden diese von den Eingabepins isoliert.
Ein Bit in diesem Register wird gesetzt, um die Isolierung aufzuheben und so zu ermöglichen, die
Analogspannung auf dem zugeordneten Pin als Digitalwert über den AD-Konverter auszulesen.
Für digitale I/O wird dieses Bit gelöscht.

#### GPIOPCTL
Jedem Pin sind 4 Bits zugeordnet (16 mögliche Werte), welche eine spezielle digitale
Übertragungsfunktion auswählen (SSI, I2C, UART, usw.). Der Wert 0 ist die normale
I/O Funktion. Dabei werden das Schreiben einer 1 in das Datenregister des Ports
in eine 3.3V und das Schreiben einer 0 in eine 0V Ausgangsspannung umgesetzt. Das Lesen des Pins wandelt die Spannungen
umgekehrt in digitale Einsen und Nullen zurück.

Tabellen im Datenblatt zum Prozessor geben an, welche Port/Pin Kombinationen welchen
speziellen Peripherie-Funktionen zugeordnet werden können.
