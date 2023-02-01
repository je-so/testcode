
import * as path from "https://deno.land/std/path/mod.ts";
import { FilePathMatcher } from "./filepathmatcher.ts";

async function scanDir(dirpath: string, matchers: FilePathMatcher[]): Promise<string[]> {
    try {
      let result: string[] = []
      let subdirResult: Promise<string[]>[] = []
      for await (const dirEntry of Deno.readDir(dirpath)) {
        const filename = dirEntry.name
        const filepath = path.join(dirpath, filename)
        if (dirEntry.isFile) {
          if (matchers.some( (matcher) => matcher.matchFilename(filename))) {
            result.push(filepath)
          }
        } else if (dirEntry.isDirectory) {
          let subdirMatchers: FilePathMatcher[] = []
          for (const matcher of matchers) {
            const subdirMatcher = matcher.matchSubDirectory(filename)
            if (subdirMatcher) {
              subdirMatchers.push(subdirMatcher)
            }
          }
          if (subdirMatchers.length) {
            subdirResult.push(scanDir(filepath, subdirMatchers))
          }
        }
      }
      for await (const result2 of subdirResult)
        result=result.concat(result2)
      return result
    } catch (e) {
      throw new Error(`Unable to scan directory '${dirpath}': ${e}`)
    }
}

export const OS_API = {
  FilePathMatcher,
  scanDir,
}
