/* Test module which implements TEST function and RUN_TEST.
   (c) 2022 Jörg Seebohn */

const testContext=[]
/** Adds a named value in case of failure to be output after an error describing exception. */
const addFailedValue=(name,value) => currentContext().values.push({name, value})
const currentContext=() => testContext.at(-1)
const log=(...args) => currentContext().log(...args)
const runWithinContext=(name,logFct,runTestFct) => {
   testContext.push({ name, values:[], passedCount:0, failedCount:0, log:(logFct??console.log) })
   runTestFct(currentContext())
   testContext.pop()
}

/** Throws an Error exception and adds arguments value(value to under test), and expect(expected result) to be output to the log. */
function THROW(value,expect,errormsg) {
   addFailedValue("value", value)
   addFailedValue("expect", expect)
   throw new Error(errormsg)
}

/** Counts failure of a single TEST call and logs an error exception for debugging (called from within TEST). */
function FAILED(errormsg,unexpected_exception) {
   try { throw new Error(errormsg) } catch(e) { log(e) }
   if (unexpected_exception !== undefined)
      addFailedValue("unexpected_exception",unexpected_exception)
   for (const v of currentContext().values) {
      log(v.name+":","{",v.value,"}")
   }
   currentContext().values.length=0
   currentContext().failedCount ++
}

/** Counts success of a single TEST call and nothing else (called from within TEST). */
const PASSED=() => currentContext().passedCount ++

/** Runs a complete test unit (i.e. unittest_of_module_A) and logs the number of TEST calls which passed or failed. */
const RUN_TEST=(testFct,logFct) => runWithinContext(testFct.name,logFct, (context) => {
   try { testFct() } catch(e) { FAILED("RUN_TEST aborted (unexpected exception)",e) }
   // log test result
   if (context.failedCount === 0)
      log(`${context.name}: all ${context.passedCount} tests PASSED`)
   else if (context.passedCount === 0)
      log(`${context.name}: all ${context.failedCount} tests FAILED`)
   else {
      log(`${context.name}: ${context.passedCount} tests PASSED`)
      log(`${context.name}: ${context.failedCount} tests FAILED`)
   }
})

/** Compares value with expect and logs errormsg in case of inequality/failure.
 * To test for exceptions value should be set to a function, cmp to "throw", and expect to the expected message string of the exception.
 * Argument cmp is either a value out of ["==","<=",">=","<",">","throw"] or a function
 * with arguments (value,expect) returning true in case of success or false in case of failure. */
function TEST(value,cmp,expect,errormsg) {
   try {
      if (typeof value === "function")
         value=value()
   }
   catch(e) {
      if (cmp !== "throw") {
         FAILED(`${errormsg} (unexpected exception)`,e)
      }
      else if (e.message !== expect) {
         addFailedValue("exception.message",e.message)
         addFailedValue("expect",expect)
         FAILED(`${errormsg} (failed: exception.message == expect)`)
      }
      return
   }

   try { doTest(value,cmp,expect,errormsg); PASSED(); } catch(e) { FAILED(e.message) }

   function doTest(value,cmp,expect,errormsg) {
      if (typeof cmp === "function") {
         let hasPassed=false;
         try { hasPassed=cmp(value,expect) } catch(e) { addFailedValue("unexpected_exception",e) }
         if (hasPassed !== true)
            THROW(value,expect,`${errormsg} (failed: ${cmp.name??"cmp"}(value,expect) == true`)
      }
      else if (Array.isArray(value)) {
         if (value.length !== expect.length)
            THROW(value,expect,`${errormsg} (failed: value.length == expect.length)`)
         for (var i=0; i<value.length; ++i) {
            doTest(value[i],cmp,expect[i],errormsg)
         }
      }
      else {
         const hasPassed= cmp=="==" ? (value === expect) :
                          cmp=="<=" ? (value <= expect) :
                          cmp==">=" ? (value >= expect) :
                          cmp=="<"  ? (value < expect) :
                          cmp==">"  ? (value > expect) :
                          cmp=="throw" ? `${errormsg} (no exception thrown)` :
                          `TEST argument cmp does not support value '${String(cmp)}'`
                          ;
         if (hasPassed !== true) {
            if (typeof hasPassed === "string")
               THROW(value,expect,hasPassed)
            THROW(value,expect,`${errormsg} (failed: value ${cmp} expect)`)
         }
      }
   }
}

export {
   // functions
   RUN_TEST,
   TEST,
}
