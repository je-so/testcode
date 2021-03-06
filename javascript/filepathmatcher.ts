
/**
 * Verwaltet ein Dateipfadmuster (Pattern) der Form "<subdir1>/<subdir2>/.../<filename.ext>".
 * Und zusätzlich welcher Anteil des Pfads schon gegen gefundene Dateisystem-Verzeichniseinträge
 * abgeglichen wurde.
 *
 * Das Muster kennt ein spezielles Zeichen '*', das null, eines oder mehrere Zeichen
 * außer '/' erkennt. Das Muster "*.jpg" erkennt etwa alle JPEG-Bilder und "*test*"
 * alle Dateinamen wie "test", "...test" und "...test..." usw.
 *
 * Das Unterverzeichnismuster '*' und '/' erkennt ein Unterverzeichnis beliebigen Namens.
 * Das spezielle Unterverzeichnismuster '**' und '/' erkennt null, eines oder mehrere
 * Unterverzeichnisebenen beliebigen Namens.
 *
 */
export class FilePathMatcher {
  // Das eigentliche Dateipfadmuster aufgeteilt in Abschnitte zwischen den einzelnen "/".
  // Die "/" sind nicht meht enthalten.
  // Das Unterverzeichnismuster "**/", das beliebige Unterverzeichnisse erkennt, ist auch nicht
  // mehr enthalten. Falls vor der i-ten Sektion ein oder mehrere "**/" gestanden hätten,
  // ist afterSubdirMatch[i] == true, sonst ist afterSubdirMatch[i] == false.
  readonly patternSections: string[]
  // Falls afterSubdirMatch[i] == true, dann gingen patternSections[i] ein oder mehrere "**/"
  // im Dateipfadmuster voraus, die jedoch nicht mehr in patternSections gespeichert sind.
  readonly afterSubdirMatch: boolean[]
  // Zeigt auf einen Patternabschnitt in patternSections[i], falls sectionPos[i] == true.
  // Die vor i im Array patternSections vorkommenden Sektionen wurden alle schon erkannt.
  // Da es sich um die Simulation eines NDA handelt, müssen mehrere Indizes unterstützt werden.
  readonly sectionPos: boolean[]
  // Ist true, falls alle Teilpfade, die dem Dateinamensmuster vorausgehen, schon erkannt wurden.
  readonly couldMatchFilename: boolean

  /*
     patterns not supported which
     1. starts with "./", "../", or "/"
     2. contains "/./" or "/../" anywhere
  */
  constructor(pattern: string | { patternSections: string[], afterSubdirMatch: boolean[], sectionPos: boolean[]}) {
    if (typeof pattern == "string") {
      let patternSections: string[] = []
      let afterSubdirMatch: boolean[] = []
      const sections = pattern.split("/")
      if (sections[sections.length-1] === "**")
        sections[sections.length-1] = "*"
      let afterSubdir = false
      let nextSectionIndex = 0
      for (const section of sections) {
        if ("" === section) continue
        if ("**" === section) {
          afterSubdir=true
        } else {
          patternSections[nextSectionIndex] = section
          afterSubdirMatch[nextSectionIndex++] = afterSubdir
          afterSubdir = false
        }
      }
      this.patternSections = patternSections
      this.afterSubdirMatch = afterSubdirMatch
      this.sectionPos = [true]
    } else {
      this.patternSections = pattern.patternSections
      this.afterSubdirMatch = pattern.afterSubdirMatch
      this.sectionPos = pattern.sectionPos
    }
    let couldMatchFilename = false;
    for (let pos in this.sectionPos) {
      if (this.patternSections.length-1 === parseInt(pos)) {
        couldMatchFilename = true;
        break
      }
    }
    this.couldMatchFilename = couldMatchFilename
  }

  /**
   * Überprüft Abschnitts-Muster gegen einen Datei- / Verzeichnis-namen.
   * @param pattern Das zu erkennende Muster.
   * @param name Der Name des Dateinamens bzw. des Verzeichnisses. Name darf kein '/' enthalten.
   * @returns Gibt die maximale Position des erkannten Musters zurück.
   */
  matchSection(pattern: string, name: string): number {
    let endOfStarPos: number[] = []
    let patternPos: number[] = []
    patternPos[0] = 0
    for (const ch of name) {
      if ('/' === ch) return -1
      let newPatternPos: number[] = []
      for (let pos of patternPos) {
        if (pos >= pattern.length) continue
        if ('*' == pattern[pos]) {
          do {
            ++pos
          } while (pos < pattern.length && '*' == pattern[pos])
          endOfStarPos[pos] = pos
          if (pos >= pattern.length) continue
        }
        if ("\\" === pattern[pos] && pos+1 < pattern.length)
          ++pos // escaped character in pattern
        if (ch !== pattern[pos]) continue
        ++pos
        newPatternPos[pos] = pos
      }
      for (const pos of endOfStarPos) {
        newPatternPos[pos] = pos
      }
      patternPos = newPatternPos
    }
    let maxpos = -1
    for (const pos of patternPos) {
      if (pos > maxpos) {
        maxpos = pos
        while (maxpos < pattern.length && '*' == pattern[maxpos])
          ++maxpos;
      }
    }
    return maxpos
  }

  /**
   * Überprüft das nächste Abschnitts-Muster (ohne '/') gegen einen Verzeichnis-namen.
   * @param dirname Name eines unterverzeichnisses. Der Name darf kein '/' enthalten.
   * @returns Gibt einen neuen Matcher zurück, falls das Unterverzeichnis erkannt wurde, sonst null.
   */
  matchSubDirectory(dirname: string): FilePathMatcher | null {
    let newSectionPos: boolean[] = []
    for (let pos in this.sectionPos) {
      const posnum = parseInt(pos)
      if (this.afterSubdirMatch[pos])
        newSectionPos[pos] = true
      if (posnum >= this.patternSections.length-1/*last section matches no subdirectory*/) continue
      const section = this.patternSections[pos]
      const len = this.matchSection(section, dirname)
      if (len < section.length) continue
      newSectionPos[posnum+1] = true
    }
    if (newSectionPos.length)
      return new FilePathMatcher({ patternSections: this.patternSections, afterSubdirMatch: this.afterSubdirMatch, sectionPos: newSectionPos})
    else
      return null
  }

  /**
   * Überprüft den Dateinamen filename gegen den letzten Abschnitt des Pfadausdrucks, wenn möglich.
   * Wurden die davorstehenden Teilpfade noch nicht erkannt, wird immer false zurückgegeben.
   * @param filename Name der Datei, die erkannt werden soll. Der Name darf kein '/' enthalten.
   * @returns Gibt true zurück, wenn filename erkannt wurde.
   */
  matchFilename(filename: string): boolean {
    if (!this.couldMatchFilename)
      return false
    const pattern = this.patternSections[this.patternSections.length-1]
    const len = this.matchSection(pattern, filename)
    return len >= pattern.length
  }

}

// let m1 = new FilePathMatcher("C-kern/**/api/**/*.h")
// console.log("m1",m1)
// let m2 = m1.matchSubDirectory("C-kern")
// console.log("m2",m2)
// let m3 = m2.matchSubDirectory("ds")
// console.log("m3",m3)
