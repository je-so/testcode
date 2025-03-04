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
 * Interface which is used to log errors during test and ot show end result.
 * In case of a failing TEST or RUN_TEST:
 * The first call is made to error to log a stack trace in the console for showing the location of the failed test.
 * The log call is made next to give a reason for its failure.
 * The last call is made to log to show the tested value or the value of a catched exception.
 * As default implementation console is used but a customized implementation
 * could be set in the constructor of TestEnvironment and as first argument to {@link NEW_TEST}.
 */
class LogInterface {
  /**
   * @param {string} msg
   */
  error(msg) { console.error(msg) }
  /**
    * @param {...any} args
    */
  log(...args) { console.log(...args) }
  /**
   * @param {undefined|LogInterface} log
   * @returns {boolean} True in case parameter log conforms to type LogInterface.
   */
  static isTypeOrNull(log) { return log == null || (typeof log.error === "function" && typeof log.log === "function") }
}

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
   /**
    * The wrapper for the console output.
    * {@type LogInterface}
    */
   log
   /**
    * @param {LogInterface} [log]
    */
   constructor(log) {
      EXPECT(LogInterface.isTypeOrNull(log), "Expect argument »log« of type LogInterface.")
      this.log = log ?? console
   }
   /**
    * @param {undefined|LogInterface} testConsole
    * @returns {LogInterface}
    */
   getLog(testConsole) { return testConsole ?? this.log }
   newTestConsole() { return new TestConsole(this) }
   signalStartRun() { return (++ this.executedRunTest, ++ this.runningTest) }
   signalEndRun() { return -- this.runningTest }
   signalTestExecuted() { return ++ this.executedTestCase }
   /**
    * @param {undefined|LogInterface} testConsole
    * @param {boolean} isSubTest True says failed test is a SUB_TEST else RUN_TEST.
    * @param {string} name
    * @param {string} reason
    * @param {{value:any}} [exceptionValue]
    * @returns {number} The number of failed RUN_TEST&SUB_TEST.
    */
   signalRunError(testConsole, isSubTest, name, reason, exceptionValue) {
      const log = this.getLog(testConsole)
      log.error(`${isSubTest?"SUB":"RUN"}_TEST failed: ${name}`)
      log.log(`Reason: ${reason}`)
      exceptionValue && log.log("Catched exception",exceptionValue.value)
      return ++ this.failedRunTest
   }
   /**
    * @param {undefined|LogInterface} testConsole
    * @param {string} errmsg
    * @param {string} reason
    * @param {any} value
    */
   signalTestError(testConsole, errmsg, reason, value) {
      const log = this.getLog(testConsole)
      log.error(`TEST failed: ${errmsg}`)
      log.log(`Reason: ${reason}`)
      log.log("Tested value",value)
      ++ this.failedTestCase
   }
   /**
    * Shows test stats on console.
    * @param {{testConsole?:LogInterface}} [options]
    */
   async showStats({testConsole}={}) {
      const log = this.getLog(testConsole)
      if (this.runningTest > 0) {
         await new Promise((resolve) => {
            let counter = 1
            const startTimer = () => setTimeout(() => {
               if (this.runningTest > 0) {
                  startTimer()
                  ++ counter
                  if (counter % 4 === 0)
                     log.log("***** Waiting for TEST RESULT *****")
               }
               else
                  resolve(true)
            }, 250)
            startTimer()
         })
      }
      log.log("***** TEST RESULT *****")
      log.log(` Executed TEST: ${this.executedTestCase}`)
      log.log(` Executed [RUN|SUB]_TEST: ${this.executedRunTest}`)
      if (!this.failedTestCase && !this.failedRunTest)
         log.log(` All tests working :-)`)
      else {
         if (this.failedTestCase)
            log.error(` Failed TEST: ${this.failedTestCase}`)
         if (this.failedRunTest)
            log.error(` Failed [RUN|SUB]_TEST: ${this.failedRunTest} `)
      }
   }

   /**
    * Options for running a test. See {@link RUN_TEST}.
    * @typedef {object} RunOptions
    * @property {string} name
    * @property {number} [timeout]
    * @property {number} [delay]
    * @property {LogInterface} [testConsole]
    */

   /**
    * Options for running a test. See {@link RUN_TEST}.
    * @typedef {object} RunOptions2
    * @property {string} [name]
    * @property {number} [timeout]
    * @property {number} [delay]
    * @property {LogInterface} [testConsole]
    */

   /**
    * Options for running a sub test. See {@link SUB_TEST}.
    * @typedef {object} SubOptions
    * @property {RunContext} context
    * @property {number} [timeout]
    * @property {number} [delay]
    * @property {LogInterface} [testConsole]
    */

   /**
     * An asynchronous callback which is called after delay milliseconds.
     * @typedef {(context:RunContext)=>void|Promise<void>} RunCallback
     */

   /**
    * Supports 2 or 3 arguments. The callback must be supplied as 2nd argument or 3d.
    * If supplied as 2nd argument the 3d one is not used.
    * @param {string|RunOptions} arg1 Name of test or Options. If arg1 contains the name the »name« within Options is not used and could be undefined.
    * @param {RunOptions2|RunCallback} arg2 Either options or callback. If arg1 provides the options then arg2 should contain the callback.
    * @param {RunCallback} arg3 An asynchronous callback which is called after delay milliseconds.
    * @returns {Promise<string>}
    */
   async runTest(arg1, arg2, arg3) {
      const callback = typeof arg2 === "function" ? arg2 : arg3
      const name = typeof arg1 === "string" ? arg1 : arg1.name
      const {timeout=0, delay=0, testConsole} = typeof arg2 === "object" ? arg2 : typeof arg1 === "object" ? arg1 : {}
      const runContext = new RunContext({name, timeout, delay, callback, testenv: this, testConsole})
      return runContext.run()
   }

   /**
    * Runs a sequence of {@link test TEST} or {@link runSubTest SUB_TEST}.
    * All sub-tests are started as pseudo parallel async functions.
    * Use await on the return value to ensure that a SUB_TEST has ended its execution before
    * another one is started. The containing context (either RUN_TEST or SUB_TEST)
    * waits for all contained SUB_TEST. The containing context is given in the config parameter context.
    * @param {SubOptions} options
    * @param {RunCallback} callback An asynchronous callback which is called after delay milliseconds.
    * @returns {Promise<string>}
    */
   async runSubTest({timeout=0, delay=0, context, testConsole}, callback) {
      EXPECT(context instanceof RunContext, "Missing argument »context« of type RunContext.")
      const childContext = new RunContext({name:context.name, timeout, delay, callback, context, testenv: this, testConsole})
      return childContext.run()
   }

   /**
    * @param {any|(()=>any)} testedValue
    * @param {string|((testValue:any, expect:any)=>boolean)} compare Either built in comparison functions (string) or a comparison function. Returning a non empty string is considered the reason why the comparison failed. Returning false is also considered as a failed test.
    * @param {any} expect The expected value »testValue« should be compared with.
    * @param {string|(() => string)} errmsg The logged error message if TEST fails.
    * @param {{testConsole?:LogInterface, context?:RunContext}} [options]
    * @returns {boolean|Promise<boolean>} True indicates a successful test.
    */
   test(testedValue, compare, expect, errmsg, options) {
      const tc = new TestCase({testedValue, compare, expect, errmsg, testenv:this, testConsole:options?.testConsole, context:options?.context})
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
    *    TEST:(testValue:any,compare:string|((value:any,expect:any)=>boolean|string),expect:any,errmsg:string|(()=>string),options?:{testConsole?:LogInterface,context?:RunContext})=>boolean,
    *    RUN_TEST:(name:string|RunOptions,options:RunOptions2|RunCallback,callback?:RunCallback)=>Promise<string>,
    *    SUB_TEST:(options:{timeout?:number,delay?:number,context?:RunContext,testConsole?:LogInterface},callback:(context:RunContext)=>void|Promise<void>)=>Promise<string>,
    *    END_TEST:(options?:{testConsole?:LogInterface})=>Promise<void>,
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
   /**
    * @param {"TEST"|"RUN_TEST"|"SUB_TEST"} name
    * @param {["log"|"error", string, any?][]} expect
    */
   compare(name, expect) {
      const calls = this.calls
      const { TEST } = this.testenv.export()
      TEST(calls.length,"=",expect.length,`Expect ${name} to call log ${expect.length} times`)
      for (let i=0; i<calls.length; ++i) {
         TEST(calls[i][0],"=",expect[i][0],`Expect ${name} to make the ${i+1}. call to console.${expect[i][0]}`)
         TEST(calls[i][1],"=",expect[i][1],`Expect ${name} to make the ${i+1}. call to console with correct 1st argument`)
         TEST(calls[i].length,"<=",3,`Expect ${name} to make the ${i+1}. call to console with no more than 2 arguments`)
         TEST(calls[i].length,"=",expect[i].length,`Expect ${name} to make the ${i+1}. call to console with exact number of arguments`)
         if (calls[i].length == 3)
            TEST(calls[i][2],"=",expect[i][2],`Expect ${name} to make the ${i+1}. call to console with correct 2nd argument`)
      }
   }
}

class RunContext {
   static ID = 0

   ID = ++RunContext.ID
   /** @type {RunContext[]} */
   subContext = []
   /** @type {TestCase[]} */
   asyncTestCases = []
   /** @type {Promise<string>} */
   waitAll = Promise.resolve("Error: run() not called")
   /** @type {string} */
   signaledError = "Error: run() not called"
   /** @type {undefined|RunContext} */
   parentContext
   /** @type {string} */
   name
   /** @type {number} timeout in millisecons ms after which callback should return. A value <= 0 disables the timeout. */
   timeout
   /** @type {number} delay in milliseconds after callback is called. A value <= 0 disables the delay. */
   delay
   /** @type {(context:RunContext)=>void|Promise<void>} Async callback is supported */
   callback
   /** @type {null|number} Timer which is used for measuring timeout. */
   timer
   /** @type {TestEnvironment} */
   testenv
   /** @type {undefined|LogInterface} */
   testConsole

   toString() {
      if (this.parentContext)
         return `RunContext(ID:${this.ID},name:»${this.name}«,parent:{${String(this.parentContext)}})`
      else
         return `RunContext(ID:${this.ID},name:»${this.name}«)`
   }

   /**
    * @param {{name:string, timeout:number, delay:number, callback:(context:RunContext)=>void|Promise<void>, context?:RunContext, testenv:TestEnvironment, testConsole:undefined|LogInterface}} options
    */
   constructor({name, timeout, delay, callback, context, testenv, testConsole}) {
      EXPECT(typeof name === "string", "Expect argument »name« of type string.")
      EXPECT(isFinite(timeout), "Expect argument »timeout« of type number.")
      EXPECT(isFinite(delay), "Expect argument »delay« of type number.")
      EXPECT(typeof callback === "function", "Expect argument »callback« of type function.")
      EXPECT(context == null || context instanceof RunContext, "Expect argument »context« of type RunContext.")
      EXPECT(testenv instanceof TestEnvironment, "Expect argument »testenv« of type TestEnvironment.")
      EXPECT(LogInterface.isTypeOrNull(testConsole), "Expect argument »testConsole« of type LogInterface.")
      this.name = name
      this.timeout = timeout
      this.delay = delay
      this.callback = callback
      this.timer = null
      this.parentContext = context
      this.testenv = testenv
      this.testConsole = testConsole
      context?.subContext.push(this)
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

   async waitForAllTestCases() {
      for (const tc of this.asyncTestCases) {
         await tc.waitForCompletion()
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
         .then(() => this.waitForAllTestCases())
         .then(() => this.clearTimer())
         .then(() => this.testenv.signalEndRun())
         .then(() => this.signaledError)
      return this.waitAll
   }
}

class TestCase {
   #testedValue
   #value
   #compare
   #expect
   #testOK
   /** @type {string|(()=>string)} */
   #errmsg
   /** @type {TestEnvironment} */
   #testenv
   /** @type {undefined|LogInterface} */
   #testConsole
   /** @type {undefined|RunContext} */
   #context
   /** @type {undefined|Promise} */
   #waitForCompletion

   /**
    * @typedef {object} TestCaseParams
    * @property {any|(()=>any)} testedValue
    * @property {string|((value:any,expect:any)=>boolean|string)} compare
    * @property {any} expect
    * @property {string|(()=>string)} errmsg
    * @property {TestEnvironment} testenv
    * @property {undefined|LogInterface} testConsole
    * @property {RunContext} [context]
    */

   /**
    * @param {TestCaseParams} params
    */
   constructor({testedValue, compare, expect, errmsg, testenv, testConsole, context}) {
      EXPECT(typeof compare === "string" || typeof compare === "function", "Expect argument »compare« of type string or function.")
      EXPECT(typeof errmsg === "string" || typeof errmsg === "function", "Expect argument »errmsg« of type string or function.")
      EXPECT(testenv instanceof TestEnvironment, "Expect argument »testenv« of type TestEnvironment.")
      EXPECT(LogInterface.isTypeOrNull(testConsole), "Expect argument »testConsole« of type LogInterface.")
      EXPECT(!context || context instanceof RunContext, "Expect argument »context« of type RunContext.")
      this.#testedValue = testedValue
      this.#value = expect
      this.#compare = compare
      this.#expect = expect
      this.#testOK = true
      this.#errmsg = errmsg
      this.#testenv = testenv
      this.#testConsole = testConsole
      this.#context = context
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
      return this.compareValue(this.#compare, this.#value, this.#expect)
   }
   /**
    * @param {(value:any, expect:any)=>boolean|string} cmpFct
    * @param {any} value
    * @param {any} expect
    * @returns {boolean} True in case comparison holds else false.
    */
   compareWithFunction(cmpFct, value, expect) {
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
      return cmpResult || this.logError(`Expect ${this.valuetoString(value)} ${compare} ${this.valuetoString(expect)}.`)
   }
   /**
    * @param {string|((value:any, expect:any)=>boolean|string)} compare
    * @param {any} value
    * @param {any} expect
    * @returns {boolean} True in case comparison holds else false.
    */
   compareValue(compare, value, expect) {
      if (typeof compare === "function")
         return this.compareWithFunction(compare, value, expect)
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
      return this.#value = (typeof this.#testedValue === "function")
                           ? this.#testedValue() : this.#testedValue
   }
   /**
    * @returns {Promise<undefined|"">}
    */
   async waitForCompletion() {
      return await this.#waitForCompletion
   }
   /**
    * @returns {undefined|((result:"")=>void)}
    */
   prepareWaitForCompletion() {
      if (this.#context) {
         let resolve
         this.#waitForCompletion = new Promise((res)=>resolve=res)
         this.#context.asyncTestCases.push(this)
         return resolve
      }
   }
   /**
    * @returns {Promise<boolean>}
    */
   async testAsync() {
      const resolve = this.prepareWaitForCompletion()
      try {
         this.#value = await this.#value
      }
      catch(exception) {
         return this.compareException(exception)
      }
      finally {
         resolve?.("")
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
   let nestedContextCounter = 0
   let nrExecutedTest=0
   const runTestResult = await RUN_TEST("Syntax of TEST", async (context) => {
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
      TEST(100,(v,e)=>e!==v,101, "use comparison function v_alue and e_xpected value")
      TEST(testenv.executedTestCase,'=',23, "testenv.executedTestCase counts nr of executed tests")
      nrExecutedTest+=24
      ////////////////////////
      // complex comparison //
      ////////////////////////
      // two arrays
      const a1=[1,2,3], a2=[2,3,4], a3=[2,3,4]
      for (const i of [0,1,2])
         if (!TEST(a1[i],'<',a2[i], () => `a1[${i}] < a2[${i}]`))
            break
      TEST(a2,"[=]",a3,"array comparison '[=]' supports comparing for equality")
      // two objects
      const s1 = Symbol(1)
      const o1 = { [s1]: 1, name:"tester" }, o2 = { [s1]: 1, name:"tester" }
      TEST(o1,"{=}",o2,"object comparison '{=}' compares o1 equal to o2") // 30
      nrExecutedTest+=5
      TEST(testenv.executedTestCase,'=',nrExecutedTest, "testenv.executedTestCase counts nr of executed tests")
      //////////////////////////
      // exception comparison //
      //////////////////////////
      TEST(()=>{throw Error("msg1")},'throw',"msg1", "'throw' compares message of thrown error")
      TEST(()=>{throw Error("msg2")},'throw',Error("msg2"), "'throw' compares type and message")
      TEST(()=>{throw Error("msg3")},'throw',{message:"msg3",constructor:Error}, "'throw' compares type and message")
      TEST(()=>{throw Error("msg4")},'throw',Error, "'throw' compares only type")
      await TEST(()=>new Promise((_,reject)=>setTimeout(()=>reject(Error("msg5")),10)),"throw",Error("msg5"), "'throw' supports async functions")
      nrExecutedTest+=6
      TEST(testenv.executedTestCase,'=',nrExecutedTest, "testenv.executedTestCase counts nr of executed tests")
      ///////////////
      // sub tests //
      ///////////////
      TEST(testenv.runningTest,'=',1, "RUN_TEST incremented runningTest")
      await SUB_TEST({context, delay:0}, (/*context*/) => {
         TEST(testenv.runningTest,'=',2, "SUB_TEST increments runningTest")
         ++ nestedContextCounter
      })
      let timerSignaled=false
      await SUB_TEST({context}, (subcontext) => {
         TEST(()=>new Promise(resolve=>setTimeout(()=>{timerSignaled=true;resolve("timeout")},100)),"=","timeout","wait for 100ms",{context:subcontext})
      })
      TEST(timerSignaled,"=",true,"SUB_TEST waits on TEST if context is provided in options")
      let thrownException=false
      await SUB_TEST({context}, (subcontext) => {
         TEST(()=>new Promise((_,reject)=>setTimeout(()=>{thrownException=true;reject("timeout")},100)),"throw","timeout","wait for 100ms",{context:subcontext})
      })
      TEST(thrownException,"=",true,"SUB_TEST waits on TEST if context is provided in options")
      SUB_TEST({context, delay:0}, (/*context*/) => {
         TEST(nestedContextCounter,'=',1, "2nd SUB_TEST executed after first")
         ++ nestedContextCounter
      })
      SUB_TEST({context, delay:10}, async (/*context*/) => {
         TEST(nestedContextCounter,'=',2, "3d SUB_TEST executes after 2nd")
         ++ nestedContextCounter
      })
      SUB_TEST({context, delay:30}, () => {
         TEST(nestedContextCounter,'=',3, "4th SUB_TEST executes after 3d")
         ++ nestedContextCounter
      })
      let waitedOnNestedContext = false
      await SUB_TEST({context, delay:0}, (context2) => {
         TEST(context,'=',context2.parentContext, "first arg of SUB_TEST points to child context")
         // RUN_TEST waits on double nested SUB_TEST (cause of context)
         SUB_TEST({context, delay:100}, (context3) => {
            TEST(context,'=',context3.parentContext, "nested SUB_TEST runs within top level context")
            ++ nestedContextCounter
         })
         // outer SUB_TEST waits on nested SUB_TEST (cause of context)
         SUB_TEST({context:context2, delay:50}, (context3) => {
            TEST(context2,'=',context3.parentContext, "nested SUB_TEST runs within context2 (context of outer SUB_TEST)")
            waitedOnNestedContext = true
         })
      })
      TEST(waitedOnNestedContext,'=',true, "SUB_TEST waits until 2nd nested SUB_TEST has been executed")
      TEST(nestedContextCounter,'=',4, "SUB_TEST did not wait on first nested SUB_TEST")
   })
   TEST(5,'=',nestedContextCounter, "RUN_TEST waited on nested and double nested SUB_TEST")
   TEST(runTestResult,'=',"", "RUN_TEST returns empty string in case of success")
   TEST(testenv.failedTestCase,'=',1, "failedTestCase counts nr of failed TEST")
   TEST(testenv.failedRunTest,'=',0, "no RUN_TEST/SUB_TEST failed")
   TEST(testenv.runningTest,'=',0, "no running tests at this point")
   nrExecutedTest+=20
   TEST(testenv.executedTestCase,'=',nrExecutedTest, "testenv.executedTestCase counts nr of executed tests")
   /////////////////////////
   // Test Console Output //
   /////////////////////////
   await RUN_TEST("check console output of failing TEST",
                  {timeout:100}, async (/*context*/) => {
      const testConsole = testenv.newTestConsole()
      TEST(0,"=",1,"FAIL1",{testConsole})
      testConsole.compare("TEST", [
         ["error","TEST failed: FAIL1"],
         ["log","Reason: Expect 0 = 1."],
         ["log","Tested value",0]
      ])
   })
   nrExecutedTest+=16
   await RUN_TEST("check returned reason of comparison function and generated FAIL message",
                  {timeout:100}, async (context) => {
      const testConsole = testenv.newTestConsole()
      const testedValue={a:1}
      const failMsg="FAILED"+Math.floor(Math.random() * 1000000)
      const generateMessage=()=>failMsg
      TEST(testedValue,(v,e) => "tested value "+JSON.stringify(v)+" != "+JSON.stringify(e),{b:1},generateMessage,{testConsole})
      testConsole.compare("TEST", [
         ["error","TEST failed: "+failMsg],
         ["log","Reason: tested value {\"a\":1} != {\"b\":1}."],
         ["log","Tested value",testedValue]
      ])
   })
   nrExecutedTest+=15
   await RUN_TEST("check console output with expected / unexpected exceptions",
                  {timeout:100}, async (/*context*/) => {
      for (const i of [1,2,3,4]) {
         const testConsole = testenv.newTestConsole()
         i === 1 && TEST(()=>1,"throw",0,"fails with expected exception",{testConsole})
         i === 2 && await TEST(async ()=>2,"throw",0,"async fails with expected exception",{testConsole})
         i === 3 && TEST(()=>{throw 3},"=",0,"fails with unexpected exception",{testConsole})
         i === 4 && await TEST(async ()=>{throw 4},"=",0,"async fails with unexpected exception",{testConsole})
         testConsole.compare("TEST", [
            ["error",`TEST failed: ${i%2?'':'async '}fails with ${i>2?'un':''}expected exception`],
            ["log","Reason: "+(i > 2 ? "Une":"E")+"xpected exception."],
            ["log","Tested value",i],
         ])
      }
   })
   nrExecutedTest+=60
   TEST(testenv.executedTestCase,'=',nrExecutedTest, "testenv.executedTestCase counts nr of executed tests")
   TEST(testenv.failedTestCase,'=',7, "failedTestCase counts nr of failed TEST")
   TEST(testenv.failedRunTest,'=',0, "no RUN_TEST/SUB_TEST failed")
   TEST(testenv.runningTest,'=',0, "no running tests at this point")
   //////////////////////////////
   // Failed RUN_TEST/SUB_TEST //
   //////////////////////////////
   // Timeout
   await RUN_TEST("SUB_TEST fails with timeout", async (context) => {
      const testConsole = testenv.newTestConsole()
      const error = await SUB_TEST({context, timeout:20, testConsole}, (context) => {
         SUB_TEST({delay:50,context}, () => {
         })
      })
      testConsole.compare("SUB_TEST", [
         ["error","SUB_TEST failed: SUB_TEST fails with timeout"],
         ["log","Reason: Timeout after 20ms"]
      ])
      TEST(error,"=","Timeout after 20ms","SUB_TEST returns error")
   })
   nrExecutedTest+=14
   TEST(testenv.executedTestCase,'=',nrExecutedTest, "testenv.executedTestCase counts nr of executed tests")
   TEST(testenv.failedTestCase,'=',7, "failedTestCase counts nr of failed TEST")
   TEST(testenv.failedRunTest,'=',1, "one SUB_TEST failed")
   TEST(testenv.runningTest,'=',0, "no running tests at this point")
   // RUN_TEST returns error
   for (let isSubTest = 0; isSubTest <= 1; ++isSubTest) {
      const testConsole = testenv.newTestConsole()
      const thrownError = Error("-- throw --")
      const error = await RUN_TEST("RUN_TEST returns error",
                                    {testConsole}, (context) => {
         if (isSubTest)
            SUB_TEST({context, testConsole}, (context) => { throw thrownError })
         else
            throw thrownError
      })
      testConsole.compare("RUN_TEST", [
         ["error",(isSubTest?"SUB":"RUN")+"_TEST failed: RUN_TEST returns error"],
         ["log","Reason: Exception: Error: -- throw --"],
         ["log","Catched exception",thrownError]
      ])
      TEST(error,"=","Exception: Error: -- throw --","RUN_TEST returns error")
   }
   nrExecutedTest+=34
   TEST(testenv.executedTestCase,'=',nrExecutedTest, "testenv.executedTestCase counts nr of executed tests")
   TEST(testenv.failedTestCase,'=',7, "failedTestCase counts nr of failed TEST")
   TEST(testenv.failedRunTest,'=',3, "3 (RUN|SUB)_TEST failed")
   TEST(testenv.runningTest,'=',0, "no running tests at this point")
}

function TEST(testedValue, compare, expect, errmsg, options) {
   return TestEnvironment.default.test(testedValue, compare, expect, errmsg, options)
}

async function RUN_TEST(arg1, arg2, arg3) {
   return TestEnvironment.default.runTest(arg1, arg2, arg3)
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

function NEW_TEST(log) {
   const TEST_ENV = new TestEnvironment(log)
   return { ...TEST_ENV.export(), TEST_ENV }
}

INTERNAL_SYNTAX_TEST()

return { TEST, RUN_TEST, SUB_TEST, END_TEST, RESET_TEST, NEW_TEST }

})()
