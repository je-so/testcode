package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Es wird "34" ausgegeben,
 * B) Es wird "12" ausgegeben,
 * C) Es wird "14" ausgegeben,
 * D) Es wird "32" ausgegeben,
 * E) Compilerfehler.
 * 
 */


public class Scope1 {
	
	private int x = 1;
	int y = 2;
	
	class Parent {
		private int x = 3;
		int y = 4;
	}

	class Child extends Parent {
		void print() {
			System.out.print(x);
			System.out.print(y);
		}
	}
	
	public static void main(String[] args) {
		new Scope1().new Child().print();
	}
}






















/* Antwort: Es wird "14" ausgegeben.
 * 
 * Erklärung:
 * Die Methode print gibt zuerst x aus.
 * Das Attribut x ist zwar in Parent mit dem Wert 3 definiert,
 * jedoch private und wird daher von Child nicht geerbt.
 * (Mit super.x könnte Child auf x in Parent zugreifen, da
 * beides innere Klassen sind und daher der Zugriff auf private 
 * Elemente erlaubt sind).
 * Der Compiler greift daher auf mit x auf Scope1.this.x zurück, 
 * das x der umschließenden Klasse zurück. Es wird "1" ausgegeben.
 * (Innere Klassen dürfen auf private Elemente zugreifen).
 * 
 * Dann gibt die Methode print y aus. y aus Parent ist in Child
 * sichtbar und daher wird dieser Wert ausgegeben, also 4.
 * 
 * 
 * Hätte die Klasse Scope1 kein Attribut des Namens x definiert,
 * gäbe es einen Compilerfehler.
 *  
 */
