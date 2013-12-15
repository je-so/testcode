/* Question:
 * Which statement is true ?
 * 1. Output is:
 *    2
 * 2. Output is:
 *    [1, 2]
 * 3. Compiler prints error.
 *    It works if you change line A to
 *    Long... longs
 * 4. Compiler prints error.
 *    It works if you change line B to
 *    long...longs
 * 5. Compiler prints error.
 *    It works if you change line C either to
 *    new Long[] { 1L, 2L }
 *    or to
 *    new long[] { 1L, 2L }
 * 6. Output is:
 *    1
 */
package java_question;

public class overloading_vararg {

	public void count(
			Long...longs // LINE A
	) {
		System.out.println(longs.length);
	}

	public void count(
			long... longs // LINE B
	) {
		System.out.println(longs.length);
	}

	public static void main(String[] args) {
		overloading_vararg obj = new overloading_vararg();
		obj.count(
				1L, 2L // LINE C
		);
	}
}
