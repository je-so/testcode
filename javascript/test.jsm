
// internal type -- ConsoleLogger

const ConsoleLogger = {
   console: console,
   error: function(TEST,description,testedValues/*array of tested values and some of them are unexpected*/) {
      this.console.log(new Error(`TEST failed: ${TEST.getPath()!=""?TEST.getPath()+": ":""}${description}`))
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
   checkParamFunccall: function(funccall,fctname,parname) {
      if (typeof funccall !== "object" || typeof funccall.length !== "number" || funccall.length !== 3)
         throw new Error(`${fctname}: Expect parameter ${parname} of type [] with length 3`)
      if (typeof funccall[0] !== "object" && typeof funccall[0] !== "function")
         throw new Error(`${fctname}: Expect parameter ${parname}[0] of type object|function`)
      if (typeof funccall[1] !== "string")
         throw new Error(`${fctname}: Expect parameter ${parname}[1] of type string`)
      if (typeof funccall[2] !== "object" || typeof funccall[2].length !== "number")
         throw new Error(`${fctname}: Expect parameter ${parname}[2] of type []`)
      return funccall
   },
   addFunctionCall: function(funccall) {
      Proxy.calls.push( Proxy.checkParamFunccall(funccall,"addFunctionCall(p)","p") )
   },
   compareCalls: function(calls) {
      Proxy.calls2 = Proxy.calls // backup for debugging
      Proxy.calls = []
      if (calls.length !== Proxy.calls2.length)
         return `expect #${calls.length} number of calls instead of #${Proxy.calls2.length}`
      for (var i=0; i<calls.length; ++i) {
         Proxy.checkParamFunccall(calls[i],"compareCalls(p)",`p[${i}]`)
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
   // remember tested variable value to print it in case of an error
   TEST.testedValues.push(value)
   return value
}

TEST.reset = function() {
   TEST.logger = ConsoleLogger
   TEST.path = ""
   TEST.proxy = Proxy
   TEST.stats = Stats
   TEST.testedValues = []
   TEST.stats.reset()
   // --- test types
   TEST.ACCEPTANCE  = "acceptance"
   TEST.INTEGRATION = "integration"
   TEST.PERFORMANCE = "performance"
   TEST.REGRESSION = "regression"
   TEST.SYSTEM = "system"
   TEST.UNIT = "unit"
   TEST.USER = "user"
}

TEST.runTest = function(type,name,testfunc) {
   var usertype = TEST.userName(type)
   TEST.logger.startTest(TEST,usertype,name)
   TEST.stats.startTest()
   testfunc(TEST,TEST.Value)
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

TEST.setPath = function(path) {
   TEST.path = path // current path under test, for example "object.subobject.f1()" or "functionX() 'in error state'"
}

TEST.getPath = function() {
   return TEST.path
}

TEST.showStats = function() {
   TEST.logger.showStats(TEST,TEST.stats.nrExecutedTests(),TEST.stats.nrFailedTests)
}

TEST.userName = function(type) {
   return type+"-test"
}

TEST.functionName = function(type) {
   return type.toUpperCase()+"_TEST"
}

TEST.reset()


// -- Test Section --

export function UNIT_TEST(TEST_,V) {

   test_Parameter()
   test_TEST_Value()
   test_ConsoleLogger()
   test_Stats()
   test_Types()
   test_Proxy()
   test_TEST()

   function test_Parameter() {
      TEST( TEST_ == TEST,"1st parameter passed to UNITTEST is TEST")
      TEST( V == TEST.Value,"2nd parameter passed to UNITTEST is TEST.Value")
   }

   function test_TEST_Value() {
      TEST.setPath("TEST.Value()")

      for(let val=-5; val<=5; ++val) {
         TEST( V(val) == val, "returns parameter value with type integer #"+val)
      }

      TEST( V("string-value") == "string-value", "returns parameter value with type string")

      TEST( V(true) == true, "returns parameter value with type boolean true")

      TEST( V(false) == false, "returns parameter value with type boolean false")

      TEST( V(0.123) == 0.123, "returns parameter value with type float")

      TEST( V(null) == null, "returns parameter value with type null")

      TEST( V(undefined) == undefined, "returns parameter value with type undefined")

      var o = { name: "Jo Bar" }
      TEST( V(o) == o, "returns parameter value with type object")

      var f = function() { return }
      TEST( V(f) == f, "returns parameter value with type function")

      for(var nrValues=1; nrValues<=10; ++nrValues) {
         for(var i=1; i<=nrValues; ++i) {
            V(i)
         }

         var testedValues = TEST.testedValues
         TEST.testedValues = []
         TEST( V(testedValues.length) == nrValues, "increments length of TEST.testedValues #"+nrValues)

         for(var i=1; i<=nrValues; ++i) {
            TEST( V(testedValues[i-1]) == i, "pushes value to TEST.testedValues #"+i)
         }
      }
   }

   function test_ConsoleLogger() {
      const old_console = ConsoleLogger.console
      const console_proxy = TEST.proxy.create(["log","error"])
      function prepare_proxy() {
         ConsoleLogger.console = console_proxy
      }
      function unprepare_proxy() {
         ConsoleLogger.console = old_console
      }
      try {
         TEST.setPath("Consolelogger.console")

         var logger = ConsoleLogger, testedValues
         TEST( console == ConsoleLogger.console, "writes to console")

         TEST.setPath("Consolelogger.error()")

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

            TEST( V(calls[0][2][0].toString()).indexOf("TEST failed: Consolelogger.error(): 123-description-456") > 0, "logs error message first")

            TEST( V(TEST.proxy.compareCalls(calls)) == "", "writes tested variables to log #"+nrValues)
         }

         TEST.setPath("Consolelogger.startTest()")

         // call function 'startTest' under test
         prepare_proxy()
         logger.startTest(TEST,"test-type-x","test-name-y")
         unprepare_proxy()

         TEST( V(TEST.proxy.compareCalls([
               [console_proxy,"log",["Run test-type-x test-name-y: ... start ..."]]
            ])) == "", "calls console output only once")

         TEST.setPath("Consolelogger.endTestOK()")

         // call function 'endTestOK' under test
         prepare_proxy()
         logger.endTestOK(TEST,"test-type-x","test-name-y")
         unprepare_proxy()

         TEST( V(TEST.proxy.compareCalls([
            [console_proxy,"log",["Run test-type-x test-name-y: OK."]]
         ])) == "", "calls console output only once")

         TEST.setPath("Consolelogger.endTestError()")

         for (var nrErrors=0; nrErrors<10; ++nrErrors) {

            // call function 'endTestError' under test
            prepare_proxy()
            logger.endTestError(TEST,"type-"+nrErrors,"name-"+nrErrors,nrErrors)
            unprepare_proxy()

            TEST( V(TEST.proxy.compareCalls([
               [console_proxy,"error",["Run type-"+nrErrors+" name-"+nrErrors+": "+nrErrors+" errors detected."]]
            ])) == "", "calls console output only once #"+nrErrors)
         }

         TEST.setPath("Consolelogger.showStats()")

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
                  ])) == "", "calls console output twice #"+nrExecutedTests+","+nrFailedTests)
               } else {
                  TEST( V(TEST.proxy.compareCalls([
                     [console_proxy,"log",["Number of tests executed: "+nrExecutedTests]],
                     [console_proxy,"error",["Number of tests failed: "+nrFailedTests]],
                  ])) == "", "calls console output twice #"+nrExecutedTests+","+nrFailedTests)
               }
            }
         }

      } finally {
         unprepare_proxy()
      }
   }

   function test_Stats() {
      var stats = Object.assign({}, TEST.stats)

      TEST.setPath("TEST.stats")

      TEST( V(TEST.stats) == Stats, "is implemented by Stats")

      TEST.setPath("Stats.reset()")

      stats.nrPassedTests = 1
      stats.nrFailedTests = 1
      stats.nrErrors = 1
      stats.reset()
      TEST( (V(stats.nrPassedTests) == 0 && V(stats.nrFailedTests) == 0 && V(stats.nrErrors) == 0),
         "sets counters to 0")

      TEST.setPath("Stats.error()")

      for (var i=1; i<100; ++i) {
         stats.error()
         TEST( V(stats.nrErrors) == i, "increments error counter: "+i)
      }

      TEST.setPath("Stats.startTest()")

      stats.nrPassedTests = 3
      stats.nrFailedTests = 4
      stats.startTest()
      TEST( V(stats.nrErrors) == 0 && V(stats.nrPassedTests) == 3 && V(stats.nrFailedTests) == 4,
         "sets error counter to 0 and does not change other counters")

      TEST.setPath("Stats.endTest()")

      for (var i=1; i<=10; ++i) {
         stats.nrPassedTests = 5+i
         stats.nrFailedTests = 6+i
         stats.nrErrors = 0
         stats.endTest()
         TEST( V(stats.nrErrors) == 0 && V(stats.nrPassedTests) == 6+i && V(stats.nrFailedTests) == 6+i,
            "increments passed counter #"+i)

         stats.nrPassedTests = 5+i
         stats.nrFailedTests = 6+i
         stats.nrErrors = i
         stats.endTest()
         TEST( V(stats.nrErrors) == i && V(stats.nrPassedTests) == 5+i && V(stats.nrFailedTests) == 7+i,
            "increments failed counter #"+i)
      }

      TEST.setPath("Stats.nrExecutedTests()")

      for (var nrPassed=0; nrPassed<10; ++nrPassed) {
         for (var nrFailed=0; nrFailed<10; ++nrFailed) {
            stats.nrPassedTests = nrPassed
            stats.nrFailedTests = nrFailed
            stats.nrErrors = nrPassed - 3

            TEST( V(stats.nrExecutedTests()) == nrPassed + nrFailed, "returns number of all tests #"+nrPassed+","+nrFailed)

            TEST( V(stats.nrPassedTests) == nrPassed && V(stats.nrFailedTests) == nrFailed && V(stats.nrErrors) == nrPassed - 3,
               "does not change state")
         }
      }

   }

   function test_Types() {
      TEST.setPath("TEST.functionName()")

      TEST( typeof V(TEST.functionName) === "function", "function should be exported")

      TEST.setPath("TEST.userName()")

      TEST( typeof V(TEST.userName) === "function", "function should be exported")


      const Types = [ "ACCEPTANCE", "INTEGRATION", "PERFORMANCE", "SYSTEM", "UNIT", "USER", "REGRESSION" ]
      for (var attr in TEST) {
         TEST.setPath("TEST."+attr)
         if (attr == attr.toUpperCase())
            TEST( Types.includes(V(attr)), "test type not known")
      }

      for (var type of Types) {
         TEST.setPath("TEST."+type)
         TEST( V(TEST[type]) !== undefined, "known type not exported: "+type)
         TEST( V(TEST[type]) === type.toLowerCase(), "test name should conform to convention #"+type)

         TEST.setPath("TEST.functionName()")
         TEST( V(TEST.functionName(TEST[type])) === type+"_TEST", "return value should conform to convention #"+type)

         TEST.setPath("TEST.userName()")
         TEST( V(TEST.userName(TEST[type])) === type.toLowerCase()+"-test", "return value should conform to convention #"+type)
      }
   }

   function test_Proxy() {
      const proxy = TEST.proxy

      TEST.setPath("TEST.proxy")

      TEST( V(proxy) == Proxy, "is implemented by Proxy")

      TEST.setPath("Proxy.reset()")

      // call 'reset' under test
      proxy.calls = [1,2,3]
      proxy.calls2 = [1,2,3]
      proxy.id = 123
      proxy.reset()

      TEST( V(proxy.calls.length) == 0 && V(proxy.calls2.length) == 0 && V(proxy.id) == 0,
         "reset initializes all fields")

      TEST.setPath("Proxy.getOwnFunctionNames()")

      let o1 = { a: null, b: undefined, i: 1, x: "", y: {}, f1: function() {} }
      let o2 = { f2: function() {}, f3: function() {return 1;} }
      let o3 = { f4: function(a) {}, f5: function(b,c) {return 1;} }

      for (var i=0; i<3; ++i) {
         var names = i==0?proxy.getOwnFunctionNames(o1):i==1?proxy.getOwnFunctionNames(o1,o2):proxy.getOwnFunctionNames(o1,o2,o3);
         TEST( V(names.length) == 2*i+1 && V(names.includes("f1")), "exports array of own properties which are of type function")
         for (var i2=2; i2<2*i+2; i2+=2)
            TEST( V(names.includes("f"+i2)) && V(names.includes("f"+(i2+1))), "exports array of own properties which are of type function #"+i2)
      }

      TEST.setPath("Proxy.create()")

      for (var i=0; i<4; ++i) {
         // call function 'create' under test
         const funcNames=[ "func1", "test", "log", "buyAProduct" ].slice(0,1+i)
         const id = TEST.proxy.id
         const proxy = TEST.proxy.create(funcNames)
         const parent = Object.getPrototypeOf(proxy)
         let props = Object.getOwnPropertyNames(proxy)

         TEST( proxy.id == id+1, "increments id of proxy #"+i)

         TEST( V(props.length) == funcNames.length, "builds proxy with x functions #"+i)
         for (var i2=0; i2<funcNames.length; ++i2) {
            TEST( typeof V(proxy[funcNames[i2]]) === "function", "function is named after given parameter #"+i,+","+i2)
         }

         props = Object.getOwnPropertyNames(parent)
         TEST( V(props.length) === 2 && V(parent.id) === id+1 && V(parent.returnValues.length) === 0, "builds proxy with own prototype #"+i)
      }

      TEST.setPath("Proxy.setReturnValues()")

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
               "sets reference to array of values and only for a certain function #"+i+","+i2)
         }
      }

      TEST.setPath("Proxy.getReturnValue()")

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
                  "returns first value of array and removes it #"+i+","+i2+","+i3)
            }
         }
      }

      function getError(func,param) {
         try { proxy[func](param,func+"(p)","p"); } catch(e) { return e.message; }
      }

      TEST.setPath("Proxy.checkParamFunccall()")

      // function 'checkParamFunccall' under test
      TEST( V(getError("checkParamFunccall", {})) == "checkParamFunccall(p): Expect parameter p of type [] with length 3", "checks input parameter")
      TEST( V(getError("checkParamFunccall", [1,2])) == "checkParamFunccall(p): Expect parameter p of type [] with length 3", "checks input arameter")
      TEST( V(getError("checkParamFunccall", [1,2,3])) == "checkParamFunccall(p): Expect parameter p[0] of type object|function", "checks input parameter")
      TEST( V(getError("checkParamFunccall", [null,2,3])) == "checkParamFunccall(p): Expect parameter p[1] of type string", "checks input parameter")
      TEST( V(getError("checkParamFunccall", [null,"",{}])) == "checkParamFunccall(p): Expect parameter p[2] of type []", "checks input parameter")

      TEST.setPath("Proxy.addFunctionCall()")

      for (var i=0; i<5; ++i) {
         // function 'addFunctionCall' under test
         let fcall = [ [{},"1",[1]], [{},"2",[1,2]], [{},"3",[4]], [{},"4",[5,6]], [{},"5",[7]] ]
         proxy.addFunctionCall(fcall[i])

         TEST( V(proxy.calls.length) == i+1 && V(proxy.calls[i]) === fcall[i], "pushes parameter to array proxy.calls #"+i)
      }
      TEST( V(getError("addFunctionCall", {})) == "addFunctionCall(p): Expect parameter p of type [] with length 3", "checks input parameter")

      TEST.setPath("Proxy.create().<function>()")

      // test created proxy returns correct values
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
      // test created proxy stores intercepted function calls
      for (var i=0; i<8; ++i) {
         var id = 1+Math.floor(i/2)%2, f = "f"+(1+i%4)
         TEST( V(proxy.calls[i][0].id) == id && V(proxy.calls[i][1]) == f
            && V(proxy.calls[i][2].length) == i%4, "compare call #"+i)
         for (var i2=0; i2<proxy.calls[i][2].length; ++i2) {
            TEST( V(proxy.calls[i][2][i2]) == i+i2, "compare parameter #"+i+","+i2)
         }
      }

      TEST.setPath("Proxy.compareCalls()")

      // test compareCalls
      proxy.reset()
      for (var i=0; i<8; ++i) {
         var calls = proxy.calls
         p1.f1(1); p2.f3(2,3,4); p1.f2(1,2,3); p2.f4(2)
         switch (i) {
         case 0: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2]]])) == "", "compareCalls works without error"); break;
         case 1: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p2,"f4",[2]]])) == "expect #3 number of calls instead of #4", "compareCalls expects less calls"); break;
         case 2: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2]] ,[p1,"f1",[1]]])) == "expect #5 number of calls instead of #4", "compareCalls expects more calls"); break;
         case 3: TEST( V(proxy.compareCalls( [ [p2,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2]]])) == "calls[0][0]: expect proxy id:2 instead of id:1", "compareCalls expects another proxy"); break;
         case 4: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f2",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2]]])) == "calls[1][1]: expect function 'f2' instead of 'f3'", "compareCalls expects another function"); break;
         case 5: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,4]], [p2,"f4",[2]]])) == "calls[2][2][2]: expect value '4' instead of '3'", "compareCalls expects another argument"); break;
         case 6: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[]]])) == "calls[3][2]: expect #0 function arguments instead of #1", "compareCalls expects less function call arguments"); break;
         case 7: TEST( V(proxy.compareCalls( [ [p1,"f1",[1]], [p2,"f3",[2,3,4]], [p1,"f2",[1,2,3]], [p2,"f4",[2,3]]])) == "calls[3][2]: expect #2 function arguments instead of #1", "compareCalls expects more function call arguments"); break;
         }
         TEST( V(proxy.calls.length) == 0, "clears intercepted calls #"+i)
         TEST( V(proxy.calls2) == calls, "makes backup for debegging into calls2 #"+i)
      }
      proxy.addFunctionCall([{},"",[]])
      TEST( V(getError("compareCalls", [ [1,2,3] ])) == "compareCalls(p): Expect parameter p[0][0] of type object|function", "checks input parameter")
      proxy.addFunctionCall([{},"",[]])
      proxy.addFunctionCall([{},"",[]])
      TEST( V(getError("compareCalls", [ proxy.calls[0], [proxy.calls[1][0],2,3] ])) == "compareCalls(p): Expect parameter p[1][1] of type string", "checks input parameter")
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

         TEST.setPath("TEST()")

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

               TEST( V(TEST.testedValues.length) == 0, "resets testedValues array to length 0")

               if (expr==0) {
                  TEST.setPath("TEST(true)")
                  TEST( V(TEST.proxy.compareCalls([])) == "", "does not change stats")
               } else {
                  TEST.setPath("TEST(false)")
                  TEST( V(TEST.proxy.compareCalls([
                           [logger_proxy,"error",[TEST,"<DESCRIPTION>",testedValues]],
                           [stats_proxy,"error",[]],
                        ])) == "", "does log an error")
               }
            }
         }

         TEST.setPath("TEST.reset()")

         // call function 'reset' under test
         var old_stats_values = Object.assign({}, TEST.stats)
         TEST.logger = undefined
         TEST.path = undefined
         TEST.proxy = undefined
         TEST.stats = undefined
         TEST.testedValues = undefined
         TEST.reset()
         var testedValues = TEST.testedValues
         TEST.testedValues = []

         TEST( V(TEST.logger) == ConsoleLogger && V(TEST.proxy) == Proxy && V(TEST.path.length) === 0
            && V(TEST.stats) == Stats && V(testedValues.length) == 0, "resets data fields to standard values")

         TEST( V(TEST.stats.nrExecutedTests()) == 0, "resets also stats")
         Object.assign(TEST.stats,old_stats_values)

         TEST.setPath("TEST.runTest()")

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
               ])) == "", "calls certain logger function in case of no error #"+nrErrors)
            } else {
               TEST( V(TEST.proxy.compareCalls([
                  [logger_proxy,"startTest",[TEST,"test-type-test","test-name"]],
                  [stats_proxy,"startTest",[]],
                  [TEST,"function(T)",[TEST]],
                  [logger_proxy,"endTestError",[TEST,"test-type-test","test-name",nrErrors]],
                  [stats_proxy,"endTest",[]],
               ])) == "", "calls certain logger function in case of an error #"+nrErrors)
            }

         }

         TEST.setPath("TEST.setLogger()")

         TEST.setLogger(undefined)
         TEST( V(TEST.logger) == undefined, "sets a new logger")

         TEST.setLogger(logger_proxy)
         TEST( V(TEST.logger) == logger_proxy, "sets a new logger")

         TEST.setPath("TEST.setDefaultLogger()")

         TEST.setLogger(undefined)
         TEST.setDefaultLogger()
         TEST( V(TEST.logger) == ConsoleLogger, "sets ConsoleLogger as default")

         TEST.setLogger(logger_proxy)
         TEST.setDefaultLogger()
         TEST( V(TEST.logger) == ConsoleLogger, "sets ConsoleLogger as default")

         TEST.setPath("TEST.showStats()")

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
               ])) == "", "calls certain logger and stats function #"+nrPassed+","+nrFailed)

            }
         }

         TEST.setPath("TEST.setPath()")

         for (var i=0; i<10; ++i) {
            var path = "TEST.setPath("+i+")"
            TEST.setPath(path)
            TEST( V(TEST.path) === path, "sets current path")
         }

         TEST.setPath("TEST.getPath()")

         for (var i=0; i<10; ++i) {
            var path = "TEST.setPath("+i+")"
            TEST.setPath(path)
            TEST( V(TEST.getPath()) === path, "returns current path")
         }

      } finally {
         unprepare_proxy()
      }
   }

}


