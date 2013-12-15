/* Question:
 * What is the result of print(null); in main() ?
 * 1. "print(SubSubclass1)"
 * 2. "print()"
 * 3. "print(Base1)"
 * 4. "print(Subclass1)"
 * 5. Compilation error
 */
package java_question;

class Base1 {
	
}

class Subclass1 extends Base1 {
	
}

class SubSubclass1 extends Subclass1 {
	
}

public class overloading_null {

	public static void print() {
		System.out.println("print()");
	}
	
	public static void print(Base1 arg) {
		System.out.println("print(Base1)");
	}

	public static void print(Subclass1 arg) {
		System.out.println("print(Subclass1)");
	}

	public static void print(SubSubclass1 arg) {
		System.out.println("print(SubSubclass1)");
	}

	public static void main(String[] args) {
		print(null);
	}
}
