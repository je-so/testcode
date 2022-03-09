/* Test module which implements TEST function and RUN_TEST.
   (c) 2022 Jörg Seebohn */

const testContext=[]
/** All supported comparison functions. ok holds the value which is returned from cmp in case of success. */
const cmpMap=new Map([
   ["==",{ cmp:(v,e)=>(v===e), ok:true }], ["!=",{ cmp:(v,e)=>(v!==e), ok:true }],
   ["<=",{ cmp:(v,e)=>(v<= e), ok:true }], [">=",{ cmp:(v,e)=>(v>= e), ok:true }],
   ["<", { cmp:(v,e)=>(v < e), ok:true }], [">", { cmp:(v,e)=>(v > e), ok:true }],
])
/** In case of failure adds a named value which is logged before the error describing exception. */
const addFailedValue=(name,value) => currentContext().values.push({name, value})
const currentContext=() => { if (testContext.length) return testContext.at(-1); THROW("no test context - call RUN_TEST first"); }
const log=(...args) => currentContext().log(...args)
/** Adds customized test comparison functions. */
const setCompare=(name,cmp,ok=true) => currentContext().cmpMap.set(name, {cmp, ok})
const runWithinContext=(name,logFct,runTestFct) => {
   testContext.push({ name, values:[], cmpMap: new Map(cmpMap), passedCount:0, failedCount:0, log:(logFct??console.log) })
   runTestFct(currentContext())
   testContext.pop()
}

/** Throws an Error exception and adds key,value pairs of failedValues to the log. */
function THROW(failedCmp,failedValues={}) {
   for (const key of Object.keys(failedValues))
      addFailedValue(key,failedValues[key])
   throw new Error(failedCmp)
}

/** Counts failure of a single TEST call, logs errormsg and also key,value pairs of failedValues (called from within TEST). */
function FAILED(failedCmp,errormsg,failedValues={}) {
   const context=currentContext()
   for (const key of Object.keys(failedValues))
      addFailedValue(key,failedValues[key])
   log(`${context.name}: TEST failed: ${failedCmp}`)
   context.values.forEach( v => log(v.name+":","{",v.value,"}"))
   try { throw new Error(errormsg) } catch(e) { log(e) }
   context.failedCount ++
   context.values.length=0
}

/** Counts success of a single TEST call any failed values added for logging in case of failure are cleared (called from within TEST). */
const PASSED=() => { const context=currentContext(); context.passedCount ++; context.values.length=0; }

/** Runs a complete test unit (i.e. unittest_of_module_A) and logs the number of TEST calls which passed or failed. */
const RUN_TEST=(testFct,logFct) => runWithinContext(testFct.name,logFct, (context) => {
   try { testFct(TEST) } catch(e) { FAILED("unexpected exception","RUN_TEST aborted",{unexpected_exception:e}) }
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
function TEST(value,cmp,expect,errormsg,vindex="",eindex="") {
   try {
      if (typeof value === "function") {
         value=value()
      }
      if (cmp === "throw") {
         return FAILED("expected exception", errormsg, { [`expect${eindex}`]: expect })
      }
   }
   catch(e) {
      if (cmp !== "throw") {
         return FAILED("unexpected exception", errormsg, { [`expect${eindex}`]:expect, unexpected_exception:e })
      }
      else if (e.message !== expect) {
         return FAILED("exception.message == expect", errormsg, { "exception.message":e.message, [`expect${eindex}`]:expect, exception:e })
      }
      return PASSED()
   }

   try { doTest(value,expect,vindex,eindex); PASSED(); } catch(e) { FAILED(e.message,errormsg) }

   function doTest(value,expect,vindex,eindex) { // vindex and eindex reflect comparison of sub-elements
      const failedValues=() => ({ [`value${vindex}`]:value, [`expect${eindex}`]:expect })
      if (Array.isArray(value) && Array.isArray(expect)) {
         if (value.length !== expect.length)
            THROW(`==(value${vindex}.length,expect${eindex}.length)`,failedValues())
         for (var i=0; i<value.length; ++i)
            doTest(value[i],expect[i],vindex+`[${i}]`,eindex+`[${i}]`)
      }
      else {
         const isCmpFct=(typeof cmp === "function")
         if (!(isCmpFct || currentContext().cmpMap.has(cmp)))
            THROW(`TEST argument cmp '${String(cmp)}' unsupported`,failedValues())
         const {cmp: cmpFct,ok: SUCCESS}=(isCmpFct ? {cmp,ok:true} : currentContext().cmpMap.get(cmp))
         let cmpResult=!SUCCESS
         try { cmpResult=cmpFct(value,expect) } catch(e) { addFailedValue("unexpected_exception",e) }
         if (cmpResult !== SUCCESS) {
            const failedCmp=( typeof cmpResult === "string" ? cmpResult
                            : `${isCmpFct?cmpFct.name||"==":String(cmp)}(value${vindex},expect${eindex})`)
            THROW(failedCmp,failedValues())
         }
      }
   }
}
// < additional TEST features >
Object.defineProperties(TEST, { setCompare:{ value:setCompare }, addFailedValue:{ value:addFailedValue }, })

export { RUN_TEST, TEST, }
