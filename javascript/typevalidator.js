/* Type validator to validate argument values.
   (c) 2022 Jörg Seebohn */

const ownKeys=Reflect.ownKeys

const strType=(arg) => {
   return strType2(arg,0)
   function strType2(arg,depth) {
      const type=typeof arg

      if (type !== "object")
         return type
      if (arg === null)
         return "null"

      const isDetail=depth<2

      if (Array.isArray(arg)) {
         return (isDetail && arg.length<5
                  ? "[" + arg.map( v => strType2(v,depth+1)).join(",") + "]"
                  : "Array")
      }

      const props=isDetail?ownKeys(arg):[]
      return (arg.constructor?.name ?? "object")
               + (0<props.length && props.length<5
               ? "{" + props.map( prop => `${strKey(prop)}:${strType2(arg[prop],depth+1)}`.join(",")) + "}"
               : "")
   }
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

const strValue=(value) => {
   const type=typeof value
   if (type === "string")
      return strLiteral(value)
   if (type !== "object" || value === null)
      return String(value)

   let str=""
   if (Array.isArray(value)) {
      for (const elem of value) {
         str+=","+strValue(elem)
         if (str.length>30) {
            str=str.substring(0,30)+" ..."
            break;
         }
      }
      return "[" + str.substring(1) + "]"
   }

   for (const prop of ownKeys(value)) {
      str+=","+strKey(prop)+":"+strValue(value[prop])
      if (str.length>30) {
         str=str.substring(0,30)+" ..."
         break;
      }
   }

   return "{" + str.substring(1) + "}"
}

//
//
//
class ValidationContext {

   // top level context (parent == null): name must be name of argument(arg)
   constructor(arg,name,parent=null) {
      this.arg=arg // argument value to check for type
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

   /** Returns undefined | TypeError.
     * You could add a debug function to typeValidator for testing */
   validateWith(typeValidator) {
      const validationResult=typeValidator.validate(this)
      typeValidator.debug?.(validationResult)
      if (validationResult !== undefined) {
         if (validationResult instanceof TypeError)
            return validationResult
         else if (validationResult === false)
            return this.error({expect: typeValidator.expect})
      }
      /* OK */
   }

   validateKey(i,typeValidator) { return this.childContext(i).validateWith(typeValidator) }

   error(arg={}) {
      const msg=arg.msg ?? `of type '${arg.expect}' not '${arg.found ?? strType(this.arg)}'`
      const value=arg.value ?? strValue(this.arg)
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
      const context=new ValidationContext(arg,argName)
      const error=context.validateWith(this)
      if (error !== undefined)
         throw error
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
      const list=typeValidators.reduce( (list,v) => list+delim+getType(v), "")
      return lp + list.substring(delim.length) + rp
   }
}

////////////////
// simple types

class AllValidator extends TypeValidator {
   get expect() { return "any|null|undefined" }
   validate() { } // validates everything
}
class AnyValidator extends TypeValidator {
   get expect() { return "any" }
   validate({arg}) { return arg!=null } // validates null or undefined as an error
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
   validate(context) {
      const nrOfKeys=ownKeys(Object(context.arg)).length
      if (nrOfKeys !== 1)
         return context.error({msg:`to have 1 property not ${nrOfKeys}`})
   }
}
class KeyStringValidator extends TypeValidator {
   get expect() { return "object{[string]:any}" }
   validate(context) {
      if (Object.getOwnPropertySymbols(Object(context.arg)).length > 0)
         return context.error({expect:this.expect(), found:"object{[symbol]:any}"})
   }
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

/////////////////
// complex types

class AndValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeValidators=typeValidators }
   get expect() { return this.typeList("(","&",")",this.typeValidators) }
   validate(context) {
      const error=this.typeValidators.reduce((err,v) => err?err:context.validateWith(v), undefined)
      return error
   }
}
class UnionValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeValidators=typeValidators }
   get expect() { return this.typeList("(","|",")",this.typeValidators) }
   validate(context) {
      const matching=this.typeValidators.find((v) => context.validateWith(v) === undefined)
      return Boolean(matching)
   }
}
class KeyValidator extends TypeValidator {
   constructor(typeValidator=TVall,keys) { super(); this.typeValidator=typeValidator; this.keys=keys; }
   get expect() {
      if (this.keys.length === 0)
         return `object{[string|symbol]:${this.typeValidator.expect}}`
      return this.typeList("object{",",","}",this.keys,(v)=>strKey(v)+":"+this.typeValidator.expect)
   }
   validate(context) {
      const keys=(this.keys.length>0 ? this.keys : ownKeys(Object(context.arg)))
      const arg=Object(context.arg)
      const validateKey=(key) => (
            (key in arg)
            ? context.validateKey(key,this.typeValidator)
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
      const matching=this.typeValidators.some( ([filter]) => context.validateWith(filter) === undefined)
      // if filter matches but detailed validator is undefined then skip detailed validator and return true
      if (matching && matching[1] !== undefined)
         return context.validateWith(matching[1])
      return Boolean(matching)
   }
}
class TupleValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeValidators=typeValidators }
   get expect() { return this.typeList("[",",","]",this.typeValidators) }
   validate(context) {
      var error=context.validateWith(TVarray)
      if (!error && context.arg.length !== this.typeValidators.length)
         error=context.error({msg: `to have array length ${this.typeValidators.length} not ${context.arg.length}`})
      error=this.typeValidators.reduce((err,v,i) => err?err:context.validateKey(i,this.typeValidators[i]), error)
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
      function TT(value,type,str) { return { value, type, str:str||type }; }

      const testTypes=[
         TT(null,"null"), TT(undefined,"undefined"),
         TT(true,"boolean","true"), TT(false,"boolean","false"),
         TT(10,"number","10"), TT(1_000_000_000_000n,"bigint",1_000_000_000_000),
         TT("abc","string","'abc'"), TT(Symbol("symbol"),"symbol","Symbol(symbol)"),
         TT([],"[]"), TT({},"Object","{}"),
         TT(() => true,"function","() => true")
      ]

      for (var i=0; i<testTypes.length; ++i) {
         const { value: v, type: t, str }=testTypes[i]

         // TEST TVnull
         if (v === null)
            TEST( () => TVnull.assertType(v,"param1"), "==", undefined, "NullValidator validates null")
         else
            TEST( () => TVnull.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'null' not '${t}' (value: ${str})`, `NullValidator does not validate ${str}`)

         // TEST TVundefined
         if (v === undefined)
            TEST( () => TVundefined.assertType(v,"param1"), "==", undefined, "UndefinedValidator validates undefined")
         else
            TEST( () => TVundefined.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'undefined' not '${t}' (value: ${str})`, `UndefinedValidator does not validate ${str}`)

         // TEST TVany
         if (v !== undefined && v !== null)
            TEST( () => TVany.assertType(v,"param1"), "==", undefined, "AnyValidator validates anything except null or undefined")
         else
            TEST( () => TVany.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'any' not '${t}' (value: ${str})`, `AnyValidator does not validate ${str}`)

         // TEST TVboolean
         if (v === true || v === false)
            TEST( () => TVboolean.assertType(v,"param1"), "==", undefined, "BooleanValidator validates boolean")
         else
            TEST( () => TVboolean.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'boolean' not '${t}' (value: ${str})`, `BooleanValidator does not validate ${str}`)

         // TEST TVnumber
         if (typeof v === "number")
            TEST( () => TVnumber.assertType(v,"param1"), "==", undefined, "NumberValidator validates number")
         else
            TEST( () => TVnumber.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'number' not '${t}' (value: ${str})`, `NumberValidator does not validate ${str}`)

         // TVbigint
         if (typeof v === "bigint")
            TEST( () => TVbigint.assertType(v,"param1"), "==", undefined, "BigintValidator validates bigint")
         else
            TEST( () => TVbigint.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'bigint' not '${t}' (value: ${str})`, `BigintValidator does not validate ${str}`)

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

         // TEST TVarray
         if (Array.isArray(v))
            TEST( () => TVarray.assertType(v,"param1"), "==", undefined, "ArrayValidator validates []")
         else
            TEST( () => TVarray.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array' not '${t}' (value: ${str})`, `ArrayValidator does not validate ${str}`)

         // TEST TVobject
         if (v !== null && typeof v === "object")
            TEST( () => TVobject.assertType(v,"param1"), "==", undefined, "ObjectValidator validates {}")
         else
            TEST( () => TVobject.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'object' not '${t}' (value: ${str})`, `ObjectValidator does not validate ${str}`)

         // TEST TVfunction
         if (v !== null && typeof v === "function")
            TEST( () => TVfunction.assertType(v,"param1"), "==", undefined, "FunctionValidator validates function")
         else
            TEST( () => TVfunction.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'function' not '${t}' (value: ${str})`, `FunctionValidator does not validate ${str}`)
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
   strType,
   strValue,
   unittest_typevalidator as unittest,
}

// simple type validators
export const TVall=new AllValidator()
export { TVall as all }
export const TVany=new AnyValidator()
export { TVany as any }
export const TVarray=new ArrayValidator();
export { TVarray as array }
export const TVbigint=new BigintValidator()
export { TVbigint as bigint }
export const TVboolean=new BooleanValidator()
export { TVboolean as boolean }
export const TVfunction=new FunctionValidator()
export { TVfunction as function }
export const TVnull=new NullValidator()
export { TVnull as null }
export const TVnumber=new NumberValidator()
export { TVnumber as number }
export const TVobject=new ObjectValidator()
export { TVobject as object }
export const TVkey1=new KeyCount1Validator()
export { TVkey1 as key1 }
export const TVkeystr=new KeyStringValidator()
export { TVkeystr as keystr }
export const TVstring=new StringValidator()
export { TVstring as string }
export const TVsymbol=new SymbolValidator()
export { TVsymbol as symbol }
export const TVundefined=new UndefinedValidator()
export { TVundefined as undefined }

// complex type validators
export const TVand=((...typeValidators) => new AndValidator(typeValidators))
export { TVand as and }
export const TVunion=((...typeValidators) => new UnionValidator(typeValidators))
export { TVunion as union }
export const TVkey=((typeValidator,...keys) => new KeyValidator(typeValidator,keys))
export { TVkey as key }
export const TVswitch=((...filterAndtypeValidators) => new SwitchValidator(filterAndtypeValidators))
export { TVswitch as switch }
export const TVtuple=((...typeValidators) => new TupleValidator(typeValidators))
export { TVtuple as tuple }
