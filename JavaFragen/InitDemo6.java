package JavaFragen;

/* Welche Ausgabe erzeugt das Programm?
 * 
 * A) "13"
 * B) "init13"
 * C) "1init3"
 * D) "13init"
 */

public class InitDemo6 {

	static class Constants {
		static final int C1 = 1;
		static final int C2 = 2;
		static final int C3;
		static {
			C3 = 3; 
		}
		static {
			System.out.print("init");
		}
	}
	
	public static void main(String[] args) {
		System.out.print(Constants.C1);
		System.out.print(Constants.C3);
	}
}


















/* Antwort: Die erzeugte Ausgabe ist "1init3"
 * Erklärung:
 * Constants.C1 ist eine compiletime Konstante. D.h. der Compiler
 * ersetzt alle Verweise auf Constants.C1 durch den Wert der Konstanten,
 * in obigem Fall durch 1.
 * In main() steht daher als erste Anweisung »System.out.print(1);«
 * und somit wird '1' ausgegeben noch bevor die Klasse Constants
 * geladen wird.
 * Der zweite Zugriff - Constants.C3 - ist der Verweis auf eine statische 
 * Variable - keine compiletime Konstante - da diese mittels statischem
 * Initialisierungsblock einen Wert erhält. Also muss vor dem Zugriff 
 * die Klasse Constants geladen und initialisiert werden. 
 * Als nächstes gibt folglich der zweite Initialiserungsblock "init" aus.
 * Zuletzt gibt das zweite print in main den Wert '3' aus.
 * 
 * Korollar:
 * Wenn sich in einem Interface eine compiletime Konstante ändert,
 * so müssen alle Klassen, die dieses Interface verwenden,
 * neu übersetzt werden, ansonsten verwenden sie den alten Wert.
 */
