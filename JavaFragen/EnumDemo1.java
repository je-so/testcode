package JavaFragen;

/* Welche Aussage ist richtig?
 * A) Es wird "green\n" ausgegeben. 
 * B) Es wird "blue\n" ausgegeben. 
 * C) Es wird "green\nblue\n" ausgegeben.
 * D) Compilerfehler, da hinter jedem case ein Color.RED bzw. Color.GREEN oder Color.BLUE
 *    stehen sollte. 
 */

public class EnumDemo1 {

	enum Color {
		RED, GREEN, BLUE
	}
	
	public static void main(String[] args) {
		Color col = Color.GREEN;
		switch (col) {
		case RED:   
			System.out.println("red");
		case GREEN:
			System.out.println("green");
		case BLUE:
			System.out.println("blue");
		}
	}
}
























/* Die richtige Antwort ist: Es wird "green\nblue\n" ausgegeben.
 * Erklärung:
 * - Innerhalb des switch dürfen nur die einfachen Namen der enum Konstanten angegeben werden,
 *   da der Compiler den enum Typ über den Referenztyp der Variable im switch kennt.
 *   Voll qualifizierte Namen wie Color.RED usw. sind nicht erlaubt.
 * - Da keine break Anweisung hinter System.out.println("green"); steht, wird auch der
 *   case BLUE: durchlaufen. 
 */
