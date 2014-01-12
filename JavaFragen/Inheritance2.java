package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Compilerfehler.
 * B) Exception zur Laufzeit.
 * C) Es wird "12" ausgegeben,
 * D) Es wird "21" ausgegeben,
 * 
 */

class Inheritance2_Parent {
	private void print() {
		System.out.print("1");
	}
}

public class Inheritance2 extends Inheritance2_Parent {

	public void print() {
		super.print();
		System.out.print("2");
	}
	
	public static void main(String[] args) {
		new Inheritance2().print();
	}
}






















/* Antwort: Compilerfehler.
 * 
 * Erklärung:
 * Der Aufruf »super.print();« ist nicht erlaubt, da die Methode
 * print in Inheritance2_Parent als private deklariert wurde und
 * daher für andere nicht sichtbar ist.  
 */
