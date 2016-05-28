Testcode
========

This repository contains unrelated stuff.

Parser
------
Prototype algorithm _Parsing with Context_ implemented
which parses simple expressions.
See [context_oriented_parser.c](parser/context_oriented_parser.c).

For a regular exression parser matcher which allows to and(&) and and-not(&!) regular expressions see [regex-main.c](parser/automat/main.c) <br> Example: `"[a-zA-Z0-9_]+ &! [0-9].*"` 

iperf
-----
**Measure performance of multiple threads/processes.**

See directory [iperf/](iperf/).

Sudoku Solver
-------------
Jump to directory [sudoku/](old-projects/sudoku).

OS API
-------------
* How to read and check a user/password in a terminal on Linux [checkpass.c](checkpass.c).

License
-------

All source code is licensed under the GNU GENERAL PUBLIC LICENSE Version 3.

Feel free to use it.
