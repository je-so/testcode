package JavaFragen;

/* Welcher Code kann in Zeile A eingegeben werden, damit
 * die Klasse compiliert? Wähle zwei mögliche aus Codestücke aus.
 * 
 * A) Nichts, da die Klasse sowieso fehlerfrei compilert.
 * B) i = 3;
 * C) if (Math.random() > 0.5) i = 3;
 * D) if (Math.random() > 0.5) i = 3; else i = 6;
 */

public class InitDemo8 {
	public final static int i;
	
	static {
		// Line A
	}
	
	public static void main(String[] args) {
		System.out.println(i);
	}
}


























/* Antwort: B + D.
 * 
 * Erklärung:
 * Der Compiler überprüft, ob finale Variablen, seien sie nun statisch oder nicht,
 * initialisiert sind. Statische finale Variablen müssen entweder direkt zugewiesen
 * werden oder in einem statischen Initialisierungsblock gesetzt werden. 
 * Ansonsten meldet der Compiler einen Fehler.
 * Die Zuweisungen »i = 3;« und »if (Math.random() > 0.5) i = 3; else i = 6;« sind
 * korrekt, da sie in jedem Fall der Variablen i einen Wert zuweisen.
 * Die Zuweisung »if (Math.random() > 0.5) i = 3;« setzt i nicht in jedem Fall,
 * der Compiler meldet daher einen Fehler.
 *  
 */
