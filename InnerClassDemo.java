package oo;

public class InnerClassDemo {

	int i1 = 1;
	int i2 = 2;
	
	class C1 {
		// Compiler
		// final InnerClassDemo this$0;
		// C1(InnerClassDemo arg0) {
		// 	this$0 = arg0;
		//      super();
		// }
		public int sum() {
			return i1 + i2;
			// Compiler
			// return this$0.i1 + this$0.i2;
		}
	}
	
	public int sum() {
		final int CONSTANT1;
		final int CONSTANT2;
		
		CONSTANT1 = 10;
		CONSTANT2 = 20;
		
		class C2 {
			// Compiler
			// final InnerClassDemo this$0;
			// final int val$CONSTANT1;
			// final int val$CONSTANT2;
			// C2(InnerClassDemo arg0, int arg1, int arg2) {
			// 	this$0 = arg0;
			//      val$CONSTANT1 = arg1;
			//      val$CONSTANT2 = arg2;
			//      super();
			// }
			public int sum() {
				return CONSTANT1 + CONSTANT2;
				// Compiler
				// return val$CONSTANT1 + val$CONSTANT2;
			}
		}
		
		return new C1().sum() + new C2().sum();
		// Compiler
		// return new C1(this).sum() + new C2(this, CONSTANT1, CONSTANT2).sum();
	}
	
	public static void main(String[] args) {
		System.out.println(new InnerClassDemo().sum());
	}
	
}
