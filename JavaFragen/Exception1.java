package JavaFragen;

/* Welche Aussage ist richtig?
 * A) Das Programm bricht mit einer Exception ab
 *    Exception in thread "main" class JavaFragen.Exception1$EX: Test
 * 	    at JavaFragen.Exception1.main(Exception1.java:26)
 * B) Compilerfehler, wenn in Zeile A das main um ein throws EX erweitert wrid,
 *    dann compiliert das Programm.
 * C) Compilerfehler in Zeile B.
 *
 */

public class Exception1 {

	public static class EX {
		private String reason;
		EX(String reason) {
			this.reason = reason;
		}
		public String toString() {
			return "" + getClass() + ": " + reason;
		}
	}
	
	public static void main(String[] args) { // LINE A
		throw new EX("Test"); // LINE B
	}	
}



















/* Antwort:
 * 
 * Erklärung:
 * Der javac Compiler bricht mit der Meldung ab:
 * src/JavaFragen/Exception1.java:26: error: incompatible types
 *      throw new EX("Test"); // LINE B
 *            ^
 *   required: Throwable
 *   found:    EX
 * 1 error
 * 
 * D.h. jede Exception muss von Throwable abgeleitet sein.
 * Das obige Programm würde compilieren und eine Exception werfen,
 * wenn EX folgendermaßen deklariert wäre
 * 
 * public static class EX extends RuntimeException {
 * 
 * und dann wäre Antwort A richtig!
 */
