/* Question:
 * What is the result of print((Subclass3_2)null); in main() ?
 * 1. "print(Base3)"
 * 2. "print(Subclass3_2)"
 * 3. "print(Subclass3_1)"
 * 4. Compilation error: Can not cast null into a reference type.
 */
package java_question;

class Base3 {
	
}

class Subclass3_1 extends Base3 {
	
}

class Subclass3_2 extends Base3 {
	
}

public class overloading_null3 {

	public static void print(Base3 arg) {
		System.out.println("print(Base3)");
	}

	public static void print(Subclass3_1 arg) {
		System.out.println("print(Subclass3_1)");
	}

	public static void print(Subclass3_2 arg) {
		System.out.println("print(Subclass3_2)");
	}
	
	public static void main(String[] args) {
		print((Subclass3_2)null);
	}
}
