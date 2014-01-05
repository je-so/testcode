package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Die Ausgabe ist "21"
 * B) Die Ausgabe ist "22"
 * C) Die Ausgabe ist "11"
 * D) Compilerfehler.
 */

public class Vararg1 {

	static void print(int i, int... a) {
		System.out.print("1");
	}

	static void print(Byte b, int... a) {
		System.out.print("2");
	}

	public static void main(String[] args) {
		print(new Byte((byte)1), 2, 3);
		print((byte)1, 2, 3);
	}
}
















/* Antwort: Es gibt einen Compilerfehler.
 * Erklärung:
 * Der Compiler wählt beim Überladen von Methoden die Methoden 
 * mit variablen Argumentlisten immer zuletzt aus. Wenn er
 * jedoch mehrere zur Auswahl hat, dann unterscheidet er nicht mehr
 * zwischen Autoboxing/Unboxing und Typ-widening bei anderen Argumenten.
 * Er erachtet daher beide print Methoden von Vararg1 als gleichwertig,
 * sowohl beim ersten Aufruf von print als auch beim zweiten.
 * 
 * Fehlermeldungen des Compilers:
 * 
 * JavaFragen/Vararg1.java:22: error: reference to print is ambiguous, both method 
 * print(int,int...) in Vararg1 and method print(Byte,int...) in Vararg1 match
 *      print(new Byte((byte)1), 2, 3);
 *      ^
 * JavaFragen/Vararg1.java:23: error: reference to print is ambiguous, both method 
 * print(int,int...) in Vararg1 and method print(Byte,int...) in Vararg1 match
 *      print((byte)1, 2, 3);
 *      ^
 * 
 * Korrigiert werden könnte dies mit
 * 
 * print(new Byte((byte)1), new int[] {2, 3});
 * print((byte)1, new int[] {2, 3});
 * 
 * Da hier beidemal keine variable Argumentliste erstellt werden muss,
 * wählt der Compiler, wie erwartet, Widening des primitiven Typs vor Autoboxing,
 * und es würde "21" ausgegeben werden.
 */
