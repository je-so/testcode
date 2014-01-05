package JavaFragen;

/* Was wird ausgegeben?
 * 
 * A) "11"
 * B) "21"
 * C) "01"
 * D) "12"
 * E) "22"
 * F) "02"
 */

public class InitDemo4 {

	static class Parent {

		int i = 1;
		
		Parent() {
			init();
		}
		
		public void init() {
			System.out.print(i);
		}
	}

	static class Child extends Parent {

		int i = 2;
		
		public void init() {
			System.out.print(i);
		}
	}

	public static void main(String[] args) {
		System.out.println(((Parent)new Child()).i);
	}
}














/* Antwort: "01" wird ausgegeben.
 * Erklärung:
 * Child() wird aufgerufen. Der Konstruktor existiert, da er vom Compiler 
 * automatisch (default constructor) angelegt wird, sofern kein eigener
 * Konstruktor implementiert wird.
 * Child() ruft mittels super() Parent() auf. Dort wird zuerst i auf 1 
 * gesetzt und dann init aufgerufen. Da Child init korrekt überschreibt,
 * wird Child.init() ausgeführt. Dort wird i gelesen, aber nicht das i
 * von Parent, sondern das i von Child. Da der Konstruktor von Child()
 * noch nichts gemacht hat, außer dem ersten super() Aufruf, ist i immer
 * noch mit dem default Wert 0 initialisiert. Es wird daher '0' ausgegeben.
 * Wenn der Konstruktor Parent() zurückkehrt, wird Child.i auf 2 gesetzt. 
 * Jetzt wird auch Child() verlassen und in main die Referenz auf Child
 * auf den Referenztyp Parent gecastet. Daher gibt main den Wert Parent.i
 * aus, also '1' und nicht das '2' in Child.i. 
 */
