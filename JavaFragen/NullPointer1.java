package JavaFragen;

/* Das folgende Programm gibt z.B. aus
 * String.valueOf((Object)null): null
 * String.valueOf((char[])null): null
 * System.out.println((Object)null): null
 * System.out.println((String)null): null
 * System.out.println((char[])null): null
 * wobei null der Rückgabewert der Methode String.valueOf bzw. der Ausgabewert 
 * der Methode System.out.println ist.
 * 
 * Das Programm kann aber auch  
 * String.valueOf((Object)null): NullPointerException
 * String.valueOf((char[])null): NullPointerException
 * System.out.println((Object)null): NullPointerException
 * System.out.println((String)null): NullPointerException
 * System.out.println((char[])null): NullPointerException
 * ausgeben, wenn bei einer Methode eine NullPointerException
 * geworfen wird.
 * 
 * Bestimmen Sie für jeden Methodenaufruf, 
 * ob das Programm null oder NullPointerException ausgegeben wird.
 * 
 * A) String.valueOf((Object)null): ?
 * B) String.valueOf((char[])null): ?
 * C) System.out.println((Object)null): ?
 * D) System.out.println((String)null): ?
 * E) System.out.println((char[])null): ?
 * */

public class NullPointer1 {

	public static void main(String... args) {
		Object   o = null;
		String   s = null;
		char[]  a = null;

		for (int i = 0; i < 2; ++i) {
			String type = "";
			String result = "";
			try {
				switch (i) {
				case 0: type = "Object" ; result = String.valueOf(o); break;
				case 1: type = "char[]" ; result = String.valueOf(a); break;
				}
			} catch (NullPointerException ex) {
				result = "NullPointerException";
			}
			System.out.print("String.valueOf((" + type + ")null): ");
			System.out.println(result);
		}

		for (int i = 0; i < 3; ++i) {
			try {
				System.out.print("System.out.println(");
				switch (i) {
				case 0: System.out.print("(Object)null): "); System.out.println(o); break;
				case 1: System.out.print("(String)null): "); System.out.println(s); break;
				case 2: System.out.print("(char[])null): "); System.out.println(a); break;
				}
			} catch (NullPointerException ex) {
				System.out.println("NullPointerException");
			}
		}
		
	}
	
}















/* Antwort: 
 * 
 * A) String.valueOf((Object)null): null
 * B) String.valueOf((char[])null): NullPointerException
 * C) System.out.println((Object)null): null
 * D) System.out.println((String)null): null
 * E) System.out.println((char[])null): NullPointerException
 * 
 * Erklärung: Keine, lerne die inkonsistente Welt von Java kennen! 
 * 
 * */
