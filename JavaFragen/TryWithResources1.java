package JavaFragen;

/* Welche Antwort ist richtig?
 * A) Die Ausgabe ist "init1\ninit2\nclose1\nclose2\n"
 * B) Die Ausgabe ist "init1\ninit2\nclose2\nclose1\n"
 * C) Die Ausgabe ist "init1\ninit2\n"
 * D) Die Ausgabe ist "init2\ninit1\n"
 * E) Compilerfehler, da Resource1 nicht Closeable implementiert.
 * F) Compilerfehler, da Resource2 nicht AutoCloseable implementiert.
 * G) Compilerfehler, da AutoCloseable.close keine throws Exception deklariert.
 * H) Compilerfehler, da java.io.Closeable.close keine throws java.io.IOException
 *    deklariert.
 */

public class TryWithResources1 {

	static class Resource1 implements AutoCloseable {
		Resource1() {
			System.out.println("init1");
		}
		@Override
		public void close() throws Exception {
			System.out.println("close1");
		}
	}

	static class Resource2 implements java.io.Closeable {
		Resource2() {
			System.out.println("init2");
		}
		@Override
		public void close() throws java.io.IOException {
			System.out.println("close2");
		}
	}

	public static void main(String[] args) {
		try(Resource1 r1 = new Resource1();
			Resource2 r2 = new Resource2();) {
			return;
		} catch (Exception ex) {
			// ignore
		}
	}
}


















/* Die Antwort ist: Die Ausgabe ist "init1\ninit2\nclose2\nclose1\n"!
 * Erklärung: Die erzeugten Resourcen werden in umgekehrter Reihenfolge wieder 
 * freigegeben.
 * Es gibt keinen Compilerfehler, da das Interface java.io.Closeable AutoCloseable erweitert
 * und Resource2 daher auch java.lang.AutoCloseable implementiert.
 * 
 * Weiterhin ist close in AutoCloseable folgendermaßen deklariert:
 *  void close() throws Exception
 * und close in java.io.Closeable ist deklariert als:
 *  void close() throws IOException
 * Da IOException eine Spezialisierung von Exception ist, überschreibt das close in
 * java.io.Closeable das close in AutoCloseable vollkommen korrekt.
 */
