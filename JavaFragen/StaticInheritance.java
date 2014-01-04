package JavaFragen;

/* Welche Antwort ist richtig ?
 * A) Das Programm gibt "121" aus. 
 * B) Das Programm gibt "122" aus.
 * C) Das Programm gibt "222" aus.
 * D) Das Programm gibt "111" aus.
 * E) Compiler Fehler: si ist private in Parent und Child
 *    darf daher auf das geerbte si nicht zugreifen.
 * F) Compiler Fehler: child_si ist private in Parent.Child
 *    und darf daher von Parent2 im statischen Initialisierungsblock
 *    nicht initialisiert werden. 
 */

class StaticInheritance {
	
	static class Parent {
		static private int si = 1;
		
		static int getI() { return si; }

		static class Child extends Parent {
			private static int child_si;
			
			static {
				si = 2;
			}
			static int getI() {
				return si;
			}
		}
	}
	
	static class Parent2 {
		static {
			Parent.Child.child_si = 4;
		}
	}
	
	public static void main(String[] args) {
		System.out.print(Parent.getI());
		System.out.print(Parent.Child.getI());
		System.out.println(Parent.getI());
		// System.out.println(Parent.Child.si);
	}
}





























/* Antwort: Das Programm gibt "122" aus.
 * Warum? 
 * - Weil mit Parent.getI() zuerst die "nested class" Parent geladen wird.
 *   Beim Laden werden alle statischen Felder initialisiert. Der Wert von Parent.si
 *   ist 1 und getI() gibt daher 1 zurück. 
 * - Mit dem Zugriff Parent.Child.getI() wird die nested class Parent.Child geladen
 *   und deren statischer Initialisierungsblock aufgerufen. Dieser setzt den Wert von
 *   Parent.si auf 2 und getI() gibt daher 2 zurück.
 * - Der letzte Aufruf Parent.getI() greift auf das geänderte Parent.si zu und gibt
 *   daher auch 2 zurück.
 * 
 * Warum gibt es keinen Compilerfehler(E)?
 * si ist in Parent private deklariert und Child erbt von Parent. Daher dürfte
 * si von Child aus nicht sichtbar sein. Ist es auch nicht. 
 * Der Zugriff auf Parent.Child.si ist daher nicht möglich. Würde in Child statt si
 * Child.si stehen, gäbe es einen Compilerfehler.
 * Aber da Child auch eine static inner class von Parent ist, sieht Child das si von Parent
 * und darf darauf zugreifen. Der Compiler setzt ein implizites Parent.si für ein si
 * in Child ein. 
 * 
 * Warum gibt es keinen Compilerfehler(F)?
 * Generell darf jede static inner class (und auch inner classes ohne static) auf alle privaten 
 * Elemente aller anderen inneren Klassen und auf die privaten Elemente der top-level Klasse
 * zugreifen.
 *   
 */
