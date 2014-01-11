package JavaFragen;

/* Welche Aussage ist richtig:
 * 
 * A) Compilerfehler bei Zeile A.
 * B) Es wird "Child" ausgegeben.
 * C) Es wird "Base" ausgegeben.
 */

class Override1_Base {
	public String getName() {
		return getNamePrivate();
	}
	
	private final String getNamePrivate() {
		return "Base";
	}
}

public class Override1 {

	public static class Child extends Override1_Base {
		protected String getNamePrivate() { // LINE A
			return "Child";
		}
	}

	public static void main(String... args) {
		System.out.println(new Child().getName());
	} 
}
















/* Antwort: Es wird "Base" ausgegeben. 
 * Erklärung:
 * Private Methoden können niemals von einer abgeleiteten Klasse
 * überschrieben werden, ob final oder nicht spielt dabei keine Rolle.
 * Es gibt somit auch keinen Compilerfehler - obwohl final vor getNamePrivate
 * steht.
 * 
 * getName() in Override1_Base ruft daher immer das getNamePrivate
 * aus Override1_Base auf und gibt immer "Base" zurück. 
 * 
 * Die Methode getNamePrivate in Child überlagert nur die Methode in 
 * getNamePrivate in Override1_Base - wobei getNamePrivate von Override1_Base
 * in Child eh nicht sichtbar ist.  
 * 
 * getNamePrivate in Child könnte aber in einer von Child abgeleiteten 
 * Klasse überschrieben werden, da sie nicht private ist.
 */
