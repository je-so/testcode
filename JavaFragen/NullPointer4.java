package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Es wird "String" ausgegeben.
 * B) Es wird "Number" ausgegeben.
 * C) Compilerfehler.
 * D) Exception zur Laufzeit.
 */

public class NullPointer4 {

	public static void test(String str) {
		System.out.println("String");
	}

	public static void test(Number nr) {
		System.out.println("Number");
	}
	
	public static void main(String[] args) {
		test(null);
	}
}
























/* Antwort: Der Compiler bricht mit einer Fehlermeldung ab.
 * 
 * Erklärung:
 * Der Aufruf 
 *  javac -d bin/ src/JavaFragen/NullPointer4.java 
 * gibt folgende Fehlermeldung aus:
 * src/JavaFragen/NullPointer4.java:22: error: reference to test is ambiguous, both method 
 * test(String) in NullPointer4 and method test(Number) in NullPointer4 match
 *      test(null);
 *      ^
 * 1 error
 * 
 * Wird test(null) aufgerufen, so weiß der Compiler nicht, 
 * welcher Ableitungshierarchie er folgen soll -- Object nach String 
 * bzw. Object nach Number, um die am weitest abgeleitete Klasse für
 * die überladene Methode test und dem Argument null zu finden.
 * 
 * Der Compilerfehler kann beseitigt werden, indem anstatt test(null);
 * 		test((String)null);
 * oder
 * 		test((Number)null);
 * geschrieben wird.
 */
