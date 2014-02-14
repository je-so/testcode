package thread;

public class NotifyDemo extends Thread {

	static Object monitor = new Object();
	
	public void run() {
		synchronized (monitor) {
			System.out.println(getName() + ": Warte");
			try {
				monitor.wait();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			System.out.println(getName() + ": Notified");
		}
	}
	
	
	public static void main(String[] args) throws Exception {
		/*
		 * Der main-Thread erzeugt n Threads.
		 * 
		 * Jeder Thread soll nach dem Aufruf seiner Start()-Methode
		 * in den Zustand WAITING gehen.
		 * 
		 * Der main-Thread legt sich dann eine Weile schlafen.
		 * Anschließend sendet er ein notifyAll, wartet auf das Ende aller
		 * Threads und beendet sich.
		 */
		
		Thread threads[] = new Thread[50];
		
		for (int i = 0; i < threads.length; ++i) {
			threads[i] = new NotifyDemo();
			threads[i].setPriority(Thread.MIN_PRIORITY + (i==0?(Thread.MAX_PRIORITY-Thread.MIN_PRIORITY):0));
			Thread.sleep(150);
			threads[i].start();
			Thread.sleep(150);
		}
		
		try {
			for (int i = 0; i < threads.length; ++i) {
				Thread.sleep(150);
				synchronized (monitor) {
//					monitor.notifyAll();
//					break;
					monitor.notify();
				}
				Thread.sleep(150);
			}
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

}
