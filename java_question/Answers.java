package java_question;

public class Answers {

	/* ----------------
	 * overloading_null
	 * ---------------- 
	 * Answer: 1. "print(SubSubclass1)"
	 */
	
	/* -----------------
	 * overloading_null2
	 * ----------------- 
	 * Answer: 4. Compilation error
	 * 
	 * > javac overloading_null2.java
	 * > overloading_null2.java:37: error: reference to print is ambiguous, 
	 * > both method print(Subclass2_1) in overloading_null2 and 
	 * > method print(Subclass2_2) in overloading_null2 match
	 * > print(null);
	 */

	/* -----------------
	 * overloading_null3
	 * ----------------- 
	 * Answer: 2. "print(Subclass3_2)"
	 */	

	/* -----------------
	 * overloading_null4
	 * -----------------
	 * Answer: 3. [1, 2, 3]
	 *            null
	 *            [null]
	 * Explanation:
	 * 1. print(null)
	 * print(String... strings); is syntactic sugar for print(String[] strings);
	 * and the compiler calls print(String[] strings) with strings set to null
	 * 2. print((String)null);
	 * The compiler has only a print(String... strings) for a String parameter
	 * and does therefore create an array print(new String[]{ null });
	 */

	/* -----------------
	 * overloading_null5
	 * -----------------
	 * Answer: 3. and 4.
	 */

	/* ------------------
	 * overloading_vararg
	 * ------------------
	 * Answer: 5.
	 * Explanation: The compiler can not decide in the context
	 * of a variable argument list if it should call the version with
	 * autoboxing or the unboxed version.
	 */

	/* -------------------
	 * overloading_vararg2
	 * -------------------
	 * Answer: 6.
	 * Explanation: The compiler can not decide in the context
	 * of a variable argument list if it should call the version with
	 * unboxing or the wrapper type version.
	 */
}
