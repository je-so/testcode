package JavaFragen;

/* Welche Aussage ist richtig?
 * 
 * A) Das Programm gibt "abc\nnull" aus.
 * B) Compilerfehler.
 * C) Exception zur Laufzeit.
 */

public class NullPointer3 {
	
	public static void main(String[] args) {
		StringBuilder sb = new StringBuilder();
		char[] chars = { 'a', 'b', 'c', '\n' };
		sb.append(chars);
		chars = null;
		sb.append(chars);
		// sb.append((CharSequence)null);
		// sb.append((Object)null);
		// sb.append((String)null);
		// sb.append((StringBuffer)null);
		System.out.println(sb);
	}
}



























/* Antwort: Das Programm bricht mit einer java.lang.NullPointerException ab, also Antwort C.
 * 
 * Erklärung:
 * Die Methoden
 *  public StringBuilder append(char[] str)
 *  public StringBuilder append(char[] str, int offset, int len)
 * brechen mit einer java.lang.NullPointerException ab, wenn das Argument str null ist.
 * 
 * Die Methoden
 *  public StringBuilder append(CharSequence s)
 *  public StringBuilder append(Object obj)
 *  public StringBuilder append(String str)
 *  public StringBuilder append(StringBuffer sb)
 * hängen dagegen "null" an den StringBuffer an.
 * 
 */
