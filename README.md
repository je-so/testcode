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
      import { RUN_TEST, TEST } from "./jslib/test.js"
      RUN_TEST(unittest_of_some_module)
      function cmpUser(o1,o2) { return o1.name === o2.name }
      function unittest_of_some_module() {
         TEST(1,"==",1,"This error message is never shown")
         TEST(1,"<",1,"This error message is shown")
         TEST(() => { throw Error("abc") },"throw","abc","Expect exception with message abc")
         TEST(() => { throw Error("abc") ; return 3; },"==",3,"Expect function returning 3")
         TEST({name:"JOhn"},cmpUser,{name:"John"},"Both logged in users must be equal")
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
