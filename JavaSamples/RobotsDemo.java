package oo;

import java.util.Arrays;
import java.util.Scanner;

public class RobotsDemo {

	static final int MAXX = 60;
	static final int MAXY = 15;
	static final int MAXROBOTS = 10;

	private static class Position {
		public int x, y;

		public Position() {
		}

		public void set(int x, int y) {
			this.x = x;
			this.y = y;
		}

		public void move(int mx, int my) {
			x += mx;
			y += my;
		}

		public int distance(Position pos) {
			return Math.max(Math.abs(pos.x - x), Math.abs(pos.y - y));
		}

		@Override
		public boolean equals(Object obj) {
			if (obj instanceof Position) {
				Position pos = (Position) obj;
				return pos.x == x && pos.y == y;
			}

			return false;
		}

		public boolean equals(int x, int y) {
			return this.x == x && this.y == y;
		}

		@Override
		public String toString() {
			StringBuffer strbuf = new StringBuffer(40);
			return strbuf.append("Position(x=").append(x).append(",y=")
					.append(y).append(")").toString();
		}
	}

	static enum GameState {
		Running, Winner, Loser
	}

	Scanner input = new Scanner(System.in);
	Position player = new Position();
	Position exit = new Position();
	Position[] robots = new Position[MAXROBOTS];
	Position[] walls = new Position[MAXROBOTS * 2];
	GameState state;

	static {
		System.out.println(" ****************************************************************** ");
		System.out.println("***  RRRRRRRR   OOOOOO   BBBBBBB    OOOOOO   TTTTTTTT   SSSSSSS  ***");
		System.out.println("***  RR    RR  OO    OO  BB    BB  OO    OO     TT     SS        ***");
		System.out.println("***  RR    RR  OO    OO  BB    BB  OO    OO     TT     SS        ***");
		System.out.println("***  RR    RR  OO    OO  BB    BB  OO    OO     TT     SS        ***");
		System.out.println("***  RRRRRRRR  OO    OO  BBBBBBB   OO    OO     TT      SSSSSS   ***");
		System.out.println("***  RRRRR     OO    OO  BB    BB  OO    OO     TT           SS  ***");
		System.out.println("***  RR RRR    OO    OO  BB    BB  OO    OO     TT           SS  ***");
		System.out.println("***  RR  RRR   OO    OO  BB    BB  OO    OO     TT           SS  ***");
		System.out.println("***  RR    RR   OOOOOO   BBBBBBB    OOOOOO     TTTT    SSSSSSS   ***");
		System.out.println(" ****************************************************************** ");
	}

	@Override
	protected void finalize() {
		input.close();
	}

	private static void generatePos(/* out */Position pos) {
		pos.set((int) (1 + Math.random() * (MAXX - 1)),
		        (int) (1 + Math.random() * (MAXY - 1)));
	}

	private void generatePlayfield() {
		generatePos(player);

		do {
			generatePos(exit);
		} while (player.distance(exit) <= 3 || player.distance(exit) >= 7);

		for (int i = 0; i < robots.length; i += player.distance(robots[i]) > 5
				&& !exit.equals(robots[i]) ? 1 : 0) {
			if (robots[i] == null)
				robots[i] = new Position();
			generatePos(robots[i]);
		}

		Arrays.fill(walls, 0, walls.length / 2, null);
		for (int i = walls.length / 2; i < walls.length; i += player
				.equals(walls[i]) || exit.equals(walls[i]) ? 0 : 1) {
			if (walls[i] == null)
				walls[i] = new Position();
			generatePos(walls[i]);
		}

		for (int i = 0; i < robots.length; ++i) {
			for (Position wall : walls) {
				if (wall != null && wall.equals(robots[i])) {
					robots[i] = null; // robot dies
					break;
				}
			}
		}
	}

	private void printPlayfield() {
		for (int y = 0; y <= MAXY; ++y) {
			for (int x = 0; x <= MAXX; ++x) {
				if (x == 0 || y == 0 || x == MAXX || y == MAXY) {
					System.out.print("*");
				} else if (player.equals(x, y)) {
					System.out.print("P");
				} else if (exit.equals(x, y)) {
					System.out.print("E");
				} else {
					boolean isRobot = false;
					boolean isWall = false;
					for (Position robot : robots) {
						if (robot == null)
							continue;
						if (robot.equals(x, y)) {
							isRobot = true;
							break;
						}
					}
					if (!isRobot) {
						for (Position wall : walls) {
							if (wall == null)
								continue;
							if (wall.equals(x, y)) {
								isWall = true;
								break;
							}
						}
					}
					System.out.print(isRobot ? "r" : isWall ? "*" : " ");
				}
			}
			System.out.println();
		}
	}

	public void handleInput() {
		char key = input.next().charAt(0);
		int mx = 0, my = 0;
		switch (key) {
		case 'w':
		case 'W':
			my = -1;
			break;
		case 'a':
		case 'A':
			mx = -1;
			break;
		case 's':
		case 'S':
			my = 1;
			break;
		case 'd':
		case 'D':
			mx = 1;
			break;
		}

		int newx = player.x + mx;
		int newy = player.y + my;

		boolean isValidMove = 0 < newx && newx < MAXX && 0 < newy
				&& newy < MAXY;

		if (isValidMove) {
			for (Position robot : robots) {
				if (robot == null)
					continue;
				if (robot.equals(newx, newy)) {
					isValidMove = false;
					break;
				}
			}
		}

		if (isValidMove) {
			for (Position wall : walls) {
				if (wall == null)
					continue;
				if (wall.equals(newx, newy)) {
					isValidMove = false;
					break;
				}
			}
		}

		if (isValidMove) {
			player.set(newx, newy);
		}
	}

	private GameState moveRobots() {
		for (int i = 0; i < robots.length; ++i) {
			Position robot = robots[i];
			if (robot == null)
				continue;

			int mx = player.x < robot.x ? -1 : player.x > robot.x ? 1 : 0;
			int my = player.y < robot.y ? -1 : player.y > robot.y ? 1 : 0;
			robot.move(mx, my);

			if (robot.equals(player)) {
				// robot touches player
				return GameState.Loser;

			} else if (robot.equals(exit)) {
				// robot dies if it runs into exit
				robots[i] = null;

			} else {
				for (Position wall : walls) {
					if (wall == null)
						continue;
					if (wall.equals(robot)) {
						// robot dies if it runs into wall
						robots[i] = null;
						break;
					}
				}
			}
		}

		// collision of two robots lets them die and builds a wall
		for (int i = 0; i < robots.length; ++i) {
			Position robot = robots[i];
			if (robot == null)
				continue;

			for (int i2 = i + 1; i2 < robots.length; ++i2) {
				Position robot2 = robots[i2];
				if (robot2 != null && robot.equals(robot2)) {
					walls[i] = robot; // robot turns into wall
					robots[i] = null; // robot i dies
					robots[i2] = null; // robot i2 dies
					break;
				}
			}
		}

		// Check that at least one robot exists
		int i;
		for (i = 0; i < robots.length; ++i) {
			if (robots[i] != null)
				break;
		}
		// if no more robots ==> player wins
		return i == robots.length ? GameState.Winner : GameState.Running;
	}

	private void gameloop() {
		do {
			printPlayfield();
			handleInput();
			if (exit.equals(player)) {
				state = GameState.Winner;
				break;
			}
			state = moveRobots();
		} while (state == GameState.Running);
	}

	private void printAwards() {
		System.out.println();
		if (state == GameState.Winner) {
			System.out.println("*** >>>> Y O U   W I N <<<< ***");
		} else {
			System.out.println("*** >>>> L O S E R <<< ***");
		}
	}

	public void start() {
		System.out.println("\nUSE ** W-A-S-D <Return> ** TO MOVE PLAYER (other keys keep her still)");
		System.out.println("GOAL: Player(P) must reach exit(E).");
		System.out.println("GAMEPLAY: If a robot(r) touches the player she dies.");
		System.out.println("GAMEPLAY: A wall(*) or the exit(E) protects the player.");
		System.out.println("GAMEPLAY: Two crashing robots generate a wall(*).");
		System.out.println("GAMEPLAY: Robots move diagonally but the player can move only");
		System.out.println("GAMEPLAY: in a vertical or horizontal direction.\n");

		generatePlayfield();
		gameloop();
		printAwards();
	}

	public static void main(String[] args) {
		new RobotsDemo().start();
	}

}