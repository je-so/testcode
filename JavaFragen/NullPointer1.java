package JavaFragen;

/* Das folgende Programm gibt 6 mal "null" oder "NullPointerException" aus.
 * Bestimmen Sie für jeden Methodenaufruf,
 * ob das Programm null oder NullPointerException ausgegeben wird.
 * 
 * A) Zeile 1: "null" oder "NullPointerException"?
 * B) Zeile 2: "null" oder "NullPointerException"?
 * C) Zeile 3: "null" oder "NullPointerException"?
 * D) Zeile 4: "null" oder "NullPointerException"?
 * E) Zeile 5: "null" oder "NullPointerException"?
 * F) Zeile 6: "null" oder "NullPointerException"?
 * */

public class NullPointer1 {

	public static void main(String... args) {
		Object   object = null;
		String   string = null;
		char[]  array = null;

		for (int i = 0; i < 6; ++i) {
			try {
				switch (i) {
				case 0: 
					System.out.println(String.valueOf(object)); 
					break;
				case 1: 
					System.out.println(String.valueOf(string)); 
					break;
				case 2: 
					System.out.println(String.valueOf(array)); 
					break;
				case 3: 
					System.out.println(object); 
					break;
				case 4: 
					System.out.println(string); 
					break;
				case 5: 
					System.out.println(array); 
					break;
				}
			} catch (NullPointerException ex) {
				System.out.println("NullPointerException");
			}
		}
	}
}


















/* Antwort: 
 * 
 * A) null
 * B) null
 * B) NullPointerException
 * C) null
 * D) null
 * E) NullPointerException
 * 
 * Erklärung: Keine, lerne die inkonsistente Welt von Java kennen! 
 * 
 * */
