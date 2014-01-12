package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Es wird "String" ausgegeben.
 * B) Es wird "Object" ausgegeben.
 * C) Compilerfehler.
 * D) Exception zur Laufzeit.
 */

public class NullPointer2 {

	public static void test(String str) {
		System.out.println("String");
	}

	public static void test(Object obj) {
		System.out.println("Object");
	}
	
	public static void main(String[] args) {
		test(null);
	}
}



















/* Antwort: Das Programm gibt "String" aus.
 * 
 * Erklärung:
 * NullPointer2 kennt zwei überladene Methoden test.
 * Wird test(null) aufgerufen, so wird diejenige Methode
 * ausgewählt, deren Argument das spezifischste, d.h. das am 
 * weitest abgeleitete ist. Dies funktioniert aber nur, wenn
 * alle Typen der Überladungen in einer Linie der Vererbungshierarchie
 * stehen. Ansonsten gäbe es einen Compilerfehler.
 */
