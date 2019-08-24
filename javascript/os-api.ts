
import * as fs from "fs";
import * as path from "path";
import { FilePathMatcher } from "./filepathmatcher";

//joining path of directory
const dirpath = '.'
let matcher = new FilePathMatcher("C-kern/**/api/**/*.h")

async function scanDir(dirpath: string, matcher: FilePathMatcher): Promise<string[]>
{
  return new Promise<string[]>( (resolve, reject) => {
    fs.readdir(dirpath, function (err, files) {
      if (err) {
        reject(new Error(`Unable to scan directory '${dirpath}': ${err}`))
      } else {
        Promise.all( files.map((filename): Promise<string[]|undefined> => {
          const filepath = path.join(dirpath, filename)
          return new Promise<string[]>( (resolve, reject) => {
            fs.stat(filepath, (err, stats) => {
              if (err) {
                reject(new Error(`Unable to get status of file '${filepath}': ${err}`))
              } else {
                let result:Promise<string[]>|string[]|undefined = undefined
                if (stats.isFile()) {
                  if (matcher.matchFilename(filename)) {
                    result=[filepath]
                  }
                } else if (stats.isDirectory()) {
                  let subdirMatcher: FilePathMatcher = matcher.matchSubDirectory(filename)
                  if (subdirMatcher) {
                    result=scanDir(filepath, subdirMatcher)
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

scanDir(dirpath, matcher).then( files => {
  console.log(files)
}).catch( err => console.log(err))
