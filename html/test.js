/**
 * Test module which implements TEST and CATCH
 * to test for an expected value or an expected exception.
 * (c) 2024 Jörg Seebohn
 */

// namespace
const { TEST, TESTTHROW, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST } = (() => {

let runContext = null
let failedTest = 0
let executedTest = 0
let failedRunTest = 0

function RESET_TEST() {
   runContext = null
   failedTest = 0
   executedTest = 0
   failedRunTest = 0
}

class RunContext {
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
      this.waitSubContext = this.newPromise()
      this.waitTimer = this.newPromise()
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
            this.waitTimer.reject(`Timeout after ${this.timeout}ms`)
         }, this.timeout)
      }
   }

   clearTimer() {
      this.timer && clearTimeout(this.timer)
      this.timer = null
      this.waitTimer.resolve()
   }

   async waitForAllSubContext() {
      for (const context of this.subContext) {
         await context.waitAll
      }
      this.waitSubContext.resolve()
   }

   run() {
      let exception
      this.waitAll = Promise.all([ this.waitCallback.promise,
         this.waitSubContext.promise, this.waitTimer.promise,
         new Promise(resolve => setTimeout(resolve,this.delay))
         .then( () => {
            this.startTimer()
            return this.callback()
         })
         .then(() => this.waitCallback.resolve())
         .then(() => this.waitForAllSubContext())
         .then(() => this.clearTimer())
         .catch( exc => {
            this.waitCallback.reject("exception: "+exc.toString())
            exception = exc
         })
      ])
      .then(() => "") // OK
      .catch( reason => {
         if (reason !== "RUN_SUBTEST") {
            console.error(`RUN_TEST failed: ${this.name}`)
            console.log(`Reason: ${reason}`)
            exception && console.error(exception)
         }
         if (this.parentContext)
            this.parentContext.waitSubContext.reject("RUN_SUBTEST")
         return reason || "ERROR"
      })
      return this.waitAll
   }
}

class TestCase {
   #testFor
   #compare
   #expectValue
   #errmsg

   #value = undefined
   #exception = undefined
   #testResult = true

   constructor(testFor, compare, expectValue, errmsg) {
      this.#testFor = testFor
      this.#compare = compare
      this.#expectValue = expectValue
      this.#errmsg = errmsg
      if (typeof this.#compare !== "string" && typeof this.#compare !== "function")
         throw Error("Expect argument compare either of type string or function.")
      if (typeof this.#errmsg !== "string" && typeof this.#errmsg !== "function")
         throw Error("Expect argument errmsg either of type string or function.")
   }

   get OK() {
      return Boolean(this.#testResult)
   }

   get FAILED() {
      return ! Boolean(this.#testResult)
   }

   logError(reason) {
      console.error(`TEST failed: ${this.errmsg}`)
      console.log(`Reason: ${reason}`)
      if (this.#exception)
         console.log("Catched exception",this.#exception)
      else
         console.log("Tested value",this.#value)
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

   testValue() {
      this.run()
      if (this.#exception) {
         this.logError(`Unexpected exception ${this.#exception.name}.`)
      }
      else {
         const compare = this.#compare, value = this.#expectValue
         if (typeof compare === "function") {
            if (!compare(this.#value, value))
               this.logError(`Expect value ${String(this.#value)} === ${String(value)}.`)
         }
         else switch (compare) {
            case "<":
               !(this.#value < value) && this.logError(`Expect ${String(this.#value)} < ${String(value)}.`)
               break
            case ">":
               !(this.#value > value) && this.logError(`Expect ${String(this.#value)} > ${String(value)}.`)
               break
            case "<=":
               !(this.#value <= value) && this.logError(`Expect ${String(this.#value)} <= ${String(value)}.`)
               break
            case ">=":
               !(this.#value >= value) && this.logError(`Expect ${String(this.#value)} >= ${String(value)}.`)
               break
            case "!..":
               if (!Array.isArray(value) || value.length !== 2)
                  throw Error("Expect argment expectValue of type [lowerBound,upperBound]")
               !((this.#value < value[0]) || (this.#value > value[1])) && this.logError(`Expect ${String(this.#value)} not in range [${String(value[0])},${String(value[1])}].`)
               break
            case "..":
               if (!Array.isArray(value) || value.length !== 2)
                  throw Error("Expect argment expectValue of type [lowerBound,upperBound]")
               !((this.#value >= value[0]) && (this.#value <= value[1])) && this.logError(`Expect ${String(this.#value)} in range [${String(value[0])},${String(value[1])}].`)
               break
            case "==":
               !(this.#value == value) && this.logError(`Expect ${String(this.#value)} == ${String(value)}.`)
               break
            case "!=":
               !(this.#value != value) && this.logError(`Expect ${String(this.#value)} != ${String(value)}.`)
               break
            case "!==":
               !(this.#value !== value) && this.logError(`Expect ${String(this.#value)} != ${String(value)}.`)
               break
            case "=":
            case "===":
            default:
               if (!(this.#value === value))
                  this.logError(`Expect ${String(this.#value)} === ${String(value)}.`)
               break
         }
      }
      return this.OK
   }

   testThrow() {
      this.run()
      if (! this.#exception)
         this.logError(`Expected exception.`)
      else {
         const compare = this.#compare
         const exception = (typeof this.#expectValue === "string")
               ? { message:this.#expectValue, constructor:this.#exception.constructor }
               : this.#expectValue
         if (typeof compare === "function") {
            if (!compare(this.#exception, this.#expectValue))
               this.logError(`Expect exception ${String(this.#exception)} === ${String(this.#expectValue)}.`)
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

function TEST(testValue, compare, expectValue, errmsg) {
   const tc = new TestCase(testValue, compare, expectValue, errmsg)
   return countTest(tc.testValue())
}

function TESTTHROW(testFct, compare, expectValue, errmsg) {
   const tc = new TestCase(testFct, compare, expectValue, errmsg)
   return countTest(tc.testThrow())
}

async function SUB_TEST({timeout=0, delay=0}, callback) {
   if (! runContext) {
      throw Error("SUB_TEST not called within RUN_TEST")
   }
   const parentContext = runContext
   const childContext = new RunContext(parentContext.name, timeout, delay, callback, parentContext)
   return childContext.run()
}

async function RUN_TEST({name, timeout=0, delay=0}, callback) {
   if (runContext) {
      throw Error("RUN_TEST called nested within another RUN_TEST")
   }
   runContext = new RunContext(name, timeout, delay, callback)
   return runContext.run().then( (err) => {
      failedRunTest += (err ? 1 : 0)
      runContext = null
   })
}

function END_TEST() {
   console.log("***** TEST RESULT *****")
   console.log(` Executed TEST: ${executedTest}`)
   if (failedTest)
      console.error(` Failed TEST: ${failedTest}`)
   if (failedRunTest)
      console.error(` Failed RUN_TEST: ${failedRunTest} `)
   if (!failedTest && !failedRunTest)
      console.log(` All tests working :-)`)
   RESET_TEST()
}

async function test_syntax_of_TEST()
{
   await RUN_TEST({name:"Syntax of TEST",timeout:500}, () => {
      TEST(99,'..',[99,101], "TEST")
      TEST(101,'..',[99,101], "TEST")
      TEST(98,'!..',[99,101], "TEST")
      TEST(102,'!..',[99,101], "TEST")
      TEST(100,(v,e)=>v===e,100, "TEST")
      TEST(100,'=',100, "TEST")
      TEST(100,'==','100', "TEST")
      TEST(100,'===',100, "TEST")
      TEST(100,'!=',101, "TEST")
      TEST(100,'!==',"100", "TEST")
      TEST(100,'<',101, "TEST")
      TEST(100,'<=',101, "TEST")
      TEST(100,'<=',100, "TEST")
      TEST(101,'>',100, "TEST")
      TEST(101,'>=',100, "TEST")
      TEST(100,'>=',100, "TEST")
      TEST(100,(v,e)=>e===v,100, "TEST")
      TEST(100,(v,e)=>e!==v,101, "TEST")
      let sub_test1 = 0, sub_test2 = 0
      SUB_TEST({delay:0}, () => {
         // complex comparison: - two arrays -
         ++ sub_test1
         const a1=[1,2,3], a2=[2,3,4]
         for (const i of [0,1,2])
            if (!TEST(a1[i],'<',a2[i], () => `a1[${i}] < a2[${i}]`))
               break
      })
      SUB_TEST({delay:10}, () => {
         TEST(sub_test1,'=',1, "sub test1 executed before sub test2")
         ++ sub_test2
         TESTTHROW(()=>{throw Error("msg1")},'=',"msg1", "TEST")
         TESTTHROW(()=>{throw Error("msg2")},'=',Error("msg2"), "TEST")
         TESTTHROW(()=>{throw Error("msg3")},'=',{message:"msg3",constructor:Error}, "TEST")
         TESTTHROW(()=>{throw Error("msg4")},(v,e)=>v instanceof Error && v.message===e,"msg4", "TEST")
         TESTTHROW(()=>{throw Error("msg5")},(v,e)=>v instanceof e.constructor && v.message===e.message,Error("msg5"), "TEST")
      })
      SUB_TEST({delay:20}, () => {
         TEST(sub_test1,'=',1, "both sub tests executed exactly once")
         TEST(sub_test2,'=',1, "both sub tests executed exactly once")
      })
   })
   if (executedTest != 29 || failedTest || failedRunTest)
      throw Error(`Internal error in TEST module ex=${executedTest} f=${failedTest} r=${failedRunTest}`)
   RESET_TEST()
}

test_syntax_of_TEST()

return { TEST, TESTTHROW, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST }

})()
