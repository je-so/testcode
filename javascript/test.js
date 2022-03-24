/* Test module which implements TEST function and RUN_TEST.
   (c) 2022 Jörg Seebohn */

/** All supported comparison functions. ok holds the value which is returned from cmp in case of success. */
const cmpMap=new Map([
   ["==",{ cmp:(v,e)=>(v===e), ok:true}], ["!=",{ cmp:(v,e)=>(v!==e), ok:true}],
   ["<=",{ cmp:(v,e)=>(v<= e), ok:true}], [">=",{ cmp:(v,e)=>(v>= e), ok:true}],
   ["<", { cmp:(v,e)=>(v < e), ok:true}], [">", { cmp:(v,e)=>(v > e), ok:true}],
])
const testContext=[] // every RUN_TEST runs within its own context (see runWithinContext)
const currentContext=() => { if (testContext.length) return testContext.at(-1); THROW("no test context - call RUN_TEST first"); }
/** In case of failure adds a named value which is logged before the error describing exception. */
const addFailedValue=(name,value) => currentContext().values.push({name, value})
/** Adds multiple failed values provided as key value pairs of an object. */
const addFailedValues=(failedValues={}) => { for (const key of Object.keys(failedValues)) addFailedValue(key,failedValues[key]) }
const log=(...args) => currentContext().log(...args)
/** Adds customized test comparison function. Exported as TEST.setCompare. */
const setCompare=(name,cmp,ok=true) => currentContext().cmpMap.set(name, {cmp, ok})
const getCompare=(cmp) => {
   const cmpok=currentContext().cmpMap.get(cmp)
   return (cmpok ? cmpok : THROW(`comparison function '${String(cmp)}' unsupported`))
}
const runWithinContext=(name,log=console.log,runTestFct) => {
   testContext.push({ name, log, values:[], cmpMap: new Map(cmpMap), passedCount:0, failedCount:0 })
   runTestFct(currentContext())
   testContext.pop()
}
const failedValues=(value,expect,index) => ({ [`value${index}`]:value, [`expect${index}`]:expect })

/** Throws an Error exception and adds key,value pairs of failedValues to the log. */
function THROW(failedCmp,failedValues) {
   addFailedValues(failedValues)
   throw new Error(failedCmp)
}

/** Counts failure of a single TEST call, logs errormsg and also key,value pairs of failedValues (called from within TEST). */
function FAILED(failedCmp,errormsg,failedValues) {
   const context=currentContext()
   addFailedValues(failedValues)
   log(`${context.name}: TEST failed: ${failedCmp}`)
   context.values.forEach( v => log(v.name+":","{",v.value,"}"))
   try { throw new Error(errormsg) } catch(e) { log(e) }
   context.failedCount ++
   context.values.length=0
   return false
}

/** Counts success of a single TEST call any failed values added for logging in case of failure are cleared (called from within TEST). */
const PASSED=() => { const context=currentContext(); context.passedCount ++; context.values.length=0; return true }

/** Runs a complete test unit (i.e. unittest_of_module_A) and logs the number of TEST calls which passed or failed. */
const RUN_TEST=(testFct,logFct) => runWithinContext(testFct.name,logFct, (context) => {
   try { testFct(TEST) } catch(e) { FAILED("unexpected exception","RUN_TEST aborted",{unexpected_exception:e}) }
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
function TEST(value,cmp,expect,errormsg,index="") {
   try {
      if (typeof value === "function") {
         value=value()
      }
      if (cmp === "throw") {
         return FAILED("expected exception", errormsg, failedValues(value,expect,index))
      }
   }
   catch(e) {
      if (cmp !== "throw")
         return FAILED("unexpected exception", errormsg, { unexpected_exception:e, ...failedValues(value,expect,index) })
      else if (e.message !== expect)
         return FAILED(`exception.message == expect${index}`, errormsg, { "exception.message":e.message, [`expect${index}`]:expect, exception:e })
      return PASSED()
   }

   try { doTest(); return PASSED(); } catch(e) { return FAILED(e.message,errormsg) }

   function doTest() {
      const {cmp:cmpFct, ok:SUCCESS}=(typeof cmp === "function" ? ({cmp,ok:true}) : getCompare(cmp))
      ;(function compare(value,expect,index) { // index reflects comparison of sub-elements
         if (Array.isArray(value) && Array.isArray(expect)) {
            if (value.length !== expect.length)
               THROW(`==(value${index}.length,expect${index}.length)`,failedValues(value,expect,index))
            for (var i=0; i<value.length; ++i)
               compare(value[i],expect[i],index+`[${i}]`)
         }
         else {
            let cmpResult=!SUCCESS
            try { cmpResult=cmpFct(value,expect) } catch(e) { addFailedValue("unexpected_exception",e) }
            if (cmpResult !== SUCCESS) {
               const comparisonName=String(typeof cmp === "function" ? cmp.name : cmp)
               THROW(`${comparisonName}(value${index},expect${index})`,failedValues(value,expect,index))
            }
         }
      }) (value,expect,index)
   }
}
// < additional TEST features >
Object.defineProperties(TEST, { setCompare:{ value:setCompare }, addFailedValue:{ value:addFailedValue }, })

export { RUN_TEST, TEST, }
