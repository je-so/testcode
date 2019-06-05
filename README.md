Testcode
========

This repository contains unrelated stuff.

Parser
------
Prototype algorithm _Parsing with Context_ implemented
which parses simple expressions.
See [context_oriented_parser.c](parser/context_oriented_parser.c).

See [regex-main.c](parser/automat/main.c) for usage of the regex-parser which supports operators and(&) and and-not(&!)
<br> Example: `"[a-zA-Z0-9_]+ &! [0-9].*"` The trick to implement this
is to build the deterministic version for the left and right hand side of `&!` and combine the results
[`makedfa2_automat(ndfa, OP_AND_NOT, ndfa2)`](parser/automat/automat.c#L2913).

iperf
-----
**Measure performance of multiple threads/processes.**

See directory [iperf/](iperf/).

Sudoku Solver
-------------
Jump to directory [sudoku/](old-projects/sudoku).

JavaScript 
----------
[Test Scheduler](https://htmlpreview.github.io/?https://github.com/je-so/testcode/blob/master/html/syncrun.html)

[Test Framework in 130 LOC](https://github.com/je-so/testcode/blob/master/javascript/test.jsm)

Use TEST( expression, "description") to test an expression for true and TEST.runTest to run a test.
The tests are part of javascript-modules and not separated from them. A later step must remove this code before
using it in production. The test module tests itself with its own framework. Testing adds another 400 LOC.

OS API
-------------
* How to read and check a user/password in a terminal on Linux [checkpass.c](checkpass.c).


License
-------

All source code is licensed under the GNU GENERAL PUBLIC LICENSE Version 3.

Feel free to use it.
