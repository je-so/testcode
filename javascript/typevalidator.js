/* Validate argument values to be of correct type.
   (c) 2022 Jörg Seebohn */

/* The module consists of functions to represent types and values as strings.
 * The class Stringifier does the conversion to a value.
 * The class TypeStringifier overwrites some methods to stringify a value into its type.
 *
 * Then there is class ValidationError which describes any found type error.
 * ValidationError reads error information from ValidationContext and an additional
 * argument object describing {expect: "expected type", found:"(unexpected) found type" }.
 * The generated error message is always the same.
 * Use {msg: "own message"} to overwrite the second half of the message.
 * See ValidationError.message for the generated error message.
 *
 * Why this stringent error template?
 * The reason is that a UnionValidator uses A LIST OF TypeValidator to select a matching one.
 * If all validators fail the union validator merges the error messages into one.
 * To be able to do that the format must always be the same.
 * The union validator shows for example the error message
 * > "Expect argument 'test' of type 'string|number' not 'bigint' (value: 1n)""
 * if called with TVunion(TVstring,TVnumber).assertType(1n,"test")
 *
 * TypeValidator offers the method assertType to test a value for obeying a certain type structure.
 * It throws a TypeError exception in case of an error.
 * To get only the error object use method validateArg.
 * In case of no validation error value undefined is returned.
 */

/** Returns own properties as array (numbers in ascending order, string properties, and then symbol properties in defined order.) */
const ownKeys=Reflect.ownKeys
/** Returns string representation of access of object property [key]. */
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
      const type=(value===null ? "null":typeof value)
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
   // Returns true in case error occurred in child context.
   get isProp() { return this.context.parent !== null }
   get key() { return this.context.name }
   get msg() { return this._msg ?? `of type '${stripBrackets(this.expect)}' not '${stripBrackets(this.found)}'` }
   get value() { return strValue(this.context.arg,Math.max(1,this.depth)) }
   get path() { return this.context.accessPath() }
   get message() { return `Expect argument${this.isProp?" property":""} '${this.path}' ${this.msg} (value: ${this.value})` }
}

/** Stores information about current argument, namely its value and name or property key.
  * This information is called the validation context. */
class ValidationContext {

   // top level context (parent == null): name must be name of argument(arg)
   constructor(arg,name,parent=null) {
      this.arg=arg // argument value whose type is validated (or property value in case of child context)
      this._name=name // name of argument (parameter name) (or name of property in case of child context)
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
      const error=this.validateArg(arg,argName)
      if (error !== undefined)
         throw new TypeError(error.message)
   }

   /** Returns undefined | ValidationError. */
   validateArg(arg,argName) { return this.originalError(this.validate(new ValidationContext(arg,argName))) }

   /** Validates property i of argument context.arg. */
   validateProperty(context,i) { return this.validate(context.childContext(i)) }

   /** Returns undefined | ValidationError.
     * This function must be implemented in a derived subtype. */
   validate(context) {
      return context.error({expect:"»implemented in subtype",found:"»not implemented«"})
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
    * Helper function to build a typename from a list of types originating from validators(or other object types).
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
    *   If getType is undefined a default function is used to access an "expect" property of the object.
    * @returns
    *   The generated list of (expected or otherwise) type names.
    */
   typeList(lp,delim,rp,typeValidators,getType=(v) => v.expect) {
      const types=new Set()
      typeValidators.forEach( (v) => types.add(getType(v)) )
      types.delete(undefined)
      const list=[...types].reduce( (list,t) => list+delim+t)
      return lp + list + rp
   }

   /** Checks argument condition to be true. If false an error is returned.
     * The calling validator must have property expect (or implement get expect()).
     * This kind of error checking is used in mostly all simple validators. */
   errorIfNot(context,condition) {
      if (!condition)
         return context.error({expect:this.expect,found:strType(context.arg,0)})
   }

   /** Build a new error with {expect, found} merged from a list of errors.
     * The depth of the error is set to maximum depth of errors. */
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
   get expect() { return "function|object" }
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
   get expect() { return "boolean|bigint|number|string|symbol" }
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
class EnumValidator extends TypeValidator {
   constructor(values) {
      super()
      this.valueMap=new Map( Array.isArray(values)
         ? values.map( v => [v,strValue(v)] )
         : ownKeys(values).map( key => [values[key],String(key)])
      )
   }
   get expect() { return `Enum[${[...this.valueMap.values()].join(",")}]` }
   found(value) { return `»unknown enum value ${strValue(value)}«` }
   validate(context) {
      if (! this.valueMap.has(context.arg))
         return context.error({expect:this.expect,found:this.found(context.arg)})
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
   missingKey(key) { return `»missing property ${strLiteral(key)}«` }
   expectProp(error) { return `{${strKey(error.key)}:${error.expect}}` }
   foundProp(error) { return `{${error.key}:${error.found}}`}
   msg(key) { return `to have property ${strLiteral(key)}` }
   wrapError(context,error) {
      return context.error({expect:this.expectProp(error), found:this.foundProp(error), depth:error.depth, child:error})
   }
   validate(context) {
      const arg=Object(context.arg)
      for (const key of (this.keys.length>0 ? this.keys : ownKeys(arg))) {
         if (! (key in arg))
            return context.error({expect:this.expectKey(key),found:this.missingKey(key),msg:this.msg(key) })
         if (this.typeVal.validateProperty(context,key) !== undefined)
            return this.wrapError(context,this.typeVal.validateProperty(context,key))
      }
   }
}
class PredicateValidator extends TypeValidator {
   constructor(predicateFct) { super(); this.predicateFct=predicateFct }
   validate(context) {
      const error=this.predicateFct(context)
      if (error === undefined || error instanceof ValidationError)
         return error
      throw Error(`Expect predicate to return type 'ValidationError' not '${strType(error,0)}'`)
   }
}
/** Use SwitchValidator instead of UnionValidator if you want simpler error messages.
  * For example if you want to validate type '[number,string,boolean]|{a:number,b:string}'
  * and do not want to show this string as expected type use TVswitch([TVarray,TVtuple],[TVobject,TVand(TVkey(TVnumber,"a"),...)]).
  * In that case if you want to validate value 1 the expected type in the error message
  * is shown as "Array|object". */
class SwitchValidator extends TypeValidator {
   // filterAndTypeValidators: [ [filterTypeValidator,typeValidator|undefined], [filter, type|undefined], ... ]
   constructor(filterAndTypeValidators) { super(); this.typeVals=filterAndTypeValidators }
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
   expectLen() { return `[].length==${this.typeVals.length}` }
   foundLen(context) { return `[].length==${context.arg.length}` }
   msg(context) { return `to have array length ${this.typeVals.length} not ${context.arg.length}`}
   expect(error) { return "["+error.key+":"+stripBrackets(error.expect)+"]" }
   found(error) { return "["+error.key+":"+stripBrackets(error.found)+"]" }
   validate(context) {
      if (TVarray.validate(context))
         return TVarray.validate(context)
      if (context.arg.length !== this.typeVals.length)
         return context.error({expect:this.expectLen(),found:this.foundLen(context),msg:this.msg(context) })
      const error=this.typeVals.reduce((err,v,i) => err?err:v.validateProperty(context,i), undefined)
      if (error)
         return context.error({expect:this.expect(error), found:this.found(error), depth:error.depth, child:error})
   }
}

function unittest_typevalidator(TEST) {

   function DummyType() { this.name="test" }
   function TestType() { this.test=true }
   // ensures that function expression has no name (else "value:()=>true" would assign name "value")
   function anon(fct) { return fct }

   const testStruct=[
   /*0*/ { value:null, strtype:"null", strval:"null" },
   /*1*/ { value:undefined, strtype:"undefined", strval:"undefined" },
   /*2*/ { value:true, strtype:"boolean", strval:"true" },
   /*3*/ { value:false, strtype:"boolean", strval:"false" },
   /*4*/ { value:10, strtype:"number", strval:"10"},
   /*5*/ { value:1_000_000_000_000n, strtype:"bigint", strval:"1000000000000n"},
   /*6*/ { value:"abc", strtype:"string", strval:"'abc'"},
   /*7*/ { value:"", strtype:"string", strval:"''"},
   /*8*/ { value:Symbol("sym"), strtype:"symbol", strval:"Symbol(sym)"},
   /*9*/ { value:[], strtype:"Array", strval:"[]"},
   /*10*/ { value:[1], strtype:"Array", strval:"[1]"},
   /*11*/ { value:[1,'2',true], strtype:"Array", strval:"[1,'2',true]"},
   /*12*/ { value:[1,2,3,4], strtype:"Array", strval:"[1,2,3,4]"},
   /*13*/ { value:[1,2,3,'x',4], strtype:"Array", strval:"[1,2,3,'x',4]"},
   /*14*/ { value:{}, strtype:"Object", strval:"{}"},
   /*15*/ { value:{a:1}, strtype:"Object", strval:"{a:1}"},
   /*16*/ { value:{a:1,b:'2'}, strtype:"Object", strval:"{a:1,b:'2'}"},
   /*17*/ { value:{p1:{p2:{p3:4}}}, strtype:"Object", strval:"{p1:{...}}"},
   /*18*/ { value:{[Symbol(1)]:1}, strtype:"Object", strval:"{[Symbol(1)]:1}"},
   /*19*/ { value:anon(()=>true), strtype:"function", strval:"()=>{}"},
   /*20*/ { value:DummyType, strtype:"function", strval:"function DummyType"},
   /*21*/ { value:TestType, strtype:"function", strval:"function TestType"},
   /*22*/ { value:new DummyType(), strtype:"DummyType", strval:"{name:'test'}"},
   /*23*/ { value:new String(123), strtype:"String", strval:"{0:'1',1:'2',2:'3',length:3}"},
   /*24*/ { value:new TestType(), strtype:"TestType", strval:"{test:true}"},
   ]
   const testValues=testStruct.map( t => t.value )
   const namedTestValues=testStruct.reduce((obj,t,i) => ({...obj, ['E-'+i]:t.value}), {})

   testFunctions()
   testSimpleValidators()
   testComplexValidators()

   function testFunctions() {
      const sym=[Symbol('0'), Symbol('1')]
      const o1={[sym[0]]:'s1',[sym[1]]:'s2', b:'b',a:'a', 1:1,0:0, 'x and y':'xy'}

      // TEST ownKeys
      TEST(ownKeys({}),"==",[],
      "Empty object has no properties")
      TEST(ownKeys(Object.create(o1)),"==",[],
      "Inherited properties are not own properties")
      TEST(ownKeys(o1),"==",["0","1","b","a","x and y",sym[0],sym[1]],
      "Returned properties: first the numbers in ascending order then strings and then symbols in defined order")

      // TEST strKey (Property key used in object literal)
      TEST(strKey(Symbol(0)),"==","[Symbol(0)]","Symbol")
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
      TEST(strIndex(Symbol('x')),"==","[Symbol(x)]","Symbol")
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
      TEST(strIndices([Symbol('_')]),"==","[Symbol(_)]","Single index")
      TEST(strIndices(1,2,3),"==","[1][2][3]","3d array access")
      TEST(strIndices([1],2,3),"==","[1]","If first arg is Array other args are ignored")
      TEST(strIndices(["o",1,",''","id",Symbol()]),"==",".o[1][',\\'\\''].id[Symbol()]","multiple properties")
      TEST(strIndices("id_",2,"\\","ty",Symbol(2)),"==",".id_[2]['\\\\'].ty[Symbol(2)]","multiple properties")

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
      TEST(strType(new DummyType()),"==","DummyType{name:string}","deep type of object")
      TEST(strType(new TestType()),"==","TestType{test:boolean}","deep type of object")
      TEST(strType([1,"2",true,[]]),"==","[number,string,boolean,Array]","deep type of array is tuple")
      TEST(strType([[1,null]],2),"==","[[number,null]]","deep type (depth:2) of array")
      for (const ts of testStruct)
         TEST(strType(ts.value,0),"==",ts.strtype,"shallow type (depth:0) equals")

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
      TEST(strValue(new DummyType()),"==","{name:'test'}","deep type of object")
      TEST(strValue(new TestType()),"==","{test:true}","deep value of object")
      TEST(strValue([1],0),"==","[...]","shallow value of array")
      TEST(strValue([1,"2",true,[],[1]],1),"==","[1,'2',true,[],[...]]","deep value 2 of array")
      TEST(strValue([[1,null,[],[1]]],2),"==","[[1,null,[],[...]]]","deep value 3 of array")
      for (const ts of testStruct)
         TEST(strValue(ts.value,1),"==",ts.strval,"deep value (depth:1) equals")
   }

   function testSimpleValidators() {

      for (var i=0; i<testStruct.length; ++i) {
         const { value: v, strtype: t, strval: str }=testStruct[i]

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
         if (v === DummyType || v === TestType)
            TEST( () => TVconstr.assertType(v,"param1"), "==", undefined, "Should validate constructor function")
         else
            TEST( () => TVconstr.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'constructor' not '${t}' (value: ${str})`, `Should not validate other types than constructor functions`)

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
            TEST( () => TVref.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'function|object' not '${t}' (value: ${str})`, `ReferenceValidator does not validate ${str}`)

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
            TEST( () => TVvalue.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'boolean|bigint|number|string|symbol' not '${t}' (value: ${str})`, `ValueValidator does not validate ${str}`)

      }
   } // testSimpleValidators

   function testComplexValidators() {

      for (var i=0; i<testStruct.length; ++i) {
         const { value: v, strtype: t, strval: str }=testStruct[i]
         const wrappers=[TVand,TVswitch,TVunion]
         const withoutV=(() => { const without=testValues.slice(); without.splice(i,1); return without })()
         const strWithout=withoutV.map( v => strValue(v) ).join(",")
         const withoutNamedV=(() => { const without=Object.assign({},namedTestValues); delete without["E-"+i]; return without })()
         const strWithoutNamed=ownKeys(withoutNamedV).join(",")

         wrappers.forEach( tvComplex => {

            // === COMPLEX validator as wrapper for exactly one simple one ===
            // same behaviour as simple one (except TVswitch,TVunion in nested case TVkey)

            const wrapTV=(tvSimple) => {
               if (tvComplex === TVswitch)
                  return tvComplex([tvSimple])
               else
                  return tvComplex(tvSimple)
            }

            // TEST TVEnum
            TEST( () => wrapTV(TVenum(v)).assertType(v,"p"), "==", undefined, "Should validate equal value")
            TEST( () => wrapTV(TVenum(...testValues)).assertType(v,"p"), "==", undefined, "Should validate value from set")
            TEST( () => wrapTV(TVenum(...withoutV)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Enum[${strWithout}]' not '»unknown enum value ${str}«' (value: ${str})`, "EnumValidator throws if value is not in set")
            if ("a" in Object(v)) {
               TEST( () => wrapTV(TVkey(TVenum(1),"a")).assertType(v,`p${i}`), "==", undefined, "Should validate {a:1}")
               if (tvComplex==TVand)
                  TEST( () => wrapTV(TVkey(TVenum(2),"a")).assertType(v,`p${i}`), "throw", `Expect argument property 'p${i}.a' of type 'Enum[2]' not '»unknown enum value 1«' (value: 1)`, "EnumValidator does not validate {a:1}")
               else
                  TEST( () => wrapTV(TVkey(TVenum(2),"a")).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '{a:Enum[2]}' not '{a:»unknown enum value 1«}' (value: ${str})`, "Should not validate {a:1}")
            }

            // TEST TVnumber
            if (typeof v === "number")
               TEST( () => wrapTV(TVnumber).assertType(v,"param1"), "==", undefined, "NumberValidator validates number")
            else
               TEST( () => wrapTV(TVnumber).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'number' not '${t}' (value: ${str})`, `NumberValidator does not validate ${str}`)

            // TEST TVobject
            if (v != null && typeof v === "object")
               TEST( () => wrapTV(TVobject).assertType(v,"param1"), "==", undefined, "ObjectValidator validates {}")
            else
               TEST( () => wrapTV(TVobject).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'object' not '${t}' (value: ${str})`, `ObjectValidator does not validate ${str}`)

            // TEST TVref
            if (v !== null && (typeof v === "object" || typeof v === "function"))
               TEST( () => wrapTV(TVref).assertType(v,"param1"), "==", undefined, "ReferenceValidator validates only function or object types")
            else
               TEST( () => wrapTV(TVref).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'function|object' not '${t}' (value: ${str})`, `ReferenceValidator does not validate ${str}`)

         })

         // TEST TVand
         const tvand=TVand(TVref,TVobject,TVkey1)
         if (typeof v === "object" && ownKeys(Object(v)).length===1)
            TEST( () => tvand.assertType(v,"p"), "==", undefined, "AndValidator validates {key:any}")
         else if (v === null || (typeof v !== "object" && typeof v !== "function"))
            TEST( () => tvand.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'function|object' not '${t}' (value: ${str})`, `ReferenceValidator does not validate ${str}`)
         else if (typeof v !== "object")
            TEST( () => tvand.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'object' not '${t}' (value: ${str})`, `ObjectValidator does not validate ${str}`)
         else
            TEST( () => tvand.assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' to have 1 property not ${Object.getOwnPropertyNames(Object(v)).length} (value: ${str})`, "KeyCount1Validator does only validate {a:1}")

         // TEST TVenum (values array)
         TEST( () => TVenum(v).assertType(v,"p"), "==", undefined, "EnumValidator validates same value")
         TEST( () => TVenum(...testValues).assertType(v,"p"), "==", undefined, "EnumValidator validates value from set")
         TEST( () => TVenum(...withoutV).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Enum[${strWithout}]' not '»unknown enum value ${str}«' (value: ${str})`, "EnumValidator throws if value is not in set")
         if ("a" in Object(v)) {
            TEST( () => TVkey(TVenum(1),"a").assertType(v,`p${i}`), "==", undefined, "EnumValidator validates {a:1}")
            TEST( () => TVkey(TVenum(2),"a").assertType(v,`p${i}`), "throw", `Expect argument property 'p${i}.a' of type 'Enum[2]' not '»unknown enum value 1«' (value: 1)`, "EnumValidator does not validate {a:1}")
         }

         // TEST TVnamedenum (named values are listed in Enum[name1,name2,name3])
         TEST( () => TVnamedenum({name:v}).assertType(v,"p"), "==", undefined, "EnumValidator should validate same named value")
         TEST( () => TVnamedenum({name:!v}).assertType(v,"p"), "throw", `Expect argument 'p' of type 'Enum[name]' not '»unknown enum value ${str}«' (value: ${str})`, "EnumValidator should throw if named value is not equal")
         TEST( () => TVnamedenum(namedTestValues).assertType(v,"p"), "==", undefined, "EnumValidator should validate value from named set")
         TEST( () => TVnamedenum(withoutNamedV).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Enum[${strWithoutNamed}]' not '»unknown enum value ${str}«' (value: ${str})`, "EnumValidator throws if value is not in set")

         // TEST InstanceValidator
         if (v instanceof DummyType)
            TEST( () => TVinstance(DummyType).assertType(v,"p"), "==", undefined, "InstanceValidator validates instanceof")
         else
            TEST( () => TVinstance(DummyType).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'class DummyType' not '${t}' (value: ${str})`, "This InstanceValidator does only validate DummyType")
         if (v instanceof String)
            TEST( () => TVinstance(String).assertType(v,"p"), "==", undefined, "InstanceValidator validates instanceof")

         // TEST TVkey
         if ("a" in Object(v))
            TEST( () => TVkey(TVnumber,"a").assertType(v,"p"), "==", undefined, "KeyValidator validates {a:any}")
         else
            TEST( () => TVkey(TVnumber,"a").assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' to have property 'a' (value: ${str})`, "KeyValidator validates no other than {a:any}")
         if ("a" in Object(v) && "b" in v)
            TEST( () => TVkey(TVany,"a","b").assertType(v,"p"), "==", undefined, "KeyValidator validates {a:any,b:any}")
         else if ("a" in Object(v))
            TEST( () => TVkey(TVany,"a","b").assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' to have property 'b' (value: ${str})`, "KeyValidator does not validate other than {a:any,b:any}")
         if ("a" in Object(v) && "b" in v)
            TEST( () => TVkey(TVnumber,"a","b").assertType(v,`p${i}`), "throw", `Expect argument property 'p${i}.b' of type 'number' not 'string' (value: ${strLiteral(v.b)})`, "KeyValidator does not validate {a:number,b:string}")
         if ("s1" in Object(v)) {
            TEST( () => TVkey(TVkey(TVkey(TVnumber,"s3"),"s2"),"s1").assertType(v,"p"), "==", undefined, "KeyValidator validates {s1:{s2:{s3:number}}}")
            TEST( () => TVkey(TVkey(TVkey(TVstring,"s3"),"s2"),"s1").assertType(v,"p"), "throw", `Expect argument property 'p.s1.s2.s3' of type 'string' not 'number' (value: 4)`, "KeyValidator does not validate {s1:{s2:{s3:number}}}")
         }
         // wrapped form
         if ("a" in Object(v))
            TEST( () => TVunion(TVkey(TVnumber,"a")).assertType(v,"p"), "==", undefined, "KeyValidator validates {a:any}")
         else
            TEST( () => TVunion(TVkey(TVnumber,"a")).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '»having property 'a'«' not '»missing property 'a'«' (value: ${str})`, "KeyValidator validates no other than {a:any}")
         if ("a" in Object(v) && "b" in v)
            TEST( () => TVunion(TVkey(TVany,"a","b")).assertType(v,"p"), "==", undefined, "KeyValidator validates {a:any,b:any}")
         else if ("a" in Object(v))
            TEST( () => TVunion(TVkey(TVany,"a","b")).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '»having property 'b'«' not '»missing property 'b'«' (value: ${str})`, "KeyValidator does not validate other than {a:any,b:any}")
         if ("a" in Object(v) && "b" in v)
            TEST( () => TVunion(TVkey(TVnumber,"a","b")).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '{b:number}' not '{b:string}' (value: ${str})`, "KeyValidator does not validate {a:number,b:string}")
         if ("s1" in Object(v)) {
            TEST( () => TVunion(TVkey(TVkey(TVkey(TVnumber,"s3"),"s2"),"s1")).assertType(v,"p"), "==", undefined, "KeyValidator validates {s1:{s2:{s3:number}}}")
            TEST( () => TVunion(TVkey(TVkey(TVkey(TVstring,"s3"),"s2"),"s1")).assertType(v,"p"), "throw", `Expect argument 'p' of type '{s1:{s2:{s3:string}}}' not '{s1:{s2:{s3:number}}}' (value: {s1:{s2:{s3:4}}})`, "KeyValidator does not validate {s1:{s2:{s3:number}}}")
         }

         // TEST TVpredicate
         TEST( () => TVpredicate(() => undefined).assertType(v,"p"), "==", undefined, "PredicateValidator validates any")
         TEST( () => TVpredicate((c) => c.error({expect:"???",found:"!!!"})).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '???' not '!!!' (value: ${str})`, "PredicateValidator fails all values")
         TEST( () => TVpredicate((c) => c.error({expect:"???",found:"!!!",msg:"MSG"})).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' MSG (value: ${str})`, "PredicateValidator fails all values with message MSG")

         // TEST TVswitch
         if (v !== null && typeof v === "object")
            TEST( () => TVswitch([TVarray],[TVobject]).assertType(v,`p${i}`), "==", undefined, "Should validate Array|object")
         else {
            TEST( () => TVswitch([TVarray],[TVobject]).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array|object' not '${t}' (value: ${str})`, "Should not validate primitives or functions")
            TEST( () => TVswitch([TVarray,TVkey(TVnumber,0)],[TVobject,TVkey(TVnumber,"a")]).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array|object' not '${t}' (value: ${str})`, "TVswitch does not validate primitives or functions")
         }
         if ("a" in Object(v) || (Array.isArray(v) && v[0] === 1))
            TEST( () => TVswitch([TVarray,TVkey(TVnumber,0)],[TVobject,TVkey(TVnumber,"a")]).assertType(v,`p${i}`), "==", undefined, "TVswitch validates [number]|{a:number}")
         else if (v !== null && typeof v === "object" && !Array.isArray(v))
            TEST( () => TVswitch([TVarray,TVkey(TVnumber,0)],[TVobject,TVkey(TVnumber,"a")]).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' to have property 'a' (value: ${str})`, "TVswitch does not validate {}")
         else if (Array.isArray(v))
            TEST( () => TVswitch([TVarray,TVkey(TVnumber,0)],[TVobject,TVkey(TVnumber,"a")]).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' to have property '0' (value: ${str})`, "TVswitch does not validate []")

         // TEST TVtypedarray
         if (Array.isArray(v) && (v.length <= 1 || v.length === 4))
            TEST( () => TVtypedarray(TVnumber).assertType(v,`p${i}`), "==", undefined, "TVtypedarray does [number,...]")
         else if (Array.isArray(v) && v.length === 3)
            TEST( () => TVtypedarray(TVnumber).assertType(v,`p${i}`), "throw", `Expect argument property 'p${i}[1]' of type 'number' not 'string' (value: '2')`, "TVtypedarray does not validate other than [number,...]")
         else if (Array.isArray(v) && v.length === 5)
            TEST( () => TVtypedarray(TVnumber).assertType(v,`p${i}`), "throw", `Expect argument property 'p${i}[3]' of type 'number' not 'string' (value: 'x')`, "TVtypedarray does not validate other than [number,...]")
         else
            TEST( () => TVtypedarray(TVnumber).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array' not '${t}' (value: ${str})`, "TVtypedarray does not validate other than [number,...]")
         // wrapped form
         if (Array.isArray(v) && (v.length <= 1 || v.length === 4))
            TEST( () => TVor(TVtypedarray(TVnumber)).assertType(v,`p${i}`), "==", undefined, "TVtypedarray does [number,...]")
         else if (Array.isArray(v) && v.length === 3)
            TEST( () => TVor(TVtypedarray(TVnumber)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '[number,...]' not '[1:string]' (value: ${str})`, "TVtypedarray does not validate other than [number,...]")
         else if (Array.isArray(v) && v.length === 5)
            TEST( () => TVor(TVtypedarray(TVnumber)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '[number,...]' not '[3:string]' (value: ${str})`, "TVtypedarray does not validate other than [number,...]")
         else
            TEST( () => TVor(TVtypedarray(TVnumber)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array' not '${t}' (value: ${str})`, "TVtypedarray does not validate other than [number,...]")

         // TEST TVtuple
         if (Array.isArray(v) && v.length === 1) {
            TEST( () => TVtuple(TVnumber).assertType(v,`p${i}`), "==", undefined, "TVtuple validates [number]")
            TEST( () => TVtuple(TVstring).assertType(v,`p${i}`), "throw", `Expect argument property 'p${i}[0]' of type 'string' not 'number' (value: ${v[0]})`, "TVtuple does not validate [number]")
         }
         else if (Array.isArray(v) && v.length !== 1)
            TEST( () => TVtuple(TVnumber).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' to have array length 1 not ${v.length} (value: ${str})`, "TVtuple does not validate [].length!=1")
         else
            TEST( () => TVtuple(TVnumber).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array' not '${t}' (value: ${str})`, "TVtuple does not validate other than []")
         if (Array.isArray(v) && v.length === 3) {
            TEST( () => TVtuple(TVnumber,TVstring,TVboolean).assertType(v,`p${i}`), "==", undefined, "TVtuple validates [number,string,boolean]")
            TEST( () => TVtuple(TVnumber,TVstring,TVor(TVnull,TVstring)).assertType(v,`p${i}`), "throw", `Expect argument property 'p${i}[2]' of type 'null|string' not 'boolean' (value: true)`, "TVtuple does not validate [number,string,boolean]")
         }
         else if (Array.isArray(v) && v.length !== 3)
            TEST( () => TVtuple(TVnumber,TVstring,TVboolean).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' to have array length 3 not ${v.length} (value: ${str})`, "TVtuple does not validate [].length!=3")
         else
            TEST( () => TVtuple(TVnumber,TVstring,TVboolean).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array' not '${t}' (value: ${str})`, "TVtuple does not validate other than []")
         // wrapped form
         if (Array.isArray(v) && v.length === 1) {
            TEST( () => TVunion(TVtuple(TVnumber)).assertType(v,`p${i}`), "==", undefined, "TVtuple validates [number]")
            TEST( () => TVunion(TVtuple(TVstring)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '[0:string]' not '[0:number]' (value: ${str})`, "TVtuple does not validate [number]")
         }
         else if (Array.isArray(v) && v.length !== 1)
            TEST( () => TVunion(TVtuple(TVnumber)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '[].length==1' not '[].length==${v.length}' (value: ${str})`, "TVtuple does not validate [].length!=1")
         else
            TEST( () => TVunion(TVtuple(TVnumber)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array' not '${t}' (value: ${str})`, "TVtuple does not validate other than []")
         if (Array.isArray(v) && v.length === 3) {
            TEST( () => TVunion(TVtuple(TVnumber,TVstring,TVboolean)).assertType(v,`p${i}`), "==", undefined, "TVtuple validates [number,string,boolean]")
            TEST( () => TVunion(TVtuple(TVnumber,TVstring,TVor(TVnull,TVstring))).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '[2:null|string]' not '[2:boolean]' (value: ${str})`, "TVtuple does not validate [number,string,boolean]")
         }
         else if (Array.isArray(v) && v.length !== 3)
            TEST( () => TVunion(TVtuple(TVnumber,TVstring,TVboolean)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type '[].length==3' not '[].length==${v.length}' (value: ${str})`, "TVtuple does not validate [].length!=3")
         else
            TEST( () => TVunion(TVtuple(TVnumber,TVstring,TVboolean)).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'Array' not '${t}' (value: ${str})`, "TVtuple does not validate other than []")

         // TEST TVunion
         if (typeof v === "number")
            TEST( () => TVunion(TVnumber).assertType(v,`p${i}`), "==", undefined, "TVunion validates number")
         else
            TEST( () => TVunion(TVnumber).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'number' not '${t}' (value: ${str})`, "TVunion does not validate other than number")
         if (v === null || typeof v === "number")
            TEST( () => TVunion(TVnumber,TVnull).assertType(v,`p${i}`), "==", undefined, "TVunion validates number|null")
         else
            TEST( () => TVunion(TVnumber,TVnull).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'number|null' not '${t}' (value: ${str})`, "TVunion does not validate other than number|null")
         if (v === null || typeof v === "number" || typeof v === "string")
            TEST( () => TVunion(TVnumber,TVnull,TVstring).assertType(v,`p${i}`), "==", undefined, "TVunion validates number|null|string")
         else
            TEST( () => TVunion(TVnumber,TVnull,TVstring).assertType(v,`p${i}`), "throw", `Expect argument 'p${i}' of type 'number|null|string' not '${t}' (value: ${str})`, "TVunion does not validate other than number|null|string")

      }

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
export const TVenum=((...values) => new EnumValidator(values))
export { TVenum as enum }
export const TVnamedenum=((namedValues) => new EnumValidator(namedValues))
export { TVnamedenum as namedenum }
export const TVinstance=((newTarget) => new InstanceValidator(newTarget))
export { TVinstance as instance }
export const TVkey=((typeValidator,...keys) => new KeyValidator(typeValidator,keys))
export { TVkey as key }
export const TVpredicate=((predicateFct) => new PredicateValidator(predicateFct))
export { TVpredicate as predicate }
export const TVswitch=((...filterAndtypeValidators) => new SwitchValidator(filterAndtypeValidators))
export { TVswitch as switch }
export const TVtypedarray=((typeValidator) => new TypedArrayValidator(typeValidator))
export { TVtypedarray as typedarray }
export const TVtuple=((...typeValidators) => new TupleValidator(typeValidators))
export { TVtuple as tuple }
export const TVunion=((...typeValidators) => new UnionValidator(typeValidators))
export { TVunion as union }
export const TVor=TVunion // TVor alias of TVunion
export { TVor as or }
