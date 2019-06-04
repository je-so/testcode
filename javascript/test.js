
// internal type -- ConsoleLogger

const ConsoleLogger = {
   console: console,
   error: function(TEST,description,testedValues/*array of unexpected values at least some of them*/) {
      this.console.log(new Error(`TEST failed: ${description}`))
      for(const v of testedValues) {
         this.console.log(`   Possibly unexpected value >${v}<`)
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
}

// public type -- Stats

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
}

// internal type -- Stats

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
      TEST.logger.endTestOK(TEST,type,name,TEST.stats.nrErrors)
   TEST.stats.endTest()
}

TEST.setLogger = function(logger) {
   TEST.logger = logger
}

TEST.reset()


// -- Test Section --

export function UNIT_TEST(TEST_) {
   const V = TEST.Value

   test_Parameter()
   test_TEST_Value()
   test_ConsoleLogger()
   test_Stats()
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
      TEST( V(o) == o, "TEST.Value with returns parameter value type object")

      var f = function() { return }
      TEST( V(f) == f, "TEST.Value with returns parameter value type function")

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
               TEST( V(console_proxy.calls[1+i][1]) == "   Possibly unexpected value >"+(3*i)+"<", "error writes tested variable to log #"+i)
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

      } finally {
         unprepare_proxy()
      }
   }

   function test_Stats() {

   }

   function test_TEST() {
      const console_proxy = {
         calls: [],
         failedTest: function(TEST,description,testedValues) {
            calls.push( [ "failedTest", TEST, description, testedValues ] )
         },
         passedTest: function(TEST) {
            calls.push( [ "passedTest", TEST ] )
         },
         start: function(TEST,type,name) {
            calls.push( [ "start", TEST, type, name ] )
         },
         end: function(TEST,type,name) {
            calls.push( [ "end", TEST, type, name ] )
         },
      }
   }

}

