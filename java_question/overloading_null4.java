/* Question:
 * What is the result of the print statements in main() ?
 * 2. [1, 2, 3]
 *    null
 *    null
 * 2. [1, 2, 3]
 *    [null]
 *    null
 * 3. [1, 2, 3]
 *    null
 *    [null]
 * 4. Compilation error
 */
package java_question;

import java.util.Arrays;

public class overloading_null4 {

	public static void main(String[] args) {
		print("1", "2", "3");
		print(null);
		print((String)null);
	}

	public static void print(String... strings) {
		System.out.println(Arrays.toString(strings));
	}
	
}
