# Linker Script

[_Externe Quelle_] (https://www.sourceware.org/binutils/docs-2.10/ld_3.html#SEC6)

Microcontroller besitzen ihre eigenen Speicherbereiche, so dass dem Linker mitgeteilt werden muss, 
an welcher Adresse sich diese befinden und wie groß diese sind. Der Aufbau eines solchen Script für 
den GNU Linker wird im Folgenden beschrieben.

Das folgende Script beschreibt zwei Speicherebereiche. Ein nur lesbares, genannt `FLASH`, das
den ausführbaren Code, konstante Werte und Initialisierungsdaten enthält und ein `SRAM` genannter
Bereich, der das gesamte auf dem Microcontroller untergebrachte statische RAM beschreibt für
les- und schreibbare Daten.


```Linker
MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x00040000 /* or 256K */
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00008000 /* or 32K */
}
```

Damit die einzelnen Input-Sektionen der Objektdateien den Speicherbereichen korrekt zugeordnet werden, muss
ein wird ein SECTIONS Abschnitt verwendet, der durch eine Leerzeile getrennt nach dem Memory Abschnitt steht.

```Linker
SECTIONS
{
    .text :
    {
        _text = .;
        *(.text_address_0x00000000) /* or KEEP(*(.text_address_0x00000000)) */
        *(.text*)
        *(.rodata*)
        _etext = .;
    } > FLASH

    .data : /* AT(ADDR(.text) + SIZEOF(.text)) */
    {
        _data = .;
        _romdata = LOADADDR (.data);
        *(.data*)
        _edata = .;
    } > SRAM AT>FLASH

    .bss :
    {
        _bss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > SRAM
}
```
### Ausgabesektionen

Innerhalb von `SECTIONS` sind verschiedene Ausgabe-Sektionen aufgelistet (.text, .data und .bss), die sich bestimmten
Eingabesektionen zusammensetzen. Eine Ausgabesektion wird definiert durch

```Linker
name : [AT(load_addr)] { <input-sections>* } > memregion [ AT > load_memregion ]
```

Die vordefinierten Namen .text, .data und .bss stehen jeweils für Programmcode und konstante Daten, 
bzw. les- und schreibbare initialiserte Daten, sowie les- und schreibbare uninitialiserte Daten, die
aber vor Programmstart mit 0 initialisiert werden. Jede solche Sektion bestimmt mit `> SRAM` bzw `> FLASH`
in welchen Speicherbereich (Virtual-Memory-Adress(VMA)) sie der Linker zuordnen soll.
Alle Objekte die im SRAM abgelegt werden haben beispielsweise eine Speicheradresse >= 0x20000000.

Mit dem optionalen Attribut `AT > load_memregion` wird die Load-Memory-Adress(LMA) festgelegt.
Damit werden die Daten an einer anderen Stelle gespeichert und geladen. Die Initialisierungswerte für
das Datensegment `.data` werden an den Programmcode angehängt und somit in den Flashspeicher geschrieben.
Die Symbolzuweisung `_data = .;` und `_edata = .;` merken sich die eigentliche VMA-Start- bzw. Endadresse 
der initialisierten Daten und die Variable `_romdata = LOADADDR(.data);` die Adresse, ab wo die Daten im 
Flash gespeichert liegen. Mit diesen Werten kann dann das Programm während des Initialiserungsvorgans die
Daten aus dem Rom in das Ram umkopieren.

### Eingabesektionen

Die GNU C Compiler erzeugt pro C-Datei die folgenden Sektionen:

| Name der Sektion | Beschreibung |
| ---------------- | ------------ |
| .rodata      | Konstante/readonly Daten. |
| .text        | Programmcode. |
| .data        | Mit Werten ungleich 0 initialisierte Daten. |
| .bss         | Nichtinitialsierte oder mit 0 initialisierte Daten |
| COMMON       | Globale Variablen, z.B. int array[100];, die nicht initiailisert wurden, die in mehreren Modulen verwendet werden, aber für die nur einmal Speicherplatz zugewiesen werden soll. |

Eine Eingabesektion steht innerhalb der Ausgabesektion, z.B. `*(.bss)` bzw. `*(.bss*)`. 
Mit dem ersten `*` werden alle Dateien bezogen auf ihren Namen selektiert, mit `foo.o(.bss .data)` würden 
nur die bss- und Datensektionen aus der Datei _foo.o_ bezogen. Der Name in der Klammer gibt den Namen der
Eingabesektion an, wobei mit Leerzeichen getrennt auch mehrere nacheinander aufgelistet werden können.

Falls statt `*(.data)` ein Suchmuster wie `*(.data*)` für den Sektionsnamen verwendet wird, liegt das daran,
dass die Endungen `.variable_name` bzw. `.function_name` an die Sektionsnamen angehängt werden, 
falls die Optionen `-ffunction-sections` bzw. `-fdata-sections` beim Compilieren angegeben wurden.
Damit würde jede Funktion bzw. jedes Datum in seine eigene Sektion gestellt. Dazu aber später mehr.

#### Erzeugung programmspezifischer Eingabesektionen

Manchmal ist es erforderlich, eine bestimmte Funktion oder Variable exakt an eine bestimmte Adresse zu linken.
Dazu kann `module_name.o(.data bzw. .text)` verwendet werden. Damit ist das Linkerskript aber vom Namen eines
Moduls abhängig. Auch kann ein Datei mehrere Objekte enthalten, die an unterschiedliche Positionen gelinkt werden sollen.

GCC bietet dazu die Möglichkeit, Objekten eine projektspezifische Sektion zuzuweisen mit der nicht portablen
Anweisung `__attribute__ ((section("name-der-sektion")))` Das folgende Beispiel ordnet die Variable 
_g_NVIC_vectortable_ der Sektion ".text_address_0x00000000" zu und stellt zusammen mit dem obigen Linkerscript 
somit sicher, das diese Tabelle immer ab Adresse 0 im Rom abgelegt wird.

```C
uint32_t g_NVIC_vectortable[155] __attribute__ ((section(".text_address_0x00000000"))) = { ... };
```

### Optimierung mit Linker-Garbage-Collection

Um einzelne vom Programm nicht verwendete Funktionen oder Variablen aus gelinkten Modulen automatisch zu entfernen, 
gibt es die Möglichkeit, jedes Objekt während des Compiliervorganges einer eigenen Sektion zuzuweisen
mittels der Compileroptionen `-ffunction-sections` bzw. `-fdata-sections`.

Während des Linkervorganges kann der Linker, sofern ihm die Linkeroption `--gc-sections` beim Aufruf mitgegeben wurde,
alle nicht referenzierten Sektionen entfernen. Dabei kann die Programmgrösse beträchtlich sinken.

Damit aber Objekte, die an bestimmte Stellen geladen werden müssen, wie z.B. eine Interruptvektortabelle, die aber
ansonsten vom Programm nicht referenziert werden, während der Garbagacollection des Linkers nicht versehentlich entfernt 
werden, muss die Eingabesektion mit `KEEP()` ummantelt , etwa wie im Beispiel `KEEP(*(.text_address_0x00000000))`.
