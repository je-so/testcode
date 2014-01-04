package JavaFragen;

/* Welche zwei Aussagen sind richtig:
 * 
 * A) Die Methode main muss mit throws Exception deklariert werden, damit das Programm compiliert.
 * B) Nach dem try in main muss ein catch (Exception ex) {} stehen, damit das Programm compiliert.
 * C) Das Label Break1: darf nur vor Schleifen wie for, while bzw do while stehen.
 * D) break Block1 ist außerhalb einer Schleife illegal.
 * E) Das Programm compiliert.
 * F) Das Programm gibt "close\n" aus.
 */

public class TryWithResources3 {
	static class Resource implements AutoCloseable {
		@Override
		public void close() {
			System.out.println("close");
		}
	}
	
	public static void main(String[] args) {
		Block1: try(AutoCloseable r = new Resource()) {
			break Block1;
		}
	}
}

























/* Antwort:
 * Die Antworten A und B sind richtig.
 * Da die Klasse Resource die Methode close ohne throws Exception deklariert, 
 * müsste die Ausnahme Exception eigentlich nicht abgefangen werden.
 * Da der Referenztyp der Variablen r aber AutoCloseable ist und AutoCloseable.close
 * mit throws Exception deklariert wurde, verlangt der Compiler, daß entweder
 * throws Exception nach main steht oder ein catch(Exception ex){} nach try.
 * Den Compiler interessiert nur der Referenztyp und nicht der eigentliche Objekttyp.
 * Nach dem Referenztyp entscheidet der Compiler, ob eine checked Exception beim
 * Aufruf einer Methode geworfen werden kann.
 */
