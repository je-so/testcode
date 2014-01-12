package JavaFragen;

/* Was passiert?
 * 
 * A) Es gibt einen Compilerfehler.
 *    Wird »print(null);« geändert in »print((Object)null);« 
 *    bzw. »print((String[])null);«, dann funktioniert es.
 * B) Es wird "Object" ausgegeben.
 * C) Es wird "String..." ausgegeben.
 * 
 */

public class NullPointer6 {

	static void print(Object obj) {
		System.out.println("Object");
	}

	static void print(String... strings) {
		System.out.println("String...");
	}
	
	public static void main(String[] args) {
		print(null);
	}
}


























/* Antwort: Es wird "String..." ausgegeben.
 * 
 * Erklärung:
 * Die variable Argument Methode print erwartet in Wirklichkeit genau ein 
 * Argument vom Typ String[]. Der Typ String[] ist abgeleitet von Object 
 * und daher spezifischer als Object, mithin wird print(String...strings) aufgerufen.
 *
 */
