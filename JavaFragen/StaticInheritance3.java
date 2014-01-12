package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Compilerfehler. 
 * B) Exception wird zur Laufzeit geworfen.
 * C) Das Programm gibt "1\n2\n". 
 * D) Das Programm gibt "2\n1\n". 
 */

class StaticInheritance3_Parent {
	public static void print() {
		System.out.println("1");
	}
}

public class StaticInheritance3 extends StaticInheritance3_Parent {
	public static void print() {
		System.out.println("2");
	}
	
	public void print2() {
		print();
		super.print();
	}
	
	public static void main(String[] args) {
		new StaticInheritance3().print2();
	}
}














/* Antwort: Das Programm gibt "2\n1\n" aus.
 *
 * Erklärung:
 * Statische können nicht überschrieben, aber verdeckt(überlagert) werden.
 * Die Methode print2() ruft daher zuerst print() aus StaticInheritance3 auf.
 * Danach wird mittels super in den Namensraum von StaticInheritance3_Parent
 * umgeschalten und mittels super.print() die statische Methode print() von 
 * StaticInheritance3_Parent aufgerufen. Das Schlüsselwort super ist zulässig,
 * da print2() eine Objektmethode ist und keine statische. Statische Methoden
 * über this oder super oder einer anderen Objektreferenz aufzurufen ist zwar
 * ungewöhnlich, aber zulässig.
 *
 */
