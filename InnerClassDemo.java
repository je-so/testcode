package oo;

public class InnerClassDemo {

	int i1 = 1;
	int i2 = 2;
	
	class C1 {
		public int sum() {
			return i1 + i2;
		}
	}
	
	public int sum() {
		final int CONSTANT1;
		final int CONSTANT2;
		
		CONSTANT1 = 10;
		CONSTANT2 = 20;
		
		class C2 {
			public int sum() {
				return CONSTANT1 + CONSTANT2;
			}
		}
		
		return new C1().sum() + new C2().sum();
	}
	
	public static void main(String[] args) {
		System.out.println(new InnerClassDemo().sum());
	}
	
}