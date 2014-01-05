package JavaFragen;

/* Welche zwei Zeilen müssen entfernt werden, 
 * damit diese Klasse fehlerfrei compiliert?
 *  
 * A) Zeile (1)
 * B) Zeile (2)
 * C) Zeile (3)
 * D) Zeile (4)
 * E) Zeile (5)
 * F) Zeile (6)
 * G) Zeile (7)
 * H) Zeile (8)
 */

public class InitDemo5 {

	static {
		si = 1; // (1)					
		System.out.println(si); // (2)
	}

	{
		i = 2; // (3)
		System.out.println(i); // (4)
	}
	
	public void init() {
		i = i + 3; // (5)
	}

	public static void sinit() {
		si = si + 4; // (6)
	}
	
	InitDemo5() {
		i = i + 5; // (7)
		si = si + 6; // (8)
	}
	
	int i;
	static int si;
}




























/* Antwort: Die Zeilen (2) und (4).
 * Erklärung:
 * Statische und nicht-statische Initialisierungsblöcke haben lesenden Zugriff 
 * nur auf Attribute, die vor dem Block definiert wurden.
 * Schreibender Zugriff ist nicht reglementiert.
 * Methoden bzw. Konstruktoren dürfen auf alle Attribute 
 * der Klasse zugreifen, unabhängig von ihrer Definition vor und hinter 
 * der Methode bzw. des Konstruktors. 
 * Vorsicht: statische Methoden dürfen aber auch nur auf statische 
 * Attribute Zugreifen.
 */
