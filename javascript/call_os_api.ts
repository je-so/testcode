import { OS_API } from "./os-api.ts";

const dirpath = '.'
let matcher = new OS_API.FilePathMatcher("C-kern/api/math/**/*.h")
let matcher2 = new OS_API.FilePathMatcher("C-kern/platform/**/*.c")

OS_API.scanDir(dirpath, [matcher,matcher2]).then( files => {
  console.log(files)
}).catch( err => console.log(err))
