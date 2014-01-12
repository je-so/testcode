package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Compilerfehler.
 * B) Exception zur Laufzeit.
 * C) Es wird "12" ausgegeben,
 * D) Es wird "32" ausgegeben,
 * 
 */

public class Inheritance3 {
	
	class Parent {
		private void print() {
			print2();
		}
		private void print2() {
			System.out.print("1");
		}
	}

	class Child extends Parent {
		private void print() {
			super.print();
			System.out.print("2");
		}
		private void print2() {
			System.out.print("3");
		}
	}
	
	public static void main(String[] args) {
		new Inheritance3().new Child().print();
	}
}






















/* Antwort: Es wird "12" ausgegeben.
 * 
 * Erklärung:
 * Der Aufruf »super.print();« ist erlaubt, da einer inneren Klasse
 * der Zugriff auf alle privaten Element aller anderen inneren 
 * Klassen bis hinauf zur top-level Klasse erlaubt ist.
 * 
 * Da print2() in Parent private ist, wird es von print2() in Child
 * nicht überschrieben und somit wird "12" ausgegeben.   
 */
