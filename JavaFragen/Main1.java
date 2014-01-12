package JavaFragen;

/* Das Programm wird mit
 * java JavaFragen.Main1
 * gestartet. Welche Aussage ist richtig?
 * 
 * A) Compilerfehler.
 * B) Es wird "1" ausgegeben.
 * C) Das Programm bricht mit einer Exception ab.
 * D) Das Programm kann nicht gestartet werden.
 */


class Main1 {

	int x = 1;
	
	static void main(String...args) {
		System.out.println(new Main1().x);
	}
}



































/* Antwort: Das Programm kann nicht gestartet werden.
 * 
 * Erklärung:
 * Das Kommando "java JavaFragen.Main1" bringt die Fehlermeldung
 * Error: Main method not found in class JavaFragen.Main1, please define the main method as:
 * public static void main(String[] args)
 * 
 * D.h. die Methode main muss public sein, private, »package sichtbar« und protected
 * funktionieren nicht.
 */
