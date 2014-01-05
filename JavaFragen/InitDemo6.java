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
 * und daher wird zuerst '1' ausgegeben ohne vorher die Klasse Constants
 * laden zu müssen.
 * Der zweite Zugriff - Constants.C3 - ist der Verweis auf eine statische 
 * Variable, aber keine compiletime Konstante, da diese in einem statischen
 * Initialisierungsblock zugewiesen wird. Vor dem Zugriff muss die Klasse 
 * Constants also geladen werden und initialisiert. 
 * Der zweite Initialiserungsblock gibt daher "init" aus.
 * Zuletzt gibt das zweite print in main den Wert '3' aus.  
 */
