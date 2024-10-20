/**
 * Implements TEST to test for an expected value or exception.
 * To create a private TestEnvironment with own stats use
 * >> const { TEST, RUN_TEST, SUB_TEST, END_TEST } = NEW_TEST()
 * (c) 2024 Jörg Seebohn
 */

// namespace
const { TEST, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST, NEW_TEST } = (() => {

/**
 * Throws Error(errmsg) if typeTest is not true.
 * @param {boolean} typeTest
 * @param {string} errmsg
 */
const EXPECT = (typeTest, errmsg) => { if (!typeTest) throw Error(errmsg) }

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
   /** The number of passed and failed {@link test TEST}. @type {number} */
   executedTestCase = 0
   /** The number of failed {@link test TEST}. @type {number} */
   failedTestCase = 0
   constructor() { }
   newTestConsole() { return new TestConsole(this) }
   signalStartRun() { return (++ this.executedRunTest, ++ this.runningTest) }
   signalEndRun() { return -- this.runningTest }
   signalTestExecuted() { return ++ this.executedTestCase }
   /**
    * @param {undefined|TestConsole} testConsole
    * @param {boolean} isSubTest True says failed test is a SUB_TEST else RUN_TEST.
    * @param {string} name
    * @param {string} reason
    * @param {{value:any}} [exceptionValue]
    * @returns {number} The number of failed RUN_TEST&SUB_TEST.
    */
   signalRunError(testConsole, isSubTest, name, reason, exceptionValue) {
      const logger = testConsole ?? console
      logger.error(`${isSubTest?"SUB":"RUN"}_TEST failed: ${name}`)
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
   signalTestError(testConsole, errmsg, reason, value) {
      const logger = testConsole ?? console
      logger.error(`TEST failed: ${errmsg}`)
      logger.log(`Reason: ${reason}`)
      logger.log("Tested value",value)
      ++ this.failedTestCase
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
    * All sub-tests are started as pseudo parallel async functions.
    * Use await on the return value to ensure that a SUB_TEST has ended its execution before
    * another one is started. The containing context (either RUN_TEST or SUB_TEST)
    * waits for all contained SUB_TEST. The containing context is given in the config parameter context.
    * @param {{context:RunContext, timeout?:number, delay?:number, testConsole?:TestConsole}} config
    * @param {(context:RunContext)=>void|Promise<void>} callback An asynchronous callback which is called after delay milliseconds.
    * @returns {Promise<string>}
    */
   async runSubTest({timeout=0, delay=0, context, testConsole}, callback) {
      EXPECT(context instanceof RunContext, "Missing argument »context« of type RunContext.")
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
    * Exports interface to simplify usage.
    * Exported methods which are bound to this object are:
    * * {@link test} as "TEST"
    * * {@link runTest} as "RUN_TEST"
    * * {@link runSubTest} as "SUB_TEST"
    * * {@link showStats} as "END_TEST"
    * @returns {{
    *    TEST:(testValue:any,compare:string|((value:any,expect:any)=>boolean|string),expect:any,errmsg:string|(()=>string),options?:{testConsole?:TestConsole})=>boolean,
    *    RUN_TEST:(config:{name:string,timeout?:number,delay?:number,testConsole?:TestConsole},callback:(context:RunContext)=>void|Promise<void>)=>Promise<string>,
    *    SUB_TEST:(config:{timeout?:number,delay?:number,context?:RunContext,testConsole?:TestConsole},callback:(context:RunContext)=>void|Promise<void>)=>Promise<string>,
    *    END_TEST:(options?:{testConsole?:TestConsole})=>Promise<void>,
    * }}
    */
   export() {
      return {
         TEST: this.test.bind(this),
         RUN_TEST: this.runTest.bind(this),
         SUB_TEST: this.runSubTest.bind(this),
         END_TEST: this.showStats.bind(this),
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
      EXPECT(typeof name === "string", "Expect argument »name« of type string.")
      EXPECT(isFinite(timeout), "Expect argument »timeout« of type number.")
      EXPECT(isFinite(delay), "Expect argument »delay« of type number.")
      EXPECT(typeof callback === "function", "Expect argument »callback« of type function.")
      EXPECT(parentContext == null || parentContext instanceof RunContext, "Expect argument »parentContext« of type RunContext.")
      EXPECT(testenv instanceof TestEnvironment, "Expect argument »testenv« of type TestEnvironment.")
      EXPECT(testConsole == null || testConsole instanceof TestConsole, "Expect argument »testConsole« of type TestConsole.")
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
      this.testenv.signalRunError(this.testConsole, Boolean(this.parentContext), this.name, reason, exceptionValue)
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
         .then(() => this.testenv.signalEndRun())
         .then(() => this.signaledError)
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
      EXPECT(typeof compare === "string" || typeof compare === "function", "Expect argument »compare« of type string or function.")
      EXPECT(typeof errmsg === "string" || typeof errmsg === "function", "Expect argument »errmsg« of type string or function.")
      EXPECT(testenv instanceof TestEnvironment, "Expect argument testenv« of type TestEnvironment.")
      EXPECT(testConsole == null || testConsole instanceof TestConsole, "Expect argument »testConsole« of type TestConsole.")
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
    * @returns {false}
    */
   logError(reason) {
      this.testenv.signalTestError(this.#testConsole, this.errmsg, reason, this.#value)
      return this.#testOK = false
   }
   /**
    * @returns {string} The message which describes the failed TEST.
    */
   get errmsg() {
      return typeof this.#errmsg === "function"
            ? this.#errmsg()
            : this.#errmsg
   }
   /**
    * @param {any} value
    * @returns {string} String representation of value (Arrays are enclosed into '[...]').
    */
   valuetoString(value) {
      return (Array.isArray(value))
            ? "["+String(value)+"]"
            : String(value)
   }
   /**
    * @returns {boolean} True in case comparison holds else false.
    */
   compare() {
      if (typeof this.#compare === "function")
         return this.compareFunction(this.#compare, this.#value, this.#expect)
      return this.compareValue(this.#compare, this.#value, this.#expect)
   }
   /**
    * @param {(value:any, expect:any)=>boolean|string} cmpFct
    * @param {any} value
    * @param {any} expect
    * @returns {boolean} True in case comparison holds else false.
    */
   compareFunction(cmpFct, value, expect) {
      const result = cmpFct(value, expect)
      if (typeof result === "string" && result !== "")
         return this.logError(`${result}.`)
      else if (typeof result !== "string" && !Boolean(result))
         return this.logError(`Expect ${String(value)} === ${String(expect)}.`)
      return true
   }
   /**
    * @param {boolean} cmpResult Reuslt of comparison: true if it holds else false.
    * @param {string} compare
    * @param {any} value
    * @param {any} expect
    * @returns {boolean} Returns cmparison result.
    */
   check(cmpResult, compare, value, expect) {
      return cmpResult || (this.logError(`Expect ${this.valuetoString(value)} ${compare} ${this.valuetoString(expect)}.`), false)
   }
   /**
    * @param {string} compare
    * @param {any} value
    * @param {any} expect
    * @returns {boolean} True in case comparison holds else false.
    */
   compareValue(compare, value, expect) {
      switch (compare) {
         case "<":
            return this.check(value < expect, compare, value, expect)
         case ">":
            return this.check(value > expect, compare, value, expect)
         case "<=":
            return this.check(value <= expect, compare, value, expect)
         case ">=":
            return this.check(value >= expect, compare, value, expect)
         case "!range":
            EXPECT(Array.isArray(expect) && expect.length === 2, "Expect argument »expect« of type [lowerBound,upperBound].")
            return this.check(value < expect[0] || expect[1] < value, compare, value, expect)
         case "range":
            EXPECT(Array.isArray(expect) && expect.length === 2, "Expect argument »expect« of type [lowerBound,upperBound].")
            return this.check(expect[0] <= value && value <= expect[1], compare, value, expect)
         case "==":
            return this.check(value == expect, compare, value, expect)
         case "!=":
            return this.check(value != expect, compare, value, expect)
         case "!==":
            return this.check(value !== expect, compare, value, expect)
         case "=": case "===":
            return this.check(value === expect, compare, value, expect)
         case "{=}":
            return this.compareObject(value, expect)
         case "[=]":
            return this.compareArray(value, expect)
         case "throw":
            return this.logError(`Expected exception.`)
         default:
            throw Error(`Unsupported argument »compare« = »${String(compare)}«`)
      }
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
   /**
    * @param {any[]} value
    * @param {any[]} expect
    * @param {string} [index]
    * @returns {boolean} True if value equals expect.
    */
   compareArray(value, expect, index="") {
      EXPECT(Array.isArray(expect), "Expect argument »expect« of type Array.")
      if (!Array.isArray(value))
         return this.logError(`Expect${index?" at "+index:""} value ${this.valuetoString(value)} of type Array.`)
      else if (value.length != expect.length)
         return this.logError(`Expect${index?" at "+index:""} array length ${value.length} == ${expect.length}.`)
      else {
         for (const entry of value.entries()) {
            if (Array.isArray(expect[entry[0]])) {
               if (!this.compareArray(entry[1], expect[entry[0]], `${index}[${entry[0]}]`))
                  return false
            }
            else if (entry[1] !== expect[entry[0]]) {
               if (typeof entry[1] !== "object")
                  return this.logError(`Expect at ${index}[${entry[0]}] ${this.valuetoString(entry[1])} == ${this.valuetoString(expect[entry[0]])}.`)
               if (!this.compareObject(entry[1], expect[entry[0]], `${index}[${entry[0]}]`))
                  return false
            }
         }
      }
      return true
   }
   /**
    * @param {{[x:string|symbol]: any}} value
    * @param {{[x:string|symbol]: any}} expect
    * @param {string} [index]
    * @returns {boolean} True if value equals expect.
    */
   compareObject(value, expect, index="") {
      EXPECT(typeof expect === "object", "Expect argument »expect« of type object.")
      if (typeof value !== "object")
         return this.logError(`Expect${index?" at "+index:""} value ${this.valuetoString(value)} of type object.`)
      else {
         const keys = new Set([...Reflect.ownKeys(value),...Reflect.ownKeys(expect)])
         for (const key of keys) {
            if (expect[key] !== null && typeof expect[key] === "object") {
               if (!this.compareObject(value[key], expect[key], `${index}[${this.indexToString(key)}]`))
                  return false
            }
            else if (value[key] !== expect[key]) {
               if (!Array.isArray(value[key]))
                  return this.logError(`Expect at ${index}[${this.indexToString(key)}] ${value[key]} == ${expect[key]}.`)
               if (!this.compareArray(value[key], expect[key], `${index}[${this.indexToString(key)}]`))
                  return false
            }
         }
      }
      return true
   }
   /**
    * Compares exception.
    * @param {any} exception The catched exception
    * @returns {boolean} True in case catched exception equals expected value.
    */
   compareException(exception) {
      const expect = this.#expect
      this.#value = exception
      if (this.#compare !== "throw")
         return this.logError(`Unexpected exception.`)
      if (exception !== expect) {
         if (expect == null || (typeof expect !== "function" && typeof expect !== "object" && typeof expect !== "string")
            || (typeof exception !== "object")) {
            if (String(exception) !== String(expect))
               return this.logError(`Expect exception »${String(exception)}« === »${String(expect)}«.`)
            else
               return this.logError(`Expect exception type »${typeof exception}« === »${typeof expect}«.`)
         }
         else {
            const expectType = (typeof expect === "function" ? expect : typeof expect === "object" ? (expect.constructor ?? Object) : undefined)
            if (expectType && !(exception instanceof expectType))
               return this.logError(`Expect exception type »${exception.constructor?.name ?? typeof exception}« === »${expectType.name}«.`)
            else {
               const expectMessage = (typeof expect === "string" ? expect : typeof expect === "object" ? expect.message ?? "": undefined)
               if (expectMessage != null && exception.message !== expectMessage) {
                  if (String(exception.message) !== String(expectMessage))
                     return this.logError(`Expect exception message »${String(exception.message)}« === »${String(expectMessage)}«.`)
                  else
                     return this.logError(`Expect exception message type »${typeof exception.message}« === »${typeof expectMessage}«.`)
               }
            }
         }
      }
      return true
   }
   /**
    * @returns {any} Returns value to test for or returned value of called function.
    */
   value() {
      return this.#value = (typeof this.#testFor === "function")
                           ? this.#testFor() : this.#testFor
   }
   /**
    * @returns {Promise<boolean>}
    */
   async testAsync() {
      try {
         this.#value = await this.#value
      }
      catch(exception) {
         return this.compareException(exception)
      }
      return this.compare()
   }
   /**
    * @returns {boolean|Promise<boolean>}
    */
   test() {
      this.testenv.signalTestExecuted()
      try {
         if (this.value() instanceof Promise)
            return this.testAsync()
      }
      catch(exception) {
         return this.compareException(exception)
      }
      return this.compare()
   }
}

async function INTERNAL_SYNTAX_TEST()
{
   const { TEST, RUN_TEST, SUB_TEST, TEST_ENV: testenv } = NEW_TEST()
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
         TEST(()=>{throw Error("msg1")},'throw',"msg1", "TEST throw compares message")
         TEST(()=>{throw Error("msg2")},'throw',Error("msg2"), "TEST throw compares type and message")
         TEST(()=>{throw Error("msg3")},'throw',{message:"msg3",constructor:Error}, "TEST throw compares type and message")
         TEST(()=>{throw Error("msg4")},'throw',Error, "TEST throw compares type")
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
   await RUN_TEST({name:"-- TEST expected / unexpected exceptions --",timeout:100}, async (/*context*/) => {
      for (const i of [1,2,3,4]) {
         const testConsole = testenv.newTestConsole()
         i === 1 && TEST(()=>1,"throw",0,"TEST fails cause of expected exception",{testConsole})
         i === 2 && await TEST(async ()=>2,"throw",0,"TEST fails cause of expected exception",{testConsole})
         i === 3 && TEST(()=>{throw 3},"=",0,"TEST fails cause of unexpected exception",{testConsole})
         i === 4 && await TEST(async ()=>{throw 4},"=",0,"TEST fails cause of unexpected exception",{testConsole})
         testConsole.compare("TEST", [
            ["error","TEST failed: TEST fails cause of "+(i > 2 ? "un":"")+"expected exception"],
            ["log","Reason: "+(i > 2 ? "Une":"E")+"xpected exception."],
            ["log","Tested value",i],
         ])
      }
   })
   if (testenv.executedTestCase !== 126 || testenv.failedTestCase !== 7 || testenv.failedRunTest !== 0 || testenv.runningTest !== 0)
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
   if (testenv.executedTestCase !== 136 || testenv.failedTestCase !== 7 || testenv.failedRunTest !== 1)
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
   if (testenv.executedTestCase !== 164 || testenv.failedTestCase !== 7 || testenv.failedRunTest !== 3 || testenv.runningTest !== 0)
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

async function END_TEST(options) {
   return TestEnvironment.default.showStats(options).finally(() => RESET_TEST())
}

function RESET_TEST() {
   TestEnvironment.default = new TestEnvironment()
}

function NEW_TEST() {
   const TEST_ENV = new TestEnvironment()
   return { ...TEST_ENV.export(), TEST_ENV }
}

INTERNAL_SYNTAX_TEST()

return { TEST, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST, NEW_TEST }

})()
