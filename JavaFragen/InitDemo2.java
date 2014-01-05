package JavaFragen;

/* Der Standard-Konstruktor InitDemo2() enthält 4
 * mögliche verkettete Aufrufe zum parametrisierten 
 * Konstruktor InitDemo2(int value) bzw. Aufrufe des Konstruktors der Basisklasse.
 * Die Aufgabe ist, vor einem auskommentierten Aufruf die Kommentare zu entfernen,
 * so daß die Klasse immer noch compilierbar ist.
 * Welche vier möglichen Aufrufe sind korrekt?
 * 
 * Bonusfrage1: Dürfen mehr als ein this(...) bzw. super(...) Aufruf stehen?
 * Bonusfrage2: Ist die Aufrufsequenz super(10); this(10); in InitDemo2() möglich?
 * 
 * A) Aufruf (1) ist korrekt. 
 * B) Aufruf (2) ist korrekt. 
 * C) Aufruf (3) ist korrekt. 
 * D) Aufruf (4) ist korrekt. 
 * E) Aufruf (5) ist korrekt. 
 * F) Aufruf (6) ist korrekt. 
 * G) Aufruf (7) ist korrekt. 
 * H) Aufruf (8) ist korrekt. 
 * 
 * Bonus1: (X1) true / (Y1) false
 * Bonus2: (X2) true / (Y2) false
 * 
 */

class InitDemo2Parent {
	InitDemo2Parent() {	}
	InitDemo2Parent(int value) { }
}

public class InitDemo2 extends InitDemo2Parent {

	public static final int VALUE = 10;
	
	private int value = 11;
	
	InitDemo2(int value) {
		this.value = value;
	}

	InitDemo2() {
		// this(VALUE); // (1)
		// this(value); // (2)
		// this(getStaticValue()); // (3)
		// this(getValue()); // (4)
		// super(VALUE); // (5)
		// super(value); // (6)
		// super(getStaticValue()); // (7)
		// super(getValue()); // (8)
	}
	
	static int getStaticValue() {
		return 12;
	}

	int getValue() {
		return 13;
	}
}















/* Antwort: Die Aufrufe this(VALUE); und super(VALUE); als auch
 *          this(getStaticValue()); und super(getStaticValue()); sind korrekt.
 * Erklärung:
 * Die Aufrufe eines verketteten Konstruktors oder des Konstruktors
 * der Superklasse müssen an erster Stelle stehen. Aber nur einer von beiden
 * oder gar keiner. Falls keiner steht, fügt der Compiler einen default 
 * super() Aufruf an erster Stelle ein.
 * Als Argumente für den Konstruktor dürfen nur die Parameter des Konstruktors,
 * statische Attribute oder Rückgabewerte von statischen Methoden der eigenen Klasse 
 * genommen werden. Die Rückgabewerte von Methodenaufrufe anderer Klassen sind immer
 * erlaubt.
 */
