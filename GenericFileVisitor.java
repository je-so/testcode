package nio;

import java.text.ParseException;
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
	private static abstract class CompareFilter implements Filter {
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
	 * SizeFilter filtert nach Größe der Dateien.
	 * Benutzt CompareFilter.compare(long,long) für den Vergleich.
	 */
	private static class SizeFilter extends CompareFilter {
		private long size;
		
		public SizeFilter(Compare cmp, long size) {
			super(cmp);
			this.size = size;
		}

		public boolean accept(Path file, BasicFileAttributes attrs) {
			return compare(attrs.size(), size);
		}
	}

	/*
	 * NameFilter filtert nach Name der Dateien.
	 * Der Name der Datei wird mittels des regulären Ausdrucks 
	 * pattern gematcht.
	 */
	private static class NameFilter implements Filter {
		private java.util.regex.Pattern pattern;
		
		public NameFilter(String pattern) {
			this.pattern = java.util.regex.Pattern.compile(pattern);
		}

		public boolean accept(Path file, BasicFileAttributes attrs) {
			return pattern.matcher(file.toAbsolutePath().toString()).matches();
		}
	}

	/*
	 * AndFilter verbindet zwei andere Filter mittels und (&&) 
	 */
	private static class AndFilter implements Filter {
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
	private static class OrFilter implements Filter {
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

	public static List<Path> selectFiles(Path dir, final String filter) throws IOException, ParseException {
		class Parser {
			int pos = 0 ;
			
			Filter parse() throws ParseException {
				Filter filterobj = parseFilter();
				/*
				 * Wurde der gesamte String geparst?
				 */
				if (pos != filter.length()) {
					throw new ParseException(filter, pos);
				}
				return filterobj;
			}

			/*
			 * Parst  '(' <regex-string> ')' und bewegt pos weiter
			 */
			private void parseRegexGroup() throws ParseException {
				if (pos == filter.length() || filter.charAt(pos) != '(') {
					throw new ParseException(filter, pos);
				}

				++pos; // matches '('

				for (boolean skipChar = false; pos < filter.length(); ++pos) {
					if (skipChar) {
						skipChar = false;
						continue;
					}
					char c = filter.charAt(pos);
					if (c == '\\')
						skipChar = true;
					else if (c == '(') {
						parseRegexGroup();
						--pos;
					}
					else if (c == ')')
						break;
				}

				if (filter.length() == pos) {
					throw new ParseException(filter, pos);
				}
				
				++pos; // matches ')'
			}
			
			/*
			 * Merkt sich die aktuelle Position pos als Startposition.
			 * Parst  '(' <regex-string> ')' indem parseRegexGroup aufgerufen wird.
			 * Die Startposition und die aktuelle Position pos bestimmen den String
			 * des regulären Ausdrucks.
			 * Es wird der reguläre Ausdruck als String zurückgeliefert. 
			 */
			private String parseRegex() throws ParseException {
				int startpos = pos;
				parseRegexGroup();
				return filter.substring(startpos+1, pos-1);
			}

			private Filter parseSize() throws ParseException {
				Compare cmp;
				char c = pos < filter.length() ? filter.charAt(pos++) : 0;
				switch (c) {
				case '=': cmp = Compare.EQUAL; break;
				case '<': cmp = Compare.LOWER; break;
				case '>': cmp = Compare.HIGHER; break;
				default: throw new ParseException(filter, pos);
				}
				int startpos = pos;
				while (pos != filter.length() && Character.isDigit(filter.charAt(pos))) {
					++pos;
				}
				if (pos == startpos) {
					throw new ParseException(filter, pos);
				}
				return new SizeFilter(cmp, Long.parseLong(filter.substring(startpos, pos)));
			}
			
			private Filter parseAtom() throws ParseException {
				Filter filterobj;
				if (filter.length() == pos) {
						throw new ParseException(filter, pos);
				}
				switch (filter.charAt(pos)) {
				case '(':
					++pos;
					filterobj = parseFilter();
					if (filter.length() == pos || filter.charAt(pos) != ')') {
						throw new ParseException(filter, pos);
					}
					++pos; // matches ')'
					break;
				case 'n':
					if (!filter.substring(pos).startsWith("name=")) {
						throw new ParseException(filter, pos);
					}
					pos += 5;
					filterobj = new NameFilter(parseRegex());
					break;
				case 's':
					if (!filter.substring(pos).startsWith("size")) {
						throw new ParseException(filter, pos);
					}
					pos += 4;
					filterobj = parseSize();
					break;
				default:
					throw new ParseException(filter, pos);
				}
				return filterobj;	
			}

			private Filter parseFilter() throws ParseException {
				Filter filterobj = parseAtom();
				LOOP: while (filter.length() != pos) {
					switch (filter.charAt(pos)) {
					case '|':
						if (!filter.substring(pos).startsWith("||")) {
							throw new ParseException(filter, pos);
						}
						pos += 2;
						filterobj = new OrFilter(filterobj, parseAtom());
						break;
					case '&':
						if (!filter.substring(pos).startsWith("&&")) {
							throw new ParseException(filter, pos);
						}
						pos += 2;
						filterobj = new AndFilter(filterobj, parseAtom());
						break;
					default:
						break LOOP;
					}
				}
				return filterobj;	
			}
			
		};

		Filter filterobj = new Parser().parse();
		
		return selectFiles(dir, filterobj);
	}

	/*
	 * Erzeugt Filter, der nur Dateien akzeptiert, deren absoluter Pfad 
	 * mit dem regülaren Muster pattern übereinstimmt.
	 */
	public static Filter createNameFilter(String pattern) {
		return new NameFilter(pattern);
	}
	
	/*
	 * Erzeugt Filter, der nur Dateien akzeptiert, deren Göße in Bytes 
	 * mit dem Wert size in einer Beziehung stehen.
	 * createSizeFilter(Compare.HIGHER, 100) erzeugt einen Filter, der
	 * nur Dateien passieren läßt, deren Größe 101 Bytes oder mehr beträgt.
	 */
	public static Filter createSizeFilter(Compare cmp, long size) {
		return new SizeFilter(cmp, size);
	}
	
	/*
	 * Erzeugt Filter, der nur Dateien akzeptiert, die sowohl 
	 * die Filter left und right passieren. 
	 */
	public static Filter createANDFilter(Filter left, Filter right) {
		return new AndFilter(left, right);
	}
	
	/*
	 * Erzeugt Filter, der nur Dateien akzeptiert, die mindestens 
	 * einen Filter, entweder left oder right, oder beide passieren. 
	 */
	public static Filter createORFilter(Filter left, Filter right) {
		return new OrFilter(left, right);
	}
	
	/*
	 * 
	 * Test Driver.
	 * 
	 */
	public static void main(String[] args) throws IOException, ParseException {
		Filter filter;
		List<Path> list;
		
		// liefert alle .java Dateien
//		filter = createNameFilter(".*\\.txt");
		filter = createANDFilter(
					createNameFilter(".*\\.txt"),
					createSizeFilter(Compare.HIGHER, 200));
		
		// liefert alle .java Dateien im strings Verzeichnis
//		filter = createNameFilter(".*\\\\strings\\\\[^\\\\]+\\.java");
		
		// liefert alle .java Dateien im strings Verzeichnis , die größer als 2000 Bytes sind.
//		filter = createANDFilter(
//					createNameFilter(".*\\\\strings\\\\[^\\\\]+\\.java"),
//					createSizeFilter(Compare.HIGHER, 2000));
		
		// liefert alle .java Dateien im strings Verzeichnis , die größer als 2000 Bytes sind.
		// und alle .txt Dateien
//		filter = createORFilter(
//					createANDFilter(
//						createNameFilter(".*\\\\strings\\\\[^\\\\]+\\.java"),
//						createSizeFilter(Compare.HIGHER, 3000)),
//					createANDFilter(
//							createNameFilter(".*\\.txt$"),
//							createSizeFilter(Compare.HIGHER, 200)));
		
//		list = selectFiles(Paths.get(""), filter);
//		
//		for (Path p : list) {
//			System.out.println(p.toAbsolutePath() + " (size=" + Files.size(p) + " bytes)");
//		}
		
		list = selectFiles(Paths.get(""), "size>200&&name=(.*\\.txt)||(size>3000&&name=(.*\\\\strings\\\\[^\\\\]+\\.java))");
		for (Path p : list) {
			System.out.println(p.toAbsolutePath() + " (size=" + Files.size(p) + " bytes)");
		}

	}

}
