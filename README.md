Testcode
========

This repository contains unrelated stuff and some references to [external articles](reading-list) see [patterns](reading-list/patterns.md), [linux-api](reading-list/linux-api.md).

Current Work
------------
```javascript
   const hiddenComputation=new jslib.modules.flow.FlowFunction("if:hidden")
   hiddenComputation.bind(
      () => { // compute hidden state of tag
         const v=param1.value
         const v2=param3.value
         const result= v2==="3" || v==="1" || v==="2" || v==="4";
         return result
      },
      param1, param3 // subscribe to value change of input parameters
   )
   const tag=jslib.tag
   const text=jslib.tagText
   var table=tag("table",{hidden: hiddenComputation},
      tag("thead",null,
         tag("tr",null,
            tag("th",null,text("head 1")),
            tag("th",null,text("head 2")),
         )//</tr>
      ),//</thead>
      tag("tbody",null,
         tag("tr",null,
            tag("td",null,jslib.tagParse("<u>data 1</u>")),
            tag("td",null,text("data 2")),
         )//</tr>
      ),//</tbody>
   )//</table>
   document.body.appendChild(table)
```   
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
[Test Scheduler](https://htmlpreview.github.io/?https://github.com/je-so/testcode/blob/master/html/syncrun.html)

[Test Framework in 190 LOC](https://github.com/je-so/testcode/blob/master/javascript/test.jsm)
```javascript
    TEST.runTest(TEST.UNIT, "value & button", function (TEST) {
         var V = TEST.Value
         var inputnr = document.querySelector("input[type=number]")
         var button = document.querySelector("input[type=button]")
         inputnr.value = ""
         TEST( V(inputnr.value) == "", "input is empty")
         button.click()
         TEST( V(inputnr.value) == "10", "input is set to minimum value")
    })
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
