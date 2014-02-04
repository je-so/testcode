package nio;

import java.util.ArrayList;
import java.util.List;
import java.io.IOException;
import java.nio.file.FileVisitResult;
import java.nio.file.FileVisitor;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.BasicFileAttributes;

public class GenericFileVisitor {

	/*
	 * Definiert das Filter Interface, das Dateien filtert. 
	 */
	public interface Filter {
		/*
		 * Liefert true zurück, falls file/attrs die Filterbedingung erfüllt.
		 */
		boolean accept(Path file, BasicFileAttributes attrs);
	}

	/*
	 * Definiert Vergleichstyp, wird in CompareFilter verwendet.
	 */
	public enum Compare {
		EQUAL, LOWER, HIGHER
	}

	/*
	 * Definiert compare Methoden, die verschiedene Datentypen miteinander vergleichen.
	 * Die Vergleichsart wird durch den Parameter Compare cmp in CompareFilter festgelegt.
	 */
	public static abstract class CompareFilter implements Filter {
		private Compare cmp;
		
		protected CompareFilter(Compare cmp) {
			this.cmp = cmp;
		}
		
		/*
		 * Vergleiche zwei long Werte. 
		 */
		boolean compare(long val1, long val2) {
			switch (cmp) {
			case EQUAL:  return val1 == val2;
			case LOWER:  return val1 < val2;
			case HIGHER: return val1 > val2;
			}
			return false;
		}
		
	}
	
	/*
	 * MatchSize filtert nach Größe der Dateien.
	 * Benutzt CompareFilter.compare(long,long) für den Vergleich.
	 */
	public static class MatchSize extends CompareFilter {
		private long size;
		
		public MatchSize(Compare cmp, long size) {
			super(cmp);
			this.size = size;
		}

		public boolean accept(Path file, BasicFileAttributes attrs) {
			return compare(attrs.size(), size);
		}
	}

	/*
	 * MatchNAme filtert nach Name der Dateien.
	 * Der Name der Datei wird mittels des regulären Ausdrucks 
	 * pattern gematchet.
	 */
	public static class MatchName implements Filter {
		private java.util.regex.Pattern pattern;
		
		public MatchName(String pattern) {
			this.pattern = java.util.regex.Pattern.compile(pattern);
		}

		public boolean accept(Path file, BasicFileAttributes attrs) {
			return pattern.matcher(file.toAbsolutePath().toString()).matches();
		}
	}

	/*
	 * AndFilter verbindet zwei andere Filter mittels und (&&) 
	 */
	public static class AndFilter implements Filter {
		Filter left; 
		Filter right;
		
		public AndFilter(Filter left, Filter right) {
			this.left = left;
			this.right = right;
		}

		public boolean accept(Path file, BasicFileAttributes attrs) {
			return left.accept(file, attrs) && right.accept(file, attrs); 
		}
	}

	/*
	 * OrFilter verbindet zwei andere Filter mittels oder (||) 
	 */
	public static class OrFilter implements Filter {
		Filter left; 
		Filter right;
		
		public OrFilter(Filter left, Filter right) {
			this.left = left;
			this.right = right;
		}

		public boolean accept(Path file, BasicFileAttributes attrs) {
			return left.accept(file, attrs) || right.accept(file, attrs); 
		}
	}
	
	/*
	 * FilterFileVisitor implementiert FileVisitor<Path> und kann daher
	 * mit Files.walkFileTree verwendet werden.
	 * 
	 * Dateien, welche vom Filter Argument filter akzeptiert werden,
	 * werden an die Liste list angehängt.
	 * 
	 * Ist Parameter filter null, dann werden einfach alle Dateien
	 * zur Liste list hinzugefügt.
	 */
	private static class FilterFileVisitor implements FileVisitor<Path> {
		
		private List<Path> list;
		private Filter filter;
		
		private FilterFileVisitor(List<Path> list, Filter filter) {
			this.list = list;
			this.filter = filter;
		}
		
		@Override
		public FileVisitResult postVisitDirectory(Path dir, IOException ex)
				throws IOException {
			return FileVisitResult.CONTINUE;
		}
		
		@Override
		public FileVisitResult preVisitDirectory(Path dir,
				BasicFileAttributes attrs) throws IOException {
			return FileVisitResult.CONTINUE;
		}
		
		@Override
		public FileVisitResult visitFile(Path file, BasicFileAttributes attrs)
				throws IOException {
			if (filter == null || filter.accept(file, attrs)) {
				list.add(file);
			}
			return FileVisitResult.CONTINUE;
		}
		
		@Override
		public FileVisitResult visitFileFailed(Path file, IOException exc)
				throws IOException {
			throw exc;
		}
		
	}

	/*
	 * Suche alle Dateien in Verzeichnis dir und in dessen Unterverzeichnissen,
	 * die das Filterkriterium filter erfüllen.
	 * Ist filter null, werden alle Dateien in dir und in allen Unterverzeichnissen zurückgeliefert.
	 * 
	 * Returns die Liste aller Dateien, welche die Filterbedingung filter erfüllen.
	 * 
	 */
	public static List<Path> selectFiles(Path dir, Filter filter) throws IOException {
		List<Path> list = new ArrayList<>(128);
		Files.walkFileTree(dir, new FilterFileVisitor(list, filter));
		return list;
	}

	/*
	 * 
	 * Test Driver.
	 * 
	 */
	public static void main(String[] args) throws IOException{
		Filter filter;
		List<Path> list;
		
		// liefert alle .java Dateien
		filter = new MatchName(".*\\.java$");
		
		// liefert alle .java Dateien im strings Verzeichnis
//		filter = new MatchName(".*\\\\strings\\\\[^\\\\]+\\.java$");
		
		// liefert alle .java Dateien im strings Verzeichnis , die größer als 2000 Bytes sind.
//		filter = new AndFilter(
//					new MatchName(".*\\\\strings\\\\[^\\\\]+\\.java$"),
//					new MatchSize(Compare.HIGHER, 2000));
		
		// liefert alle .java Dateien im strings Verzeichnis , die größer als 2000 Bytes sind.
		// und alle .txt Dateien
//		filter = new OrFilter(
//					new AndFilter(
//						new MatchName(".*\\\\strings\\\\[^\\\\]+\\.java$"),
//						new MatchSize(Compare.HIGHER, 2000)),
//					new MatchName(".*\\.txt$"));
		
		list = selectFiles(Paths.get(""), filter);
		
		for (Path p : list) {
			System.out.println(p.toAbsolutePath() + " (size=" + Files.size(p) + " bytes)");
		}
	}

}
