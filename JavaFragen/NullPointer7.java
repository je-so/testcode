package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Es wird "String..." ausgegeben.
 * B) Es wird "Object..." ausgegeben.
 * C) Es gibt einen Compilerfehler in Zeile A.
 */

public class NullPointer7 {
	public static void main(String[] args) {
		print((String)null); // LINE A
	}

	public static void print(String... strings) {
		System.out.println("String...");
	}

	public static void print(String string) {
		System.out.println("String");
	}
}



























/* Antwort: Es gibt einen Compilerfehler in Zeile A.
 * 
 * Erklärung:
 * Beide Typen der Argumente beider print Methoden sind einmal String[]
 * und String. Beide Typen sind von Object abgeleitet, stehen aber nicht
 * in einer Hierarchielinie. Der Aufruf ist somit mehrdeutig.
 * 
 * Mittels des Aufrufs print((String)null) würde "String" ausgegeben
 * und mittels print((String[])null) der String "String...".
 * 
 */
