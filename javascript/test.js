// internal type -- TestedValue

function TestedValue(value,name) {
   this.value=value
   this.name=name
}

TestedValue.prototype.valueOf = function () {
   return this.value
}

// internal type -- ConsoleLogger

const ConsoleLogger = {
   console: console,
   failedTest: function(TEST,description,testedValues/*array of {name,value} pairs where some of them has been unexpected*/) {
      this.console.log(new Error(`TEST failed: ${description}`))
      for(const v of testedValues) {
         this.console.log(`   Possibly unexpected value >${v.value}< (${v.name?v.name:""})`)
      }
   },
   passedTest: function(TEST) {
      this.console.log("OK")
   },
   start: function(TEST,type,name) {
      this.console.log(`Run ${type} ${name}: ... start ...`)
   },
   end: function(TEST,type,name) {
      if (TEST.stats.failedCount)
         this.console.error(`Run ${type} ${name}: ${TEST.stats.failedCount} test${TEST.stats.failedCount!==1?"s":""} failed out of ${TEST.stats.passedCount+TEST.stats.failedCount}.`)
      else
         this.console.log(`Run ${type} ${name}: ${TEST.stats.passedCount} tests passed.`)
   },
}

// public type -- Stats

const Stats = {
   passedCount: 0,
   failedCount: 0,
   reset: function() {
      this.passedCount = 0
      this.failedCount = 0
   },
   passedTest: function() {
      ++this.passedCount
   },
   failedTest: function() {
      ++this.failedCount
   }
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
      TEST.stats.failedTest()
      TEST.logger.failedTest(TEST,description,TEST.testedValues)
   } else {
      TEST.stats.passedTest()
      TEST.logger.passedTest(TEST)
   }
   TEST.testedValues = []
}

TEST.testedValues = []

TEST.Value = function(value,name) {
   // encapsulates variable value which is tested for having certain value
   var v = new TestedValue(value,name)
   TEST.testedValues.push(v)
   return v
}

TEST.start = function(type,name) {
   TEST.stats.reset()
   TEST.logger.start(TEST,type,name)
}

TEST.end = function(type,name) {
   TEST.logger.end(TEST,type,name)
}

TEST.setLogger = function(logger) {
   TEST.logger = logger
}

TEST.logger = ConsoleLogger

TEST.stats = Stats

TEST.types = TestTypes


// -- Test Section --

export function UNIT_TEST(TEST_) {
   test_Parameter()
   test_TestedValue_primitives()
   test_TestedValue_nonprimitives()
   test_ConsoleLogger()
   test_Stats()
   test_TEST()
   
   function test_Parameter() {
      TEST( TEST_ == TEST,"1st parameter passed to UNITTEST is TEST")
   }

   function test_TestedValue_primitives() {
      // comparison operators with TestedValue and primitive types works cause of overwriting valueOf
      var v;
      for(let val=-5; val<=5; ++val) {
         v = new TestedValue(val,"name-"+val)
         new TestedValue(v.value,"value of TestedValue:"+val)
         TEST( v.value == val, "TestedValue Constructor")
         new TestedValue(v.name,"name of TestedValue:"+val)
         TEST( v.name == "name-"+val, "TestedValue Constructor")
         new TestedValue(v.valueOf(),"valueOf of TestedValue:"+val)
         TEST( v == val, "TestedValue.valueOf")
      }
      v = new TestedValue("string-value","string-type")
      TEST( v == "string-value", "TestedValue.valueOf type string")
      v = new TestedValue(true,"true-type")
      TEST( v == true, "TestedValue.valueOf type boolean")
      v = new TestedValue(false,"false-type")
      TEST( v == false, "TestedValue.valueOf type boolean")
      v = new TestedValue(0.123,"float-type")
      TEST( v == 0.123, "TestedValue.valueOf type float")
   }

   function test_TestedValue_nonprimitives() {
      // need to access value explicitly for null,undefined and object types
      var v = new TestedValue(null,"null-type")
      TEST( v.value == null, "TestedValue with type null")
      v = new TestedValue(undefined,"undefined-type")
      TEST( v.value == undefined, "TestedValue with type undefined")
      var o = { name: "Jo Bar" }
      v = new TestedValue(o,"object-type")
      TEST( v.value == o, "TestedValue with type object")
      var f = function() { return }
      v = new TestedValue(f,"function-type")
      TEST( v.value == f, "TestedValue.valueOf type function")
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
         TEST.stats.reset()
      }
      try {
         var logger = TEST.logger, v, testedValues
         TEST( ConsoleLogger == TEST.logger, "ConsoleLogger is default logger")
         TEST( console == TEST.logger.console, "ConsoleLogger writes to console")

         // test function 'failedTest'
         for (var nrValues=0; nrValues<10; ++nrValues) {
            testedValues = []
            for (var i=0; i<nrValues; ++i) {
               testedValues.push( new TestedValue(3*i, "name"+i) )
            }

            // call function 'failedTest' under test
            prepare_proxy()
            logger.failedTest(TEST,"123-description-456",testedValues)
            unprepare_proxy()

            v = TEST.Value(console_proxy.calls.length, "number calls in iteration #"+nrValues)
            TEST( v == 1+nrValues, "failedTest calls console output x-times")

            for (var i=0; i<=nrValues; ++i) {
               v = TEST.Value(console_proxy.calls[0][0], "log call")
               TEST( v == "log", "failedTest calls only log as output function")
            }

            v = TEST.Value(console_proxy.calls[0][1], "error object message")
            TEST( v.value.indexOf("TEST failed: 123-description-456") > 0, "failedTest logs error object first")

            for (var i=0; i<nrValues; ++i) {
               v = TEST.Value(console_proxy.calls[1+i][1], "tested variable #"+i)
               TEST( v == "   Possibly unexpected value >"+(3*i)+"< ("+("name"+i)+")", "failedTest logs tested variable #"+i)
            }
         }

         // test function 'passedTest'

         // call function 'passedTest' under test
         prepare_proxy()
         logger.passedTest(TEST)
         unprepare_proxy()

         v = TEST.Value(console_proxy.calls.length, "number of calls of console output")
         TEST( v == 1, "passedTest calls console output only once")

         v = TEST.Value(console_proxy.calls[0][0], "log call")
         TEST( v == "log", "passedTest calls only log as output function")

         v = TEST.Value(console_proxy.calls[0][1], "log text")
         TEST( v == "OK", "passedTest logs only OK")

         // test function 'start'

         // call function 'start' under test
         prepare_proxy()
         logger.start(TEST,"test-type-x","test-name-y")
         unprepare_proxy()

         v = TEST.Value(console_proxy.calls.length, "number of calls of console output")
         TEST( v == 1, "start calls console output only once")

         v = TEST.Value(console_proxy.calls[0][0], "log call")
         TEST( v == "log", "start calls only log as output function")

         v = TEST.Value(console_proxy.calls[0][1], "log text")
         TEST( v == "Run test-type-x test-name-y: ... start ...", "start logs a start message")

         // test function 'end'
         for (var nrTest=0; nrTest<10; ++nrTest) {

            for (var nrTestFailed=0; nrTestFailed<=nrTest; ++nrTestFailed) {

               // call function 'end' under test
               prepare_proxy()
               TEST.stats.failedCount = nrTestFailed
               TEST.stats.passedCount = nrTest - nrTestFailed
               logger.end(TEST,"type-"+nrTest,"name-"+nrTest)
               unprepare_proxy()

               v = TEST.Value(console_proxy.calls.length, "number of calls of console output")
               TEST( v == 1, "end calls console output only once")

               if (nrTestFailed) {

                  v = TEST.Value(console_proxy.calls[0][0], "error call")
                  TEST( v == "error", "end calls error as output function in case of failed tests")

                  v = TEST.Value(console_proxy.calls[0][1], "log text")
                  var test_s = nrTestFailed==1?"test":"tests"
                  TEST( v == "Run type-"+nrTest+" name-"+nrTest+": "+nrTestFailed+" "+test_s+" failed out of "+nrTest+".", "end logs an error statistics message #"+nrTest+","+nrTestFailed)
               } else {

                  v = TEST.Value(console_proxy.calls[0][0], "log call")
                  TEST( v == "log", "end calls only log as output function")

                  v = TEST.Value(console_proxy.calls[0][1], "log text")
                  TEST( v == "Run type-"+nrTest+" name-"+nrTest+": "+nrTest+" tests passed.", "end logs a statistics message #"+nrTest)
               }
            }
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

