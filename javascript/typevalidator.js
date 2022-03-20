/* Validate argument values to be of correct type.
   (c) 2022 Jörg Seebohn */

/** Returns own properties as array (numbers in ascending order, string properties, and then symbol properties in defined order.) */
const ownKeys=Reflect.ownKeys
/** Returns string representation of access of object property whose name is stored in argument key. */
const strIndex=(key) => {
   return (typeof key === "symbol" || /^([1-9][0-9]*|0)$/.test(key)
          ? "[" + String(key) + "]"
          : /^([$_a-zA-Z][$_a-zA-Z0-9]*)$/.test(key)
          ? "." + key
          : "[" + strLiteral(key) + "]")
}
/** Returns string representation of property access path (see also strIndex).
  * You could call it either with (["a","b",3]) or ("a","b",3) to produce return value ".a.b[3]". */
const strIndices=(...keys) => {
   const path=(Array.isArray(keys[0])? keys[0]:keys).reduce((str,k) => str+strIndex(k), "")
   return path
}
/** Returns string representation of object property name(key) used to build an object literal. */
const strKey=(key) => {
   return (typeof key === "symbol"
          ? "[" + String(key) + "]"
          : /^([$_a-zA-Z][$_a-zA-Z0-9]*|[1-9][0-9]*|0)$/.test(key)
          ? key
          : strLiteral(key))
}
/** Returns string literal representing any value surrounded with ' as delimiter not ". */
const strLiteral=(value)=>"'"+String(value).replace(/['\\]/g,c=>"\\"+c)+"'"
/** Returns object type which is name of its constructor function.
  * Returns "object" if property constructor or constructor.name is undefined. */
const strObjectType=(value) => { return value.constructor?.name || "object" }
/** Returns string representation of a type of any value. Parameter maxdepth determines how deeply nested a type is represented.
  * The default value 1 returns for value {a:1} the string "Object{a:1}". A depth of 0 returns only "Object". */
const strType=(value,maxdepth=1,maxlen=130) => {
   return new TypeStringifier(maxdepth,maxlen).toString(value)
}
/** Returns string representation of any value. Parameter maxdepth determines how deeply nested a value is represented.
  * The default value 1 returns for value {a:1} the string "{a:1}". A depth of 0 returns only "{...}" or "{}" for the empty object. */
const strValue=(value,maxdepth=1,maxlen=130) => {
   return new Stringifier(maxdepth,maxlen).toString(value)
}
/** Removes leading and trailing brackets */
const stripBrackets=(str) => {
   let nr=0, end=str.length-1
   while (str[nr]==='(' && str[end-nr]===')')
      ++nr
   return nr ? str.substring(nr,str.length-nr) : str
}


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
            const isShallow=(this.depth > this.maxdepth)
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

   strPrimitive(type,value) {
      this.append( type === "string" ? strLiteral(value) :
         type === "bigint" ? String(value) + "n" :
         String(value)
      )
   }

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
         for (const next of it)
            this.append(",").toString(next)
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
         this.append(":").toString(value[first.value])
         for (const next of it) {
            this.append(",").strKey(next)
            this.append(":").toString(value[next])
         }
      }
      this.append("}")
   }
}

class TypeStringifier extends Stringifier {
   constructor(maxdepth,maxlen) { super(maxdepth,maxlen) }
   // overwrite value conversions into type ones
   strPrimitive(type/*,value*/) { this.append(type) }
   strFunction() { this.append("function") }
   strShallowArray() { this.append("Array") }
   strShallowObject(value) { this.append(strObjectType(value)) }
   strObject(value) { this.append(strObjectType(value)); super.strObject(value) }
}

class ValidationError {
   constructor(context, err) {
      this.context=context
      this.depth=err.depth ?? context.depth // store maximum nested context depth: used for strValue
      this.expect=err.expect
      this.found=err.found
      this._msg=err.msg
      this.child=err.child
   }
   // A true value means validation of argument failed (not a nested property). It does not mean that the validator is the top most (i.e a UnionValidator validates at the same depth as its childs)
   get isRoot() { return this.context.parent === null }
   get key() { return this.context.name }
   get msg() { return this._msg ?? `of type '${stripBrackets(this.expect)}' not '${stripBrackets(this.found)}'` }
   get value() { return strValue(this.context.arg,Math.max(1,this.depth)) }
   get path() { return this.context.accessPath() }
   get message() { return `Expect argument${this.isRoot?"":" property"} '${this.path}' ${this.msg} (value: ${this.value})` }
}

/** Stores information about current argument, namely its value and name or property key.
  * This information is called the validation context. */
class ValidationContext {

   // top level context (parent == null): name must be name of argument(arg)
   constructor(arg,name,parent=null) {
      this.arg=arg // argument value whose type is validated
      this._name=name // parameter name
      this.parent=parent
      this.depth=(parent ? parent.depth+1 : 0)
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

   get name() { return (typeof this._name === "function" ? this._name() : this._name) }

   error(err) { return new ValidationError(this,err) }
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
         throw new TypeError(error.message)
   }

   /** Returns undefined | ValidationError */
   validateType(arg,argName) { return this.originalError(this.validate(new ValidationContext(arg,argName))) }

   /** Validates property i of argument context.arg. */
   validateProperty(context,i) { return this.validate(context.childContext(i)) }

   /** Returns undefined | ValidationError
     * This function must be implemented in a derived subtype. */
   validate(context) {
      return context.error({expect:"»implementation in subtype",found:"»not implemented«"})
   }

   /** Ensures that top level errors like "expect argument A of type '{a:{b:{c:number}}}'"
     * are shown as "expect argument property A.a.b.c of type 'number'" (if possible). */
   originalError(error) {
      if (error) {
         while (error.child)
            error=error.child
      }
      return error
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
      const types=new Set()
      typeValidators.forEach( (v) => types.add(getType(v)) )
      types.delete(undefined)
      const list=[...types].reduce( (list,t) => list+delim+t)
      return lp + list + rp
   }

   errorIfNot(context,condition) {
      if (!condition)
         return context.error({expect:this.expect,found:strType(context.arg,0)})
   }

   // merge errors
   mergeErrors(context,lp,delim,rp,errors) {
      const depth=errors.reduce((maxDepth,e) => Math.max(maxDepth,e.depth),0)
      const found=this.typeList(lp,delim,rp,errors,e=>e.found)
      const expect=this.typeList(lp,delim,rp,errors,e=>e.expect)
      return new ValidationError(context, {expect, found, depth})
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
   validate(context) { return this.errorIfNot(context,Array.isArray(context.arg)) }
}
class BigintValidator extends TypeValidator {
   get expect() { return "bigint" }
   validate(context) { return this.errorIfNot(context,typeof context.arg === "bigint") }
}
class BooleanValidator extends TypeValidator {
   get expect() { return "boolean" }
   validate(context) { return this.errorIfNot(context,typeof context.arg === "boolean") }
}
class ConstructorValidator extends TypeValidator {
   get expect() { return "constructor" }
   validate(context) {
      // Reflect.construct expects 3rd arg of type newTarget (== constructor)
      try { Reflect.construct(Object, [], context.arg) } catch(e) { return this.errorIfNot(context,false) }
   }
}
class FunctionValidator extends TypeValidator {
   get expect() { return "function" }
   validate(context) { return this.errorIfNot(context,typeof context.arg === "function") }
}
class NullValidator extends TypeValidator {
   get expect() { return "null" }
   validate(context) { return this.errorIfNot(context,context.arg === null) }
}
class NumberValidator extends TypeValidator {
   get expect() { return "number" }
   validate(context) { return this.errorIfNot(context,typeof context.arg === "number") }
}
class ObjectValidator extends TypeValidator {
   get expect() { return "object" }
   validate(context) { return this.errorIfNot(context,typeof context.arg === "object" && context.arg !== null) }
}
class KeyCount1Validator extends TypeValidator {
   get expect() { return "ownKeys(@)==1" }
   validate(context) {
      const nrOfKeys=ownKeys(Object(context.arg)).length
      if (nrOfKeys !== 1)
         return context.error({expect:this.expect,found:`ownKeys(@)==${ownKeys(Object(context.arg))}`,msg:`to have 1 property not ${nrOfKeys}`})
   }
}
class KeyStringValidator extends TypeValidator {
   get expect() { return "{[string]:any}" }
   validate(context) {
      if (Object.getOwnPropertySymbols(Object(context.arg)).length !== 0)
         return context.error({expect:this.expect,found:"{[symbol]:any}"})
   }
}
/** Reference types or complex types are usually mutable (null is excluded). */
class ReferenceValidator extends TypeValidator {
   get expect() { return "»reference«" }
   validate(context) { return this.errorIfNot(context,context.arg!=null && (typeof context.arg==="object" || typeof context.arg==="function")) }
}
class StringValidator extends TypeValidator {
   get expect() { return "string" }
   validate(context) { return this.errorIfNot(context,typeof context.arg === "string") }
}
class SymbolValidator extends TypeValidator {
   get expect() { return "symbol" }
   validate(context) { return this.errorIfNot(context,typeof context.arg === "symbol") }
}
class UndefinedValidator extends TypeValidator {
   get expect() { return "undefined" }
   validate(context) { return this.errorIfNot(context,context.arg === undefined) }
}
/** Value types or primitive types are always immutable (excluding null and undefined). */
class ValueValidator extends TypeValidator {
   get expect() { return "»value«" }
   validate(context) { return this.errorIfNot(context,context.arg!=null && typeof context.arg!=="object" && typeof context.arg!=="function") }
}

/////////////////
// complex types

class AndValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeVals=typeValidators }
   // get expect() { return this.typeList("(","&",")",this.typeVals) }
   validate(context) {
      const error=this.typeVals.reduce((err,v) => err?err:v.validate(context), undefined)
      return error
   }
}
class InstanceValidator extends TypeValidator {
   constructor(newTarget) { super(); this.newTarget=newTarget; }
   get expect() { return `class ${this.newTarget.name}` }
   validate(context) { return this.errorIfNot(context,context.arg instanceof this.newTarget) }
}
class KeyValidator extends TypeValidator {
   constructor(typeValidator=TVany,keys) { super(); this.typeVal=typeValidator; this.keys=keys; }
   expectKey(key) { return `»having property ${strLiteral(key)}«` }
   expectErr(error) { return `{${strKey(error.key)}:${error.expect}}` }
   found(error) { return `{${error.key}:${error.found}}`}
   missing(key) { return `»missing property ${strLiteral(key)}«` }
   msg(key) { return `»to have property ${strLiteral(key)}«` }
   wrapError(context,error) {
      return (error.isRoot ? error
         : context.error({expect:this.expectErr(error), found:this.found(error), depth:error.depth, child:error}))
   }
   validate(context) {
      const arg=Object(context.arg)
      for (const key of (this.keys.length>0 ? this.keys : ownKeys(arg))) {
         if (! (key in arg))
            return context.error({expect:this.expectKey(key),found:this.missing(key),msg:this.msg(key) })
         if (this.typeVal.validateProperty(context,key) !== undefined)
            return this.wrapError(context,this.typeVal.validateProperty(context,key))
      }
   }
}
class SwitchValidator {
   // filterAndTypeValidators: [ [filterTypeValidator,typeValidator|undefined], [filter, type|undefined], ... ]
   constructor(filterAndTypeValidators) { this.typeVals=filterAndTypeValidators }
   validate(context) {
      const matching=this.typeVals.find( ([filter]) => filter.validate(context) === undefined)
      if (matching === undefined)
         return this.mergeErrors(context,"(","|",")",this.typeVals.map( ([filter]) => filter.validate(context)))
      // if filter matches but detailed validator is undefined then skip detailed validator
      if (matching[1] !== undefined)
         return matching[1].validate(context)
   }
}
class UnionValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeVals=typeValidators }
   validate(context) {
      const matching=this.typeVals.find( (v) => v.validate(context) === undefined)
      if (matching === undefined)
         return this.mergeErrors(context,"(","|",")",this.typeVals.map( (v) => v.validate(context)))
   }
}
class TypedArrayValidator extends TypeValidator {
   constructor(typeValidator) { super(); this.typeVal=typeValidator }
   expect(error) { return "["+stripBrackets(error.expect)+",...]" }
   found(error) { return "["+error.key+":"+stripBrackets(error.found)+"]" }
   validate(context) {
      if (TVarray.validate(context))
         return TVarray.validate(context)
      for (let i=0; i<context.arg.length; ++i) {
         const error=this.typeVal.validateProperty(context,i)
         if (error !== undefined)
            return context.error({expect:this.expect(error), found:this.found(error), depth:error.depth, child:error})
      }
   }
}
class TupleValidator extends TypeValidator {
   constructor(typeValidators) { super(); this.typeVals=typeValidators }
   get expect() { return this.typeList("[",",","]",this.typeVals) }
   found(context) { return `@.length===${context.arg.length}` }
   msg(context) { return `to have array length ${this.typeVals.length} not ${context.arg.length}`}
   validate(context) {
      if (TVarray.validate(context))
         return TVarray.validate(context)
      if (context.arg.length !== this.typeVals.length)
         return context.error({expect:this.expect,found:this.found(context),msg:this.msg(context) })
      const error=this.typeVals.reduce((err,v,i) => err?err:v.validateProperty(context,i), undefined)
      return error
   }
}

function unittest_typevalidator(TEST) {

   testFunctions()
   testSimpleValidators()
   testComplexValidators()

   function testFunctions() {
      const syms=[ Symbol(0), Symbol(1), Symbol(2), Symbol(3) ]
      const o1={[syms[1]]:'s1',[syms[2]]:'s2',b:'b',a:'a',1:1,0:0,'x and y':'xy'}
      const oinherit=Object.create(o1)
      function TestType() { this.name="test" }

      // TEST ownKeys
      TEST(ownKeys({}),"==",[],
      "Empty object has no properties")
      TEST(ownKeys(oinherit),"==",[],
      "Inherited properties are not own properties")
      TEST(ownKeys(o1),"==",["0","1","b","a","x and y",syms[1],syms[2]],
      "Returned properties: first the numbers in ascending order then strings and then symbols in defined order")

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

      // TEST strLiteral
      TEST(strLiteral(""),"==","''","Empty string literal")
      TEST(strLiteral("abc"),"==","'abc'","3 letter string literal")
      TEST(strLiteral("\"\\'"),"==","'\"\\\\\\''","string literal with escape sequences")
      TEST(strLiteral("😀"),"==","'😀'","Emoji literal")

      // TEST strObjectType
      TEST(strObjectType(()=>true),"==","Function","function type")
      TEST(strObjectType({}),"==","Object","Object type")
      TEST(strObjectType(new TestType()),"==","TestType","Object type")
      TEST(strObjectType([]),"==","Array","Array type")
      TEST(strObjectType(new String()),"==","String","String type")
      TEST(strObjectType(new (function(){})),"==","object","Anonymous constructor")

      // TEST strType
      TEST(strType(undefined),"==","undefined","type of undefined value")
      TEST(strType(null),"==","null","type of null value")
      TEST(strType(false),"==","boolean","type of boolean value")
      TEST(strType(true),"==","boolean","type of boolean value")
      TEST(strType(1),"==","number","type of number value")
      TEST(strType(1n),"==","bigint","type of bigint value")
      TEST(strType({},0),"==","Object","type of object is constructor name")
      TEST(strType([],0),"==","Array","type of array value")
      TEST(strType(new TestType(),0),"==","TestType","type of object is constructor name")
      TEST(strType(TestType),"==","function","type of named function")
      TEST(strType(()=>true),"==","function","type of function expression")
      TEST(strType({a:1,b:"2",[Symbol(1)]:[]}),"==","Object{a:number,b:string,[Symbol(1)]:Array}","deep type of object")
      TEST(strType(new TestType()),"==","TestType{name:string}","deep type of object")
      TEST(strType([1,"2",true,[]]),"==","[number,string,boolean,Array]","deep type of array is tuple")
      TEST(strType([[1,null]],2),"==","[[number,null]]","deep type (depth:2) of array")

      // TEST strValue
      TEST(strValue(undefined),"==","undefined","undefined value")
      TEST(strValue(null),"==","null","null value")
      TEST(strValue(false),"==","false","boolean value")
      TEST(strValue(true),"==","true","boolean value")
      TEST(strValue(0),"==","0","number value")
      TEST(strValue(12),"==","12","number value")
      TEST(strValue(1234n),"==","1234n","bigint value")
      TEST(strValue({},0),"==","{}","empty object value")
      TEST(strValue(new TestType(),0),"==","{...}","non empty object")
      TEST(strValue([],0),"==","[]","empty array value")
      TEST(strValue([1],0),"==","[...]","non empty array value")
      TEST(strValue(TestType),"==","function TestType","value of named function")
      TEST(strValue(()=>true),"==","()=>{}","value of function expression")
      TEST(strValue({a:1,b:"2",[Symbol(1)]:[]}),"==","{a:1,b:'2',[Symbol(1)]:[]}","deep value of object")
      TEST(strValue(new TestType()),"==","{name:'test'}","deep value of object")
      TEST(strValue([1],0),"==","[...]","shallow value of array")
      TEST(strValue([1,"2",true,[],[1]],1),"==","[1,'2',true,[],[...]]","deep value 2 of array")
      TEST(strValue([[1,null,[],[1]]],2),"==","[[1,null,[],[...]]]","deep value 3 of array")
   }

   function testSimpleValidators() {
      function TT(value,type,strValue) { return { value, type, strValue:strValue||type }; }
      function DummyType(name) { this.name=name }

      const testTypes=[
         TT(null,"null"), TT(undefined,"undefined"),
         TT(true,"boolean","true"), TT(false,"boolean","false"),
         TT(10,"number","10"), TT(1_000_000_000_000n,"bigint",1_000_000_000_000+"n"),
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
         if (v !== null && (typeof v === "object" || typeof v === "function"))
            TEST( () => TVref.assertType(v,"param1"), "==", undefined, "ReferenceValidator validates only function or object types")
         else
            TEST( () => TVref.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '»reference«' not '${t}' (value: ${str})`, `ReferenceValidator does not validate ${str}`)

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
         if (v !== null && v !== undefined && typeof v !== "object" && typeof v !== "function")
            TEST( () => TVvalue.assertType(v,"param1"), "==", undefined, "ValueValidator validates only primitive types")
         else
            TEST( () => TVvalue.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '»value«' not '${t}' (value: ${str})`, `ValueValidator does not validate ${str}`)
      }
   } // testSimpleValidators

   function testComplexValidators() {

      // TEST TVand
      // TODO:

      // TEST TVinstance
      // TODO:

      // TEST TVkey
      // TODO:

      // TEST TVswitch
      // TODO:

      // TEST TVtuple
      // TODO:

      // TEST TVunion
      // TODO:

   } // testComplexValidators
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
export const TVtypedarray=((typeValidator) => new TypedArrayValidator(typeValidator))
export { TVtypedarray as typedarray }
export const TVtuple=((...typeValidators) => new TupleValidator(typeValidators))
export { TVtuple as tuple }
export const TVunion=((...typeValidators) => new UnionValidator(typeValidators))
export { TVunion as union, TVunion as or }
