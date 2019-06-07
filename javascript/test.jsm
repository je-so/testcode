
// internal type -- ConsoleLogger

const ConsoleLogger = {
   console: console,
   error: function(TEST,description,testedValues/*array of tested values and some of them are unexpected*/) {
      this.console.log(new Error(`TEST failed: ${description}`))
      for(const v of testedValues) {
         this.console.log(`   Value used in TEST >${v}<`)
      }
   },
   startTest: function(TEST,type,name) {
      this.console.log(`Run ${type} ${name}: ... start ...`)
   },
   endTestOK: function(TEST,type,name) {
      this.console.log(`Run ${type} ${name}: OK.`)
   },
   endTestError: function(TEST,type,name,nrErrors) {
      this.console.error(`Run ${type} ${name}: ${nrErrors} errors detected.`)
   },
   showStats: function(TEST,nrExecutedTests,nrFailedTests) {
      this.console.log("Number of tests executed: " + nrExecutedTests)
      if (nrFailedTests)
         this.console.error("Number of tests failed: " + nrFailedTests)
      else
         this.console.log("Every test passed")
   },
}

// internal type -- Stats

const Stats = {
   nrPassedTests: 0,
   nrFailedTests: 0,
   nrErrors: 0,
   reset: function() {
      this.nrPassedTests = 0
      this.nrFailedTests = 0
      this.nrErrors = 0
   },
   error: function() {
      ++this.nrErrors;
   },
   startTest: function() {
      this.nrErrors = 0
   },
   endTest: function() {
      if (this.nrErrors)
         ++this.nrFailedTests
      else
         ++this.nrPassedTests
   },
   nrExecutedTests: function() {
      return this.nrPassedTests + this.nrFailedTests
   }
}

// public type -- TestTypes

const TestTypes = {
   CONFORMANCE: "conformance",
   INTEGRATION: "integration",
   PERFORMANCE: "performance",
   UNIT: "unit",
   USER: "user",
   REGRESSION: "regression",
   userName: function(type) {
      return type+"-test"
   },
   functionName: function(type) {
      return type.toUpperCase()+"_TEST"
   }
}

// public type -- Proxy

const Proxy = {
   calls: [], // every entry stores information about a single intercepted function call
              // every entry is of type: [this,"functionname",[function-arguments]]
   id: 0,
   reset: function() {
      Proxy.id=0; Proxy.calls = []; Proxy.calls2 = [];
   },
   getOwnFunctionNames: function(...objects) {
      var funcnames = []
      for (var obj of objects)
         for (var name of Object.getOwnPropertyNames(obj))
            if (typeof obj[name] === "function")
               funcnames.push(name)
      return funcnames
   },
   create: function(functionNamesArray) {
      var proxy = Object.create( { id: ++Proxy.id, returnValues: [] } )
      for (var functionName of functionNamesArray)
         eval(`proxy[functionName] = function (...args) { Proxy.addFunctionCall([this,'${functionName}',args]); return Proxy.getReturnValue(this,'${functionName}'); }`)
      return proxy
   },
   setReturnValues: function(createdProxy,functionName,returnValuesArray) {
      const parent = Object.getPrototypeOf(createdProxy)
      parent.returnValues[functionName] = returnValuesArray
   },
   getReturnValue: function(createdProxy,functionName) {
      const parent = Object.getPrototypeOf(createdProxy)
      if (parent.returnValues[functionName] && parent.returnValues[functionName].length>0)
         return parent.returnValues[functionName].shift()
   },
   addFunctionCall: function(funccall) {
      Proxy.calls.push( funccall )
   },
   compareCalls: function(calls) {
      Proxy.calls2 = Proxy.calls // backup for debugging
      Proxy.calls = []
      if (calls.length!=Proxy.calls2.length)
         return `expect #${calls.length} number of calls instead of #${Proxy.calls2.length}`
      for (var i=0; i<calls.length; ++i) {
         if (calls[i].length!=3 || typeof calls[i][2].length !== "number")
            return 'calls[${i}]: expect parameter format [ [proxy,"fctname", [paramlist]], ... ]'
         if (Proxy.calls2[i][0] !== calls[i][0])
            return `calls[${i}][0]: expect proxy id:${Object.getPrototypeOf(calls[i][0]).id} instead of id:${Object.getPrototypeOf(Proxy.calls2[i][0]).id}`
         if (Proxy.calls2[i][1] !== calls[i][1])
            return `calls[${i}][1]: expect function '${calls[i][1]}' instead of '${Proxy.calls2[i][1]}'`
         if (Proxy.calls2[i][2].length !== calls[i][2].length)
            return `calls[${i}][2]: expect #${calls[i][2].length} function arguments instead of #${Proxy.calls2[i][2].length}`
         for (var i2=0; i2<calls[i][2].length; ++i2)
            if (Proxy.calls2[i][2][i2] !== calls[i][2][i2])
               return `calls[${i}][2][${i2}]: expect value '${calls[i][2][i2]}' instead of '${Proxy.calls2[i][2][i2]}'`
      }
      return "" // no error
   }
}

// exported module interface -- TEST

export default function TEST(expression,description) {
   // checks that expression is true in case of false an error is printed
   if (!expression) {
      TEST.logger.error(TEST,description,TEST.testedValues)
      TEST.stats.error()
   }
   TEST.testedValues = []
}

TEST.Value = function(value) {
   // encapsulates variable value which is tested for having a certain value
   TEST.testedValues.push(value)
   return value
}

TEST.reset = function() {
   TEST.logger = ConsoleLogger
   TEST.proxy = Proxy
   TEST.stats = Stats
   TEST.testedValues = []
   TEST.types = TestTypes
   TEST.stats.reset()
}

TEST.runTest = function(type,name,testfunc) {
   var usertype = TEST.types.userName(type)
   TEST.logger.startTest(TEST,usertype,name)
   TEST.stats.startTest()
   testfunc(TEST)
   if (TEST.stats.nrErrors)
      TEST.logger.endTestError(TEST,usertype,name,TEST.stats.nrErrors)
   else
      TEST.logger.endTestOK(TEST,usertype,name)
   TEST.stats.endTest()
}

TEST.setDefaultLogger = function() {
   TEST.logger = ConsoleLogger
}

TEST.setLogger = function(logger) {
   TEST.logger = logger
}

TEST.showStats = function() {
   TEST.logger.showStats(TEST,TEST.stats.nrExecutedTests(),TEST.stats.nrFailedTests)
}

TEST.reset()


// -- Test Section --

export function UNIT_TEST(TEST_) {
   const V = TEST.Value

   test_Parameter()
   test_TEST_Value()
   test_ConsoleLogger()
   test_Stats()
   test_TestTypes()
   test_Proxy()
   test_TEST()

   function test_Parameter() {
      TEST( TEST_ == TEST,"1st parameter passed to UNITTEST is TEST")
   }

   function test_TEST_Value() {
      for(let val=-5; val<=5; ++val) {
         TEST( V(val) == val, "TEST.Value returns parameter value with type integer #"+val)
      }

      TEST( V("string-value") == "string-value", "TEST.Value returns parameter value with type string")

      TEST( V(true) == true, "TEST.Value returns parameter value with type boolean true")

      TEST( V(false) == false, "TEST.Value returns parameter value with type boolean false")

      TEST( V(0.123) == 0.123, "TEST.Value returns parameter value with type float")

      TEST( V(null) == null, "TEST.Value returns parameter value with type null")

      TEST( V(undefined) == undefined, "TEST.Value returns parameter value with type undefined")

      var o = { name: "Jo Bar" }
      TEST( V(o) == o, "TEST.Value returns parameter value with type object")

      var f = function() { return }
      TEST( V(f) == f, "TEST.Value returns parameter value with type function")

      for(var nrValues=1; nrValues<=10; ++nrValues) {
         for(var i=1; i<=nrValues; ++i) {
            V(i)
         }

         var testedValues = TEST.testedValues
         TEST.testedValues = []
         TEST( V(testedValues.length) == nrValues, "TEST.Value increments length of TEST.testedValues #"+nrValues)

         for(var i=1; i<=nrValues; ++i) {
            TEST( V(testedValues[i-1]) == i, "TEST.Value pushes value to TEST.testedValues #"+i)
         }
      }
   }

   function test_ConsoleLogger() {
      const old_console = TEST.logger.console
      const console_proxy = TEST.proxy.create(["log","error"])
      function prepare_proxy() {
         TEST.logger.console = console_proxy
      }
      function unprepare_proxy() {
         TEST.logger.console = old_console
      }
      try {
         var logger = TEST.logger, v, testedValues
         TEST( ConsoleLogger == TEST.logger, "ConsoleLogger is default logger")
         TEST( console == TEST.logger.console, "ConsoleLogger writes to console")

         // test function 'error'
         for (var nrValues=0; nrValues<10; ++nrValues) {
            testedValues = []
            for (var i=0; i<nrValues; ++i) {
               testedValues.push( 3*i)
            }

            // call function 'error' under test
            prepare_proxy()
            logger.error(TEST,"123-description-456",testedValues)
            unprepare_proxy()

            var calls = [ [console_proxy,"log",[TEST.proxy.calls[0][2][0]]] ]
            for (var i=0; i<nrValues; ++i) {
               calls.push( [console_proxy,"log",[`   Value used in TEST >${3*i}<`]] )
            }

            TEST( V(calls[0][2][0].toString()).indexOf("TEST failed: 123-description-456") > 0, "error logs error message first")

            TEST( V(TEST.proxy.compareCalls(calls)) == "", "error writes tested variables to log #"+nrValues)
         }

         // test function 'startTest'

         // call function 'startTest' under test
         prepare_proxy()
         logger.startTest(TEST,"test-type-x","test-name-y")
         unprepare_proxy()

         TEST( V(TEST.proxy.compareCalls([
               [console_proxy,"log",["Run test-type-x test-name-y: ... start ..."]]
            ])) == "", "startTest calls console output only once")

         // test function 'endTestOK'

         // call function 'endTestOK' under test
         prepare_proxy()
         logger.endTestOK(TEST,"test-type-x","test-name-y")
         unprepare_proxy()

         TEST( V(TEST.proxy.compareCalls([
            [console_proxy,"log",["Run test-type-x test-name-y: OK."]]
         ])) == "", "endTestOK calls console output only once")

         // test function 'endTestError'
         for (var nrErrors=0; nrErrors<10; ++nrErrors) {

            // call function 'endTestError' under test
            prepare_proxy()
            logger.endTestError(TEST,"type-"+nrErrors,"name-"+nrErrors,nrErrors)
            unprepare_proxy()

            TEST( V(TEST.proxy.compareCalls([
               [console_proxy,"error",["Run type-"+nrErrors+" name-"+nrErrors+": "+nrErrors+" errors detected."]]
            ])) == "", "endTestError calls console output only once #"+nrErrors)
         }

         // test function 'showStats'
         for (var nrExecutedTests=0; nrExecutedTests<10; ++nrExecutedTests) {
            for (var nrFailedTests=0; nrFailedTests<nrExecutedTests; ++nrFailedTests) {

               // call function 'showStats' under test
               prepare_proxy()
               logger.showStats(TEST,nrExecutedTests,nrFailedTests)
               unprepare_proxy()

               if (nrFailedTests == 0) {
                  TEST( V(TEST.proxy.compareCalls([
                     [console_proxy,"log",["Number of tests executed: "+nrExecutedTests]],
                     [console_proxy,"log",["Every test passed"]],
                  ])) == "", "showStats calls console output twice #"+nrExecutedTests+","+nrFailedTests)
               } else {
                  TEST( V(TEST.proxy.compareCalls([
                     [console_proxy,"log",["Number of tests executed: "+nrExecutedTests]],
                     [console_proxy,"error",["Number of tests failed: "+nrFailedTests]],
                  ])) == "", "showStats calls console output twice #"+nrExecutedTests+","+nrFailedTests)
               }
            }
         }

      } finally {
         unprepare_proxy()
      }
   }

   function test_Stats() {
      var stats = Object.assign({}, TEST.stats)

      TEST( V(TEST.stats) == Stats, "Stats implements TEST.stats")

      stats.nrPassedTests = 1
      stats.nrFailedTests = 1
      stats.nrErrors = 1
      stats.reset()
      TEST( (V(stats.nrPassedTests) == 0 && V(stats.nrFailedTests) == 0 && V(stats.nrErrors) == 0),
         "stats.reset() sets counters to 0")

      for (var i=1; i<100; ++i) {
         stats.error()
         TEST( V(stats.nrErrors) == i, "stats.error() increments error counter: "+i)
      }

      stats.nrPassedTests = 3
      stats.nrFailedTests = 4
      stats.startTest()
      TEST( V(stats.nrErrors) == 0 && V(stats.nrPassedTests) == 3 && V(stats.nrFailedTests) == 4,
         "stats.startTest() sets error counter to 0 and does not change other counters")

      for (var i=1; i<=10; ++i) {
         stats.nrPassedTests = 5+i
         stats.nrFailedTests = 6+i
         stats.nrErrors = 0
         stats.endTest()
         TEST( V(stats.nrErrors) == 0 && V(stats.nrPassedTests) == 6+i && V(stats.nrFailedTests) == 6+i,
            "stats.endTest() increments passed counter #"+i)

         stats.nrPassedTests = 5+i
         stats.nrFailedTests = 6+i
         stats.nrErrors = i
         stats.endTest()
         TEST( V(stats.nrErrors) == i && V(stats.nrPassedTests) == 5+i && V(stats.nrFailedTests) == 7+i,
            "stats.endTest() increments failed counter #"+i)
      }

      for (var nrPassed=0; nrPassed<10; ++nrPassed) {
         for (var nrFailed=0; nrFailed<10; ++nrFailed) {
            stats.nrPassedTests = nrPassed
            stats.nrFailedTests = nrFailed
            stats.nrErrors = nrPassed - 3

            TEST( V(stats.nrExecutedTests()) == nrPassed + nrFailed, "stats.nrExecutedTests() returns number of all tests #"+nrPassed+","+nrFailed)

            TEST( V(stats.nrPassedTests) == nrPassed && V(stats.nrFailedTests) == nrFailed && V(stats.nrErrors) == nrPassed - 3,
               "stats.nrExecutedTests() does not change state")
         }
      }

   }

   function test_TestTypes() {
      TEST( V(TEST.types) == TestTypes, "TestTypes implements TEST.types")
      TEST( typeof V(TEST.types.functionName) === "function", "TEST.types should export functionName()")

      const Types = [ "CONFORMANCE", "INTEGRATION", "PERFORMANCE", "UNIT", "USER", "REGRESSION" ]
      for (var attr in TEST.types) {
         if (attr == attr.toUpperCase())
            TEST( Types.includes(attr), "TEST.types should offer only tested types: "+attr)
      }

      for (var type of Types) {
         TEST( V(TEST.types)[type] !== undefined, "TEST.types should offer at least type: "+type)

         TEST( V(TEST.types[type]) === type.toLowerCase(), "Test name should conform to convention: "+type)

         TEST( V(TEST.types.functionName(TEST.types[type])) === type+"_TEST", "Mapped function name should conform to convention: "+type)

         TEST( V(TEST.types.userName(TEST.types[type])) === type.toLowerCase()+"-test", "Mapped user name should conform to convention: "+type)
      }
   }

   function test_Proxy() {
      const proxy = TEST.proxy

      TEST( V(proxy) == Proxy, "Proxy is default implementation")

      // call 'reset' under test
      proxy.calls = [1,2,3]
      proxy.calls2 = [1,2,3]
      proxy.id = 123
      proxy.reset()

      TEST( V(proxy.calls.length) == 0 && V(proxy.calls2.length) == 0 && V(proxy.id) == 0,
         "reset initializes all fields")

      let o1 = { a: null, b: undefined, i: 1, x: "", y: {}, f1: function() {} }
      let o2 = { f2: function() {}, f3: function() {return 1;} }
      let o3 = { f4: function(a) {}, f5: function(b,c) {return 1;} }

      for (var i=0; i<3; ++i) {
         var names = i==0?proxy.getOwnFunctionNames(o1):i==1?proxy.getOwnFunctionNames(o1,o2):proxy.getOwnFunctionNames(o1,o2,o3);
         TEST( V(names.length) == 2*i+1 && V(names.includes("f1")), "getOwnFunctionNames exports array of own properties which are of type function")
         for (var i2=2; i2<2*i+2; i2+=2)
            TEST( V(names.includes("f"+i2)) && V(names.includes("f"+(i2+1))), "getOwnFunctionNames exports array of own properties which are of type function #"+i2)
      }

      for (var i=0; i<4; ++i) {
         // call function 'create' under test
         const funcNames=[ "func1", "test", "log", "buyAProduct" ].slice(0,1+i)
         const id = TEST.proxy.id
         const proxy = TEST.proxy.create(funcNames)
         const parent = Object.getPrototypeOf(proxy)
         let props = Object.getOwnPropertyNames(proxy)

         TEST( proxy.id == id+1, "create increments id of proxy #"+i)

         TEST( V(props.length) == funcNames.length, "create builds proxy with x functions #"+i)
         for (var i2=0; i2<funcNames.length; ++i2) {
            TEST( typeof V(proxy[funcNames[i2]]) === "function", "function is named after given parameter #"+i,+","+i2)
         }

         props = Object.getOwnPropertyNames(parent)
         TEST( V(props.length) === 2 && V(parent.id) === id+1 && V(parent.returnValues.length) === 0, "create builds proxy with own prototype #"+i)
      }

      for (var i=0; i<5; ++i) {
         const funcNames=["some","func","names"]
         const values=[ 1, 0.5, "str", { name: "name" }, [1,2,3] ].slice(0,1+i)
         const proxy = TEST.proxy.create(funcNames)
         const parent = Object.getPrototypeOf(proxy)

         for (var i2=0; i2<funcNames.length; ++i2) {
            // call function 'setReturnValues' under test
            const length = Object.getOwnPropertyNames(parent.returnValues).length
            TEST.proxy.setReturnValues(proxy,funcNames[i2],values)

            TEST( V(Object.getOwnPropertyNames(parent.returnValues).length) == length+1 &&
               V(parent.returnValues[funcNames[i2]]) == values,
               "setReturnValues sets reference to array of values and only for a certain function #"+i+","+i2)
         }
      }

      for (var i=0; i<5; ++i) {
         const funcNames=["some","func","names"]
         const values=[ 1, 0.5, "str", { name: "name" }, [1,2,3] ].slice(0,1+i)
         const proxy = TEST.proxy.create(funcNames)
         const parent = Object.getPrototypeOf(proxy)
         for (var i2=0; i2<funcNames.length; ++i2)
            TEST.proxy.setReturnValues(proxy,funcNames[i2],values.slice()/*make independant clones of values array*/)

         for (var i2=0; i2<funcNames.length; ++i2) {
            const returnValues = parent.returnValues[funcNames[i2]]
            for (var i3=0; i3<=i; ++i3) {
               // call function 'getReturnValue' under test
               const returnvalue = TEST.proxy.getReturnValue(proxy,funcNames[i2])

               TEST( V(returnValues.length) == i-i3 && V(returnvalue) == values[i3],
                  "getReturnValue returns first value of array and removes it #"+i+","+i2+","+i3)
            }
         }
      }

      for (var i=0; i<5; ++i) {
         // function 'addFunctionCall' under test
         let fcall = [ [{},"1",[1]], [{},"2",[1,2]], [{},"3",[4]], [{},"4",[5,6]], [{},"5",[7]] ]
         proxy.addFunctionCall(fcall[i])

         TEST( V(proxy.calls.length) == i+1 && V(proxy.calls[i]) === fcall[i], "addFunctionCall pushes parameter to array proxy.calls #"+i)
      }


      // test created proxy intercepts functions
      proxy.reset()
      const p1 = proxy.create(["f1","f2"])
      const p2 = proxy.create(["f3","f4"])
      TEST( V(p1.id) == 1 && V(p2.id) == 2, "create two proxies")
      proxy.setReturnValues(p1,"f1",[1,5])
      proxy.setReturnValues(p1,"f2",[2,6])
      proxy.setReturnValues(p2,"f3",[3,7])
      proxy.setReturnValues(p2,"f4",[4,8])
      for (var i=0; i<=1; ++i) {
         TEST( V(p1.f1()) == 1+4*i, "call p1.f1 #"+i)
         TEST( V(p1.returnValues["f1"].length) ==1-i, "retval removed from f1 #"+i)
         TEST( V(p1.f2(1+4*i)) == 2+4*i, "call p1.f2 #"+i)
         TEST( V(p1.returnValues["f2"].length) ==1-i, "retval removed from f2 #"+i)
         TEST( V(p2.f3(2+4*i,3+4*i)) == 3+4*i, "call p1.f2 #"+i)
         TEST( V(p2.returnValues["f3"].length) ==1-i, "retval removed from f3 #"+i)
         TEST( V(p2.f4(3+4*i,4+4*i,5+4*i)) == 4+4*i, "call p1.f2 #"+i)
         TEST( V(p2.returnValues["f4"].length) ==1-i, "retval removed from f4 #"+i)
      }
      for (var i=0; i<8; ++i) {
         var id = 1+Math.floor(i/2)%2, f = "f"+(1+i%4)
         TEST( V(proxy.calls[i][0].id) == id && V(proxy.calls[i][1]) == f
            && V(proxy.calls[i][2].length) == i%4, "compare call #"+i)
         for (var i2=0; i2<proxy.calls[i][2].length; ++i2) {
            TEST( V(proxy.calls[i][2][i2]) == i+i2, "compare parameter #"+i+","+i2)
         }
      }

      // test compareCalls
      proxy.reset()
      for (var i=0; i<8; ++i) {
         var calls = proxy.calls
         p1.f1(1); p2.f3(2,3,4); p1.f2(1,2,3); p2.f4(2)
         switch (i) {
         case 0: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2]]])) == "", "compareCalls works OK"); break;
         case 1: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p2,"f4",[2]]])) == "expect #3 number of calls instead of #4", "compareCalls expects less calls"); break;
         case 2: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2]] ,[p1,"f1",[1]]])) == "expect #5 number of calls instead of #4", "compareCalls expects more calls"); break;
         case 3: TEST( V(proxy.compareCalls( [ [p2,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2]]])) == "calls[0][0]: expect proxy id:2 instead of id:1", "compareCalls expects another proxy"); break;
         case 4: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f2",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2]]])) == "calls[1][1]: expect function 'f2' instead of 'f3'", "compareCalls expects another function"); break;
         case 5: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,4]], [p2,"f4",[2]]])) == "calls[2][2][2]: expect value '4' instead of '3'", "compareCalls expects another argument"); break;
         case 6: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[]]])) == "calls[3][2]: expect #0 function arguments instead of #1", "compareCalls expects less function call arguments"); break;
         case 7: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2,3]]])) == "calls[3][2]: expect #2 function arguments instead of #1", "compareCalls expects more function call arguments"); break;
         }
         TEST( V(proxy.calls.length) == 0, "compareCalls clears intercepted calls #"+i)
         TEST( V(proxy.calls2) == calls, "compareCalls makes backup for debegging into calls2 #"+i)
      }

   }

   function test_TEST() {
      const old_logger = TEST.logger
      const old_stats = TEST.stats
      const logger_proxy = TEST.proxy.create(TEST.proxy.getOwnFunctionNames(TEST.logger))
      const stats_proxy = TEST.proxy.create(TEST.proxy.getOwnFunctionNames(TEST.stats))
      Object.assign(stats_proxy, { nrFailedTests: 0, nrPassedTests: 0, nrErrors: 0, })
      function prepare_proxy() {
         logger_proxy.calls = []
         stats_proxy.calls = []
         TEST.logger = logger_proxy
         TEST.stats = stats_proxy
      }
      function unprepare_proxy() {
         TEST.logger = old_logger
         TEST.stats = old_stats
      }
      try {

         for (var expr=0; expr<=1; ++expr) {
            for (var nrTestedValues=0; nrTestedValues<5; ++nrTestedValues) {
               for (var i=0; i<nrTestedValues; ++i) {
                  V(i)
               }
               const testedValues = TEST.testedValues

               // call function 'TEST' under test
               prepare_proxy()
               TEST(expr==0,"<DESCRIPTION>")
               unprepare_proxy()

               TEST( V(TEST.testedValues.length) == 0, "TEST() resets testedValues array to length 0")

               if (expr==0) {
                  TEST( V(TEST.proxy.compareCalls([])) == "", "TEST(true) does not change stats")
               } else {
                  TEST( V(TEST.proxy.compareCalls([
                           [logger_proxy,"error",[TEST,"<DESCRIPTION>",testedValues]],
                           [stats_proxy,"error",[]],
                        ])) == "", "TEST(false) does log an error")
               }
            }
         }

         // call function 'reset' under test
         var old_stats_values = Object.assign({}, TEST.stats)
         TEST.logger = undefined
         TEST.proxy = undefined
         TEST.stats = undefined
         TEST.testedValues = undefined
         TEST.types = undefined
         TEST.reset()
         var testedValues = TEST.testedValues
         TEST.testedValues = []

         TEST( V(TEST.logger) == ConsoleLogger && V(TEST.proxy) == Proxy && V(TEST.stats) == Stats
            && V(testedValues.length) == 0 && V(TEST.types) == TestTypes,
            "Test.reset resets data fields to standard values")

         TEST( V(TEST.stats.nrExecutedTests()) == 0, "Test.reset resets also stats")
         Object.assign(TEST.stats,old_stats_values)

         for (var nrErrors=0; nrErrors<=2; ++nrErrors) {
            // call function 'runTest' under test
            prepare_proxy()
            TEST.runTest("test-type","test-name", function(T) {
               TEST.proxy.addFunctionCall([T,"function(T)",[T]])
               TEST.stats.nrErrors = nrErrors
            })
            unprepare_proxy()

            if (nrErrors == 0) {
               TEST( V(TEST.proxy.compareCalls([
                  [logger_proxy,"startTest",[TEST,"test-type-test","test-name"]],
                  [stats_proxy,"startTest",[]],
                  [TEST,"function(T)",[TEST]],
                  [logger_proxy,"endTestOK",[TEST,"test-type-test","test-name"]],
                  [stats_proxy,"endTest",[]],
               ])) == "", "runTest calls certain logger function in case of no error #"+nrErrors)
            } else {
               TEST( V(TEST.proxy.compareCalls([
                  [logger_proxy,"startTest",[TEST,"test-type-test","test-name"]],
                  [stats_proxy,"startTest",[]],
                  [TEST,"function(T)",[TEST]],
                  [logger_proxy,"endTestError",[TEST,"test-type-test","test-name",nrErrors]],
                  [stats_proxy,"endTest",[]],
               ])) == "", "runTest calls certain logger function in case of an error #"+nrErrors)
            }

         }

         TEST.setLogger(undefined)
         TEST( V(TEST.logger) == undefined, "setLogger sets a new logger")

         TEST.setDefaultLogger()
         TEST( V(TEST.logger) == ConsoleLogger, "setDefaultLogger sets ConsoleLogger as default")

         TEST.setLogger(logger_proxy)
         TEST( V(TEST.logger) == logger_proxy, "setLogger sets a new logger")

         TEST.setDefaultLogger()
         TEST( V(TEST.logger) == ConsoleLogger, "setDefaultLogger sets ConsoleLogger as default")

         for (var nrPassed=0; nrPassed<=5; ++nrPassed) {
            for (var nrFailed=0; nrFailed<=5; ++nrFailed) {
               // call function 'showStats' under test
               stats_proxy.nrPassedTests = nrPassed
               stats_proxy.nrFailedTests = nrFailed
               TEST.proxy.setReturnValues(stats_proxy,"nrExecutedTests",[nrPassed+nrFailed])
               prepare_proxy()
               TEST.showStats()
               unprepare_proxy()

               TEST( V(TEST.proxy.compareCalls([
                  [stats_proxy,"nrExecutedTests",[]],
                  [logger_proxy,"showStats",[TEST,nrPassed+nrFailed,nrFailed]],
               ])) == "", "TEST.showStats calls certain logger and stats function #"+nrPassed+","+nrFailed)

            }
         }

      } finally {
         unprepare_proxy()
      }
   }

}


