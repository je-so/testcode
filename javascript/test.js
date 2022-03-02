/* Test module which implements TEST function and RUN_TEST.
   (c) 2022 Jörg Seebohn */

const testContext=[]
/** All supported comparison functions. ok holds the value which is returned from cmp in case of success. */
const cmpMap=new Map([
   ["==",{ cmp:(v,e)=>(v===e), ok:true }], ["!=",{ cmp:(v,e)=>(v!==e), ok:true }],
   ["<=",{ cmp:(v,e)=>(v<= e), ok:true }], [">=",{ cmp:(v,e)=>(v>= e), ok:true }],
   ["<", { cmp:(v,e)=>(v < e), ok:true }], [">", { cmp:(v,e)=>(v > e), ok:true }],
])
/** Adds a named value in case of failure to be output after an error describing exception. */
const addFailedValue=(name,value) => currentContext().values.push({name, value})
const currentContext=() => { if (testContext.length) return testContext.at(-1); throw new Error("no test context - call RUN_TEST first"); }
const log=(...args) => currentContext().log(...args)
/** Adds customized test comparison functions. */
const setCompare=(name,cmp,ok=true) => currentContext().cmpMap.set(name, {cmp, ok})
const runWithinContext=(name,logFct,runTestFct) => {
   testContext.push({ name, values:[], cmpMap: new Map(cmpMap), passedCount:0, failedCount:0, log:(logFct??console.log) })
   runTestFct(currentContext())
   testContext.pop()
}

/** Throws an Error exception and adds arguments value(value to test for), and expect(expected result) to be output to the log. */
function THROW(value,expect,errormsg,vindex="",eindex="") {
   addFailedValue(`value${vindex}`, value)
   addFailedValue(`expect${eindex}`, expect)
   throw new Error(errormsg)
}

/** Counts failure of a single TEST call and logs an error exception for debugging (called from within TEST). */
function FAILED(errormsg,unexpected_exception) {
   let context=currentContext()
   context.failedCount ++
   if (unexpected_exception !== undefined)
      addFailedValue("unexpected_exception",unexpected_exception)
   log(`${context.name}: TEST failed`)
   context.values.forEach( v => log(v.name+":","{",v.value,"}"))
   context.values.length=0
   try { throw new Error(errormsg) } catch(e) { log(e) }
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
function TEST(value,cmp,expect,errormsg,vindex="",eindex="") {
   try {
      if (typeof value === "function")
         value=value()
      if (cmp === "throw")
         return FAILED(`${errormsg} (no exception thrown)`)
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

   try { doTest(value,expect,vindex,eindex); PASSED(); } catch(e) { FAILED(e.message) }

   function doTest(value,expect,vindex,eindex) {
      if (Array.isArray(value) && Array.isArray(expect)) {
         if (value.length !== expect.length)
            THROW(value,expect,`${errormsg} (failed: ==(value${vindex}.length,expect${eindex}.length))`,vindex,eindex)
         for (var i=0; i<value.length; ++i) {
            doTest(value[i],expect[i],vindex+`[${i}]`,eindex+`[${i}]`) // vindex and eindex reflect comparison of sub-elements
         }
      }
      else {
         const isCmpFct=(typeof cmp === "function")
         if (!(isCmpFct || currentContext().cmpMap.has(cmp)))
            THROW(value,expect,`TEST argument cmp does not support value '${String(cmp)}'`)
         const {cmp: cmpFct,ok: SUCCESS}=(isCmpFct ? {cmp,ok:true} : currentContext().cmpMap.get(cmp))
         let cmpResult=!SUCCESS
         try { cmpResult=cmpFct(value,expect) } catch(e) { addFailedValue("unexpected_exception",e) }
         if (cmpResult !== SUCCESS) {
            if (typeof cmpResult === "string")
               THROW(value,expect,`${errormsg} (${cmpResult})`)
            THROW(value,expect,`${errormsg} (failed: ${isCmpFct?cmpFct.name||"cmp":String(cmp)}(value${vindex},expect${eindex}))`,vindex,eindex)
         }
      }
   }
}
// < additional TEST features >
Object.defineProperty(TEST, "setCompare", { value: setCompare })

export { RUN_TEST, TEST, }
