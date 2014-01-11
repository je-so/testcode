package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Compilerfehler in Zeile A
 * B) Compilerfehler in Zeile B
 * C) Es wird "java.lang.Number" ausgegeben.
 * D) Es wird "java.lang.String" ausgegeben.
 * 
 */

public class Inheritance1 {

	public interface I1 {
		String getData();
	}
	
	public static class C1 {
		public Number getData() {
			return 1; // Line A
		}
	}
	
	public static class C2 extends C1 implements I1 { // Line B
	}
	
	public static void main(String[] args) {
		System.out.println(new C2().getData().getClass());
	}
}






















/* Antwort: Compilerfehler in Zeile B
 * Erklärung:
 * Die Klasse C2 erbt von I1 und C1 jeweils die Deklaration einer
 * Methode getData() mit derselben Signatur (gleicher Name und Anzahl der 
 * Parameter und deren Typen stimmen überein). Da die Rückgabewerte
 * vom Typ String und Number (Number ist die Basisklasse der Wrapper-Klassen)
 * aber inkompatibel sind (nicht in einer Vererbungshierarchie), gibt es
 * einen Compilerfehler.
 */
