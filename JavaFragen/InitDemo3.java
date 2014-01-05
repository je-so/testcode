package JavaFragen;

/* Welche Ausgabe ist richtig?
 * 
 * A) "ab1AcB1C1"
 * B) "ab2cAB2C2"
 * C) "ab1cAB1C1"
 * D) "ab2AcB2C2"
 * E) "Aab1B1C1c"
 * F) "Aab2B2C2c"
 */

public class InitDemo3 {

	static class Parent {
		static {
			System.out.print("a");
			s1 = 2;
		}
		
		static int s1 = 1;
		
		static {
			System.out.print("b" + s1);
		}

		Parent() {
			System.out.print("c");
		}

	}

	static class Child extends Parent {

		static {
			System.out.print("A");
		}
		
		{
			v1 = 2;
		}
		
		int v1 = 1; 
		
		{
			System.out.print("B" + v1);
		}
		
		Child() {
			System.out.print("C" + v1);
		}
		
	}

	public static void main(String[] args) {
		new Child();
	}
}



























/* Antwort: Die Ausgabe "ab1AcB1C1" ist richtig.
 * Erklärung: Beim Anlegen von einem Child Objekt in main werden
 * zuerst die Klassen Parent und dann Child geladen. Parent wird
 * deshalb vor Child geladen, weil Child von Parent erbt.
 * Jeweils nach dem Laden werden die statischen Initialisierungsblöcke
 * und Zuweisungen in derselben Reihenfolge ausgeführt, wie sie im
 * Quelltext von oben nach unten stehen.
 * Zuerst wird daher 'a' ausgegeben und s1 auf 2 gesetzt. Dann wird
 * s1 auf 1 gesetzt und schliesslich 'b1' ausgegeben.
 * Jetzt wird statische Initialisierung von Child durchgeführt,
 * wobei hier nur 'A' ausgegeben wird.    
 * Jetzt wird Speicherplatz für das Child Objekt angefordert (new) 
 * und der Konstruktor Child() aufgerufen. Die erste implizit vom Compiler
 * eingefügte Anweisung ist super(), daher wird sogleich nach Parent() 
 * verzweigt und 'c' ausgegeben. Nach Rückkehr in Child() werden dort
 * alle Initialisierungsblöcke und Variablenzuweisungen in der Reihenfolge
 * ausgeführt, wie sie im Quellcode von oben nach unten vorkommen.
 * Zuerst wird v1 auf 2 gesetzt, dann mit 1 überschrieben und jetzt die
 * Zeichen 'B1' ausgegeben. Am Ende wird der Code in Child() ausgeführt,
 * welcher 'C1' ausgibt.
 */



