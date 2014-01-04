package JavaFragen;

/* Welche Aussage ist richtig?
 * A) Es wird "red\n" ausgegeben. 
 * B) Es wird "green\n" ausgegeben. 
 * C) Es wird "blue\n" ausgegeben. 
 * D) Es wird gar nichts ausgegeben.
 * E) Es gibt einen Compilerfehler, da obj.color eine uninitialisierte lokale Variable ist.
 * F) Statt einer Ausgabe wird eine NullPointerException geworfen.
 */

public class EnumDemo2 {

	enum Color { RED, GREEN, BLUE }
	
	Color color;
	
	public static void main(String[] args) {
		EnumDemo2 obj = new EnumDemo2();
		
		switch (obj.color) {
		case RED:   
			System.out.println("red");
			break;
		case GREEN:
			System.out.println("green");
			break;
		case BLUE:
			System.out.println("blue");
			break;
		}
	}
}


















/* Antwort: Das Programm bricht mit einer NullPointerException ab.
 * Erklärung:
 * Intern wandelt der Compiler die switch(EnumTyp) Anweisung in eine
 * switch(int) Anweisung um.
 * Jeder Enumtyp erbt von class java.lang.Enum<E>, die eine Methode
 * ordinal() implementiert, welche die Ordnungszahl einer Enumkonstanten
 * ermittelt. Die erste definierte Konstante im enum erhält den Wert 0,
 * die zweite den Wert 1 usw. Daher gilt:
 * Color.RED.ordinal()   == 0
 * && Color.GREEN.ordinal() == 1
 * && Color.BLUE.ordinal()  == 2
 * Somit will der Compiler über obj.color.ordinal() den Integer-Wert
 * von obj.color ermitteln und in das entsprechende case verzweigen.
 * obj.color ist jedoch ein »object field« oder Variable einer Instanz
 * und wird daher, wenn nicht anders angegeben, auf null initialisiert
 * (0, 0.0 bzw. false bei primitiven Typen). 
 */
