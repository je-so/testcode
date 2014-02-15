package thread;

import java.util.*;
import java.util.concurrent.locks.*;

public class WorkerThreadDemo {

	static class TaskQueue {

		/*
		 * Die Queue verwaltet maximal 10 Aufträge.
		 */
		private static final int MAXSIZE = 10;

		private static class Monitor {
			Lock lock = new ReentrantLock();
			Condition notFull = lock.newCondition();
			Condition notEmpty = lock.newCondition();
		}

		/*
		 * Explizites Monitor-Objekt.
		 */
                private Monitor monitor = new Monitor();

		/*
		 * Ein Auftrag wird als simpler int repräsentiert.
		 */
		private Queue<Integer> tasks = new ArrayDeque<>();
		
		public int removeTask() throws InterruptedException {
			monitor.lock.lockInterruptibly();
			try {
				while (tasks.size() == 0) {
					System.out.println(Thread.currentThread().getName() + ": Warte, da Queue leer");
					monitor.notEmpty.await();
					System.out.println(Thread.currentThread().getName() + ": Warten zu Ende");
				}
				return tasks.remove();
			} finally {
				monitor.notFull.signal();
				monitor.lock.unlock();
			}
		}

		public void addTask(int task) throws InterruptedException {
			monitor.lock.lockInterruptibly();
			try {
				while (tasks.size() == MAXSIZE) {
					System.out.println(Thread.currentThread().getName() + ": Warte, da Queue voll");
					monitor.notFull.await();
					System.out.println(Thread.currentThread().getName() + ": Warten zu Ende");
				}
				tasks.add(task);
			} finally {
				monitor.notEmpty.signal();
				monitor.lock.unlock();
			}
		}

		public int size() throws InterruptedException {
			monitor.lock.lockInterruptibly();
			try {
				return tasks.size();
			} finally {
				monitor.lock.unlock();
			}
		}
	}


	public static void main(String[] args) throws InterruptedException {
		Thread[] workers = new Thread[3];
		final TaskQueue queue = new TaskQueue();

		for (int i = 0; i < workers.length; ++i) {
			workers[i] = new Thread() {
				@Override public void run() {
					System.out.println(getName() + ": START");
					try {
						Random rnd = new Random();
						for (;;) {
							int task = queue.removeTask();
							System.out.println(getName() + ": Bearbeite Task " + task);
							Thread.sleep(900 + 500 * rnd.nextInt(10));
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
			Thread.sleep(50 + 50 * rnd.nextInt(10));
		}

		while (queue.size() > 0) {
			Thread.sleep(500);
		}

		for (Thread thread : workers) {
			thread.interrupt();
		}

	}

}
