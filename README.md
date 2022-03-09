Testcode
========

This repository contains unrelated stuff, and a [reading list](reading-list) in particular [patterns](reading-list/patterns.md), [linux-api](reading-list/linux-api.md), [books](reading-list/books.md).

Parser
------
Prototype algorithm _Parsing with Context_ implemented
which parses simple expressions.
See [context_oriented_parser.c](parser/context_oriented_parser.c).

See [regex-main.c](parser/automat/main.c) for usage of the regex-parser which supports operators and(&) and and-not(&!)
<br> Example: `"[a-zA-Z0-9_]+ &! [0-9].*"` The trick to implement this
is to build the deterministic version for the left and right hand side of `&!` and combine the results
[`makedfa2_automat(ndfa, OP_AND_NOT, ndfa2)`](parser/automat/automat.c#L2913).

JavaScript 
----------
[Test Framework in 110 LOC](https://github.com/je-so/testcode/blob/master/javascript/test.js)
```javascript
<script type="module">
import { RUN_TEST } from "./jslib/test.js"
RUN_TEST(unittest_of_some_module)
function unittest_of_some_module(TEST) {
   TEST(1,"==",1,"no message shown")
   TEST(1,"<",1,"message shown")
   TEST(() => { throw Error("abc") },"throw","abc","test for exception with message abc")
   // use cmpUser to compare objects
   const cmpUser=(o1,o2) => (o1.name === o2.name)
   TEST({name:"JOhn"},cmpUser,{name:"John"},"test for same user (is not)")
   // or use "user"
   TEST.setCompare("user",cmpUser,true/*return value for success*/)
   TEST({name:"JOhn"},"user",{name:"John"},"test for same user")
   // compare array and show `value[2]` in error message
   const a=[5,10,15]
   for (let i=0; i<a.length; ++i)
      TEST(a[i],"==",5*(i+1)+(i==2),"index=2 should cause an error",`[${i}]`)
   // but TEST does array comparison for us
   TEST([[1,2],[3,4,5]],"==",[[1,2],[3,4,6]],"digits 5 and 6 differ")
}
</script>
```

OS API
-------------
* How to read and check a user/password in a terminal on Linux [checkpass.c](checkpass.c).

iperf
-----
**Measure performance of multiple threads/processes.**

See directory [iperf/](iperf/).

Sudoku Solver
-------------
Jump to directory [sudoku/](old-projects/sudoku).


License
-------

All source code is licensed under the GNU GENERAL PUBLIC LICENSE Version 3.

Feel free to use it.
