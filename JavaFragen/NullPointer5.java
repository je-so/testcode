package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Es wird "[null]" und dann "null" ausgegeben.
 * B) Es wird "null" und dann "[null]" ausgegeben.
 * C) Es wird "[null]" und dann "[null]" ausgegeben.
 * C) Es wird "null" und dann "null" ausgegeben.
 * D) Compilerfehler.
 * E) Exception zur Laufzeit.
 */

public class NullPointer5 {

	public static void test(String...strings) {
		if (strings == null)
			System.out.println("[null]");
		else
			System.out.println(strings[0]);
	}

	public static void main(String[] args) {
		test(null);
		test((String)null);
	}
}
























/* Antwort: Es wird erst "[null]" und dann "null" ausgegeben.
 * 
 * Erklärung:
 * Der erste Aufruf der Methode mit variabler Argumentliste mit dem
 * Argument null setzt den Parameterwert auf null, in unserem
 * Fall ist strings == null.
 * 
 * Der zweite Aufruf castet null explizit auf (String) und folglich
 * wird test mit dem Argumentarray new String[] { (String)null }
 * aufgerufen. Die Ausgabe von strings[0] ist somit "null".
 * 
 */
