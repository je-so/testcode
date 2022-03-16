/* Type validator to validate argument values.
   (c) 2022 Jörg Seebohn */

const ownKeys=Reflect.ownKeys

class Stringifier {
   constructor(maxdepth,maxlen) {
      this.depth=0
      this.result=""
      this.maxlen=maxlen
      this.maxdepth=maxdepth
      this.overflow=false
   }

   append(str) {
      const len=this.result.length
      this.overflow=(str.length>this.maxlen || (this.maxlen-str.length) < len)
      if (len<this.maxlen)
         this.result+=str.substr(0,this.maxlen-len)
      return this
   }

   handleOverflow() {
      const len=this.result.length
      this.result=(len > 4 ? this.result.substr(0,len-4) + " ..." : "...")
   }

   toString(value) {
      if (value === null)
         this.append("null")
      else if (value === undefined)
         this.append("undefined")
      else {
         const type=typeof value
         if (type === "object") {
            ++ this.depth
            const isShallow=(this.depth >= this.maxdepth)
            if (Array.isArray(value)) {
               if (isShallow)
                  this.strShallowArray(value)
               else
                  this.strArray(value)
            }
            else {
               if (isShallow)
                  this.strShallowObject(value)
               else
                  this.strObject(value)
            }
            -- this.depth
         }
         else if (type === "function")
            this.strFunction(value)
         else
            this.strPrimitive(type,value)
      }
      if (this.depth === 0 && this.overflow)
         this.handleOverflow()
      return this.result
   }

   strKey(value) { this.append(strKey(value)) }

   strPrimitive(type,value) { this.append(type === "string" ? strLiteral(value) : String(value)) }

   strFunction(value) {
      if (value.name)
         this.append("function ").append(value.name)
      else
         this.append("()=>{}")
   }

   strShallowArray(value) {
      if (value.length > 0)
         this.append("[...]")
      else
         this.append("[]")
   }

   strShallowObject(value) {
      if (ownKeys(value).length > 0)
         this.append("{...}")
      else
         this.append("{}")
   }

   strArray(value) {
      this.append("[")
      const it=value.values()
      const first=it.next()
      if (!first.done) {
         this.toString(first.value)
         for (let next; !(next=it.next()).done; this.toString(next.value))
            this.append(",")
      }
      this.append("]")
   }

   strObject(value) {
      const keys=ownKeys(value)
      const it=keys.values()
      const first=it.next()
      this.append("{")
      if (!first.done) {
         this.strKey(first.value)
         this.append(":")
         this.toString(value[first.value])
         for (let next; !(next=it.next()).done; this.toString(value[next.value])) {
            this.append(",")
            this.strKey(next.value)
            this.append(":")
         }
      }
      this.append("}")
   }
}

class TypeStringifier extends Stringifier {
   constructor(maxdepth,maxlen) { super(maxdepth,maxlen) }
   // overwrite string value conversions for types
   strPrimitive(type/*,value*/) { this.append(type) }
   strFunction() { this.append("function") }
   strShallowArray() { this.append("Array") }
   strShallowObject(value) { this.append(strObjectType(value)) }
   strObject(value) { this.append(strObjectType(value)); super.strObject(value) }
}

const strObjectType=(value) => { return value.constructor?.name ?? "object" }

const strType=(value,maxdepth=2,maxlen=130) => {
   return new TypeStringifier(maxdepth,maxlen).toString(value)
}

const strValue=(value,maxdepth=2,maxlen=130) => {
   return new Stringifier(maxdepth,maxlen).toString(value)
}

const strLiteral=(value)=>"'"+String(value).replace(/['\\]/g,c=>"\\"+c)+"'"

const strKey=(key) => {
   return (typeof key === "symbol"
          ? "[" + String(key) + "]"
          : /^([$_a-zA-Z][$_a-zA-Z0-9]*|[1-9][0-9]*|0)$/.test(key)
          ? key
          : strLiteral(key))
}

const strIndex=(key) => {
   return (typeof key === "symbol" || /^([1-9][0-9]*|0)$/.test(key)
          ? "[" + String(key) + "]"
          : /^([$_a-zA-Z][$_a-zA-Z0-9]*)$/.test(key)
          ? "." + key
          : "[" + strLiteral(key) + "]")
}

/** Call it with (["key1","key2",3]) or ("key1","key2",3) to get ".key1.key2[3]". */
const strIndices=(...keys) => {
   const path=(Array.isArray(keys[0])? keys[0]:keys).reduce((str,k) => str+strIndex(k), "")
   return path
}

/** Stores information about current argument, namely its value and name or property key.
  * This information is called the validation context. */
class ValidationContext {

   // top level context (parent == null): name must be name of argument(arg)
   constructor(arg,name,parent=null) {
      this.arg=arg // argument value whose type is validated
      this.name_=name // parameter name
      this.parent=parent
   }

   childContext(i) { return new ValidationContext(this.arg[i],i,this) }

   names() {
      const names=[]
      for (let context=this; context; context=context.parent)
         names.unshift(context.name)
      return names
   }

   accessPath() {
      const names=this.names()
      return String(names.shift()) + strIndices(names)
   }

   get name() { return (typeof this.name_ === "function" ? this.name_() : this.name_) }

   error(err={}) {
      const msg=err.msg ?? `of type '${err.expect}' not '${err.found?.(this) ?? strType(this.arg,0)}'`
      const value=err.value ?? strValue(this.arg)
      const prop=(this.parent ? " property":"")
      return new TypeError(`Expect argument${prop} '${this.accessPath()}' ${msg} (value: ${value})`)
   }
}

class TypeValidator {

   /**
    * Test argument value to be of a certain javascript type.
    * Throws an error if argument does not conform to the expected type.
    * @param {any} arg
    *   The argument value to test for.
    * @param {string|function} argName
    *   The name of the argument. Used in error message.
    */
   assertType(arg,argName) {
      const error=this.validateType(arg,argName)
      if (error !== undefined)
         throw error
   }

   validateType(arg,argName) { return this.validateWith(new ValidationContext(arg,argName)) }

   /** Validates property with key i of argument context.arg. */
   validateProperty(context,i) { return this.validateWith(context.childContext(i)) }

   /** Returns undefined | TypeError.
     * You could add a debug function to typeValidator for testing.
     * This template function calls validate which must be implemented in a derived subtype. */
   validateWith(context) {
      const validationResult=this.validate(context)
      this.debug?.(validationResult)
      if (validationResult !== undefined) {
         if (validationResult instanceof TypeError)
            return validationResult
         else if (validationResult === false)
            return context.error(this)
      }
      /* OK */
   }

   /**
    * Helper function to build a typename from a list of type validators.
    * @param {string} lp
    *   The start prefix of the build typename. Usually "(".
    * @param {string} delim
    *   The delimiter between different typenames in the list. Usually ",".
    * @param {string} rp
    *   The end prefix of the build typename. Usually ")".
    * @param {TypeValidator|object} typeValidators
    *   The list of type validators.
    * @param {undefined|function} getType
    *   The function to access the expected type name of the validator.
    *   If getType is not a function it is assumed typeValidator has an "expect" property.
    * @returns
    *   The generated list of expected type names.
    */
   typeList(lp,delim,rp,typeValidators,getType=(v) => v.expect) {
      const list=typeValidators.reduce( (list,v) => { const t=getType(v); return t ? list+delim+t : list; }, "")
      return (list.length ? lp + list.substring(delim.length) + rp : undefined)
   }
}

////////////////
// simple types

class AnyValidator extends TypeValidator {
   get expect() { return "any" }
   validate() { } // validates everything
}
class ArrayValidator extends TypeValidator {
   get expect() { return "Array" }
   validate({arg}) { return Array.isArray(arg) }
}
class BigintValidator extends TypeValidator {
   get expect() { return "bigint" }
   validate({arg}) { return typeof arg === "bigint" }
}
class BooleanValidator extends TypeValidator {
   get expect() { return "boolean" }
   validate({arg}) { return typeof arg === "boolean" }
}
class ConstructorValidator extends TypeValidator {
   get expect() { return "constructor" }
   validate({arg}) {
      // Reflect.construct expects 3rd arg of type newTarget (== constructor)
      try { Reflect.construct(Object, [], arg) } catch(e) { return false }
   }
}
class FunctionValidator extends TypeValidator {
   get expect() { return "function" }
   validate({arg}) { return typeof arg === "function" }
}
class NullValidator extends TypeValidator {
   get expect() { return "null" }
   validate({arg}) { return arg === null }
}
class NumberValidator extends TypeValidator {
   get expect() { return "number" }
   validate({arg}) { return typeof arg === "number" }
}
class ObjectValidator extends TypeValidator {
   get expect() { return "object" }
   validate({arg}) { return typeof arg === "object" && arg !== null }
}
class KeyCount1Validator extends TypeValidator {
   get expect() { return "ownKeys({[any]:any})==1" }
   found(context) { return "ownKeys({[any]:any})=="+ownKeys(Object(context.arg)).length }
   validate(context) {
      const nrOfKeys=ownKeys(Object(context.arg)).length
      if (nrOfKeys !== 1)
         return context.error({msg:`to have 1 property not ${nrOfKeys}`})
   }
}
class KeyStringValidator extends TypeValidator {
   get expect() { return "{[string]:any}" }
   found() { return "{[symbol]:any}" }
   validate(context) { return (Object.getOwnPropertySymbols(Object(context.arg)).length == 0) }
}
/** Reference type validator: reference types or complex types are mutable (without null). */
class ReferenceValidator extends TypeValidator {
   get expect() { return "(function|object)" }
   validate({arg}) { return arg != null && (typeof arg === "object" || typeof arg === "function") }
}
class StringValidator extends TypeValidator {
   get expect() { return "string" }
   validate({arg}) { return typeof arg === "string" }
}
class SymbolValidator extends TypeValidator {
   get expect() { return "symbol" }
   validate({arg}) { return typeof arg === "symbol" }
}
class UndefinedValidator extends TypeValidator {
   get expect() { return "undefined" }
   validate({arg}) { return arg === void 0 }
}
/** Value type validator: value types or primitive types are immutable (without null and undefined) */
class ValueValidator extends TypeValidator {
   get expect() { return "(bigint|boolean|number|string|symbol)" }
   validate({arg}) { return arg != null && typeof arg !== "object" && typeof arg !== "function" }
}

/////////////////
// complex types

class AndValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeValidators=typeValidators }
   get expect() { return this.typeList("(","&",")",this.typeValidators) }
   found(context) { return this.typeList("(","&",")",this.typeValidators,(v)=>v.found(context)) }
   validate(context) {
      const error=this.typeValidators.reduce((err,v) => err?err:v.validateWith(context), undefined)
      return error
   }
}
class InstanceValidator extends TypeValidator {
   constructor(newTarget) { super(); this.newTarget=newTarget; }
   get expect() { return `class ${this.newTarget.name}` }
   validate({arg}) { return arg instanceof this.newTarget }
}
class KeyValidator extends TypeValidator {
   constructor(typeValidator=TVany,keys) { super(); this.typeValidator=typeValidator; this.keys=keys; }
   get expect() {
      if (this.keys.length === 0)
         return `object{[string|symbol]:${this.typeValidator.expect}}`
      return this.typeList("object{",",","}",this.keys,(v)=>strKey(v)+":"+this.typeValidator.expect)
   }
   found(context) {
      return (this.keys.length>0 ? strType(context.arg,2) : undefined)
   }
   validate(context) {
      const keys=(this.keys.length>0 ? this.keys : ownKeys(Object(context.arg)))
      const arg=Object(context.arg)
      const validateKey=(key) => (
            (key in arg)
            ? this.typeValidator.validateProperty(context,key)
            : context.error({msg:`to contain property ${strLiteral(key)}`})
      )
      const error=keys.reduce((err,key) => err?err:validateKey(key), undefined)
      return error
   }
}
class SwitchValidator {
   // filterAndTypeValidators: [ [filterTypeValidator,typeValidator|undefined], [filter, type|undefined], ... ]
   constructor(filterAndTypeValidators) { this.typeValidators=filterAndTypeValidators }
   get expect() { return this.typeList("(","|",")",this.typeValidators, ([f,t]) => f.expect) }
   validate(context) {
      const matching=this.typeValidators.some( ([filter]) => filter.validateWith(context) === undefined)
      // if filter matches but detailed validator is undefined then skip detailed validator and return true
      if (matching && matching[1] !== undefined)
         return matching[1].validateWith(context)
      return Boolean(matching)
   }
}
class UnionValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeValidators=typeValidators }
   get expect() { return this.typeList("(","|",")",this.typeValidators) }
   found(context) { return this.typeList("(","|",")",this.typeValidators,(v)=>v.found(context)) }
   validate(context) {
      const matching=this.typeValidators.find((v) => v.validateWith(context) === undefined)
      return Boolean(matching)
   }
}
class TupleValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeValidators=typeValidators }
   get expect() { return this.typeList("[",",","]",this.typeValidators) }
   validate(context) {
      var error=TVarray.validateWith(context)
      if (!error && context.arg.length !== this.typeValidators.length)
         error=context.error({msg: `to have array length ${this.typeValidators.length} not ${context.arg.length}`})
      error=this.typeValidators.reduce((err,v,i) => err?err:this.typeValidators[i].validateProperty(context,i), error)
      return error
   }
}

function unittest_typevalidator(TEST) {

   testFunctions()
   testSimpleTV()
   // TODO: testComplexTV()

   function testFunctions() {
      const syms=[ Symbol(0), Symbol(1), Symbol(2), Symbol(3) ]
      const o1={[syms[1]]:'s1',[syms[2]]:'s2',b:'b',a:'a',1:1,0:0,'x and y':'xy'}
      const oinherit=Object.create(o1)

      // TEST ownKeys
      TEST(ownKeys({}),"==",[],
      "Empty object has no properties")
      TEST(ownKeys(oinherit),"==",[],
      "Inherited properties are not own properties")
      TEST(ownKeys(o1),"==",["0","1","b","a","x and y",syms[1],syms[2]],
      "Returned properties: 1. numbers in ascending order then strings and then symbols in defined order")

      // TEST strKey (Property key used in object literal)
      TEST(strKey(syms[0]),"==","[Symbol(0)]","Symbol")
      TEST(strKey("abc0xyz"),"==","abc0xyz","identifier")
      TEST(strKey("_$abcefghijklmnopqrstuvwxyz0123456789"),"==","_$abcefghijklmnopqrstuvwxyz0123456789","identifier")
      TEST(strKey("_"),"==","_","identifier")
      TEST(strKey("$"),"==","$","identifier")
      const upper="A".charCodeAt(0)-"a".charCodeAt(0)
      for (var i="a".charCodeAt(0); i<="z".charCodeAt(0); ++i) {
         TEST(strKey(String.fromCharCode(i)),"==",String.fromCharCode(i),"single letter")
         TEST(strKey(String.fromCharCode(upper+i)),"==",String.fromCharCode(upper+i),"single letter")
      }
      for (var i=0; i<=9; ++i) {
         TEST(strKey(String(i)),"==",`${i}`,"Single digit numbers could be used directly")
         TEST(strKey(String(i+(i==0))+"123456789"),"==",`${i+(i==0)}123456789`,"Numbers could be used directly")
         TEST(strKey(`0${i}`),"==","'0"+i+"'","Numbers could not begin with 0 except for single digit 0")
      }

      // TEST strIndex (Index used to access object property, either dot operator or array [] brackets)
      TEST(strIndex(""),"==","['']","Symbol")
      TEST(strIndex(syms[2]),"==","[Symbol(2)]","Symbol")
      TEST(strIndex("'\\'"),"==","['\\'\\\\\\'']","String with \\ and \'")
      TEST(strIndex(null),"==",".null","null")
      TEST(strIndex(undefined),"==",".undefined","undefined")
      TEST(strIndex("abc01"),"==",".abc01","identifier")
      // test numbers
      for (var i=0; i<=9; ++i) {
         TEST(strIndex(`${i}`),"==","["+i+"]","number")
         TEST(strIndex(`${i}a`),"==",`['${i}a']`,"number followed by something")
         TEST(strIndex(`${i} `),"==",`['${i} ']`,"number followed by something")
         TEST(strIndex(`y${i}`),"==",`.y${i}`,"alpha followed by number")
         TEST(strIndex(`${i+1}${i}`),"==",`[${i+1}${i}]`,"multiple digits")
         TEST(strIndex(`0${i}`),"==",`['0${i}']`,"multiple digits starting with 0")
      }
      // test identifier characters
      TEST(strIndex("_"),"==","._","identifier")
      TEST(strIndex("$"),"==",".$","identifier")
      for (var i="a".charCodeAt(0); i<="z".charCodeAt(0); ++i) {
         TEST(strIndex(String.fromCharCode(i)),"==","."+String.fromCharCode(i),"Index used to access object property")
         TEST(strIndex(String.fromCharCode(upper+i)),"==","."+String.fromCharCode(upper+i),"Index used to access object property")
      }

      // TEST strIndices
      TEST(strIndices(),"==","","Empty access path")
      TEST(strIndices([]),"==","","Empty access path")
      TEST(strIndices(["o1"]),"==",".o1","Single index")
      TEST(strIndices(1),"==","[1]","Single index")
      TEST(strIndices([Symbol(1)]),"==","[Symbol(1)]","Single index")
      TEST(strIndices(1,2,3),"==","[1][2][3]","3d array access")
      TEST(strIndices([1],2,3),"==","[1]","If first arg is Array other args are ignored")
      TEST(strIndices(["o",1,",''","id",syms[1]]),"==",".o[1][',\\'\\''].id[Symbol(1)]","multiple properties")
      TEST(strIndices("id_",2,"\\","ty",syms[2]),"==",".id_[2]['\\\\'].ty[Symbol(2)]","multiple properties")

      // TEST strType
      // TODO:

      // TEST strValue
      // TODO:
   }

   function testSimpleTV() {
      function TT(value,type,strValue) { return { value, type, strValue:strValue||type }; }
      function DummyType(name) { this.name=name }

      const testTypes=[
         TT(null,"null"), TT(undefined,"undefined"),
         TT(true,"boolean","true"), TT(false,"boolean","false"),
         TT(10,"number","10"), TT(1_000_000_000_000n,"bigint",1_000_000_000_000),
         TT("abc","string","'abc'"), TT(Symbol("symbol"),"symbol","Symbol(symbol)"),
         TT([],"Array","[]"),
         TT({},"Object","{}"), TT({ a:1 },"Object","{a:1}"), TT({ a:1,b:2 },"Object","{a:1,b:2}"),
         TT({ [Symbol("sym")]:1 },"Object","{[Symbol(sym)]:1}"),
         TT(() => true,"function","()=>{}"),
         TT(DummyType,"function","function DummyType")
      ]

      for (var i=0; i<testTypes.length; ++i) {
         const { value: v, type: t, strValue: str }=testTypes[i]

         // TEST TVany
         TEST( () => TVany.assertType(v,"param1"), "==", undefined, "AnyValidator validates everything even null and undefined")

         // TEST TVarray
         if (Array.isArray(v))
            TEST( () => TVarray.assertType(v,"param1"), "==", undefined, "ArrayValidator validates []")
         else
            TEST( () => TVarray.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array' not '${t}' (value: ${str})`, `ArrayValidator does not validate ${str}`)

         // TVbigint
         if (typeof v === "bigint")
            TEST( () => TVbigint.assertType(v,"param1"), "==", undefined, "BigintValidator validates bigint")
         else
            TEST( () => TVbigint.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'bigint' not '${t}' (value: ${str})`, `BigintValidator does not validate ${str}`)

         // TEST TVboolean
         if (v === true || v === false)
            TEST( () => TVboolean.assertType(v,"param1"), "==", undefined, "BooleanValidator validates boolean")
         else
            TEST( () => TVboolean.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'boolean' not '${t}' (value: ${str})`, `BooleanValidator does not validate ${str}`)

         // TEST TVconstr
         if (v === DummyType)
            TEST( () => TVconstr.assertType(v,"param1"), "==", undefined, "ConstructorValidator validates function which serves as constructor")
         else
            TEST( () => TVconstr.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'constructor' not '${t}' (value: ${str})`, `ConstructorValidator does only validate DummyType`)

         // TEST TVfunction
         if (v !== null && typeof v === "function")
            TEST( () => TVfunction.assertType(v,"param1"), "==", undefined, "FunctionValidator validates function")
         else
            TEST( () => TVfunction.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'function' not '${t}' (value: ${str})`, `FunctionValidator does not validate ${str}`)

         // TEST TVkey1
         if (Object.getOwnPropertyNames(Object(v)).length === 1 || Object.getOwnPropertySymbols(Object(v)).length === 1)
            TEST( () => TVkey1.assertType(v,"param1"), "==", undefined, "KeyCount1Validator does only validate {a:1}")
         else
            TEST( () => TVkey1.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' to have 1 property not ${Object.getOwnPropertyNames(Object(v)).length} (value: ${str})`, "KeyCount1Validator does only validate {a:1}")

         // TEST TVkeystr
         if (Object.getOwnPropertySymbols(Object(v)).length === 0)
            TEST( () => TVkeystr.assertType(v,"param1"), "==", undefined, "KeyStringValidator does only validate {[string]:any}")
         else
            TEST( () => TVkeystr.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '{[string]:any}' not '{[symbol]:any}' (value: ${str})`, "KeyStringValidator does only validate {[string]:any}")

         // TEST TVnull
         if (v === null)
            TEST( () => TVnull.assertType(v,"param1"), "==", undefined, "NullValidator validates null")
         else
            TEST( () => TVnull.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'null' not '${t}' (value: ${str})`, `NullValidator does not validate ${str}`)

         // TEST TVnumber
         if (typeof v === "number")
            TEST( () => TVnumber.assertType(v,"param1"), "==", undefined, "NumberValidator validates number")
         else
            TEST( () => TVnumber.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'number' not '${t}' (value: ${str})`, `NumberValidator does not validate ${str}`)

         // TEST TVobject
         if (v != null && typeof v === "object")
            TEST( () => TVobject.assertType(v,"param1"), "==", undefined, "ObjectValidator validates {}")
         else
            TEST( () => TVobject.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'object' not '${t}' (value: ${str})`, `ObjectValidator does not validate ${str}`)

         // TEST TVref
         if (v != null && (typeof v === "object" || typeof v === "function"))
            TEST( () => TVref.assertType(v,"param1"), "==", undefined, "ReferenceValidator validates only function or object types")
         else
            TEST( () => TVref.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '(function|object)' not '${t}' (value: ${str})`, `ReferenceValidator does not validate ${str}`)

         // TVstring
         if (typeof v === "string")
            TEST( () => TVstring.assertType(v,"param1"), "==", undefined, "StringValidator validates string")
         else
            TEST( () => TVstring.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'string' not '${t}' (value: ${str})`, `StringValidator does not validate ${str}`)

         // TVsymbol
         if (typeof v === "symbol")
            TEST( () => TVsymbol.assertType(v,"param1"), "==", undefined, "SymbolValidator validates string")
         else
            TEST( () => TVsymbol.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'symbol' not '${t}' (value: ${str})`, `SymbolValidator does not validate ${str}`)

         // TEST TVundefined
         if (v === undefined)
            TEST( () => TVundefined.assertType(v,"param1"), "==", undefined, "UndefinedValidator validates undefined")
         else
            TEST( () => TVundefined.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'undefined' not '${t}' (value: ${str})`, `UndefinedValidator does not validate ${str}`)

         // TVvalue
         if (v != null && typeof v !== "object" && typeof v !== "function")
            TEST( () => TVvalue.assertType(v,"param1"), "==", undefined, "ValueValidator validates only primitive types")
         else
            TEST( () => TVvalue.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '(bigint|boolean|number|string|symbol)' not '${t}' (value: ${str})`, `ValueValidator does not validate ${str}`)
      }
   } // testSimpleTV

}

export {
   // functions
   ownKeys,
   strIndex,
   strIndices,
   strKey,
   strLiteral,
   strObjectType,
   strType,
   strValue,
   unittest_typevalidator as unittest,
}

// simple type validators
export const TVany=new AnyValidator()
export { TVany as any }
export const TVarray=new ArrayValidator();
export { TVarray as array }
export const TVbigint=new BigintValidator()
export { TVbigint as bigint }
export const TVboolean=new BooleanValidator()
export { TVboolean as boolean }
export const TVconstr=new ConstructorValidator()
export { TVconstr as constr }
export const TVfunction=new FunctionValidator()
export { TVfunction as function }
export const TVkey1=new KeyCount1Validator()
export { TVkey1 as key1 }
export const TVkeystr=new KeyStringValidator()
export { TVkeystr as keystr }
export const TVnull=new NullValidator()
export { TVnull as null }
export const TVnumber=new NumberValidator()
export { TVnumber as number }
export const TVobject=new ObjectValidator()
export { TVobject as object }
export const TVref=new ReferenceValidator() // reference type (without null)
export { TVref as ref }
export const TVstring=new StringValidator()
export { TVstring as string }
export const TVsymbol=new SymbolValidator()
export { TVsymbol as symbol }
export const TVundefined=new UndefinedValidator()
export { TVundefined as undefined }
export const TVvalue=new ValueValidator()
export { TVvalue as value } // value type (without null and undefined)

// complex type validators
export const TVand=((...typeValidators) => new AndValidator(typeValidators))
export { TVand as and }
export const TVinstance=((newTarget) => new InstanceValidator(newTarget))
export { TVinstance as instance }
export const TVkey=((typeValidator,...keys) => new KeyValidator(typeValidator,keys))
export { TVkey as key }
export const TVswitch=((...filterAndtypeValidators) => new SwitchValidator(filterAndtypeValidators))
export { TVswitch as switch }
export const TVtuple=((...typeValidators) => new TupleValidator(typeValidators))
export { TVtuple as tuple }
export const TVunion=((...typeValidators) => new UnionValidator(typeValidators))
export { TVunion as union }
