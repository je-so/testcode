/**
 * Test module which implements TEST and TESTTHROW
 * to test for an expected value or an expected exception.
 * (c) 2024 Jörg Seebohn
 */

// namespace
const { TEST, TESTTHROW, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST, INTERNAL_SYNTAX_TEST } = (() => {

let runContext = null
let failedTest = 0
let executedTest = 0
let testConsole = null

function getConsole() {
   return testConsole || console
}

function RESET_TEST() {
   runContext = null
   failedTest = 0
   executedTest = 0
   RunContext.FailedRunTest = 0
   RunContext.FailedSubTest = 0
   testConsole = null
}

class RunContext {
   static ID = 0
   static FailedRunTest = 0
   static FailedSubTest = 0

   ID = ++RunContext.ID

   toString() {
      if (this.parentContext)
         return `RunContext(ID:${this.ID},name:»${this.name}«,parentContext:{${String(this.parentContext)}})`
      else
         return `RunContext(ID:${this.ID},name:»${this.name}«)`
   }

   constructor(name, timeout, delay, callback, parentContext) {
      this.name = name
      this.timeout = timeout // ms after which callback should return
      this.delay = delay // delay in ms until callback is run
      this.callback = callback
      this.subContext = []
      this.timer = null
      this.parentContext = parentContext
      parentContext && parentContext.subContext.push(this)
      this.waitCallback = this.newPromise()
      this.waitAll = null
      if (typeof this.name !== "string")
         throw Error("Expect argument name of type strimg")
      if (!isFinite(this.timeout))
         throw Error("Expect argument timeout of type number")
      if (!isFinite(this.delay))
         throw Error("Expect argument delay of type number")
      if (typeof this.callback !== "function")
         throw Error("Expect argument callback of type function")
   }

   newPromise() {
      const wrapper = { resolve:null, reject:null, promise:null }
      wrapper.promise = new Promise( (resolve, reject) => {
         wrapper.resolve = resolve
         wrapper.reject = reject
      })
      return wrapper
   }

   startTimer() {
      if (this.timeout > 0) {
         this.timer ??= setTimeout( () => {
            this.waitCallback.reject(`Timeout after ${this.timeout}ms`)
         }, this.timeout)
      }
   }

   clearTimer() {
      this.timer && clearTimeout(this.timer)
      this.timer = null
   }

   async waitForAllSubContext() {
      for (const context of this.subContext) {
         await context.waitAll
      }
   }

   run() {
      let exception
      this.waitAll = Promise.all([ this.waitCallback.promise,
         new Promise(resolve => setTimeout(resolve,this.delay))
         .then( () => {
            this.startTimer()
            return this.callback(this)
         })
         .then(() => this.waitForAllSubContext())
         .then(() => this.clearTimer())
         .then(() => this.waitCallback.resolve())
         .catch( exc => {
            this.waitCallback.reject("exception: "+exc.toString())
            exception = exc
         })
      ])
      .then(() => "") // OK
      .catch( reason => {
         if (this.parentContext) {
            ++ RunContext.FailedSubTest
            getConsole().error(`SUB_TEST failed: ${this.name}`)
         }
         else {
            ++ RunContext.FailedRunTest
            getConsole().error(`RUN_TEST failed: ${this.name}`)
         }
         getConsole().log(`Reason: ${reason}`)
         exception && getConsole().error(exception)
         return reason || "ERROR"
      })
      return this.waitAll
   }
}

class TestCase {
   #testFor
   #compare
   #expect
   #errmsg

   #value = undefined
   #exception = undefined
   #testResult = true

   constructor(testFor, compare, expect, errmsg) {
      this.#testFor = testFor
      this.#compare = compare
      this.#expect = expect
      this.#errmsg = errmsg
      if (typeof this.#compare !== "string" && typeof this.#compare !== "function")
         throw Error("Expect argument compare either of type string or function.")
      if (typeof this.#errmsg !== "string" && typeof this.#errmsg !== "function")
         throw Error("Expect argument errmsg either of type string or function.")
   }

   get OK() {
      return this.#testResult
   }

   logError(reason) {
      getConsole().error(`TEST failed: ${this.errmsg}`)
      getConsole().log(`Reason: ${reason}`)
      if (this.#exception)
         getConsole().log("Catched exception",this.#exception)
      else
         getConsole().log("Tested value",this.#value)
      this.#testResult = false
   }

   get errmsg() {
      return typeof this.#errmsg === "function"
            ? this.#errmsg()
            : this.#errmsg
   }

   run() {
      try {
         this.#value = (typeof this.#testFor === "function")
                     ? this.#testFor()
                     : this.#testFor
      }
      catch(exc) {
         this.#exception = exc
      }
   }

   valuetoString(value) {
      if (Array.isArray(value))
         return "["+String(value)+"]"
      return String(value)
   }

   compareValue(compare, value, expect) {
      switch (compare) {
         case "<":
            if (value < expect) return true
            break
         case ">":
            if (value > expect) return true
            break
         case "<=":
            if (value <= expect) return true
            break
         case ">=":
            if (value >= expect) return true
            break
         case "!range":
            if (!Array.isArray(expect) || expect.length !== 2)
               throw Error("Expect arg expect of type [lowerBound,upperBound]")
            if (value < expect[0] || expect[1] < value) return true
            break
         case "range":
            if (!Array.isArray(expect) || expect.length !== 2)
               throw Error("Expect arg expect of type [lowerBound,upperBound]")
            if (expect[0] <= value && value <= expect[1]) return true
            break
         case "==":
            if (value == expect) return true
            break
         case "!=":
            if (value != expect) return true
            break
         case "!==":
            if (value !== expect) return true
            break
         case "=":
            compare = "==="
            if (value === expect) return true
            break
         case "===":
            if (value === expect) return true
            break
         default:
            throw Error(`Unsupported arg compare »${String(compare)}«`)
      }
      this.logError(`Expect ${this.valuetoString(value)} ${compare} ${this.valuetoString(expect)}.`)
      return false
   }

   indexToString(index) {
      if (typeof index === "string" && (!isFinite(index) || (""+parseInt(index)!==index)))
         return "'"+index+"'"
      return String(index)
   }

   compareArray(value, expect, index="") {
      if (!Array.isArray(expect))
         throw Error("Expect arg expect of type Array")
      if (!Array.isArray(value))
         this.logError(`Expect${index?" at "+index:""} value ${this.valuetoString(value)} of type Array.`)
      else if (value.length != expect.length)
         this.logError(`Expect${index?" at "+index:""} array length ${value.length} == ${expect.length}.`)
      else {
         for (const entry of value.entries()) {
            if (Array.isArray(expect[entry[0]])) {
               if (!this.compareArray(entry[1], expect[entry[0]], `${index}[${this.indexToString(entry[0])}]`))
                  return false
            }
            else if (entry[1] !== expect[entry[0]]) {
               this.logError(`Expect at ${index}[${this.indexToString(entry[0])}] ${this.valuetoString(entry[1])} == ${this.valuetoString(expect[entry[0]])}.`)
               return false
            }
         }
         return true
      }
      return false
   }

   compareObject(value, expect, index="") {
      if (typeof expect !== "object")
         throw Error("Expect arg expect of type object")
      if (typeof value !== "object")
         this.logError(`Expect${index?" at "+index:""} value ${this.valuetoString(value)} of type object.`)
      else {
         const keys = new Set([...Reflect.ownKeys(value),...Reflect.ownKeys(expect)])
         for (const key of keys) {
            if (expect[key] !== null && typeof expect[key] === "object") {
               if (!this.compareObject(value[key], expect[key], `${index}[${this.indexToString(key)}]`))
                  return false
            }
            else if (value[key] !== expect[key]) {
               this.logError(`Expect at ${index}[${this.indexToString(key)}] ${value[key]} == ${expect[key]}.`)
               return false
            }
         }
         return true
      }
      return false
   }

   testValue() {
      this.run()
      if (this.#exception) {
         this.logError(`Unexpected exception ${this.#exception.name}.`)
      }
      else {
         const compare = this.#compare
         if (typeof compare === "function") {
            const result = compare(this.#value, this.#expect)
            if (typeof result === "boolean" && !result)
               this.logError(`Expect value ${String(this.#value)} === ${String(this.#expect)}.`)
            else if (typeof result === "string" && result)
               this.logError(`Expect ${String(result)}.`)
         }
         else switch (compare) {
            case "{=}":
               this.compareObject(this.#value, this.#expect)
               break
            case "[=]":
               this.compareArray(this.#value, this.#expect)
               break
            default:
               this.compareValue(compare, this.#value, this.#expect)
               break
         }
      }
      return this.OK
   }

   testThrow() {
      this.run()
      if (! this.#exception) {
         this.logError(`Expected exception.`)
      }
      else {
         const compare = this.#compare
         const exception = (typeof this.#expect === "string")
               ? { message:this.#expect, constructor:this.#exception.constructor }
               : this.#expect
         if (typeof compare === "function") {
            if (!compare(this.#exception, this.#expect))
               this.logError(`Expect exception ${String(this.#exception)} === ${String(this.#expect)}.`)
         }
         else if (!(this.#exception instanceof exception.constructor)) {
            this.logError(`Expect exception type »${this.#exception.name}« === »${exception.constructor?.name}«.`)
         }
         else if (this.#exception.message !== exception.message) {
            this.logError(`Expect exception message »${this.#exception.message}« === »${exception.message}«.`)
         }
      }
      return this.OK
   }
}

function countTest(isOK) {
   ++ executedTest
   !isOK && ( ++ failedTest)
   return isOK
}

function TEST(testValue, compare, expect, errmsg) {
   const tc = new TestCase(testValue, compare, expect, errmsg)
   return countTest(tc.testValue())
}

function TESTTHROW(testFct, compare, expect, errmsg) {
   const tc = new TestCase(testFct, compare, expect, errmsg)
   return countTest(tc.testThrow())
}

async function SUB_TEST({timeout=0, delay=0, context}, callback) {
   if (! runContext) {
      throw Error("SUB_TEST not called within RUN_TEST")
   }
   const parentContext = context ?? runContext
   const childContext = new RunContext(parentContext.name, timeout, delay, callback, parentContext)
   return childContext.run()
}

async function RUN_TEST({name, timeout=0, delay=0}, callback) {
   if (runContext) {
      throw Error("RUN_TEST called nested within another RUN_TEST")
   }
   runContext = new RunContext(name, timeout, delay, callback, null)
   return runContext.run().then( (err) => {
      runContext = null
      return err
   })
}

function END_TEST() {
   console.log("***** TEST RESULT *****")
   console.log(` Executed TEST: ${executedTest}`)
   if (failedTest)
      console.error(` Failed TEST: ${failedTest}`)
   if (RunContext.FailedRunTest)
      console.error(` Failed RUN_TEST: ${RunContext.FailedRunTest} `)
   if (RunContext.FailedSubTest)
      console.error(` Failed SUB_TEST: ${RunContext.FailedSubTest} `)
   if (!failedTest && !(RunContext.FailedRunTest+RunContext.FailedSubTest))
      console.log(` All tests working :-)`)
   RESET_TEST()
}

async function INTERNAL_SYNTAX_TEST()
{
   await RUN_TEST({name:"Syntax of TEST",timeout:500}, async (context) => {
      TEST(runContext,'=',context, "first arg of RUN_TEST points to global runContext")
      TEST(context.parentContext,'=',null, "first arg of RUN_TEST points to global runContext")
      TEST(99,'range',[99,101], "test for inclusive range")
      TEST(101,'range',[99,101], "test for inclusive range")
      TEST(98,'!range',[99,101], "test not in inclusive range")
      TEST(102,'!range',[99,101], "test not in inclusive range")
      TEST(100,(v,e)=>v===e,100, "use comparison function v_alue and e_xpected value")
      TEST(100,'=',100, "test for equality no conversions allowed")
      TEST(100,'==','100', "test for equality with conversion allowed")
      TEST(100,'===',100, "test for equality no conversions allowed")
      TEST(100,'!=',101, "test for inequality conversions allowed")
      TEST(100,'!==',"100", "test for inequality no conversions allowed")
      TEST(100,'<',101, "test for less than")
      TEST(100,'<=',101, "test for less than or equality")
      TEST(100,'<=',100, "test for less than or equality")
      TEST(101,'>',100, "test for greater than")
      TEST(101,'>=',100, "test for greater than or equality")
      TEST(100,'>=',100, "test for greater than or equality")
      TEST(100,(v,e)=>e===v,100, "use comparison function v_alue and e_xpected value")
      TEST(100,(v,e)=>e!==v,101, "use comparison function v_alue and e_xpected value") // 20
      let sub_test1 = 0, sub_test2 = 0
      SUB_TEST({delay:0}, (context) => {
         // complex comparison: - two arrays -
         ++ sub_test1
         const a1=[1,2,3], a2=[2,3,4], a3=[2,3,4]
         for (const i of [0,1,2])
            if (!TEST(a1[i],'<',a2[i], () => `a1[${i}] < a2[${i}]`))
               break
         // equality is supported by comparison op "[=]"
         TEST(a2,"[=]",a3,"array a2 equals a3")
         // - two objects -
         const s1 = Symbol(1)
         const o1 = { [s1]: 1, name:"tester" }, o2 = { [s1]: 1, name:"tester" }
         TEST(o1,"{=}",o2,"object o1 equals o2")
      })
      SUB_TEST({delay:10}, () => {
         TEST(sub_test1,'=',1, "sub test1 executed before sub test2")
         ++ sub_test2
         TESTTHROW(()=>{throw Error("msg1")},'=',"msg1", "TEST")
         TESTTHROW(()=>{throw Error("msg2")},'=',Error("msg2"), "TEST")
         TESTTHROW(()=>{throw Error("msg3")},'=',{message:"msg3",constructor:Error}, "TEST")
         TESTTHROW(()=>{throw Error("msg4")},(v,e)=>v instanceof Error && v.message===e,"msg4", "TEST")
         TESTTHROW(()=>{throw Error("msg5")},(v,e)=>v instanceof e.constructor && v.message===e.message,Error("msg5"), "TEST") // 29
      })
      // RUN_TEST is waiting on nested SUB_TEST (tested with executedTest)
      SUB_TEST({delay:20}, () => {
         TEST(sub_test1,'=',1, "both sub tests executed exactly once")
         TEST(sub_test2,'=',1, "both sub tests executed exactly once")
      })
      let waitedOnNestedContext = false
      await SUB_TEST({delay:0}, (context) => {
         TEST(runContext,'!=',context, "first arg of SUB_TEST points to child context")
         TEST(runContext,'=',context.parentContext, "first arg of SUB_TEST points to child context")
         SUB_TEST({delay:0}, (context2) => {
            TEST(runContext,'=',context2.parentContext, "nested SUB_TEST runs within global context")
         })
         // outer SUB_TEST is waiting until end of nested SUB_TEST
         // cause of providing argument context
         // if context is undefined RUN_TEST would wait on double nested SUB_TEST
         SUB_TEST({delay:100,context}, (context2) => {
            TEST(context,'=',context2.parentContext, "nested SUB_TEST runs within context provided as argument")
            waitedOnNestedContext = true
         })
      })
      TEST(waitedOnNestedContext,'=',true, "SUB_TEST waits until nested SUB_TEST has been executed")
   })
   if (executedTest != 38 || failedTest || RunContext.FailedRunTest || RunContext.FailedSubTest || runContext != null)
      throw Error(`Internal error in TEST module nrExecutedTest=${executedTest} failed=${failedTest} failedRun=${RunContext.FailedRunTest} failedSub=${RunContext.FailedSubTest}`)
   //
   // intercept console.log / console.error calls and test for correct error output
   //
   const _testConsole = {
      calls: [],
      log(...args) {
         this.calls.push(["log",...args])
      },
      error(...args) {
         this.calls.push(["error",...args])
      },
      switchOn() {
         testConsole = this
         this.calls = []
      },
      switchOff() {
         testConsole = null
      },
      compare(name, expect) {
         const calls = this.calls
         TEST(calls.length,"=",expect.length,`Failed ${name} produces ${expect.length} console output-lines`)
         for (let i=0; i<calls.length; ++i) {
            TEST(calls[i].length,"=",expect[i].length,`${name} output-line ${i+1} logs ${expect[i].length-1} arguments`)
            TEST(calls[i][0],"=",expect[i][0],`${name} output-line ${i+1} calls console.${expect[i][0]}`)
            TEST(calls[i][1],"=",expect[i][1],`${name} output-line ${i+1} calls console.${expect[i][0]} with correct 1st argument`)
            if (calls[i].length == 3)
               TEST(calls[i][2],"=",expect[i][2],`${name} output-line ${i+1} calls console.${expect[i][0]} with correct 2nd argument`)
            else
               TEST(calls[i].length,"=",2,`${name} output-line ${i+1} calls console.${expect[i][0]} with one argument`)
         }
      }
   }
   await RUN_TEST({name:"-- TEST fails --",timeout:100}, async (context) => {
      _testConsole.switchOn()
      TEST(0,"=",1,"FAIL1")
      _testConsole.switchOff()
      _testConsole.compare("TEST", [
         ["error","TEST failed: FAIL1"],
         ["log","Reason: Expect 0 === 1."],
         ["log","Tested value",0]
      ])
   })
   if (executedTest != 52 || failedTest != 1 || RunContext.FailedRunTest != 0 || RunContext.FailedSubTest != 0 || runContext != null)
      throw Error(`Internal error in TEST module nrExecutedTest=${executedTest} failed=${failedTest} failedRun=${RunContext.FailedRunTest} failedSub=${RunContext.FailedSubTest}`)
   await RUN_TEST({name:"-- Timeout --",timeout:100}, async (context) => {
      _testConsole.switchOn()
      const error = await SUB_TEST({timeout:20}, (context) => {
         SUB_TEST({delay:50,context}, () => {
         })
      })
      _testConsole.switchOff()
      _testConsole.compare("SUB_TEST", [
         ["error","SUB_TEST failed: -- Timeout --"],
         ["log","Reason: Timeout after 20ms"]
      ])
      TEST(error,"=","Timeout after 20ms","SUB_TEST returns error")
   })
   if (executedTest != 62 || failedTest != 1 || RunContext.FailedRunTest != 0 || RunContext.FailedSubTest != 1)
      throw Error(`Internal error in TEST module nrExecutedTest=${executedTest} failed=${failedTest} failedRun=${RunContext.FailedRunTest} failedSub=${RunContext.FailedSubTest}`)
   _testConsole.switchOn()
   const thrownError = Error("-- throw --")
   const error = await RUN_TEST({name:"-- Exception --"}, (context) => {
      throw thrownError
   })
   _testConsole.switchOff()
   _testConsole.compare("RUN_TEST", [
      ["error","RUN_TEST failed: -- Exception --"],
      ["log","Reason: exception: Error: -- throw --"],
      ["error",thrownError]
   ])
   TEST(error,"=","exception: Error: -- throw --","RUN_TEST returns error")
   if (executedTest != 76 || failedTest != 1 || RunContext.FailedRunTest != 1 || RunContext.FailedSubTest != 1 || runContext != null)
      throw Error(`Internal error in TEST module nrExecutedTest=${executedTest} failed=${failedTest} failedRun=${RunContext.FailedRunTest} failedSub=${RunContext.FailedSubTest}`)
   RESET_TEST()
}

return { TEST, TESTTHROW, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST, INTERNAL_SYNTAX_TEST }

})()

INTERNAL_SYNTAX_TEST()
