package JavaFragen;

/* Welche Zeile im Programm produziert einen Compilerfehler und warum?
 * 
 * Wenn diese Zeile entfernt wurde, welche Ausgabe produziert das Programm?
 * 
 * A) "103"
 * B) "303"
 * C) "333"
 * D) "13"
 * E) "03"
 */

class InitDemo7Parent {
	InitDemo7Parent() {
		print();
	}
	public void print() {
		System.out.print("1");
	}
}

public class InitDemo7 extends InitDemo7Parent {
	public final int i;

	public InitDemo7() {
		System.out.print(i);
		i = 3;
	}

	public void print() {
		System.out.print(i);
	}

	public static void main(String[] args) {
		new InitDemo7().print();
	}
}

























/* Antwort: Die erste Zeile »System.out.print(i);« im Konstruktor InitDemo7()
 *          erzeugt einen Compilerfehler.
 *          Nach Entfernung der Zeile ist die Ausgabe "03".
 *          
 * Erklärung:
 * Warum: Der Compiler prüft, ob finale Attribute(final fields) einen Wert zugewiesen
 * bekommen haben, bevor auf sie zugegriffen wird. Dies gilt für Objektattribute
 * als auch statische Klassenattribute.
 * Der Zugriff auf i im Konstruktor, bevor es initialisiert wurde, produziert daher 
 * einen Compilerfehler.
 *  
 * Wird diese Zeile entfernt, wird mit dem Anlegen eines InitDemo7 Objektes
 * in main erst der Konstruktor von InitDemo7Parent ausgeführt, der die
 * überschriebene Methode print ausführt. Da der Konstruktor von InitDemo7
 * noch nicht durchlaufen wurde (nur der implizite super() an erster Stelle),
 * ist i noch auf den Standardwert 0 initialisiert. Der Wert "0" wird daher
 * ausgegeben. Nach Rückkehr aus InitDemo7Parent() wird der Konstruktor von 
 * InitDemo7 durchlaufen, der i auf 3 setzt. Jetz wird in main print
 * aufgerufen und der Wert "3" ausgegeben.  
 *
 */
