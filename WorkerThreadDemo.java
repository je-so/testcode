package thread;

import java.util.*;

public class WorkerThreadDemo {

	static class TaskQueue {
		/*
		 * Ein Auftrag wird als simpler int repräsentiert.
		 * Die Queue verwaltet maximal 10 Aufträge.
		 */
		int[] tasks = new int[10];
		int   first_task_index;
		int   nr_of_task;
		
		/*
		 * Aufgabe: Schreiben Sie removeTask so um, dass die Methode im Falle einer leeren Queue wartet,
		 * bis ein Task verfügbar ist.
		 */
		public int removeTask() throws InterruptedException {
			int task;
			while ((task = sync_remove()) == -1) {
				System.out.println(Thread.currentThread().getName() + ": Warte auf Task");
				synchronized (tasks) {
					tasks.wait();
				}
				System.out.println(Thread.currentThread().getName() + ": Warten zu Ende");
			}
			return task;
		}

		/*
		 * Aufgabe: Schreiben Sie addTask so um, dass die Methode im Falle einer vollen Queue wartet,
		 * bis wieder Platz verfügbar ist.
		 */
		public synchronized void addTask(int task) throws InterruptedException {
			if (task < 0) {
				throw new IllegalArgumentException("task < 0");
			}
			
			while (nr_of_task == tasks.length) {
				System.out.println(Thread.currentThread().getName() + ": Warte bis Queue nicht mehr voll");
				this.wait();
				System.out.println(Thread.currentThread().getName() + ": Warten zu Ende");
			}		

			final int index = (first_task_index + nr_of_task) % tasks.length;
			++ nr_of_task;
			tasks[index] = task;
			synchronized (tasks) {
				tasks.notify(); // wake up possible waiting threads in removeTask
			}
		}

		/*
		 * Liefert einen Task zurück (>= 0) oder -1, falls
		 * die Queue leer ist.
		 */
		public synchronized int sync_remove() {
			if (nr_of_task == 0) {
				return -1;
			}
			-- nr_of_task;
			final int index = first_task_index;
			first_task_index = (first_task_index+1) % tasks.length;
			this.notify(); // wake up possible waiting threads in addTask
			return tasks[index];
		}

		public synchronized int size() {
			return nr_of_task;
		}
	}

	static TaskQueue queue = new TaskQueue();

	public static void main(String[] args) throws InterruptedException {
		Thread[] workers = new Thread[3];

		for (int i = 0; i < workers.length; ++i) {
			workers[i] = new Thread() {
				@Override public void run() {
					System.out.println(getName() + ": START");
					try {
						Random rnd = new Random();
						for (;;) {
							int task = queue.removeTask();
							System.out.println(getName() + ": Bearbeite Task " + task);
							Thread.sleep(800 + 500 * rnd.nextInt(10));
						}
					} catch (InterruptedException e) { 
						System.out.println(getName() + ": Interrupted");
					}
					System.out.println(getName() + ": ENDE");
				}
			};
			workers[i].start();
		}

		Random rnd = new Random();

		Thread.sleep(500);
		for (int task = 0; task < 20; ++task) {
			System.out.println(Thread.currentThread().getName() + ": Erzeuge neue Task " + task);
			queue.addTask(task);
			Thread.sleep(100 + 100 * rnd.nextInt(10));
		}

		while (queue.size() > 0) {
			Thread.sleep(500);
		}

		for (Thread thread : workers) {
			thread.interrupt();
		}

	}

}
