package JavaFragen;

/* Welche 2 Aussagen sind richtig?
 * 
 * A) Zeile A compiliert. 
 * B) Zeile B compiliert.
 * C) Zeile A ist fehlerhaft. 
 * D) Zeile B ist fehlerhaft. 
 */

class StaticInheritance2_Parent {
	public void print() {}
	public static void sprint() {}
}

public class StaticInheritance2 extends StaticInheritance2_Parent {
	public void sprint() {} // Line A
	public static void print() {} // Line B
}














/* Antwort: Zeilen A und B sind fehlerhaft (Nr. C und D).
 *
 * Erklärung:
 * Die Methode print in StaticInheritance2_Parent kann nicht durch
 * eine statische Methode print in StaticInheritance2 überschrieben werden.
 * 
 * Die statische Methode sprint in StaticInheritance2_Parent kann nicht durch
 * die Methode sprint in StaticInheritance2 überschrieben werden, da statische 
 * Methoden nicht überschrieben, sondern nur verdeckt(überlagert) werden können.
 *
 */
