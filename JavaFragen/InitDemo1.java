package JavaFragen;

/* Welche Ausgabe produziert das Programm?
 * 
 * A) "SIfalseXtrue0C" 
 * B) "SItrueXtrue10C" 
 * C) "IfalseSXtrue0C" 
 * D) "ItrueSXtrue10C" 
 * E) "CSIfalseXtrue0" 
 * F) "CSItrueXtrue10" 
 * G) "CIfalseSXtrue0" 
 * H) "CItrueSXtrue10" 
 */

public class InitDemo1 {

	int v1 = getInt();
	boolean v2 = true;
	int v3 = getStaticInt(this);
	
	InitDemo1() {
		System.out.println("C");
	}
	
	private int getInt() {
		System.out.print("I");
		System.out.print(v2);
		return 10;
	}
	
	static {
		System.out.print("S");
	}

	private static int getStaticInt(InitDemo1 obj) {
		System.out.print("X");
		System.out.print(obj.v2);
		System.out.print(obj.v3);
		return 10;
	}

	public static void main(String[] args) {
		new InitDemo1();
	}
}














/* Antwort: Das Programm produziert die Ausgabe "SIfalseXtrue0C".
 * Erklärung:
 * Beim Anlegen des ersten Objektes einer Klasse (new InitDemo1();) wird
 * zuerst die Klasse geladen. Danach werden die statischen Attribute 
 * initialisiert und die statischen Initialisierungsblöcke ausgeführt.
 * Und abwechselnd nach Vorkommen in der Reihenfolge von oben nach unten.
 * 'S' muss also als erster stehen.
 * 
 * Beim Anlegen des Objektes werden zuerst alle Attribute auf 0,0.0,false bzw. null
 * initialisert. Dann wird der Konstruktor aufgerufen.
 * Im Konstruktor werden zuerst alle schon bei der Deklaration mit einem Initialwert 
 * versehenen Attribute initialisiert und alle Initialisierungsblöcke ausgeführt,
 * und zwar abwechselnd nach Vorkommen in der Reihenfolge von oben nach unten, 
 * es sei denn, der Konstruktor würde mittels this() einen anderen Konstruktor aufrufen.
 * Danach wird der Code im Konstruktor ausgeführt.
 * 'C' muss also als letzter stehen.
 * 
 * Das Attribut v1 wird über die Methode getInt() als erstes initialisiert.
 * Der Buchstabe 'I' kommt daher an zweiter Stelle. Das Attribut v2 wurde noch
 * nicht initialisiert, also wird der Defaultwert false ausgegeben.
 * Dann wird v2 auf true gesetzt. Anschliessend wird v3 mit dem Rückgabewert
 * von getStaticInt(this) initialisiert. Ein 'X' wird ausgegeben. getStaticInt
 * gibt den initialisierten Wert von v2 aus, also true, und den Defaultwert
 * von v3, also 0, da erst nach Rückkehr von getStaticInt der Returnwert an
 * v3 zugewiesen werden kann.  
 */
