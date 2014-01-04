package JavaFragen;

/* Welche Antwort ist richtig?
 * A) Die Ausgabe ist so ähnlich wie
 * close
 * Exception in thread "main" java.lang.RuntimeException
 *         at JavaFragen.TryWithResources2.main(TryWithResources2.java:30)
 * B) Die Ausgabe ist "close\n".
 * C) Die Ausgabe ist so ähnlich wie
 * Exception in thread "main" java.lang.RuntimeException
 *         at JavaFragen.TryWithResources2.main(TryWithResources2.java:30)
 * D) Compilerfehler, da Resource nicht Closeable implementiert.
 * E) Compilerfehler, da main keine throws RuntimeException deklariert.
 * F) Compilerfehler, da die Methode close in Resource nicht throws Exception deklariert.
 * G) Compilerfehler, da in main entweder die Ausnahme Exception "gecatched" werden muß 
 *    oder alternativ ein throws Exception deklariert werden muß.
 */
public class TryWithResources2 {

	static class Resource implements AutoCloseable {
		@Override
		public void close() {
			System.out.println("close");
		}
	}
	
	public static void main(String[] args) {
		try(Resource r = new Resource()) {
			// System.exit(0);
			throw new RuntimeException();
		}
	}
}










/* Antwort:
 * Die Ausgabe ist so ähnlich wie
 * close
 * Exception in thread "main" java.lang.RuntimeException
 *         at JavaFragen.TryWithResources2.main(TryWithResources2.java:30)
 * Erklärung: Ein try mit Ressourcen erwartet, daß alle erzeugten Ressourcen
 * das Interface AutoCloseable implementieren. java.io.Closeable ist aber auch erlaubt,
 * da das Interface Closeable von AutoCloseable erbt.
 * Resource überschreibt die Methode von AutoCloseable korrekt,
 * da das Weglassen einer throws Deklaration erlaubt ist. Da das try-with-Resources
 * in main close von r aufruft, r den Referenztyp »Resource« besitzt und Resource.close
 * keine throws Exception in der Deklaration besitzt, muss Exception in main
 * auch nicht deklariert oder aufgefangen werden (handle or declare).
 * Die Methode main muss auch kein throws RuntimeException deklarieren, das RuntimeException
 * das Basisklasse aller "unchecked exceptions" ist neben der Klasse Error.
 * Das try mit Ressourcen stellt bei Verlassen des try Blockes immer sicher, daß close 
 * aufgerufen wird. Es sei denn System.exit(0) würde im try Block aufgerufen.  
 */
