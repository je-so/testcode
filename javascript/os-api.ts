
import * as fs from "fs";
import * as path from "path";
import { FilePathMatcher } from "./filepathmatcher";

async function scanDir(dirpath: string, matchers: FilePathMatcher[]): Promise<string[]>
{
  return new Promise<string[]>( (resolve, reject) => {
    fs.readdir(dirpath, function (err, files) {
      if (err) {
        reject(new Error(`Unable to scan directory '${dirpath}': ${err}`))
      } else {
        Promise.all( files.map((filename): Promise<string[]|undefined> => {
          const filepath = path.join(dirpath, filename)
          return new Promise<string[]|undefined>( (resolve, reject) => {
            fs.stat(filepath, (err, stats) => {
              if (err) {
                reject(new Error(`Unable to get status of file '${filepath}': ${err}`))
              } else {
                let result:Promise<string[]>|string[]|undefined = undefined
                if (stats.isFile()) {
                  if (matchers.some( (matcher) => matcher.matchFilename(filename))) {
                    result=[filepath]
                  }
                } else if (stats.isDirectory()) {
                  let subdirMatchers: FilePathMatcher[] = []
                  for (const matcher of matchers) {
                    const subdirMatcher = matcher.matchSubDirectory(filename)
                    if (subdirMatcher) {
                      subdirMatchers.push(subdirMatcher)
                    }
                  }
                  if (subdirMatchers.length) {
                    result=scanDir(filepath, subdirMatchers)
                  }
                }
                resolve(result)
              }
            })
          })
        })).then( (files) => {
          let all: string[] = []
          for (const file of files)
            if (file && file.length)
              all = all.concat(file)
          resolve(all)
        }).catch( (err) => {
          reject(err)
        });
      }
    })
  })
}

export const OS_API = {
  FilePathMatcher,
  scanDir,
}
