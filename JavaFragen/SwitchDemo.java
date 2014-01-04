package JavaFragen;

/* Welche 3 statischen Methoden sind fehlerfrei ?
 * A) test1(int i)
 * B) test2(int i)
 * C) test3(int i)
 * D) getC()
 * E) test4(int i)
 * F) main(String[] args)
 */

public class SwitchDemo {

	public static void test1(int i) {
		final int C = 3;
		switch (i) {
		case 0: System.out.println(0); break;
		case 1: System.out.println(1); break;
		case C: System.out.println(C); break;
		}
	}

	public static void test2(int i) {
		final int C;
		C = 2;
		switch (i) {
		case 0: System.out.println(0); break;
		case 1: System.out.println(1); break;
		case C: System.out.println(C); break;
		}
	}

	public static void test3(int i) {
		final int C = 1;
		switch (i) {
		case 0: System.out.println(0); break;
		case 1: System.out.println(1); break;
		case C: System.out.println(C); break;
		}
	}

	public static int getC() { return 3;	}
	
	public static void test4(int i) {
		final int C = getC();
		switch (i) {
		case 0: System.out.println(0); break;
		case 1: System.out.println(1); break;
		case C: System.out.println(C); break;
		}
	}
	
	public static void main(String[] args) {
		int i = 1;
		test1(i);
		test2(i);
		test3(i);
		test4(i);
	}
}






























/* Antwort:
 * - Die Methoden test1,getC und main sind fehlerfrei.
 * - Die variable C in test2 ist keine compile-time Konstante und daher im switch nicht zugelassen.
 *   Der Compiler meldet einen »case expressions must be constant expressions« Fehler.
 * - Die Konstante C in test3 denselben Wert 1 wie »case 1:« im switch Statement.
 *   Der Compiler meldet einen »Duplicate case« Fehler.
 * - Die variable C in test4 ist keine compile-time Konstante und daher im switch nicht zugelassen.
 *   Der Compiler meldet einen »case expressions must be constant expressions« Fehler.
 * */
