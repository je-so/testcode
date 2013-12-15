/* Question:
 * What is the result of print(null); in main() ?
 * 1. "print(Base2)"
 * 2. "print(Subclass2_1)"
 * 3. "print(Subclass2_2)"
 * 4. Compilation error
 */
package java_question;

class Base2 {
	
}

class Subclass2_1 extends Base2 {
	
}

class Subclass2_2 extends Base2 {
	
}

public class overloading_null2 {

	public static void print(Base2 arg) {
		System.out.println("print(Base2)");
	}

	public static void print(Subclass2_1 arg) {
		System.out.println("print(Subclass2_1)");
	}

	public static void print(Subclass2_2 arg) {
		System.out.println("print(Subclass2_2)");
	}
	
	public static void main(String[] args) {
		print(null);
	}
}
