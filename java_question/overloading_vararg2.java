/* Question:
 * Which statement is true ?
 * 1. Output is:
 *    2
 * 2. Output is:
 *    1.0, 2.0
 * 3. Output is:
 *    2.0
 * 3. Compiler prints error.
 *    It works if you change line A to
 *    (Float)1f, (Float)2f
 * 4. Compiler prints error.
 *    It works if you change line A to
 *    (Float)1.0f, (Float)2.0f
 * 5. Compiler prints error.
 *    It works if you change line A either to
 *    new Float[] { 1.0, 2.0 }
 *    or to
 *    new float[] { 1.0, 2.0 }
 * 6. Compiler prints error.
 *    It works if you change line A either to
 *    new Float[] { 1.0f, 2.0f }
 *    or to
 *    new float[] { 1.0f, 2.0f }
 */
package java_question;

public class overloading_vararg2 {
	public void count(
			Float... floats
	) {
		System.out.println(floats.length);
	}

	public void count(
			float... floats
	) {
		System.out.println(floats.length);
	}

	public static void main(String[] args) {
		overloading_vararg2 obj = new overloading_vararg2();
		obj.count(
				(Float)1f, (Float)2 // LINE A
		);
	}
}
