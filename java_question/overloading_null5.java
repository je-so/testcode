/* Question:
 * Which 2 statements are true ?
 * 1. Output is:
 *    2
 *    0
 * 2. Output is:
 *    2
 *    null
 * 3. In line A compiler error.
 *    If you change it to print((String)null); it works.
 * 4. In line B compiler error
 *    If you change it to strings.length it works.
 * 5. In line C compiler error
 *    If you change it to string.length it works.
 * 6. There is no compilation error.
 */
package java_question;

public class overloading_null5 {

	public static void main(String[] args) {
		print("a", "b");
		print(null); // LINE A
	}

	public static void print(String... strings) {
		System.out.println(strings != null ?
			strings.length() // LINE B
			: 0);
	}

	public static void print(String string) {
		System.out.println(string != null ?
			string.length() // LINE C
			: null);
	}
}
