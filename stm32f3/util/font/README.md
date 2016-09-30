# Erstellen eines Bitmapfonts
Zur Ausgabe von Text wird auf einem µC mindestens ein Font benötigt. Da zumeist beschränkte Ressourcen zu berücksichtigen sind, kommt in der Regel ein vorgefertigter Bitmapfont in Frage.

In diesem Verzeichnis befindet sich ein für Linux entwickeltes Shellskript, das einen Font mit dem freien Programm <code>fontforge</code> aus gleichnamigem Paket einliest und diesem per Skript mitteilt, die ASCII Zeichen von 32 Bit 126 als png-Bild zu exportieren.

Diese Bild der 95 gebräuchlichsten ASCII-Zeichen wird mit Hilfe des <code>convert</code> Programms aus dem Paket _imagemagick_
in ein s/w-Bild im Gray-Format (8-Bit-Grauwert pro Pixel, ein Byte für jeden Pixel) umkodiert.

Am Ende kommt ein kleines C-Programm zur Ausführung, das aus dem s/w-Bild ein C-Modul mit Fontbeschreibung in einer Tabelle erstellt.
Unterstützt werden momentan nur Bitmapfonts mit maximal 32 Pixel breiten Zeichen, da jede horizontale Bitmapzeile eines Zeichens genau einem <code>uint32_t</code> zugeordnet ist. Der Wert des Bits 0 beschreibt dabei die Farbe des am weitesten links stehenden Pixels. Ist der Bitwert 0, dann ist damit die Zeichenfarbe Schwarz kodiert. Der Bitwert 1 kodiert die Hintergrundfarbe weiß.

Das Skript [generate_bitmap_font.sh](/util/font/generate_bitmap_font.sh) enthält zuerst die Zuweisung <code>font="Absoluter-Pfad-Zum-Font"</code>. Mit Änderung dieser Zuweisung kann der Font angepasst werden, von dem ein Bitmapfont erstellt werden soll.

Gestart wird das Skript mit einem simplen Aufruf von <code>make</code>. Der Font kann in font-sw.png angeschaut werden und das erstellte C-Modul *font_22x40.c* muss jetzt nur noch eingebunden werden. Eine Beispielansteuerung ist in [lcd_ili9341.c](/stm32f3_discovery/hwunit/lcd_ili9341.c) zu finden.
