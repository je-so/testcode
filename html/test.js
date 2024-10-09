/**
 * Test module which implements TEST and TESTTHROW
 * to test for an expected value or an expected exception.
 * (c) 2024 Jörg Seebohn
 */

// namespace
const { TEST, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST, INTERNAL_SYNTAX_TEST, TestEnvironment } = (() => {

/**
 * Contains stats counter and test console for testing TEST output.
 */
class TestEnvironment {
   static default = new TestEnvironment()
   /** The number of started but not finished {@link runTest RUN_TEST} and {@link runSubTest SUB_TEST}. @type {number} */
   runningTest = 0
   /** The number of started {@link runTest RUN_TEST} and {@link runSubTest SUB_TEST}. @type {number} */
   executedRunTest = 0
   /** @type {number} */
   failedRunTest = 0
   /** @type {number} */
   executedTestCase = 0
   /** @type {number} */
   failedTestCase = 0
   constructor() { }
   newTestConsole() { return new TestConsole(this) }
   signalStartRun() { return (++ this.executedRunTest, ++ this.runningTest) }
   signalEndRun() { return -- this.runningTest }
   /**
    * @param {undefined|TestConsole} testConsole
    * @param {string} name
    * @param {string} reason
    * @param {{value:any}} [exceptionValue]
    * @returns {number} The number of failed RUN_TEST.
    */
   signalRunTestError(testConsole, name, reason, exceptionValue) {
      const logger = testConsole ?? console
      logger.error(`RUN_TEST failed: ${name}`)
      logger.log(`Reason: ${reason}`)
      exceptionValue && logger.log("Catched exception",exceptionValue.value)
      return ++ this.failedRunTest
   }
   /**
    * @param {undefined|TestConsole} testConsole
    * @param {string} name
    * @param {string} reason
    * @param {{value:any}} [exceptionValue]
    * @returns {number} The number of failed SUB_TEST.
    */
   signalSubTestError(testConsole, name, reason, exceptionValue) {
      const logger = testConsole ?? console
      logger.error(`SUB_TEST failed: ${name}`)
      logger.log(`Reason: ${reason}`)
      exceptionValue && logger.log("Catched exception",exceptionValue.value)
      return ++ this.failedRunTest
   }
   /**
    * @param {undefined|TestConsole} testConsole
    * @param {string} errmsg
    * @param {string} reason
    * @param {any} value
    */
   signalTestCaseError(testConsole, errmsg, reason, value) {
      const logger = testConsole ?? console
      logger.error(`TEST failed: ${errmsg}`)
      logger.log(`Reason: ${reason}`)
      logger.log("Tested value",value)
      ++ this.failedTestCase
   }
   signalTestCaseExecuted() {
      ++ this.executedTestCase
   }
   /**
    * Shows test stats on console.
    * @param {{testConsole?:TestConsole}} [options]
    */
   async showStats({testConsole}={}) {
      const logger = testConsole ?? console
      if (this.runningTest > 0) {
         await new Promise((resolve) => {
            let counter = 1
            const startTimer = () => setTimeout(() => {
               if (this.runningTest > 0) {
                  startTimer()
                  ++ counter
                  if (counter % 4 === 0)
                     logger.log("***** Waiting for TEST RESULT *****")
               }
               else
                  resolve(true)
            }, 250)
            startTimer()
         })
      }
      logger.log("***** TEST RESULT *****")
      logger.log(` Executed TEST: ${this.executedTestCase}`)
      logger.log(` Executed [RUN|SUB]_TEST: ${this.executedRunTest}`)
      if (!this.failedTestCase && !this.failedRunTest)
         logger.log(` All tests working :-)`)
      else {
         if (this.failedTestCase)
            logger.error(` Failed TEST: ${this.failedTestCase}`)
         if (this.failedRunTest)
            logger.error(` Failed [RUN|SUB]_TEST: ${this.failedRunTest} `)
      }
   }

   /**
    *
    * @param {{name:string, timeout?:number, delay?:number, testConsole?:TestConsole}} config
    * @param {(context:RunContext)=>void|Promise<void>} callback An asynchronous callback which is called after delay milliseconds.
    * @returns {Promise<string>}
    */
   async runTest({name, timeout=0, delay=0, testConsole}, callback) {
      const runContext = new RunContext({name, timeout, delay, callback, testenv: this, testConsole})
      return runContext.run()
   }

   /**
    * Runs a sequence of {@link test TEST} or nested calls to {@link runSubTest SUB_TEST}.
    * All sub-tests are started as pseudo parallel asymc functions.
    * Use await on the return value to ensure that a SUB_TEST has ended its execution before
    * another one is started. The containing context (either RUN_TEST or SUB_TEST)
    * waits for all contained SUB_TEST. The containing context is given in the config parameter context.
    * @param {{context:RunContext, timeout?:number, delay?:number, testConsole?:TestConsole}} config
    * @param {(context:RunContext)=>void|Promise<void>} callback An asynchronous callback which is called after delay milliseconds.
    * @returns {Promise<string>}
    */
   async runSubTest({timeout=0, delay=0, context, testConsole}, callback) {
      if (!(context instanceof RunContext))
         throw Error("Missing argument »context« of type RunContext.")
      const parentContext = context
      const name = parentContext.name
      const childContext = new RunContext({name, timeout, delay, callback, parentContext, testenv: this, testConsole})
      return childContext.run()
   }

   /**
    * @param {any|(()=>any)} testValue
    * @param {string|((testValue:any, expect:any)=>boolean)} compare Either built in comparison functions (string) or a comparison function. Returning a non empty string is considered the reason why the comparison failed. Returning false is also considered as a failed test.
    * @param {any} expect The expected value argument testValue should be compared with.
    * @param {string|(() => string)} errmsg The error message to show if TEST failed.
    * @param {{testConsole?:TestConsole}} [options]
    * @returns {boolean|Promise<boolean>} True indicates a successful test.
    */
   test(testValue, compare, expect, errmsg, options) {
      const tc = new TestCase(testValue, compare, expect, errmsg, this, options?.testConsole)
      return tc.test()
   }

   /**
    * Exports interface to simplifiy usage.
    * The exported methods which are bound to this object are:
    * * {@link runTest} as "RUN_TEST"
    * * {@link runSubTest} as "SUB_TEST"
    * * {@link showStats} as "END_TEST"
    * * {@link test} as "TEST"
    * * {@link testThrow} as "TESTTHROW"
    * @returns {{
    *    RUN_TEST:(config:{name:string,timeout?:number,delay?:number,testConsole?:TestConsole},callback:(context:RunContext)=>void|Promise<void>)=>Promise<string>,
    *    SUB_TEST:(config:{timeout?:number,delay?:number,context?:RunContext,testConsole?:TestConsole},callback:(context:RunContext)=>void|Promise<void>)=>Promise<string>,
    *    END_TEST:(options?:{testConsole?:TestConsole})=>Promise<void>,
    *    TEST:(testValue:any,compare:string|((value:any,expect:any)=>boolean|string),expect:any,errmsg:string|(()=>string),options?:{testConsole?:TestConsole})=>boolean
    * }}
    */
   export() {
      return {
         RUN_TEST: this.runTest.bind(this),
         SUB_TEST: this.runSubTest.bind(this),
         END_TEST: this.showStats.bind(this),
         TEST: this.test.bind(this),
      }
   }
}

/**
 * Allows to intercept console.log / console.error calls and test for correct error output.
 */
class TestConsole {
   /** @type {[string, ...any][]} */
   calls = []
   /**
    * @param  {TestEnvironment} testenv
    */
   constructor(testenv) { this.testenv = testenv }
   /**
    * @param  {...any} args
    */
   log(...args) { this.calls.push(["log",...args]) }
   /**
    * @param  {...any} args
    */
   error(...args) { this.calls.push(["error",...args]) }
   compare(name, expect) {
      const calls = this.calls
      const { TEST } = this.testenv.export()
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

class RunContext {
   static ID = 0

   ID = ++RunContext.ID
   /** @type {RunContext[]} */
   subContext = []
   /** @type {string|Promise<string>} */
   waitAll = "Error: run() not called"
   /** @type {string} */
   signaledError = "Error: run() not called"

   toString() {
      if (this.parentContext)
         return `RunContext(ID:${this.ID},name:»${this.name}«,parentContext:{${String(this.parentContext)}})`
      else
         return `RunContext(ID:${this.ID},name:»${this.name}«)`
   }

   /**
    *
    * @param {{name:string, timeout:number, delay:number, callback:(context:RunContext)=>void|Promise<void>, parentContext?:RunContext, testenv:TestEnvironment, testConsole:undefined|TestConsole}} options
    */
   constructor({name, timeout, delay, callback, parentContext, testenv, testConsole}) {
      if (typeof name !== "string")
         throw Error("Expect argument »name« of type string")
      if (!isFinite(timeout))
         throw Error("Expect argument »timeout« of type number")
      if (!isFinite(delay))
         throw Error("Expect argument »delay« of type number")
      if (typeof callback !== "function")
         throw Error("Expect argument »callback« of type function")
      if (parentContext != null && !(parentContext instanceof RunContext))
         throw Error("Expect argument »parentContext« of type RunContext")
      if (!(testenv instanceof TestEnvironment))
         throw Error("Expect argument »testenv« of type TestEnvironment")
      if (testConsole && !(testConsole instanceof TestConsole))
         throw Error("Expect argument »testConsole« of type TestConsole")
      this.name = name
      this.timeout = timeout // ms after which callback should return
      this.delay = delay // delay in ms until callback is run
      this.callback = callback
      this.timer = null
      this.parentContext = parentContext
      this.testenv = testenv
      this.testConsole = testConsole
      this.waitAll = this.signaledError = "Error: run() not called"
      parentContext?.subContext?.push(this)
   }

   startTimer() {
      if (this.timeout > 0) {
         this.clearTimer()
         this.timer = setTimeout( () => {
            this.signalError(`Timeout after ${this.timeout}ms`)
            this.timer = null
         }, this.timeout)
      }
   }

   clearTimer() {
      if (this.timer) {
         clearTimeout(this.timer)
         this.timer = null
      }
   }

   async waitForAllSubContext() {
      for (const context of this.subContext) {
         const error = await context.waitAll
         if (error) this.signaledError = error // already signaled in sub_test, only return value in waitall, but do not signal twice (by calling signalError)
      }
   }
   /**
    * @param {string} reason
    * @param {{value:any}} [exceptionValue]
    */
   signalError(reason, exceptionValue) {
      if (this.parentContext)
         this.testenv.signalSubTestError(this.testConsole, this.name, reason, exceptionValue)
      else
         this.testenv.signalRunTestError(this.testConsole, this.name, reason, exceptionValue)
      this.signaledError = reason
   }

   run() {
      this.signaledError = "" // OK
      this.testenv.signalStartRun()
      this.waitAll = new Promise(resolve => setTimeout(resolve, this.delay))
         .then( () => {
            this.startTimer()
            return this.callback(this)
         })
         .catch( exception => {
            this.clearTimer()
            this.signalError("Exception: " + exception.toString(), { value: exception })
         })
         .then(() => this.waitForAllSubContext())
         .then(() => this.clearTimer())
         .then(() => this.signaledError)
         .finally(() => this.testenv.signalEndRun())
      return this.waitAll
   }
}

class TestCase {
   #testFor
   #value
   #compare
   #expect
   #testOK
   #errmsg
   #testenv
   #testConsole

   /**
    * @param {any|(()=>any)} testFor
    * @param {string|((value:any,expect:any)=>boolean|string)} compare
    * @param {any} expect
    * @param {string|(()=>string)} errmsg
    * @param {TestEnvironment} testenv
    * @param {undefined|TestConsole} testConsole
    */
   constructor(testFor, compare, expect, errmsg, testenv, testConsole) {
      if (typeof compare !== "string" && typeof compare !== "function")
         throw Error("Expect argument »compare« either of type string or function.")
      if (typeof errmsg !== "string" && typeof errmsg !== "function")
         throw Error("Expect argument »errmsg« either of type string or function.")
      if (!(testenv instanceof TestEnvironment))
         throw Error("Expect argument testenv« of type TestEnvironment.")
      if (testConsole && !(testConsole instanceof TestConsole))
         throw Error("Expect argument »testConsole« of type TestConsole")
      this.#testFor = testFor
      this.#value = expect
      this.#compare = compare
      this.#expect = expect
      this.#testOK = true
      this.#errmsg = errmsg
      this.#testenv = testenv
      this.#testConsole = testConsole
   }

   get testenv() {
      return this.#testenv
   }
   get OK() {
      return this.#testOK
   }
   /**
    * @param {string} reason
    */
   logError(reason) {
      this.testenv.signalTestCaseError(this.#testConsole, this.errmsg, reason, this.#value)
      this.#testOK = false
   }
   /**
    * @returns {string} The message which describes the failed TEST.
    */
   get errmsg() {
      return typeof this.#errmsg === "function"
            ? this.#errmsg()
            : this.#errmsg
   }

   valuetoString(value) {
      if (Array.isArray(value))
         return "["+String(value)+"]"
      return String(value)
   }
   /**
    * @param {(value:any, expect:any)=>boolean|string} compare
    * @param {any} value
    * @param {any} expect
    * @returns {boolean} True in case comparison holds else false.
    */
   compareFunction(compare, value, expect) {
      const result = compare(value, expect)
      if (  (typeof result !== "string" && result)
         || (typeof result === "string" && result === ""))
         return true
      if (typeof result === "string")
         this.logError(`${result}.`)
      else
         this.logError(`Expect ${String(value)} === ${String(expect)}.`)
      return false
   }
   /**
    * @returns {boolean} True in case comparison holds else false.
    */
   compare() {
      const compare = this.#compare
      const value = this.#value
      const expect = this.#expect
      if (typeof compare === "function")
         return this.compareFunction(compare, value, expect)
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
               throw Error("Expect argument »expect« of type [lowerBound,upperBound]")
            if (value < expect[0] || expect[1] < value) return true
            break
         case "range":
            if (!Array.isArray(expect) || expect.length !== 2)
               throw Error("Expect argument »expect« of type [lowerBound,upperBound]")
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
         case "=": case "===":
            if (value === expect) return true
            break
         case "{=}":
            return this.compareObject(value, expect)
         case "[=]":
            return this.compareArray(value, expect)
         case "throw":
            return this.compareException(value, expect)
         default:
            throw Error(`Unsupported argument »compare« = »${String(compare)}«`)
      }
      this.logError(`Expect ${this.valuetoString(value)} ${compare} ${this.valuetoString(expect)}.`)
      return false
   }

   /**
    * @param {string|symbol} index
    * @returns {string} Either "'<index>'" or "<index>" if index contains only digits.
    */
   indexToString(index) {
      if (/^(0|[1-9][0-9]*)$/.test(String(index)))
         return String(index)
      return "'"+String(index)+"'"
   }

   compareArray(value, expect, index="") {
      if (!Array.isArray(expect))
         throw Error("Expect argument »expect« of type Array")
      if (!Array.isArray(value))
         this.logError(`Expect${index?" at "+index:""} value ${this.valuetoString(value)} of type Array.`)
      else if (value.length != expect.length)
         this.logError(`Expect${index?" at "+index:""} array length ${value.length} == ${expect.length}.`)
      else {
         for (const entry of value.entries()) {
            if (Array.isArray(expect[entry[0]])) {
               if (!this.compareArray(entry[1], expect[entry[0]], `${index}[${entry[0]}]`))
                  return false
            }
            else if (entry[1] !== expect[entry[0]]) {
               if (typeof entry[1] !== "object") {
                  this.logError(`Expect at ${index}[${entry[0]}] ${this.valuetoString(entry[1])} == ${this.valuetoString(expect[entry[0]])}.`)
                  return false
               }
               if (!this.compareObject(entry[1], expect[entry[0]], `${index}[${entry[0]}]`))
                  return false
            }
         }
         return true
      }
      return false
   }

   compareObject(value, expect, index="") {
      if (typeof expect !== "object")
         throw Error("Expect argument »expect« of type object")
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
               if (!Array.isArray(value[key])) {
                  this.logError(`Expect at ${index}[${this.indexToString(key)}] ${value[key]} == ${expect[key]}.`)
                  return false
               }
               if (!this.compareArray(value[key], expect[key], `${index}[${this.indexToString(key)}]`))
                  return false
            }
         }
         return true
      }
      return false
   }

   compareException(exception, expect) {
      if (exception !== expect) {
         if (expect == null || (typeof expect !== "function" && typeof expect !== "object" && typeof expect !== "string")
            || (typeof exception !== "object")) {
            if (String(exception) !== String(expect))
               this.logError(`Expect exception »${String(exception)}« === »${String(expect)}«.`)
            else
               this.logError(`Expect exception type »${typeof exception}« === »${typeof expect}«.`)
            return false
         }
         else {
            const expectType = (typeof expect === "function" ? expect : typeof expect === "object" ? (expect.constructor ?? Object) : undefined)
            if (expectType && !(exception instanceof expectType)) {
               this.logError(`Expect exception type »${exception.constructor?.name ?? typeof exception}« === »${expectType.name}«.`)
               return false
            }
            else {
               const expectMessage = (typeof expect === "string" ? expect : typeof expect === "object" ? expect.message ?? "": undefined)
               if (expectMessage != null && exception.message !== expectMessage) {
                  if (String(exception.message) !== String(expectMessage))
                     this.logError(`Expect exception message »${String(exception.message)}« === »${String(expectMessage)}«.`)
                  else
                     this.logError(`Expect exception message type »${typeof exception.message}« === »${typeof expectMessage}«.`)
                  return false
               }
            }
         }
      }
      return true
   }

   /**
    * @returns {any} Either value to test for or return value of called function.
    */
   run() {
      this.testenv.signalTestCaseExecuted()
      const isFunction = (typeof this.#testFor === "function")
      this.#value = isFunction ? this.#testFor() : this.#testFor
      return this.#value
   }
   /**
    * @returns {Promise<boolean>}
    */
   async testAsync() {
      try {
         this.#value = await this.#value
         if (this.#compare === "throw")
            this.logError(`Expected exception.`)
      }
      catch(exception) {
         this.#value = exception
         if (this.#compare !== "throw")
            this.logError(`Unexpected exception.`)
      }
      return this.OK && this.compare()
   }
   /**
    * @returns {boolean|Promise<boolean>}
    */
   test() {
      try {
         const value = this.#value = this.run()
         if (value instanceof Promise)
            return this.testAsync()
         if (this.#compare === "throw")
            this.logError(`Expected exception.`)
      }
      catch(exception) {
         this.#value = exception
         if (this.#compare !== "throw")
            this.logError(`Unexpected exception.`)
      }
      return this.OK && this.compare()
   }
}

async function INTERNAL_SYNTAX_TEST()
{
   const testenv = new TestEnvironment()
   const { TEST, RUN_TEST, SUB_TEST } = testenv.export()
   await RUN_TEST({name:"Syntax of TEST",timeout:500}, async (context) => {
      TEST(testenv.runningTest,'=',1, "one test is running")
      TEST(context.parentContext,'=',undefined, "first arg of RUN_TEST points to global runContext")
      const tResult = TEST(0,"=",0,"TEST works")
      TEST(tResult,"=",true,"TEST returns true")
      const testConsole = testenv.newTestConsole()
      const fResult = TEST(0,'=',1,"TEST fails",{testConsole})
      TEST(fResult,"=",false,"TEST returns false")
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
      await SUB_TEST({context, delay:0}, (/*context*/) => {
         TEST(testenv.runningTest,'=',2, "SUB_TEST increments runningTest")
      })
      let sub_test1 = 0, sub_test2 = 0
      SUB_TEST({context, delay:0}, (/*context*/) => {
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
      SUB_TEST({context, delay:10}, async (/*context*/) => {
         TEST(sub_test1,'=',1, "sub test1 executed before sub test2")
         ++ sub_test2
         TEST(()=>{throw Error("msg1")},'throw',"msg1", "TESTTHROW compares message")
         TEST(()=>{throw Error("msg2")},'throw',Error("msg2"), "TESTTHROW compares type and message")
         TEST(()=>{throw Error("msg3")},'throw',{message:"msg3",constructor:Error}, "TESTTHROW compares type and message")
         TEST(()=>{throw Error("msg4")},'throw',Error, "TESTTHROW compares type")
         await TEST(()=>new Promise((_,reject)=>setTimeout(()=>reject(Error("msg7")),10)),"throw",Error("msg7"), "TEST 'throw' supports async functions")
      })
      // RUN_TEST is waiting on nested SUB_TEST (tested with executedTestCase)
      SUB_TEST({context, delay:50}, () => {
         TEST(sub_test1,'=',1, "both sub tests executed exactly once")
         TEST(sub_test2,'=',1, "both sub tests executed exactly once")
      })
      let waitedOnNestedContext = false
      await SUB_TEST({context, delay:0}, (context2) => {
         TEST(context,'=',context2.parentContext, "first arg of SUB_TEST points to child context")
         SUB_TEST({context, delay:0}, (context3) => {
            TEST(context,'=',context3.parentContext, "nested SUB_TEST runs within top level context")
         })
         // outer SUB_TEST is waiting until end of nested SUB_TEST
         // cause of providing argument context
         // if context is undefined RUN_TEST would wait on double nested SUB_TEST
         SUB_TEST({context:context2, delay:100}, (context3) => {
            TEST(context2,'=',context3.parentContext, "nested SUB_TEST runs within context provided as argument")
            waitedOnNestedContext = true
         })
      })
      TEST(waitedOnNestedContext,'=',true, "SUB_TEST waits until nested SUB_TEST has been executed")
   })
   if (testenv.executedTestCase !== 42 || testenv.failedTestCase !== 1 || testenv.failedRunTest || testenv.runningTest !== 0)
      throw Error(`Internal error in TEST module nrExecutedTest=${testenv.executedTestCase} failed=${testenv.failedTestCase} failedRun=${testenv.failedRunTest} nrRunning=${testenv.runningTest}`)
   await RUN_TEST({name:"-- TEST fails --",timeout:100}, async (/*context*/) => {
      const testConsole = testenv.newTestConsole()
      TEST(0,"=",1,"FAIL1",{testConsole})
      testConsole.compare("TEST", [
         ["error","TEST failed: FAIL1"],
         ["log","Reason: Expect 0 = 1."],
         ["log","Tested value",0]
      ])
   })
   await RUN_TEST({name:"-- TEST fails with own reason --",timeout:100}, async (context) => {
      const testConsole = testenv.newTestConsole()
      const testedValue={a:1}
      TEST(testedValue,(v,e) => "tested value "+JSON.stringify(v)+" != "+JSON.stringify(e),{b:1},"FAIL2",{testConsole})
      testConsole.compare("TEST", [
         ["error","TEST failed: FAIL2"],
         ["log","Reason: tested value {\"a\":1} != {\"b\":1}."],
         ["log","Tested value",testedValue]
      ])
   })
   await RUN_TEST({name:"-- TEST 'throw' fails --",timeout:100}, async (/*context*/) => {
      const testConsole = testenv.newTestConsole()
      TEST(()=>0,"throw",0,"TEST fails cause of NO exception",{testConsole})
      testConsole.compare("TEST", [
         ["error","TEST failed: TEST fails cause of NO exception"],
         ["log","Reason: Expected exception."],
         ["log","Tested value",0]
      ])
   })
   if (testenv.executedTestCase !== 84 || testenv.failedTestCase !== 4 || testenv.failedRunTest !== 0 || testenv.runningTest !== 0)
      throw Error(`Internal error in TEST module nrExecutedTest=${testenv.executedTestCase} failed=${testenv.failedTestCase} failedRun=${testenv.failedRunTest} nrRunning=${testenv.runningTest}`)
   await RUN_TEST({name:"-- Timeout --",timeout:100}, async (context) => {
      const testConsole = testenv.newTestConsole()
      const error = await SUB_TEST({context, timeout:20, testConsole}, (context) => {
         SUB_TEST({delay:50,context}, () => {
         })
      })
      testConsole.compare("SUB_TEST", [
         ["error","SUB_TEST failed: -- Timeout --"],
         ["log","Reason: Timeout after 20ms"]
      ])
      TEST(error,"=","Timeout after 20ms","SUB_TEST returns error")
   })
   if (testenv.executedTestCase !== 94 || testenv.failedTestCase !== 4 || testenv.failedRunTest !== 1)
      throw Error(`Internal error in TEST module nrExecutedTest=${testenv.executedTestCase} failed=${testenv.failedTestCase} failedRun=${testenv.failedRunTest}`)
   for (let isSubTest = 0; isSubTest <= 1; ++isSubTest) {
      const testConsole = testenv.newTestConsole()
      const thrownError = Error("-- throw --")
      const error = await RUN_TEST({name:"-- Exception --", testConsole}, (context) => {
         if (isSubTest)
            SUB_TEST({context, testConsole}, (context) => { throw thrownError })
         else
            throw thrownError
      })
      testConsole.compare("RUN_TEST", [
         ["error",(isSubTest?"SUB":"RUN")+"_TEST failed: -- Exception --"],
         ["log","Reason: Exception: Error: -- throw --"],
         ["log","Catched exception",thrownError]
      ])
      TEST(error,"=","Exception: Error: -- throw --","RUN_TEST returns error")
   }
   if (testenv.executedTestCase !== 122 || testenv.failedTestCase !== 4 || testenv.failedRunTest !== 3 || testenv.runningTest !== 0)
      throw Error(`Internal error in TEST module nrExecutedTest=${testenv.executedTestCase} failed=${testenv.failedTestCase} failedRun=${testenv.failedRunTest} nrRunning=${testenv.runningTest}`)
}

function TEST(testValue, compare, expect, errmsg, options) {
   return TestEnvironment.default.test(testValue, compare, expect, errmsg, options)
}

async function RUN_TEST(options, callback) {
   return TestEnvironment.default.runTest(options, callback)
}

async function SUB_TEST(options, callback) {
   return TestEnvironment.default.runSubTest(options, callback)
}

async function END_TEST() {
   return TestEnvironment.default.showStats().finally(() => RESET_TEST())
}

function RESET_TEST() {
   TestEnvironment.default = new TestEnvironment()
}

return { TEST, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST, INTERNAL_SYNTAX_TEST, TestEnvironment }

})()

INTERNAL_SYNTAX_TEST()
