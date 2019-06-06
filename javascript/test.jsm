
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
   CONFORMANCE: "conformance-test",
   INTEGRATION: "integration-test",
   PERFORMANCE: "performance-test",
   UNIT: "unit-test",
   USER: "user-test",
   REGRESSION: "regression-test",
   functionName: function(type) {
      const map = {
         CONFORMANCE: "CONFORMANCE_TEST",
         INTEGRATION: "INTEGRATION_TEST",
         PERFORMANCE: "PERFORMANCE_TEST",
         UNIT:        "UNIT_TEST",
         USER:        "USER_TEST",
         REGRESSION:  "REGRESSION_TEST",
      }
      return map[type]
   }
}

// public type -- Proxy

const Proxy = {
   calls: [], // every entry stores information about a single function call
              // every entry is of type: [this,"functionname",[function-arguments]]
   id: 0,
   reset: function() {
      id=0
      Proxy.calls = []
      Proxy.calls2 = []
   },
   getOwnFunctionNames(...objects) {
      var funcnames = []
      for (var obj of objects)
         for (var name of Object.getOwnPropertyNames(obj))
            if (typeof obj[name] === "function")
               funcnames.push(name)
      return funcnames
   },
   createProxy: function(functionNamesArray) {
      var proxy = Object.create( { id: ++Proxy.id, returnValues: [] } )
      for (var functionName of functionNamesArray)
         eval(`proxy[functionName] = function (...args) { Proxy.addFunctionCall(this,'${functionName}',args); return Proxy.getReturnValue(this,'${functionName}'); }`)
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
   addFunctionCall(proxy,name,args) {
      Proxy.calls.push( [proxy,name,args] )
   },
   compareCalls(calls) {
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
            return `calls[${i}][2]: expect nr of parameters #${calls[i][2].length} instead of #${Proxy.calls2[i][2].length}`
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
   TEST.proxy = Proxy // TODO: test
   TEST.stats = Stats
   TEST.testedValues = []
   TEST.types = TestTypes
   TEST.stats.reset()
}

TEST.runTest = function(type,name,testfunc) {
   TEST.logger.startTest(TEST,type,name)
   TEST.stats.startTest()
   testfunc(TEST)
   if (TEST.stats.nrErrors)
      TEST.logger.endTestError(TEST,type,name,TEST.stats.nrErrors)
   else
      TEST.logger.endTestOK(TEST,type,name)
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
      const console_proxy = {
         calls: [],
         log: function(str) {
            this.calls.push( [ "log", str.toString() ])
         },
         error: function(str) {
            this.calls.push( [ "error", str.toString() ])
         },
      }
      function prepare_proxy() {
         console_proxy.calls = []
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

            TEST( V(console_proxy.calls.length) == 1+nrValues, "error calls console output "+(1+nrValues)+" times")

            for (var i=0; i<=nrValues; ++i) {
               TEST( V(console_proxy.calls[0][0]) == "log", "error calls only log as output function")
            }

            TEST( V(console_proxy.calls[0][1]).indexOf("TEST failed: 123-description-456") > 0, "error logs error object first")

            for (var i=0; i<nrValues; ++i) {
               TEST( V(console_proxy.calls[1+i][1]) == "   Value used in TEST >"+(3*i)+"<", "error writes tested variable to log #"+i)
            }
         }

         // test function 'startTest'

         // call function 'startTest' under test
         prepare_proxy()
         logger.startTest(TEST,"test-type-x","test-name-y")
         unprepare_proxy()

         TEST( V(console_proxy.calls.length) == 1, "startTest calls console output only once")

         TEST( V(console_proxy.calls[0][0]) == "log", "startTest calls only log as output function")

         TEST( V(console_proxy.calls[0][1]) == "Run test-type-x test-name-y: ... start ...", "startTest writes a start message to log")

         // test function 'endTestOK'

         // call function 'endTestOK' under test
         prepare_proxy()
         logger.endTestOK(TEST,"test-type-x","test-name-y")
         unprepare_proxy()

         TEST( V(console_proxy.calls.length) == 1, "endTestOK calls console output only once")

         TEST( V(console_proxy.calls[0][0]) == "log", "endTestOK calls only log as output function")

         TEST( V(console_proxy.calls[0][1]) == "Run test-type-x test-name-y: OK.", "endTestOK writes OK to the log")

         // test function 'endTestError'
         for (var nrErrors=0; nrErrors<10; ++nrErrors) {

            // call function 'endTestError' under test
            prepare_proxy()
            logger.endTestError(TEST,"type-"+nrErrors,"name-"+nrErrors,nrErrors)
            unprepare_proxy()

            TEST( V(console_proxy.calls.length) == 1, "endTestError calls console output only once")

            TEST( V(console_proxy.calls[0][0]) == "error", "endTestError calls error as output function in case of any errors")

            TEST( V(console_proxy.calls[0][1]) == "Run type-"+nrErrors+" name-"+nrErrors+": "+nrErrors+" errors detected.", "endTest logs an error statistics message #"+nrErrors)
         }

         // test function 'showStats'
         for (var nrExecutedTests=0; nrExecutedTests<10; ++nrExecutedTests) {
            for (var nrFailedTests=0; nrFailedTests<nrExecutedTests; ++nrFailedTests) {

               // call function 'showStats' under test
               prepare_proxy()
               logger.showStats(TEST,nrExecutedTests,nrFailedTests)
               unprepare_proxy()

               TEST( V(console_proxy.calls.length) == 2, "showStats calls console output twice")

               if (nrFailedTests == 0) {
                  TEST( V(console_proxy.calls[1][0]) == "log", "showStats calls log as output function")

                  TEST( V(console_proxy.calls[1][1]) == "Every test passed", "showStats logs passed tests #"+nrExecutedTests)
               } else {
                  TEST( V(console_proxy.calls[1][0]) == "error", "showStats calls error as output function")

                  TEST( V(console_proxy.calls[1][1]) == "Number of tests failed: "+nrFailedTests, "showStats logs number of failed tests #"+nrExecutedTests+","+nrFailedTests)
               }

               TEST( V(console_proxy.calls[0][0]) == "log", "showStats calls log as output function")

               TEST( V(console_proxy.calls[0][1]) == "Number of tests executed: "+nrExecutedTests, "showStats logs number of executed tests #"+nrExecutedTests)
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

         TEST( V(TEST.types[type]) === type.toLowerCase()+"-test", "Test name should conform to convention: "+type)

         TEST( V(TEST.types.functionName(type)) === type+"_TEST", "Mapped function name should conform to convention: "+type)
      }
   }

   function test_TEST() {
      const old_logger = TEST.logger
      const old_stats = TEST.stats
      const logger_proxy = TEST.proxy.createProxy(TEST.proxy.getOwnFunctionNames(TEST.logger))
      const stats_proxy = TEST.proxy.createProxy(TEST.proxy.getOwnFunctionNames(TEST.stats))
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
         TEST.stats = undefined
         TEST.testedValues = undefined
         TEST.types = undefined
         TEST.reset()
         var testedValues = TEST.testedValues
         TEST.testedValues = []

         TEST( V(TEST.logger) == ConsoleLogger && V(TEST.stats) == Stats && V(testedValues.length) == 0
            && V(TEST.types) == TestTypes,
            "Test.reset resets data fields to standard values")

         TEST( V(TEST.stats.nrExecutedTests()) == 0, "Test.reset resets also stats")
         Object.assign(TEST.stats,old_stats_values)

         for (var nrErrors=0; nrErrors<=2; ++nrErrors) {
            // call function 'runTest' under test
            prepare_proxy()
            TEST.runTest("test-type","test-name", function(T) {
               TEST.proxy.addFunctionCall(T,"function(T)",[T])
               TEST.stats.nrErrors = nrErrors
            })
            unprepare_proxy()

            if (nrErrors == 0) {
               TEST( V(TEST.proxy.compareCalls([
                  [logger_proxy,"startTest",[TEST,"test-type","test-name"]],
                  [stats_proxy,"startTest",[]],
                  [TEST,"function(T)",[TEST]],
                  [logger_proxy,"endTestOK",[TEST,"test-type","test-name"]],
                  [stats_proxy,"endTest",[]],
               ])) == "", "runTest calls certain logger function in case of no error #"+nrErrors)
            } else {
               TEST( V(TEST.proxy.compareCalls([
                  [logger_proxy,"startTest",[TEST,"test-type","test-name"]],
                  [stats_proxy,"startTest",[]],
                  [TEST,"function(T)",[TEST]],
                  [logger_proxy,"endTestError",[TEST,"test-type","test-name",nrErrors]],
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


