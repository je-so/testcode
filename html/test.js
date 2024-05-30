/**
 * Test module which implements TEST and CATCH
 * to test for an expected value or an expected exception.
 * (c) 2024 Jörg Seebohn
 */

// namespace
const { TEST, CATCH, RUN_TEST, END_TEST } = (() => {

let runContext = null
let failedTest = 0
let executedTest = 0
let failedRunTest = 0

class RunContext {
   constructor(description, timeout, callback) {
      this.description = description
      this.timeout = timeout
      this.callback = callback
      this.waitDone = new Promise( (resolve, reject) => {
         this.done = resolve
         this.abort = reject
      })
      this.timer = null
   }

   startTimer() {
      this.timer ??= setTimeout( () => {
         this.abort(`Timeout after ${this.timeout}ms`)
      }, this.timeout)
   }

   clearTimer() {
      this.timer && clearTimeout(this.timer)
      this.timer = null
   }

   async run() {
      this.startTimer()
      this.callback(this.done, this.abort)
      return this.waitDone
            .then( () => this.clearTimer())
            .catch( (reason) => {
               this.clearTimer()
               console.error(`RUN_TEST failed: ${this.description}`)
               console.log(`Reason: ${reason}`)
               throw reason
            })
   }
}

class TestCase {
   #testedValue
   #comparisons
   #errmsg

   #value = undefined
   #exception = undefined
   #comparisonError = ""
   #testResult = true

   constructor(testedValue, comparisons, errmsg) {
      this.#testedValue = testedValue
      this.#comparisons = [comparisons].flat()
      this.#errmsg = errmsg
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
         this.#value = (typeof this.#testedValue === "function")
                     ? this.#testedValue()
                     : this.#testedValue
      }
      catch(e) {
         this.#exception = e
      }
   }

   testValue() {
      this.run()
      if (this.#exception) {
         this.logError(`Unexpected exception ${this.#exception.name}.`)
      }
      else {
         this.#comparisons.every( compare => {
            if (typeof compare === "function") {
               this.#comparisonError = compare(this.#value)
               if (this.#comparisonError)
                  this.logError(`Wrong value: ${this.#comparisonError}.`)
            }
            else if (typeof compare === "string") {
               const opEnd = compare.indexOf(" ")
               const value = compare.substring(opEnd+1)
               switch (opEnd == -1 ? "" : compare.substring(0,opEnd)) {
                  case "<":
                     !(this.#value < value) && this.logError(`Expected ${String(this.#value)} ${String(compare)}.`)
                     break
                  case ">":
                     !(this.#value > value) && this.logError(`Expected ${String(this.#value)} ${String(compare)}.`)
                     break
                  case "<=":
                     !(this.#value <= value) && this.logError(`Expected ${String(this.#value)} ${String(compare)}.`)
                     break
                  case ">=":
                     !(this.#value >= value) && this.logError(`Expected ${String(this.#value)} ${String(compare)}.`)
                     break
                  case "==":
                     !(this.#value == value) && this.logError(`Expected ${String(this.#value)} ${String(compare)}.`)
                     break
                  case "!=":
                     !(this.#value != value) && this.logError(`Expected ${String(this.#value)} ${String(compare)}.`)
                     break
                  default:
                     if (this.#value !== compare)
                        this.logError(`Expected value ${String(this.#value)} === ${String(compare)}.`)
                     break
               }
            }
            else if (this.#value !== compare)
               this.logError(`Expected value ${String(this.#value)} === ${String(compare)}.`)
            return this.OK
         })
      }
      return this.OK
   }

   testCatch() {
      this.run()
      if (! this.#exception)
         this.logError(`Expected exception.`)
      else {
         this.#comparisons.every( compare => {
            if (typeof compare === "function" && this.#exception instanceof compare) {
            }
            else if (typeof compare === "function" && !(compare instanceof Error)) {
               this.#comparisonError = compare(this.#exception)
               if (this.#comparisonError)
                  this.logError(`Wrong exception: ${this.#comparisonError}.`)
            }
            else if (typeof compare === "string") {
               if (this.#exception.message !== compare)
                  this.logError(`Expected exception message »${compare}« not »${this.#exception.message}«.`)
            }
            else if (!(this.#exception instanceof compare.constructor)) {
               this.logError(`Expected exception type »${compare.name}« not »${this.#exception.name}«.`)
            }
            else if (this.#exception.message !== compare.message) {
               this.logError(`Expected exception message »${compare.message}« not »${this.#exception.message}«.`)
            }
            return this.OK
         })
      }
      return this.OK
   }
}

function countTest(isOK) {
   ++ executedTest
   !isOK && ( ++ failedTest)
   return isOK
}

function TEST(testedValue, comparisons, errmsg) {
   const tc = new TestCase(testedValue, comparisons, errmsg)
   return countTest(tc.testValue())
}

function CATCH(testedFct, comparisons, errmsg) {
   const tc = new TestCase(testedFct, comparisons, errmsg)
   return countTest(tc.testCatch())
}

async function RUN_TEST(description, timeout, callback) {
   if (runContext) {
      throw Error("RUN_TEST called nested within another RUN_TEST")
   }
   runContext = new RunContext(description, timeout, callback)
   return runContext.run().catch( () => ++failedRunTest).then( () => { runContext = null })
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
   failedTest = 0
   executedTest = 0
   failedRunTest = 0
}

return { TEST, CATCH, RUN_TEST, END_TEST }

})()
