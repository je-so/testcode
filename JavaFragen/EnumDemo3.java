package JavaFragen;

/* Welche Aussage ist richtig?
 * A) Compilerfehler, da enums keine Interface implementieren dürfen.
 * B) Compilerfehler, da enums zwar Interfaces implementieren dürfen,
 *    die Syntax aber falsch ist, mit der für jede Konstante eine
 *    eigene Implementierung angegeben wird.
 * C) Compilerfehler, da das erweiterte for in main »for (int i : a)«
 *    nicht über ein a vom Typ Array iterieren kann.
 * D) Das Programm erzeugt die Ausgabe "1\n10\n100\n".
 * E) Das Programm erzeugt die Ausgabe "1\n10\n100\n3\n30\n300\n".
 * F) Das Programm erzeugt keine Ausgabe.
 * G) Das Programm erzeugt die Ausgabe "2\n20\n200\n".
 */

import java.util.Arrays;
import java.util.Iterator;

public class EnumDemo3 {

	enum Array implements Iterable<Integer> {
		A1 {
			public Iterator<Integer> iterator() {
				return Arrays.asList(1, 10, 100).iterator();
			}
		},
		A2 {
			public Iterator<Integer> iterator() {
				return Arrays.asList(2, 20, 200).iterator();
			}
		},
		A3 {
			public Iterator<Integer> iterator() {
				return Arrays.asList(3, 30, 300).iterator();
			}
		}
	}
	
	public static void main(String[] args) {
		for (Array a : Array.values()) {
			if (a != Array.A2) continue;
			for (int i : a) {
				System.out.println(i);
			}
		}
	}
}










/* Richtige Antwort: Das Programm erzeugt die Ausgabe "2\n20\n200\n".
 * Erklärung: Ein enum darf ein oder mehrere Interfaces implementieren.
 * Jede Konstante implementiert dabei »ihr eigenes Ding«.
 * Die Anweisung for (Array a : Array.values())
 * iteriert in a über alle enum Werte.
 * Mit der Anweisung if (a != Array.A2) continue;
 * werden alle Werte bis auf A2 übersprungen.
 * Dann wird mit der Anweisung for (int i : a) 
 * über alle Werte der Menge A2 iteriert. Das funktioniert,
 * weil A2 das Interface Iterable<E> implementiert.
 * Die A2 Implementierung der Methode iterator() gibt einen
 * Iterator auf die Menge { 2, 20, 200 } zurück.
 * Die einzelnen Integer Objekte werden mittels Unboxing
 * zu einem primitiven int Wert konvertiert und nacheinander
 * ausgegeben.
 */
