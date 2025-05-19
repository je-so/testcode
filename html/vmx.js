/**
 * Class-Types for handling HTML-Views.
 *
 * === Helper Classes ===
 * * PromiseHolder
 * * OptionsParser
 * * // TODO: ClientLog
 * * // TODO: HTTP
 * === module Context ===
 * * LogIDBinding
 * * ViewModelContext (stored in »const VMX = new ViewModelContext()«)
 * * LogID
 * === Wrapping of event listeners
 * * ViewListener (capture + bubble)
 * * ViewListeners
 * * ViewModelActionLocking
 * * ViewController
 * * ViewDrawListener
 * === Wrapping HTML
 * (not fully, htmlElem is accessed within ViewModel,Controller,and Decorator.)
 * * ViewElem
 * * View
 * === Binding of View, ViewModel, and Controllers
 * * ViewDecorator
 * * ResizeDecorator
 * * WindowDecorator
 * === Wrapping state and displayed data of a view
 * * ViewModel
 * * ResizeVM
 * * WindowVM
 * === Model ===
 * * ClientLogModel
 */


class BasicTypeChecker {
   /**
    * @typedef {(this:BasicTypeChecker,value:any,typeCheckerContext:object)=>false|((name:string)=>string)} BasicTypeCheckerValidateType Returns *false* if validation was correct else error string.
    * @typedef {(value:any,typeCheckerContext:object)=>boolean} BasicTypeCheckerTypePredicate Returns *true* if type is valid else false.
    */
   /**
    * Return *false*, if type is valid, else error description.
    * @type {BasicTypeCheckerValidateType}
    */
   validateType
   /**
    * Description of subtype contained in parent container type.
    * @type {undefined|BasicTypeChecker}
    */
   subtype
   /**
    * Set value to *true*, if a value must exist.
    * @type {undefined|boolean}
    */
   required
   /**
    * @param {BasicTypeCheckerValidateType} validateType
    * @param {null|BasicTypeChecker} [subtype]
    */
   constructor(validateType, subtype) {
      this.validateType = validateType
      if (subtype) this.subtype = subtype
   }
   /** @param {boolean} required @return BasicTypeChecker */
   makeRequired(required=true) { return this.required = required, this }
   /**
    * @param {any} value
    * @param {object} typeCheckerContext
    * @return {false|((name:string)=>string)}
    */
   validate(value, typeCheckerContext) { return this.validateType(value,typeCheckerContext) }
   /**
    * @param {BasicTypeCheckerTypePredicate} typePredicate
    * @param {string} type Name of expected type.
    * @returns {BasicTypeChecker}
    */
   static simple(typePredicate, type) { return new BasicTypeChecker((value,typeCheckerContext) => !typePredicate(value,typeCheckerContext) && BasicTypeChecker.expectValueOfType(value,type)) }
   /**
    * @param {string} ofSmething Description of validation expectation.
    * @return {(name:string)=>string} Error description generator.
    */
   static expectSomething(ofSmething) { return (name)=>`Expect ${name} ${ofSmething}.` }
   /**
    * @param {string} name Name of argument.
    * @param {any} value Value of validated argument.
    * @param {string} type Name of expected type.
    * @return {string} Error description.
    */
   static expectNameValueOfType(name, value, type) { return this.expectValueOfType(value,type)(name) }
   /**
    * @param {any} value Value of validated argument.
    * @param {string} type Name of expected type.
    * @return {(name:string)=>string} Error description generator.
    */
   static expectValueOfType(value, type) { return (name)=>`Expect ${name} of type ${type} instead of ${VMX.typeof(value)}.` }
   /**
    * @return {(name:string)=>string} Error description generator.
    */
   static requiredValue() { return (name) => `Required ${name} is missing.` }
   /**
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static validateBoolean(value) { return typeof value !== "boolean" && BasicTypeChecker.expectValueOfType(value,"boolean") }
   static booleanType = new BasicTypeChecker(BasicTypeChecker.validateBoolean)
   /**
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static validateNumber(value) { return typeof value !== "number" && BasicTypeChecker.expectValueOfType(value,"number") }
   static numberType = new BasicTypeChecker(BasicTypeChecker.validateNumber)
   /**
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static validateString(value) { return typeof value !== "string" && BasicTypeChecker.expectValueOfType(value,"string") }
   static stringType = new BasicTypeChecker(BasicTypeChecker.validateString)
   /**
    * @this {BasicTypeChecker}
    * @param {any} value
    * @param {object} typeCheckerContext
    * @return {false|((name:string)=>string)}
    */
   static validateIterable(value, typeCheckerContext) {
      return !(value != null && typeof value === "object" && Symbol.iterator in value) && BasicTypeChecker.expectValueOfType(value,"iterable")
            || BasicTypeChecker.validateIterableSubtype(value, this.subtype, typeCheckerContext)
   }
   /**
    * Validate types contained in iterable type.
    * @param {any} value
    * @param {undefined|BasicTypeChecker} subtype
    * @param {object} typeCheckerContext
    * @return {false|((name:string)=>string)}
    */
   static validateIterableSubtype(value, subtype, typeCheckerContext) {
      if (subtype) {
         let i=0
         for (const subvalue of value) {
            const error = subtype.validate(subvalue,typeCheckerContext)
            if (error) return (name) => error(name+"["+i+"]")
            ++i
         }
      }
      return false
   }
   /**
    * @param {{constructor:Function}} caller
    * @param {{[option:string]:any}} options
    * @param {{[allowed:string]:BasicTypeChecker}} allowedOptions
    */
   static validateOptions(caller, options, allowedOptions) {
      if (options == null || typeof options !== "object")
         return VMX.throwError(caller, `Expect options of type object instead of ${VMX.typeof(options)}.`)
      for (const name in options) {
         const value = options[name]
         if (!(name in allowedOptions))
            VMX.logError(caller, `Unknown option »${name}« of type ${VMX.typeof(value)}.`)
      }
      for (const name in allowedOptions) {
         const value = options[name]
         if (name in options || allowedOptions[name].required) {
            const error = allowedOptions[name].validate(value,options)
            if (error) return VMX.throwError(caller, error(`option »${name}«`))
         }
      }
   }
}

/** @typedef {"c"|"l"|"v"|"vm"} VMXCategoryType */

class VMXConfig {
   /** @type {"c"} */
   static CONTROLLER = "c"
   /** @type {"l"} */
   static LISTENER = "l"
   /** @type {"v"} */
   static VIEW = "v"
   /** @type {"vm"} */
   static VIEWMODEL = "vm"
}

/**
 * Simplifies creating a new promise.
 */
class PromiseHolder {
   /** @type {Promise<any>} */
   promise
   /** @type {(value:any)=>void} */
   resolve
   /** @type {(reason?:any)=>void} */
   reject
   constructor() {
      this.promise = new Promise( (resolve, reject) => (this.resolve = resolve, this.reject = reject))
   }
}

class OptionsParser {
   #options
   /**
    * @param {string} inputStr A string defining all options as name value pairs separated by comma.
    */
   constructor(inputStr) {
      this.#options = OptionsParser.parse(inputStr)
   }
   /**
    * @return {[string,any][]} An array of all top level option names and their value.
    */
   entries() { return Object.entries(this.#options) }
   /**
    * @param {string} name
    * @return {boolean} True in case option is defined.
    */
   has(name) { return name in this.#options }
   /**
    * @param {string} name
    * @return {any} Value of option.
    */
   value(name) { return this.#options[name] }
   /**
    * @return {string[]} An array of all top level option names.
    */
   names() { return Object.keys(this.#options) }
   /**
    * @param {string} inputStr
    * @return {object} An object which contains all options with their values.
    */
   static parse(inputStr) {
      /** Contains string of unparsed character without leading white space. @type {string} */
      let unparsed = inputStr.trimStart()
      /** Stores name of last matched name. Used in generating error msg. @type {string} */
      let lastMatchedName = ""
      /** Returns position of next unparsed character. @type {()=>string} */
      const pos = () => `${lastMatchedName ? " after name »"+lastMatchedName+"«" : ""}${unparsed.length==0 ? "" : unparsed.length==1 ? " at last position" : " at position " + (inputStr.length-unparsed.length)}`
      /** Skips nrChar characters and all following white space. Returns nrChar skipped characters. @type {(nrChar:number)=>string} */
      const skip = (nrChar) => { const skipped = unparsed; unparsed = unparsed.substring(nrChar).trimStart(); return skipped.substring(0, nrChar) }
      /**
       * Throws "expect...instead of..." error msg.
       * @type {(expect:string, insteadOfLen?:number)=>never}
       */
      const throwExpect = (expect, insteadOfLen=1) => { throw Error(`OptionsParser error: expect »${expect}« instead of »${unparsed ? unparsed.substring(0,insteadOfLen) : "end of input"}«${pos()}.\ninput=${inputStr}`) }
      /** Validates that next unparsed character equals char and skips it. @type {(char:string, insteadOfLen?:number)=>void} */
      const expect = (char, insteadOfLen=1) => { if (unparsed[0] != char) throwExpect(char, insteadOfLen); skip(1) }
      /**
       * Matches non empty name of pattern [_a-zA-Z][_a-zA-Z0-9]*
       * @type {()=>string}
       */
      const matchName = () => {
         let c = unparsed && unparsed[0].toLowerCase()
         if (c == "_" || ("a" <= c && c <= "z"))
            for (let len = 1; len < unparsed.length; ++len) {
               c = unparsed[len].toLowerCase()
               if (!(c == "_" || ("a" <= c && c <= "z") || ("0" <= c && c <= "9")))
                  return skip(len)
            }
         throwExpect("name")
      }
      /** Match boolean literal and return either true or false. @type {() => boolean} */
      const matchBoolean = () => { const val = unparsed[0]=="t"; if (!unparsed.startsWith("true") && !unparsed.startsWith("false")) expect("boolean", val?4:5); skip(val?4:5); return val }
      /** Match null literal and return null. @type {() => null} */
      const matchNull = () => { if (!unparsed.startsWith("null")) expect("null", 4); skip(4); return null }
      /** Match number literal and return the parsed value. @type {()=>number} */
      const matchNumber = () => { const nrstr = /^[-+0-9e.]+/.exec(unparsed)?.[0]; const val = Number(nrstr); if (isNaN(val)) expect("number", nrstr?.length??4); skip(nrstr?.length??0); return val }
      /** @type{(openChar:string, closingChar:string, value:any, callback:(value:any)=>any)=>any} */
      const matchEnclosed = (openChar, closingChar, value, callback) => { openChar && expect(openChar); const result = callback(value); closingChar && expect(closingChar); return result }
      /**
       * Match string literal and return the contained string.
       * Doubling the enclosing char letter removes its special meaning.
       * @type {(enclosingChar: string)=>string}
       */
      const matchString = (enclosingChar) => matchEnclosed(enclosingChar, enclosingChar, unparsed, (value) => {
         while (unparsed) {
            if (unparsed[0] == enclosingChar && (unparsed.length <= 1 || unparsed[1] != enclosingChar))
               // found enclosingChar which is not followed by enclosingChar
               return value.substring(1, value.length-unparsed.length).replaceAll(enclosingChar+enclosingChar,enclosingChar)
            skip(unparsed[0] == enclosingChar ? 2 : 1)
         }
      })
      /**
       * Match array literal and return the parsed value.
       * @type {()=>any[]}
       */
      const matchArray = () => matchEnclosed("[", "]", ([]), (value) => {
         while (unparsed && unparsed[0] != ']') {
            value.push(matchValue())
            unparsed && unparsed[0] != ']' && expect(",")
         }
         return value
      })
      /**
       * Match object literal and return the parsed value.
       * @type {(matchBrackets?:boolean)=>{[o:string]:any}}
       */
      const matchObject = (matchBrackets=true) => matchEnclosed(matchBrackets ? "{":"", matchBrackets ? "}":"", ({}), (value) => {
         while (unparsed && unparsed[0] != '}') {
            value[lastMatchedName = matchName()] = (expect(":"), matchValue())
            unparsed && unparsed[0] != '}' && expect(",")
         }
         return value
      })
      /**
       * Match any value literal and return its parsed value.
       * @type {()=>null|boolean|number|string|any[]|{[o:string]:any}}
       */
      const matchValue = () => {
         const c = unparsed && unparsed[0]
         if (c == "\"" || c == "'") return matchString(c)
         if (c == "+" || c == "-" || ("0" <= c && c <= '9') || c == '.') return matchNumber()
         if (c == "t" || c == "f") return matchBoolean()
         if (c == "n") return matchNull()
         if (c == "[") return matchArray()
         if (c == "{") return matchObject()
         throwExpect("value")
      }
      const values = matchObject(unparsed[0] == '{' /*no brackets at top level needed*/)
      unparsed && throwExpect("end of input")
      return values
   }
}

class LogIDOwner {
   /** @return {LogID} */
   getLogID() { return VMX.throwError(this,"getLogID not implemented.") }
   /** @return {undefined|LogIDOwner} */
   getLogIDParent() { return VMX.throwError(this,"getLogIDParent not implemented.") }
}

/**
 * Manages relationship between LogIDs and their owners and their attached HTML elements.
 * Allows to search for elements which are bound to viewmodels, controllers, or listeners.
 * And also retrieves the objects which are bound to a certain element.
 */
class LogIDBinding {
   /** @typedef OwnerAndElements
    * @property {LogIDOwner} owner
    * @property {(EventTarget)[]} elements
    */
   /** This mapping prevents garbage collection. @type {Map<number,OwnerAndElements>} */
   id2OwnerElems = new Map()
   /** @type {WeakMap<EventTarget,LogIDOwner[]>} */
   elem2Owner = new WeakMap()

   /**
    * @param {LogID} logid
    * @param {LogIDOwner} owner
    * @param {undefined|EventTarget|(EventTarget)[]} [elems]
    */
   setBinding(logid, owner, elems) {
      this.deleteBinding(logid)
      const elements = Array.isArray(elems) ? elems : (elems instanceof EventTarget) ? [elems] : []
      const ownerElems = {owner, elements}
      this.id2OwnerElems.set(logid.id, ownerElems)
      for (const elem of ownerElems.elements) {
         const owners = this.elem2Owner.get(elem)
         if (owners)
            owners.push(owner)
         else
            this.elem2Owner.set(elem, [owner])
      }
   }
   /**
    * @param {LogID} logid
    */
   deleteBinding(logid) {
      const ownerElems = this.id2OwnerElems.get(logid.id)
      if (!ownerElems) return
      this.id2OwnerElems.delete(logid.id)
      for (const elem of ownerElems.elements) {
         const owners = this.elem2Owner.get(elem)
         if (owners) {
            const owners2 = owners.filter(o => o !== ownerElems.owner)
            if (owners2.length === 0)
               this.elem2Owner.delete(elem)
            else
               this.elem2Owner.set(elem, owners2)
         }
      }
   }
   /**
    * @param {{index?:number,logID?:LogID,owner?:LogIDOwner,rootElem?:Element|Document}} searchOptions
    * @return {undefined|Element|Document}
    */
   getElementBy({index,logID,owner,rootElem}) {
      if (owner) logID = owner.getLogID()
      if (logID instanceof LogID) index = logID.id
      if (typeof index === "number") {
         const ownerElems = this.id2OwnerElems.get(index)
         if (ownerElems && ownerElems.elements.length > 0) {
            const elem = ownerElems.elements.find((el) => (el instanceof Document || el instanceof Element) && (!rootElem || rootElem.contains(el)))
            return (elem instanceof Document || elem instanceof Element) ? elem : undefined
         }
      }
      else if (rootElem) {
         return this.traverseNodes([rootElem], (node) => this.elem2Owner.get(node) !== undefined )
      }
   }
   /**
    * @param {{index?:number,logID?:LogID,owner?:LogIDOwner,rootElem?:Element|Document}} searchOptions
    * @return {(Element|Document)[]}
    */
   getElementsBy({index, logID, owner, rootElem}) {
      if (owner) logID = owner.getLogID()
      if (logID instanceof LogID) index = logID.id
      if (typeof index === "number") {
         const ownerElems = this.id2OwnerElems.get(index)
         if (ownerElems && ownerElems.elements.length > 0) {
            const elements = ownerElems.elements.filter((el) => (el instanceof Document || el instanceof Element))
            return rootElem ? elements.filter(el => rootElem.contains(el)) : elements
         }
      }
      const result = []
      if (rootElem)
         this.traverseNodes([rootElem], (node) => { this.elem2Owner.get(node) && result.push(node) })
      return result
   }
   /**
    * @param {number} index
    * @return {undefined|LogIDOwner}
    */
   getOwnerByIndex(index) {
      const ownerElems = this.id2OwnerElems.get(index)
      return ownerElems?.owner
   }
   /**
    * @template {LogIDOwner} T
    * @param {new(...args:any[])=>T} type
    * @param {Element|Document} elem
    * @return {T[]}
    */
   getOwnersByTypeAndElem(type, elem) {
      const owners = this.elem2Owner.get(elem)
      /** @type {T[]} */
      const typedOwners = []
      if (owners) {
         for (const owner of owners)
            if (owner instanceof type)
               typedOwners.push(owner)
      }
      return typedOwners
   }
   /**
    * Breadth first search over a given set of nodes.
    * @param {(Node)[]} nodes
    * @param {(node:Element|Document)=>void|boolean} visitNode Computes result of given node and adds it to result. Returns *true* if no more nodes should be processed (processinng is done,result complete).
    * @return {undefined|Element|Document}
    */
   traverseNodes(nodes, visitNode) {
      for(var i=0; i<nodes.length; ++i) {
         const node = nodes[i]
         if (node instanceof Element || node instanceof Document) {
            if (visitNode(node))
               return node
            node.hasChildNodes() && nodes.push(...node.childNodes)
         }
      }
   }
   /**
    * If this function returns a non empty set then disconnected html elements
    * exist with undetached (not freed) ViewModels.
    * @return {Set<LogIDOwner>} All owners which contain a LogID but are not bound to connected HTML elements.
    */
   disconnectedOwners() {
      const disconnected = new Set()
      for (const {owner,elements} of this.id2OwnerElems.values()) {
         if (elements.every(el => !(el instanceof Node) || !el.isConnected))
            disconnected.add(owner)
      }
      return disconnected
   }
   freeDisconnectedOwners() {
      for (const owner of this.disconnectedOwners()) {
         // TODO: add support for other views ... (does ViewModel free all views ???)
         if (owner instanceof ViewModel && owner.free)
            owner.free()
      }
      if (this.disconnectedOwners().size)
         return VMX.throwError(this,`Not all disconnected owners could be freed.`)
   }
}

class Logger {
   /** @typedef {0} LogLevel_NOLOG */
   /** @typedef {1} LogLevel_FATAL */
   /** @typedef {2} LogLevel_ERROR */
   /** @typedef {3} LogLevel_WARNING */
   /** @typedef {4} LogLevel_DEBUG */
   /** @typedef {5} LogLevel_INFO */
   /** @typedef {LogLevel_NOLOG|LogLevel_FATAL|LogLevel_ERROR|LogLevel_WARNING|LogLevel_DEBUG|LogLevel_INFO} LogLevel */
   /** @type {LogLevel_NOLOG}  */
   static OFF=0
   /** @type {LogLevel_FATAL} */
   static FATAL=1
   /** @type {LogLevel_ERROR} */
   static ERROR=2
   /** @type {LogLevel_WARNING} */
   static WARNING=3
   /** @type {LogLevel_DEBUG} */
   static DEBUG=4
   /** @type {LogLevel_INFO} */
   static INFO=5
   /** @type {{NOLOG:LogLevel_NOLOG,FATAL:LogLevel_FATAL,ERROR:LogLevel_ERROR,WARNING:LogLevel_WARNING,DEBUG:LogLevel_DEBUG,INFO:LogLevel_INFO}} References all log levels. */
   static LEVELS={ NOLOG:0, FATAL:1, ERROR:2, WARNING:3, DEBUG:4, INFO:5 }
   /** @type {["NoLog","Fatal","Error","Warning","Debug","Info"]} Names off all known log levels. */
   static LEVELNAMES=[ "NoLog", "Fatal", "Error", "Warning", "Debug", "Info" ]
   /**
    * Converts provided value to a string.
    * @param {any} v Value which is converted to a string.
    * @return {string} The string representation of *v*.
    */
   static stringOf(v) {
      if (v != null && typeof v === "object") {
         if (Array.isArray(v)) return "[" + v.map(o=>this.stringOf(o)).toString() + "]"
         else                  return "{" + Object.entries(v).map(e => e[0]+":"+this.stringOf(e[1])).toString() + "}"
      }
      if (typeof v === "bigint") return String(v) + "n"
      return String(v)
   }
   /**
    * @param {{name:string}|{constructor:{name:string}}|LogIDOwner} component Class, object, or object which has a LogID.
    * @return {string}
    */
   static componentName(component) {
      if ("name" in component) return String(component.name)
      const logid = ("getLogID" in component) && component.getLogID()
      return logid ? logid.name(component) : this.typeof(component)
   }
   /**
    * @param {any} value
    * @return {string}
    */
   static typeof(value) {
      const type = typeof value
      if (type === "object") {
         if (value === null) return "null"
         const className = value.constructor?.name
         if (className && className !== "Object") return className
      }
      return type
   }

   /** @type {LogLevel} */
   #level

   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /**
    * @param {LogLevel} [level]
    */
   constructor(level) {
      this.#level = level ?? Logger.WARNING
   }
   /** @typedef {undefined|object|Error} LogArgs An object with named arguments written to the console or a catched exception which is the reason for the log. */
   /**
    * @param {LogLevel} level
    * @param {string} levelName
    * @param {string} component
    * @param {string|Error} message
    * @param {LogArgs} args Catched Error or additional arguments.
    */
   writeLog(level, levelName, component, message, args) {
      console.log(component, levelName+":", message)
      if (args) console.log(args instanceof Error ? "Caused by:" : "Log Args:", args)
   }
   ////////////////
   // Properties //
   ////////////////
   /** Names of all logging levels. Index into corresponds to level. @return {typeof Logger.LEVELNAMES} */
   get levelNames() { return [...Logger.LEVELNAMES] }
   /** @return {typeof Logger.LEVELS} */
   get levels() { return {...Logger.LEVELS} }
   /**
    * @param {LogLevel} level
    * @return {string}
    */
   levelName(level) { return (Logger.LEVELNAMES[level] ?? String(level)) }
   /** @return {LogLevel} */
   get level() { return this.#level }
   /** Sets log level with {@link LogLevel_OFF} turning logging off. @param {LogLevel} level */
   set level(level) { this.#level = level }
   ////////////////////////
   // String Conversions //
   ////////////////////////
   /** @typedef {{name:string}|{constructor:{name:string}}|LogIDOwner} Logger_Component Class, object, or object which has a LogID. */
   /**
    * @param {Logger_Component} component Class, object, or object which has a LogID.
    * @return {string}
    */
   componentName(component) { return Logger.componentName(component) }
   /**
    * @param {any} value
    * @return {string}
    */
   typeof(value) { return Logger.typeof(value) }
   /**
    * Converts provided value to a string.
    * @param {any} v Value which is converted to a string.
    * @return {string} The string representation of *v*.
    */
   stringOf(v) { return Logger.stringOf(v) }
   ////////////////
   // Format Log //
   ////////////////
   /**
    * @param {LogLevel} level
    * @param {Logger_Component} component Class, object, or object which has a LogID.
    * @param {string} message
    * @param {LogArgs} args Catched Error or additional arguments.
    */
   log(level, component, message, args) {
      if (level > this.#level) return
      const isSevere = level <= Logger.ERROR
      const message2 = isSevere ? new Error(message,args==null?undefined:{cause:args}) : message
      for (let componentName;;) {
         try {
            componentName ??= this.componentName(component)
            this.writeLog(level, this.levelName(level), componentName, message2, isSevere ? undefined : args)
            return
         }
         catch(error) {
            // ignore failure to write log
            if (componentName !== undefined) {
               console.log(this.formatError(Logger.ERROR,this,"writeLog failed",error))
               return
            }
            componentName = "-" + String(component) + "-"
         }
      }
   }
   /**
    * @param {LogLevel} level
    * @param {Logger_Component} component Class, object, or object which has a LogID.
    * @param {string} message
    * @param {LogArgs} args Catched Error or additional arguments.
    */
   formatError(level, component, message, args) {
      return Error(`${this.componentName(component)} ${this.levelName(level)}: ${message}`,args==null?undefined:{cause:args})
   }
   ////////////////////////////////
   // Do Logging from Components //
   ////////////////////////////////
   /**
    * @callback LogWithObjectContext
    * @param {Logger_Component} obj Class, object, or object which has a LogID.
    * @param {string} message
    * @param {LogArgs} [args] Catched Error or additional arguments.
    * @return {void}
    */
   /** @type {LogWithObjectContext} */
   debug(obj, message, args) { this.log(Logger.DEBUG, obj, message, args) }
   /** @type {LogWithObjectContext} */
   error(obj, message, args) { this.log(Logger.ERROR, obj, message, args) }
   /** @type {LogWithObjectContext} */
   info(obj, message, args) { this.log(Logger.INFO, obj, message, args) }
   /** @type {LogWithObjectContext} */
   warning(obj, message, args) { this.log(Logger.WARNING, obj, message, args) }
   /**
    * @param {Logger_Component} obj Class, object, or object which has a LogID.
    * @param {string} message
    * @param {LogArgs} [args] Catched Error or additional arguments.
    * @return {never}
    */
   throwError(obj, message, args) {
      this.error(obj, message, args)
      throw this.formatError(Logger.ERROR,obj,message,args)
   }
}

class VMXExtensionPoint {
   /** @type {undefined|((arg:WindowView)=>void)|((arg:WindowView)=>void)[]} */
   #closeWindowView

   /**
    * @template ARG
    * @param {undefined|((arg:ARG)=>void)|((arg:ARG)=>void)[]} oldstate
    * @param {(arg:ARG)=>void} callback
    * @return {((arg:ARG)=>void)|((arg:ARG)=>void)[]}
    */
   #register(oldstate, callback) {
      if (typeof callback !== "function") return VMX.throwError(this,`Expect argument »callback« of type function stead of ${VMX.typeof(callback)}.`)
      const newstate = !oldstate ? callback : Array.isArray(oldstate) ? [...oldstate, callback] : [oldstate, callback]
      return newstate
   }
   /**
    * @template ARG
    * @param {undefined|((arg:ARG)=>void)|((arg:ARG)=>void)[]} state
    * @param {ARG} arg
    * @return {void}
    */
   #runExtension(state, arg) { for (const cb of Array.isArray(state) ? state : state ? [state] : []) cb(arg) }

   /** @param {(arg:WindowView)=>void} callback  */
   registerCloseWindowView(callback) { this.#closeWindowView = this.#register(this.#closeWindowView, callback) }
   /** @param {WindowView} windowView @return {void} */
   closeWindowView(windowView) { this.#runExtension(this.#closeWindowView,windowView) }
}

/**
 * Allocated at bottom of source and assigned to global variable »VMX«.
 */
class ViewModelContext {
   /** The view model for the whole document.documentElement. @type {DocumentVM} */
   documentVM
   /** The view model for the whole document.documentElement. @type {VMXExtensionPoint} */
   extpoint
   /** All ViewListener are managed by this single instance. @type {ViewListeners} */
   listeners
   /** Supports logging with default logger. @type {Logger} */
   log
   /** Stores references from logIDs to their owners and attached HTML elements. @type {LogIDBinding} */
   logidBinding
   /** Manages callbacks which are called before the View is rendered to the screen the next time. @type {ViewDrawListener} */
   viewDrawListener
   /** @type {ViewModelObserverHub} */
   vmObserver

   init() {
      this.extpoint = new VMXExtensionPoint()
      this.log = new Logger()
      this.logidBinding = new LogIDBinding()
      this.listeners = new ViewListeners()
      this.viewDrawListener = new ViewDrawListener()
      this.vmObserver = new ViewModelObserverHub()
      this.documentVM = new DocumentVM()
   }

   ////////////
   // Facade //
   ////////////

   /**
    * @return {ViewModel[]} All {@link ViewModel} having HTML elements connected to the document.
    */
   getConnectedViewModels() {
      const elements = this.logidBinding.getElementsBy({rootElem:document})
      return elements.flatMap( el =>
         this.logidBinding.getOwnersByTypeAndElem(ViewModel,el)
      )
   }
   /**
    * @param {Document|HTMLElement} rootElem The root and all its childs are queried for listeners.
    * @param {string} [eventType] Optional event type a listener is listening on.
    * @return {(Document|HTMLElement)[]} An array of HTML elements who has attached listeners.
    */
   getListenedElements(rootElem, eventType) {
      return VMX.listeners.getListenedElements(rootElem,eventType)
   }

   //////////////////////////////////
   // JavaScript Low Level Helpers //
   //////////////////////////////////

   /**
    * @param {Function} constructor
    * @returns {boolean} *true* if constructor as a function serving as a constructor.
    */
   isConstructor(constructor) {
      try { return (Reflect.construct(String,[],constructor), true) } catch(error) { return false }
   }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @return {string}
    */
   componentName(obj) { return this.log.componentName(obj) }
   /**
    * @param {any} value
    * @return {string} Either name of constructor or name of primitive type.
    */
   typeof(value) { return this.log.typeof(value) }
   /**
    * Converts provided value to a string.
    * @param {any} value Value which is converted to a string.
    * @return {string} The string representation of *v*.
    */
   stringOf(value) { return this.log.stringOf(value) }
   /**
    * @param {object} obj
    * @param {string} name
    * @return {undefined|PropertyDescriptor}
    */
   getPropertyDescriptor(obj, name) {
      for (var proto = obj; proto; proto = Object.getPrototypeOf(proto)) {
         const descr = Object.getOwnPropertyDescriptor(proto, name)
         if (descr) return descr
      }
   }

   /////////////////////////
   // Log & Error Support //
   /////////////////////////

   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    * @param {object|Error} [args]
    * @return {never}
    */
   throwError(obj, msg, args) { return this.log.throwError(obj,msg,args) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    */
   logError(obj, msg) { this.log.error(obj, msg) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    * @param {any} exception
    */
   logException(obj, msg, exception) { this.log.error(obj, msg, Object(exception)) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    */
   logInfo(obj, msg) { this.log.info(obj, msg) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    */
   logDebug(obj, msg) { this.log.debug(obj, msg) }
}

/**
 * Assigns a log-ID to a component which produces log entries.
 */
class LogID {
   /** @type {number} */
   #id = 0
   //
   // Generate unique index
   //
   static ID = 0
   /** @type {number[]} */
   static FREE_IDs = []
   /** @return {number} */
   static nextID() {
      if (this.FREE_IDs[0] === this.ID) {
         this.FREE_IDs.shift()
         -- this.ID
      }
      return this.FREE_IDs.shift() ?? ++ this.ID
   }
   /** @param {number} id */
   static freeID(id) { (id > 0) && (id === this.ID ? --this.ID : this.FREE_IDs.push(id)) }
   /**
    * Binds LogID index to owner and possible HTML elements.
    * @param {LogIDOwner} owner
    * @param {EventTarget|(EventTarget)[]} [elems]
    */
   constructor(owner, elems) { this.init(owner,elems) }
   /**
    * Frees binding
    */
   free() { this.isFree() || (VMX.logidBinding.deleteBinding(this), LogID.freeID(this.#id), this.#id=0) }
   /**
    * @return {boolean} *true*, if binding was already freed.
    */
   isFree() { return this.#id === 0 }
   /**
    * Binds LogID index to owner and possible HTML elements.
    * @param {LogIDOwner} owner
    * @param {EventTarget|(EventTarget)[]} [elems]
    */
   init(owner, elems) {
      this.free()
      this.#id = LogID.nextID()
      if (VMX.logidBinding.getOwnerByIndex(this.#id)) return VMX.throwError(owner,`LogID.id not unique id=${this.#id}.`)
      VMX.logidBinding.setBinding(this, owner, elems)
      if (VMX.logidBinding.getOwnerByIndex(this.#id) !== owner)
         throw Error("... owner differs ... (TODO: remove)")
   }
   /** @return {number} The log ID which is a unique number. */
   get id() { return this.#id }
   /** @return {undefined|LogIDOwner} */
   get owner() { return VMX.logidBinding.getOwnerByIndex(this.#id) }
   /** @return {string} Name of owner which is '[ppid/pid/id typename]'. */
   toString() { return this.name(this.owner) }
   /**
    * Name of owner and parents if depth > 0.
    * @param {undefined|LogIDOwner} owner
    * @param {number} depth Default is 3. The number of parents chained into this path.
    * @return {string} Returns path name like 'ppid/pid/id-typename'
    */
   name(owner, depth=3) {
      !owner || (owner.getLogID() === this) || VMX.throwError(this, `Argument »owner« returns wrong LogID.`)
      let path = `${this.#id}-${VMX.typeof(owner)}`, parent = owner
      if (parent)
         while (depth-- > 0 && (parent = parent.getLogIDParent())) {
            path = parent.getLogID().id + "/" + path
         }
      return path
   }
}

/**
 * Stores callbacks which are called before the
 * View is rendered to the screen.
 * Implemented with help of requestAnimationFrame.
 */
class ViewDrawListener {
   /** @type {((timestamp:number)=>void)[]} */
   #callbacks = []
   /** The number of the next requested animation frame
    * @type {null|number} */
   #frameID = null
   constructor(delayCount, callback) {
      callback && this.onceOnDrawFrame(delayCount, callback)
   }
   /**
    * @param {number} timestamp
    */
   #onDraw(timestamp) {
      const callbacks = this.#callbacks
      this.#callbacks = []
      this.#frameID = null
      for (const callback of callbacks)
         callback(timestamp)
   }
   /**
    * @param {number} delayCount A value of 0 means to execute callback before next view update. A value > 0 delays the skips the execution for the next delayCount updates.
    * @param {(timestamp?:number)=>void} [callback]
    */
   onceOnDrawFrame(delayCount, callback) {
      if (typeof callback !== "function") return
      this.#callbacks.push( 0 < delayCount && delayCount < 300
         ? () => this.onceOnDrawFrame(delayCount-1,callback)
         : callback
      )
      if (this.#frameID == null)
         this.#frameID = requestAnimationFrame(this.#onDraw.bind(this))
   }
}

class ControllerEvent {
   /** @type {string} */
   type
   /** @type {{controllerTarget:Document|HTMLElement, listenerTarget:EventTarget, target:EventTarget, timeStamp:number }} */
   event
   /** @type {Event} The original event received by an (HTML)-EventTarget. */
   oevent
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {Event} e
    */
   constructor(type, controller, e) {
      this.type = type
      this.event = { controllerTarget:controller.htmlElem, listenerTarget:e.currentTarget??controller.htmlElem, target:e.target??controller.htmlElem, timeStamp:e.timeStamp }
      this.oevent = e
   }
   get eventPhaseCapture() { return this.oevent.eventPhase === 1 }
   get eventPhaseTarget() { return this.oevent.eventPhase === 2 }
   get eventPhaseBubbling() { return this.oevent.eventPhase === 3 }
   get eventTarget() { return this.event.target }
   get eventListenerTarget() { return this.event.listenerTarget }
   get eventControllerTarget() { return this.event.controllerTarget }
   // TODO: ! add view (if possible) !
}

class ViewListenerConfig {
   static Category = VMXConfig.LISTENER
}

/**
 * Stores state of event listener added to HTML view.
 */
class ViewListener {
   //////////////////////
   // implement Config //
   //////////////////////
   /** @return {typeof ViewListenerConfig} */
   get ClassConfig() { return ViewListenerConfig }
   //////////////////////////
   // implement LogIDOwner //
   //////////////////////////
   getLogID() { return this.logid }
   getLogIDParent() { return this.owner }

   /////////////
   // Options //
   /////////////
   /* PHASE
    * === Background info
    * HTML events know a target and a currentTarget.
    * The target is where the event originated and the currentTarget
    * denotes the element for which the executed listener has been registered.
    * Therefore elements which are nested within each other all receive the same event.
    * During capture phase the outermost element receives the event first and during bubbling phase
    * the innermost element receives it first.
    * ===
    */
   /** @type {1} Event is received if the event originated from the element for which the listener was registered or from one of its childs. */
   static CAPTURE_PHASE = 1
   /** @type {2} Event is received if the event originated from the element for which the listener was registered or from one of its childs. */
   static BUBBLE_PHASE = 2
   /** @type {2} If no PHASE is set default is BUBBLE_PHASE. */
   static DEFAULT_PHASE = 2
   static KEYS={
      COMPOSE:"Compose",
      CONTROL:"Control", CONTROL_LEFT_CODE:"ControlLeft", CONTROL_RIGHT_CODE:"ControlRight",
      ENTER:"Enter",
      ESCAPE:"Escape",
      F1:"F1",
      META:"Meta", META_LEFT_CODE:"MetaLeft", META_RIGHT_CODE:"MetaRight",
      TAB:"Tab",
   }
   ////////////////
   // EventTypes //
   ////////////////
   static CLICK="click"
   static DBLCLICK="dblclick"
   static KEYDOWN="keydown"
   static KEYUP="keyup"
   static MOUSEDOWN="mousedown"
   static MOUSEMOVE="mousemove"
   static MOUSEUP="mouseup"
   static TOUCHSTART="touchstart"
   static TOUCHEND="touchend"
   static TOUCHCANCEL="touchcancel"
   static TOUCHMOVE="touchmove"
   static TRANSITIONCANCEL="transitioncancel"
   static TRANSITIONEND="transitionend"
   static TRANSITIONRUN="transitionrun"
   static ANIMATION={ CANCEL:"cancel", FINISH:"finish", REMOVE:"remove" }
   /**
    * @param {Event|KeyboardEvent} e
    * @return {string}
    */
   static eventToString(e) {
      if (!e || typeof e !== "object") return String(e)
      let strevent = e.type + " "
      switch (e.type) {
         case ViewListener.KEYDOWN: case ViewListener.KEYUP:
            if (e instanceof KeyboardEvent) {
               strevent += "key="
               e.altKey && (strevent += "Alt+")
               e.ctrlKey && (strevent += "Control+")
               e.metaKey && (strevent += "Meta+")
               e.shiftKey && (strevent += "Shift+")
               strevent += "'"+e.key+"'"
               strevent += " code='"+e.code+"'"
               break
            }
         default:
            break
      }
      return strevent
   }
   /**
    * @param {undefined|null|Event} e
    * @return {boolean} *true* if either touch event or mouse event with main button
    */
   static isMainButton(e) { return this.isButton(e, 0) }
   /**
    * @param {undefined|null|Event} e
    * @param {number} button The number of the button (0==main(left),1==wheel(middle),2==second(right))
    * @return {boolean} *true* if button is pressed or released
    */
   static isButton(e, button) {
      return Boolean(e) && button === (e instanceof MouseEvent ? e.button : 0)
   }
   /**
    * @param {undefined|null|Event} e
    * @return {e is MouseEvent}
    */
   static isMouseEvent(e) { return e instanceof MouseEvent }
   /**
    * @param {undefined|null|Event} e
    * @return {e is TouchEvent}
    */
   static isTouchEvent(e) { return "TouchEvent" in window && e instanceof TouchEvent }

   /**
    * Contains added listener state.
    * Use {@link ViewListeners.addListener} of object VMX.listeners to create a new listener and add it to an html element.
    *
    * @param {EventTarget|ViewElem} elem - The HTML element event handler is added to.
    * @param {string} eventType - The event type ("click","touchstart",...).
    * @param {(e:Event)=>void} eventHandler - Callback which is called to handle events of the given type. The first argument is the HTML event.
    * @param {{owner?:LogIDOwner, phase?:number, once?:boolean}} options - Owner of the listener should be set.
    */
   constructor(elem, eventType, eventHandler, {owner,phase,once}) {
      this.htmlElem = ViewElem.toEventTarget(elem)
      this.eventType = eventType
      this.once = Boolean(once)
      this.eventHandler = this.once // auto-free in case of once
         ? (...args) => { try { eventHandler.apply(this.htmlElem,args) } finally { this.free() } }
         : eventHandler
      this.owner = owner
      this.phase = phase || ViewListener.DEFAULT_PHASE
      this.logid = new LogID(this, this.htmlElem)
      this.#add()
   }
   free() {
      if (!this.logid.isFree()) {
         this.#remove()
         this.logid.free()
      }
   }
   isBubblePhase() { return this.phase === ViewListener.BUBBLE_PHASE }
   isCapturePhase() { return this.phase === ViewListener.CAPTURE_PHASE }
   #add() {
      VMX.listeners.addListener(this)
      if (this.isBubblePhase())
         this.htmlElem.addEventListener(this.eventType,this.eventHandler,{capture:false,once:this.once,passive:false})
      else if (this.isCapturePhase())
         this.htmlElem.addEventListener(this.eventType,this.eventHandler,{capture:true,once:this.once,passive:false})
   }
   #remove() {
      VMX.listeners.removeListener(this)
      if (this.isBubblePhase())
         this.htmlElem.removeEventListener(this.eventType,this.eventHandler,{capture:false})
      else if (this.isCapturePhase())
         this.htmlElem.removeEventListener(this.eventType,this.eventHandler,{capture:true})
   }
}

/**
 * Manages event listeners. Both phases of events capture and bubble phase
 * are handled. Callbacks are currently only done within bubble phase.
 * So child elements always receives events first and are therefore prioritized.
 * Prioritizing containing elements must be supported by other means
 * or by adding a feature to this component like callbacks during capturing phase
 * (to acquire locks on actions for example).
 */
class ViewListeners {
   /** @type {WeakMap<EventTarget, {[eventType:string]: Map<(e:Event)=>void,ViewListener>}>} */
   listenedElements = new WeakMap()
   /** @type {Map<string,number>} Number of times for every event type for which function boundOnEndEventLoop is registered as listener on window. */
   windowListeners = new Map()
   /** @type {(e:Event)=>void} */
   boundOnEndEventLoop
   /** @type {null|{lastEvent:Event, callbacks:((e:Event)=>void)[], holder:PromiseHolder}} */
   registeredEndEventLoop = null
   /**
    * Initializes a new ViewListener. There should be only a single one per window.
    */
   constructor() {
      this.boundOnEndEventLoop = this.onEndEventLoop.bind(this)
   }
   /**
    * @param {ControllerEvent|Event} ce
    * @param {undefined|((e:Event) => void)} callback
    * @return {Promise<Event>}
    */
   onceEndEventLoop(ce, callback) {
      const e = ce instanceof ControllerEvent ? ce.oevent : ce
      if (!e.isTrusted) return VMX.throwError(this,`Can process only trusted events.`)
      if (this.registeredEndEventLoop && this.registeredEndEventLoop.lastEvent !== e) {
         const e2 = this.registeredEndEventLoop.lastEvent
         VMX.logError(this, `onEndEventLoop not called for last event {type:${e2.type}, target:${VMX.typeof(e2.target)}.`)
         this.onEndEventLoop(e2)
      }
      const registered = (this.registeredEndEventLoop ??= { lastEvent:e, callbacks:[], holder:new PromiseHolder() })
      if (typeof callback === "function")
         registered.callbacks.push(callback)
      return registered.holder.promise
   }
   /**
    * @param {Event} e
    */
   onEndEventLoop(e) {
      if (!this.registeredEndEventLoop) return
      const { lastEvent:e2, callbacks, holder } = this.registeredEndEventLoop
      if (e !== e2) VMX.logError(this, `onEndEventLoop called with wrong event {type:${e.type}, target:${VMX.typeof(e.target)} instead of {type:${e2.type}, target:${VMX.typeof(e2.target)}.`)
      this.registeredEndEventLoop = null
      callbacks.forEach( callback => callback(e))
      holder.resolve(e)
   }
   /**
    * A new created {@link ViewListener} is added.
    * If tuple (html element, event type, event handler) is not unique
    * (was already added from another view listener) an exception is thrown.
    * @param {ViewListener} listener
    */
   addListener(listener) {
      const eventTypes = this.listenedElements.get(listener.htmlElem) || {}
      const eventType = listener.eventType
      const evListeners = eventTypes[eventType] || new Map()
      if (evListeners.has(listener.eventHandler))
         return VMX.throwError(this,`Can not add same event handler twice for type ${listener.eventType}.`)
      evListeners.set(listener.eventHandler,listener)
      if (evListeners.size === 1) {
         eventTypes[eventType] = evListeners
         this.listenedElements.set(listener.htmlElem, eventTypes)
         const nrTimes = this.windowListeners.get(eventType)
         if (nrTimes === undefined) {
            window.addEventListener(eventType,this.boundOnEndEventLoop,{capture:false,passive:true})
            this.windowListeners.set(eventType, 1)
         }
         else
            this.windowListeners.set(eventType, nrTimes+1)
      }
   }
   /**
    * @param {ViewListener} listener
    */
   removeListener(listener) {
      const eventTypes = this.listenedElements.get(listener.htmlElem)
      if (eventTypes) {
         const eventType = listener.eventType
         const evListeners = eventTypes[eventType]
         if (evListeners) {
            evListeners.delete(listener.eventHandler)
            if (evListeners.size === 0) {
               delete eventTypes[eventType]
               const nrTimes = this.windowListeners.get(eventType)
               if (!nrTimes || nrTimes <= 1) {
                  window.removeEventListener(eventType,this.boundOnEndEventLoop,{capture:false})
                  this.windowListeners.delete(eventType)
               }
               else
                  this.windowListeners.set(eventType, nrTimes-1)
            }
         }
      }
   }
   /**
    * Returns iterator for iterating over all matching listeners.
    * @param {null|EventTarget} htmlElem The HTML element whose listeners are queried.
    * @param {string} [eventType] Optional type of the event the listener must match else listeners with any event type are matched.
    * @return {Iterator<ViewListener>&Iterable<ViewListener>}
    */
   get(htmlElem, eventType) {
      const eventTypes = htmlElem ? this.listenedElements.get(htmlElem) : undefined
      if (!eventTypes)
         return [].values()
      return (function*() {
         for (const key in eventTypes) {
            if (!eventType || key === eventType) {
               const evListeners = eventTypes[key]
               if (evListeners) {
                  yield* evListeners.values()
               }
            }
         }
      })()
   }
   /**
    * @param {Document|HTMLElement} htmlElem The HTML element whose listeners are queried.
    * @param {string} [eventType] Optional type of the event the listener must match else listeners with any event type are matched.
    * @return {boolean}
    */
   isListener(htmlElem, eventType) {
      return !this.get(htmlElem,eventType).next().done
   }
   /**
    * @param {Document|HTMLElement} rootElem The root HTML and all its child elements are queried for listeners.
    * @param {string} [eventType] Optional type of the event the listener must match else listeners with any event type are matched.
    * @return {(Document|HTMLElement)[]}
    */
   getListenedElements(rootElem, eventType) {
      const result = []
      if (this.isListener(rootElem,eventType))
         result.push(rootElem)
      for (const htmlElem of rootElem.querySelectorAll("*")) {
         if (htmlElem instanceof HTMLElement && this.isListener(htmlElem,eventType))
            result.push(htmlElem)
      }
      return result
   }
}

class ViewControllerConfig {
   /**
    * @this {BasicTypeChecker}
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static isGroupMember(value) { return !(value instanceof EventTarget) && BasicTypeChecker.expectValueOfType(value,"EventTarget") }
   /**
    * @this {BasicTypeChecker}
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static isPhaseType(value) { return !(value === ViewListener.CAPTURE_PHASE || value === ViewListener.BUBBLE_PHASE) && BasicTypeChecker.expectValueOfType(value,ViewListener.CAPTURE_PHASE+"|"+ViewListener.BUBBLE_PHASE) }
   /**
    * @this {BasicTypeChecker}
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static isViewType(value) { return !(value instanceof View) && BasicTypeChecker.expectValueOfType(value,"View") }
   /**
    * @this {BasicTypeChecker}
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static isViewModelType(value) { return !(value instanceof ViewModel) && BasicTypeChecker.expectValueOfType(value,"ViewModel") }
   /**
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static isElementType(value) { return !(value instanceof Document || value instanceof HTMLElement) && BasicTypeChecker.expectValueOfType(value,"Document|HTMLElement") }

   static Category = VMXConfig.CONTROLLER

   /**
    * @typedef ViewControllerConfigInitOptions
    * @property {BasicTypeChecker} phase Optional value which determines phase when event listener is triggered. Allowed values are ViewListener.BUBBLE_PHASE or ViewListener.CAPTURE_PHASE. Default value is ViewListener.BUBBLE_PHASE.
    * @property {BasicTypeChecker} view Optional view this controller is attached to. View is parent of controller.
    * @property {BasicTypeChecker} viewModel Optional ViewModel this controller is attached to. ViewModel is parent of controller.
    * @property {BasicTypeChecker} group Group of elements to be listened for change events instead of a single htmlElem.
    * @property {BasicTypeChecker} enableClick Enables CLICK action. Default is false.
    * @property {BasicTypeChecker} enableDoubleClick Enables DOUBLECLICK action. Default is false.
    * @property {BasicTypeChecker} nomouse *true*, if mouse should be disabled. Default is false.
    */

   /** @type {ViewControllerConfigInitOptions} */
   static InitOptions = {
      phase: new BasicTypeChecker(ViewControllerConfig.isPhaseType),
      view: new BasicTypeChecker(ViewControllerConfig.isViewType),
      viewModel: new BasicTypeChecker(ViewControllerConfig.isViewModelType),
      group: new BasicTypeChecker(BasicTypeChecker.validateIterable, new BasicTypeChecker(ViewControllerConfig.isGroupMember)),
      enableClick: BasicTypeChecker.booleanType,
      enableDoubleClick: BasicTypeChecker.booleanType,
      nomouse: BasicTypeChecker.booleanType,
   }
}

/**
 * Supports event listeners and blocking of other controllers by locking of the provided action.
 *
 * **Callbacks**
 * If a callback is provided instead of a {@link ViewModelAction}
 * no locking is done and callAction always calls the callback.
 *
 * **Actions:**
 * The provided action callback which is of type {@link ViewModelAction}
 * supports startAction/endAction.
 * On activation the controller tries to get a lock on the action
 * by calling startAction and holds it until it is deactivated.
 * Calling an action (see {@link callAction} or {@link callActionOnce})
 * auto-starts the action if it was not started before (tries acquire action lock).
 * An action is only done if it was started succesfully.
 * - To change state of action {@link tryStartAction} or {@link endAction}.
 * - To prevent starting during {@link activate} set option noStartAction to true.
 * - To acquire lock at end of event processing set option actionEvent to the current Event.
 * - The action is ended during {@link deactivate}.
 */
class ViewController {
   //////////////////////
   // implement Config //
   //////////////////////
   /** @return {typeof ViewControllerConfig} */
   get ClassConfig() { return ViewControllerConfig }
   //////////////////////////
   // implement LogIDOwner //
   //////////////////////////
   getLogID() { return this.#logid }
   getLogIDParent() { return this.#view ?? this.#viewModel }

   static preventDefault = (e) => e.preventDefault()

   /** @type {boolean} */
   #isActive = false
   /** @type {boolean} */
   #muted = false
   /** @type {undefined|1|2} */
   #phase
   /** @type {ViewListener[]} */
   #listeners = []
   /** @type {ViewListener[]} */
   #activeListeners = []
   /** @type {Document|HTMLElement} htmlElem */
   #htmlElem
   /** @type {undefined|ViewModelActionInterface} */
   #action
   /** @type {undefined|ViewModel} */
   #viewModel
   /** @type {View} */
   #view
   /** @type {undefined|number} */
   #activationDelay
   /** @type {undefined|number} */
   #autoDeactivation
   /** @type {LogID} */
   #logid

   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /**
    * @typedef {undefined|null|ViewModelActionInterface|((e:ControllerEvent)=>void)} ViewControllerAction
    *
    * @typedef ViewControllerOptions
    * @property {1|2} [phase] Optional value which determines phase when event listener is triggered. Allowed values are ViewListener.BUBBLE_PHASE or ViewListener.CAPTURE_PHASE. Default value is ViewListener.BUBBLE_PHASE.
    * @property {View} [view] Optional view this controller is attached to. View is parent of controller.
    * @property {ViewModel} [viewModel] TODO: remove
    *
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|ViewControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      const htmlElem = this.#htmlElem = ViewElem.toHTMLElement(elem)
      this.#logid = new LogID(this, htmlElem)
      const elemError = ViewControllerConfig.isElementType(htmlElem)
      if (elemError) return VMX.throwError(this, elemError("argument elem«"))
      if (options) {
         this.validateInitOptions(options)
         if (options.view)
            this.#view = options.view
         if (options.viewModel)
            this.#viewModel = options.viewModel
         if (options.phase)
            this.#phase = options.phase
      }
      this.setAction(action)
   }
   free() {
      this.deactivate()
      this.removeListeners(this.#listeners)
      this.removeListeners(this.#activeListeners)
      this.#logid.free()
      this.#action?.endAction()
   }
   /**
    * Called if delay option timed out (if activate called with delay set to a value > 0).
    * Does nothing. Should be overwritten in a subtype if any action should be taken.
    */
   onActivationDelay() { }
   /**
    * Called if autoDeactivation option timed out (if activate called with autoDeactivation set to a value > 0).
    * Deactivates the controller. Should be overwritten in a subtype if additional action should be taken.
    */
   onAutoDeactivation() { this.deactivate() }
   /////////////////
   // Init Helper //
   /////////////////
   /**
    * @param {null|ViewModelActionInterface|((e:ControllerEvent)=>void)} [action]
    */
   setAction(action) {
      this.#action = typeof action === "function" ? new ViewModelActionWrapper(action)
                     : ViewModelActionInterface.isType(action) ? action
                     : action == null ? undefined
                     : VMX.throwError(this, `Expect argument »action« of type function or ViewModelActionInterface instead of ${VMX.typeof(action)}.`)
   }
   /**
    * @param {{[option:string]:any}} options
    */
   validateInitOptions(options) {
      const allowedOptions = this.ClassConfig.InitOptions
      BasicTypeChecker.validateOptions(this, options, allowedOptions)
   }
   ////////////////
   // Properties //
   ////////////////
   /**
    * @type {undefined|View}
    */
   get view() { return this.#view }
   get htmlElem() { return this.#htmlElem }
   get isActive() { return this.#isActive }
   get actionStarted() { return Boolean(this.#action && this.#action.isStarted()) }
   get isActivationDelay() { return this.#activationDelay !== undefined }
   get isAutoDeactivation() { return this.#autoDeactivation !== undefined }
   /** @return {number} Number of calls to {@link addActiveListener}. Calling {@link removeActiveListenerForElement} resets this value to 0. */
   get nrActiveListeners() { return this.#activeListeners.length }
   /** @return {boolean} *true* if controller is muted(+deactived) and does not generate any events. */
   get muted() { return this.#muted }
   /** @param {boolean} muted Set value to *true* in case you want to prevent action callbacks. */
   set muted(muted) { (this.#muted = muted) && this.deactivate() }
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @return {boolean} *true* if an listener is listening on htmlElem for event of type eventType.
    */
   hasListenerFor(htmlElem, eventType) {
      return this.#matchListener(htmlElem, eventType, this.#listeners)
   }
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @return {boolean} *true* if an listener in active state is listening on htmlElem for event of type eventType.
    */
   hasActiveListenerFor(htmlElem, eventType) {
      return this.#matchListener(htmlElem, eventType, this.#activeListeners)
   }
   /**
    * @param {string} eventType
    */
   hasActiveListenerForEvent(eventType) {
      return this.#activeListeners.some(listener => listener.eventType == eventType)
   }
   /**
    * @param {null|EventTarget|Document|HTMLElement} htmlElem
    */
   hasActiveListenerForElement(htmlElem) {
      return htmlElem ? this.#activeListeners.some( listener => listener.htmlElem === htmlElem) : false
   }
   ////////////////////
   // Action Support //
   ////////////////////
   /**
    * @param {ControllerEvent} ce
    */
   callAction(ce) {
      const action = this.#action
      if (!this.#muted && action && action.isStarted()) {
         try { action.doAction(ce) } catch(error) { VMX.logException(this, "callAction failed.", error) }
      }
   }
   /**
    * @param {ControllerEvent} ce
    */
   callActionOnce(ce) {
      this.callAction(ce)
      this.#action = undefined
   }
   /**
    * @param {undefined|Event} e
    */
   tryStartAction(e) {
      const action = this.#action
      if (!action || action.isStarted()) return
      e && VMX.listeners.onceEndEventLoop(e, () => action.startAction()) || action.startAction()
   }
   /**
    * Removes lock from action so that other controllers could start their action.
    */
   endAction() {
      const action = this.#action
      action?.isStarted() && action.endAction()
   }
   /** @param {()=>void} onStarted Called if action could be successfully started. @return {undefined|true} *true* if callback called with action started. */
   withStartedAction(onStarted) {
      const action = this.#action
      if (!action) return
      const alreadyStarted = action.isStarted()
      if (alreadyStarted || action.startAction()) {
         onStarted()
         alreadyStarted || action.endAction()
         return true
      }
  }
   /**
    * @param {{actionEvent:undefined|Event, autoDeactivation?:number, delay?:number, noStartAction?:boolean}} options
    * @param {()=>void} [onActivateCallback]
    */
   activate({actionEvent, autoDeactivation, delay, noStartAction}, onActivateCallback) {
      if (!this.#isActive && !this.#muted) {
         this.#isActive = true
         noStartAction || this.tryStartAction(actionEvent)
         onActivateCallback?.()
         this.clearActivationDelay()
         if (delay)
            this.#activationDelay = setTimeout(() => { this.#activationDelay = undefined; this.onActivationDelay() }, delay)
         this.clearAutoDeactivation()
         if (autoDeactivation)
            this.#autoDeactivation = setTimeout(() => { this.#autoDeactivation = undefined; this.onAutoDeactivation() }, autoDeactivation)
      }
   }
   /**
    * @param {()=>void} [onDeactivateCallback]
    */
   deactivate(onDeactivateCallback) {
      if (this.#isActive) {
         this.#isActive = false
         this.removeListeners(this.#activeListeners)
         this.clearActivationDelay()
         this.clearAutoDeactivation()
         this.endAction()
         onDeactivateCallback?.()
      }
   }
   /**
    * Stops delay timer. If delay option is set to a value > 0 in call to activate
    * a timer is started and getter {@link isActivationDelay} returns true.
    * After return the timer is cleared and isActivationDelay returns false.
    */
   clearActivationDelay() {
      if (this.#activationDelay !== undefined) {
         clearTimeout(this.#activationDelay)
         this.#activationDelay = undefined
      }
   }
   /**
    * Stops auto deactivation timer. If autoDeactivation option is set to a value > 0 in call to activate
    * a timer is started and getter {@link isAutoDeactivation} returns true.
    * After return the timer is cleared and isAutoDeactivation returns false.
    */
   clearAutoDeactivation() {
      if (this.#autoDeactivation !== undefined) {
         clearTimeout(this.#autoDeactivation)
         this.#autoDeactivation = undefined
      }
   }
   //////////////////////
   // Listener Support //
   //////////////////////
   /**
    * @param {EventTarget|ViewElem} elem
    * @param {string} eventType
    * @param {(e:Event)=>void} eventHandler
    * @return {undefined|ViewListener} A value of undefined means a listener for the given element and event type already exists.
    */
   addListener(elem, eventType, eventHandler) {
      const htmlElem = ViewElem.toEventTarget(elem)
      if (!this.#matchListener(htmlElem, eventType, this.#listeners)) {
         const listener = this.#newListener(htmlElem, eventType, eventHandler)
         this.#listeners.push(listener)
         return listener
      }
   }
   /**
    * @param {EventTarget|ViewElem} elem
    * @param {string} eventType
    * @param {(e:Event)=>void} eventHandler
    * @return {undefined|ViewListener} A value of undefined means a listener for the given element and event type already exists.
    */
   addActiveListener(elem, eventType, eventHandler) {
      const htmlElem = ViewElem.toEventTarget(elem)
      if (!this.#matchListener(htmlElem, eventType, this.#activeListeners)) {
         const listener = this.#newListener(htmlElem, eventType, eventHandler)
         this.#activeListeners.push(listener)
         return listener
      }
   }
   /**
    * @param {ViewListener[]} listenerArray
    */
   removeListeners(listenerArray) {
      listenerArray.forEach(listener => listener && listener.free())
      listenerArray.length = 0
   }
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @param {(e:Event)=>void} eventHandler
    * @return {ViewListener}
    */
   #newListener(htmlElem, eventType, eventHandler) {
      return new ViewListener(htmlElem, eventType, eventHandler, { owner:this, phase:this.#phase })
   }
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @param {ViewListener[]} listeners
    * @return {boolean} *true* if in listeners exists a listener which is listening on htmlElem for event of type eventType.
    */
   #matchListener(htmlElem, eventType, listeners) {
      return listeners.some( listener => listener.htmlElem == htmlElem && listener.eventType == eventType)
   }
   /**
    * @param {EventTarget} htmlElem
    * @param {string[]} eventTypes
    * @param {(e:Event)=>void} eventHandler
    */
   addListeners(htmlElem, eventTypes, eventHandler) {
      eventTypes.forEach(eventType=>{
         if (!this.#matchListener(htmlElem, eventType, this.#listeners))
            this.#listeners.push(this.#newListener(htmlElem, eventType, eventHandler))
      })
   }
   /**
    * @param {EventTarget} htmlElem
    * @param {string[]} eventTypes
    * @param {(e:Event)=>void} eventHandler
    */
   addActiveListeners(htmlElem, eventTypes, eventHandler) {
      eventTypes.forEach(eventType=>{
         if (!this.#matchListener(htmlElem, eventType, this.#activeListeners))
            this.#activeListeners.push(this.#newListener(htmlElem, eventType, eventHandler))
      })
   }
   /**
    * @param {null|EventTarget} htmlElem
    * @param {string} [eventType]
    */
   removeActiveListenerFor(htmlElem, eventType) {
      const matchingListeners = []
      this.#activeListeners = this.#activeListeners.filter( listener =>
         (listener.htmlElem == htmlElem && (eventType === undefined || listener.eventType === eventType)
            ? (matchingListeners.push(listener), false)
            : true)
      )
      this.removeListeners(matchingListeners)
   }
   ///////////////////
   // Touch Support //
   ///////////////////
   /**
    * @param {Event} e
    * @return {string} Contains listing of touch IDs. All IDs, target IDs and changed IDs.
    */
   touchIDs(e) {
      let all="", target="", changed=""
      if (ViewListener.isTouchEvent(e)) {
         for (const t of e.touches)
            all += ","+t.identifier
         for (const t of e.targetTouches)
            target += ","+t.identifier
         for (const t of e.changedTouches)
            changed += ","+t.identifier
      }
      return "touchids all:["+all.slice(1)+"] tg:["+target.slice(1)+"] ch:["+changed.slice(1)+"]"
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @return {number}
    */
   nrTargetTouches(e) { return this.computeTargetTouches(e).length }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @return {Touch[]}
    */
   computeTargetTouches(e) {
      const targetTouches = []
      const eventListenerTarget = e.currentTarget
      if (ViewListener.isTouchEvent(e) && eventListenerTarget instanceof HTMLElement) {
         for (const t of e.touches) {
            if (t.target instanceof Node
               && (  eventListenerTarget.isSameNode(t.target)
                  || eventListenerTarget.contains(t.target)))
               targetTouches.push(t)
         }
      }
      return targetTouches
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @return {undefined|Touch}
    */
   getChangedTouch(e) { return ViewListener.isTouchEvent(e) ? e.changedTouches[0] : undefined }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @param {undefined|Touch} pos
    * @return {{x:number, y:number}}
    */
   extractPos(e,pos) {
      const p = pos ?? (e instanceof MouseEvent && e)
      if (p && typeof p.clientX === "number" && typeof p.clientY === "number")
         return { x:p.clientX, y:p.clientY }
      return VMX.throwError(this, BasicTypeChecker.expectNameValueOfType("argument »pos«",pos,"{clientX:number,clientY:number}"), pos)
   }
}

class TouchSupportController extends ViewController {
   //////////////////////
   // Listener Support //
   //////////////////////
   /**
    * @param {(e:MouseEvent|TouchEvent)=>void} onStart
    * @param {boolean} [noMouse] *true*, if mouse should not be supported
    */
   addStartListener(onStart, noMouse) {
      onStart = onStart.bind(this)
      !noMouse && this.addListener(this.htmlElem,ViewListener.MOUSEDOWN,onStart)
      this.addListener(this.htmlElem,ViewListener.TOUCHSTART,onStart)
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @param {(e:MouseEvent|TouchEvent) => void} onEnd
    */
   addActiveEndListener(e, onEnd) {
      ViewListener.isTouchEvent(e)
      ? ( this.addActiveListener(this.htmlElem,ViewListener.TOUCHEND,onEnd.bind(this)),
          this.addActiveListener(this.htmlElem,ViewListener.TOUCHCANCEL,onEnd.bind(this)))
      : this.addActiveListener(document,ViewListener.MOUSEUP,onEnd.bind(this))
   }
   /**
    * Enables {@link onMove} event handler.
    * @param {(e:MouseEvent|TouchEvent)=>void} onMove
    */
   addActiveMoveListener(onMove) {
      this.hasActiveListenerForEvent(ViewListener.TOUCHEND)
      ? this.addActiveListener(this.htmlElem,ViewListener.TOUCHMOVE,onMove.bind(this))
      : this.addActiveListener(document,ViewListener.MOUSEMOVE,onMove.bind(this))
   }
   /**
    * Removes active move listener added with call to {@link addActiveMoveListener}.
    */
   removeActiveMoveListener() {
      this.hasActiveListenerForEvent(ViewListener.TOUCHEND)
      ? this.removeActiveListenerFor(this.htmlElem,ViewListener.TOUCHMOVE)
      : this.removeActiveListenerFor(document,ViewListener.MOUSEMOVE)
   }
}

class TouchControllerEvent extends ControllerEvent {
   /** @type {"touch"|"mouse"} */
   device
   /** @type {boolean} */
   contained
   /** @type {boolean} */
   cancel
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {Event} e
    * @param {boolean} contained
    * @param {boolean} cancel
    */
   constructor(type, controller, e, contained, cancel) {
      super(type, controller, e)
      this.device = e.type[0]=="t" ? "touch" : "mouse"
      this.contained = contained
      this.cancel = cancel
   }
}

class TouchController extends TouchSupportController {
   /////////////
   // ACTIONS //
   /////////////
   static TOUCHSTART=ViewListener.TOUCHSTART
   static TOUCHEND=ViewListener.TOUCHEND
   static CLICK="click"
   static DOUBLECLICK="doubleclick"

   /**
    * End of last touch (or mouseup). Used to recognize a simulated mouse click (in case of touchstart/touchend).
    * @type {undefined|{timeStamp: number, type:string}}
    */
   #lastEvent

   /**
    * @typedef {ViewControllerOptions&_TouchControllerOptions} TouchControllerOptions
    * @typedef _TouchControllerOptions
    * @property {boolean} [enableClick] Enables CLICK action. Default is false.
    * @property {boolean} [enableDoubleClick] Enables DOUBLECLICK action. Default is false.
    * @property {boolean} [nomouse] *true*, if mouse should be disabled. Default is false.
    *
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|TouchControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      super(elem, options, action)
      this.addStartListener(this.onStart, options?.nomouse)
      const onClick = this.onClick.bind(this)
      if (options) {
         if (options.enableClick) this.addListener(this.htmlElem,ViewListener.CLICK,onClick)
         if (options.enableDoubleClick) this.addListener(this.htmlElem,ViewListener.DBLCLICK,onClick)
      }
   }
   /**
    * @param {Event} e
    * @param {undefined|{timeStamp:number, type:string}} lastEvent
    * @return {undefined|boolean} *true* in case last event was a touch event and e is a mouse event at same time
    */
   isSimulatedMouse(e, lastEvent) {
      return lastEvent?.type.startsWith("t") && e.type.startsWith("m")
             && (lastEvent.timeStamp-e.timeStamp) <= 10
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onStart(e) {
      VMX.logInfo(this, `onStart ${e.type} ${e.eventPhase} ${this.touchIDs(e)}`)
      if (ViewListener.isMainButton(e)) {
         this.activate({actionEvent:e}, () => {
            this.addActiveEndListener(e, this.onEnd)
            if (!this.isSimulatedMouse(e, this.#lastEvent))
               this.callAction(new TouchControllerEvent(TouchController.TOUCHSTART, this, e, true, false))
         })
      }
   }
   /**
    * @param {MouseEvent} e
    */
   onClick(e) {
      VMX.logInfo(this, `onClick ${e.type} ${e?.detail}`)
      this.withStartedAction( () => {
         // e.detail === 0|undefined (custom event); e.detail === 1 (single click)
         // before DBLCLICK: CLICK is sent twice with e.detail === 2 (e.detail === 3 in case of TRIPPLE click)
         if (e.type === ViewListener.CLICK && !(e.detail >= 2))
            this.callAction(new TouchControllerEvent(TouchController.CLICK, this, e, true, false))
         else if (e.type === ViewListener.DBLCLICK)
            this.callAction(new TouchControllerEvent(TouchController.DOUBLECLICK, this, e, true, false))
      })
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onEnd(e) {
      VMX.logInfo(this, `${this.getLogID().name(this)} onEnd ${e.type} ${this.touchIDs(e)}`)
      if (this.nrTargetTouches(e) === 0 && ViewListener.isMainButton(e)) {
         const isSimulatedMouse = this.isSimulatedMouse(e, this.#lastEvent)
         this.#lastEvent = { type:e.type, timeStamp:e.timeStamp }
         if (!isSimulatedMouse) {
            const pos = this.extractPos(e,this.getChangedTouch(e))
            const contained = isFinite(pos.x) && isFinite(pos.y) && this.htmlElem.contains(document.elementFromPoint(pos.x,pos.y))
            const cancel = e.type.endsWith("l") // true => 'touchcancel' (false => 'mouseup'|'touchend')
            VMX.logInfo(this, `TOUCHEND call action contained=${contained} lock=${this.actionStarted}`)
            this.callAction(new TouchControllerEvent(TouchController.TOUCHEND, this, e, contained, cancel))
         }
         this.deactivate()
      }
   }
}

class ClickController extends TouchController {
   /**
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|TouchControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      super(elem, { enableClick:true, ...options }, action)
   }
   /**
    * Filter all actions except CLICK actions (also DOUBLECLICK if enableDoubleClick set to true).
    * @param {ControllerEvent} ce
    */
   callAction(ce) {
      if (ce.type.endsWith(ClickController.CLICK)) {
         VMX.logInfo(this, "click action")
         super.callAction(ce)
      }
   }
}

class MoveControllerEvent extends ControllerEvent {
   /** @type {number} */
   dx
   /** @type {number} */
   dy
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {Event} e
    * @param {{dx:number, dy:number}} delta
    */
   constructor(type, controller, e, delta) {
      super(type, controller, e)
      this.dx = delta.dx
      this.dy = delta.dy
   }
}

class MoveController extends TouchSupportController {
   /////////////
   // ACTIONS //
   /////////////
   static MOVE="move"

   /** @type {{x:number, y:number}} */
   #pos
   /** @type {{x:number, y:number}} */
   #totalxy

   /**
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|TouchControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      super(elem, options, action)
      this.addStartListener(this.onStart)
   }
   /**
    * @param {undefined|Touch|Event} e
    * @return {boolean} *true* in case target where event originated should not be considered as start of touch/moudesdown operation
    */
   blockedTarget(e) {
      return ViewElem.isInteractiveElement(e?.target)
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onStart(e) {
      const nrActiveTouches = this.nrTargetTouches(e)
      VMX.logDebug(this, `onStart ${e.type} blocked:${this.blockedTarget(e)} ${this.touchIDs(e)}`)
      if (nrActiveTouches <= 1 && !this.blockedTarget(e)) {
         this.activate({actionEvent:e, delay:30, autoDeactivation:400}, () => {
            this.#pos = this.extractPos(e,this.getChangedTouch(e))
            this.#totalxy = { x:0, y:0 }
            this.addActiveEndListener(e, this.onEnd)
         })
      }
      else
         this.deactivate()
   }
   onActivationDelay() {
      this.addActiveMoveListener(this.onMove)
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onMove(e) {
      const targetTouches = this.computeTargetTouches(e)
      VMX.logInfo(this, `onMove ${e.type} ${this.touchIDs(e)}`)
      if (targetTouches.length <= 1) {
         e.preventDefault()
         this.clearAutoDeactivation()
         const pos = this.extractPos(e,targetTouches[0])
         const delta = { dx:Math.trunc(pos.x-this.#pos.x), dy:Math.trunc(pos.y-this.#pos.y) }
         if (delta.dx || delta.dy) {
            this.#pos = { x:delta.dx+this.#pos.x, y:delta.dy+this.#pos.y }
            this.#totalxy = { x:delta.dx+this.#totalxy.x, y:delta.dy+this.#totalxy.y }
            this.callAction(new MoveControllerEvent(MoveController.MOVE,this,e,delta))
         }
      }
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onEnd(e) {
      const targetTouches = this.computeTargetTouches(e)
      const nrActiveTouches = targetTouches.length
      VMX.logDebug(this, `onEnd ${e.type} ${this.touchIDs(e)}`)
      if (nrActiveTouches === 1 && !this.blockedTarget(targetTouches[0])) {
         this.#pos = this.extractPos(e,targetTouches[0])
         this.tryStartAction(e)
         this.addActiveMoveListener(this.onMove)
      }
      else if (nrActiveTouches === 0)
         this.deactivate()
   }
}

class TouchResizeControllerEvent extends ControllerEvent {
   /** @type {number} */
   dx
   /** @type {number} */
   dy
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {Event} e
    * @param {{dx:number, dy:number}} delta
    */
   constructor(type, controller, e, delta) {
      super(type, controller, e)
      this.dx = delta.dx
      this.dy = delta.dy
   }
}

class TouchResizeController extends TouchSupportController {
   /////////////
   // ACTIONS //
   /////////////
   static RESIZE="resize"
   /** @type {{x:number, y:number}} */
   #pos
   /** @type {{x:number, y:number}} */
   #totalxy
   /**
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|TouchControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      super(elem, options, action)
      this.addStartListener(this.onStart, /*nomouse*/true)
   }
   /**
    * @param {Touch} p1
    * @param {Touch} p2
    */
   extractPos2(p1, p2) {
      if (p1 && p2)
         return { x:Math.abs(p1.clientX - p2.clientX), y:Math.abs(p1.clientY - p2.clientY) }
      return VMX.throwError(this, "Called with less than 2 target touches.")
   }
   /**
    * @param {Touch} t
    * @returns {boolean} *true*, in case touch should not be considered.
    */
   static blockedTouch(t) { return ViewElem.isInteractiveElement(t.target) }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onStart(e) {
      const targetTouches = this.computeTargetTouches(e)
      const nrActiveTouches = targetTouches.length
      VMX.logInfo(this, `onStart ${e.type} ${this.touchIDs(e)}`)
      if (nrActiveTouches === 2 && !targetTouches.some(t => TouchResizeController.blockedTouch(t))) {
         this.activate({actionEvent:e}, () => {
            this.addActiveEndListener(e, this.onEnd)
            this.#pos = this.extractPos2(targetTouches[0],targetTouches[1])
            this.#totalxy = { x:0, y:0 }
            e.preventDefault()
            this.addActiveMoveListener(this.onMove)
         })
      }
      else
         this.deactivate()
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onMove(e) {
      if (!this.actionStarted) return
      const targetTouches = this.computeTargetTouches(e)
      const nrActiveTouches = targetTouches.length
      if (nrActiveTouches === 2) {
         VMX.logDebug(this, `Resize.onMove ${this.touchIDs(e)}`)
         e.preventDefault()
         const pos = this.extractPos2(targetTouches[0],targetTouches[1])
         const delta = { dx:Math.trunc(pos.x-this.#pos.x), dy:Math.trunc(pos.y-this.#pos.y) }
         if (delta.dx || delta.dy) {
            this.#pos = { x:delta.dx+this.#pos.x, y:delta.dy+this.#pos.y }
            this.#totalxy = { x:delta.dx+this.#totalxy.x, y:delta.dy+this.#totalxy.y }
            this.callAction(new TouchResizeControllerEvent(TouchResizeController.RESIZE,this,e,delta))
         }
      }
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onEnd(e) {
      const targetTouches = this.computeTargetTouches(e)
      const nrActiveTouches = targetTouches.length
      VMX.logDebug(this, `onEnd ${e.type} ${this.touchIDs(e)}`)
      if (nrActiveTouches === 2 && !targetTouches.some(t => TouchResizeController.blockedTouch(t))) {
         this.#pos = this.extractPos2(targetTouches[0],targetTouches[1])
         this.tryStartAction(e)
         this.addActiveMoveListener(this.onMove)
      }
      else if (nrActiveTouches) {
         this.endAction()
         this.removeActiveMoveListener()
      }
      else
         this.deactivate()
   }
}

class TransitionControllerEvent extends ControllerEvent {
   /** @ŧype {boolean} */
   isTransition = false
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {Event} e
    */
   constructor(type, controller, e) {
      super(type, controller, e)
   }
}

class TransitionController extends ViewController {
   /////////////
   // ACTIONS //
   /////////////
   /** Sent at end of transition (only once). Afterwards controller is removed from target. */
   static TRANSITIONEND="transitionend"
   /**
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {undefined|null|TouchControllerOptions} options
    * @param {ViewControllerAction&((e:TransitionControllerEvent)=>void)} [action]
    */
   constructor(elem, options, action) {
      super(elem, options, action)
      this.addListener(this.htmlElem,ViewListener.TRANSITIONRUN,this.onStart.bind(this))
      this.onStart()
   }

   isTransition() {
      return this.htmlElem && TransitionController.isTransition(this.htmlElem)
   }

   ensureInTransitionElseEnd() {
      if (! this.isActive)
         this.onEnd(new Event(ViewListener.TRANSITIONEND))
   }
   /**
    * @param {TransitionEvent} [e]
    */
   onStart(e) {
      if (!this.isActive && TransitionController.isTransition(this.htmlElem)) {
         this.activate({actionEvent:undefined}, () => {
            const onEnd = this.onEnd.bind(this)
            this.addActiveListener(this.htmlElem, ViewListener.TRANSITIONCANCEL, onEnd)
            this.addActiveListener(this.htmlElem, ViewListener.TRANSITIONEND, onEnd)
         })
      }
   }
   /**
    * @param {Event} e
    */
   onEnd(e) {
      if (!this.isTransition()) {
         this.callActionOnce(new TransitionControllerEvent(TransitionController.TRANSITIONEND, this, e))
         this.free()
      }
   }
   /**
    * @param {Document|HTMLElement} htmlElem
    * @return {Animation[]}
    */
   static getTransitions(htmlElem) {
      return htmlElem.getAnimations(/*includes no childs*/).filter(animation => animation instanceof CSSTransition)
   }
   /**
    * @param {Document|HTMLElement} htmlElem
    * @return {boolean} True in case transition animations are running.
    */
   static isTransition(htmlElem) {
      return htmlElem.getAnimations(/*includes no childs*/).some(animation => animation instanceof CSSTransition)
   }
   /**
    * @param {Document|HTMLElement} htmlElem The element whose running transitions should be finished.
    */
   static endTransition(htmlElem) {
      const transitions = this.getTransitions(htmlElem)
      transitions.forEach( transition => transition.finish())
   }
}

class DropControllerEvent extends ControllerEvent {
   /** @type {undefined|DataTransferItemList} */
   items
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {DragEvent} e
    */
   constructor(type, controller, e) {
      super(type, controller, e)
      this.items = e.dataTransfer?.items
   }
}

/**
 * Supports handling of drop operations in an HTML element (called dropzone).
 *
 * Following HTML events are handled:
 * "dragenter":  "dragged user data entered dropzone" (is sent before dragleave in case entering child element)
 *               event: {type:"dropstart", items: {type,kind}[], target:HTMLElement }
 *               action: indicate drop possible or impossible
 *
 * "dragover":   "dragged user data is within dropzone and moving"
 *               controller prevents only default which enables customized behaviour
 *
 * "drop":       "user dropped data over dropzone"
 *               event: {type:"drop", items: {type:string, kind:string, getAsFile():File,getAsString():string}[], target:HTMLElement }
 *               action: handle dropped data
 *               contoller calls "dragleave" (see dragleave)
 *
 * "dragleave":  "drop operation ends, user exited dropzone, or canceled operation (ESCape button)"
 *               event: {type:"dropend", items, target }
 *               action: remove indication drop possible or impossible
 */
class DropController extends ViewController {
   /**
    * Needed to support child elements within dropzone.
    * A value > 1 means
    */
   #enterCount = 0
   /**
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|ViewControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      super(elem, options, action)
      this.addListener(this.htmlElem,"dragenter",this.onStart.bind(this))
      // enable executing drop handler
      this.addListener(this.htmlElem,"dragover",DropController.preventDefault)
      // prevent opening of images if dropped not into target zone
      this.addListener(document,"drop",DropController.preventDefault)
   }
   /**
    * @param {DragEvent} e
    */
   onStart(e) {
      VMX.logInfo(this, `onStart ${e.type}`)
      ++ this.#enterCount // dragenter for a child is sent before dragleave of parent
      this.activate({actionEvent:e}, () => {
         this.callAction(new DropControllerEvent("dropstart",this,e))
         this.addActiveListener(this.htmlElem,"drop",this.onDrop.bind(this)),
         this.addActiveListener(this.htmlElem,"dragleave",this.onEnd.bind(this))
      })
   }
   /**
    * @param {DragEvent} e
    */
   onDrop(e) {
      VMX.logInfo(this, `onDrop ${e.type}`)
      e.preventDefault()
      this.callAction(new DropControllerEvent("drop",this,e))
      this.onEnd(e)
   }
   /**
    * @param {DragEvent} e
    */
   onEnd(e) {
      VMX.logInfo(this, `onEnd ${e.type}`)
      if (0 === (--this.#enterCount)) {
         this.callAction(new DropControllerEvent("dropend",this,e))
         this.deactivate()
      }
   }
}

class ChangeControllerEvent extends ControllerEvent {
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {Event} e
    */
   constructor(type, controller, e) {
      super(type, controller, e)
      this.checked = ViewElem.getChecked(e.target)
      this.value = (e.target && "value" in e.target) && this.checked !== false
                  ? String(e.target.value) : ""
      this.name = e.target && "name" in e.target ? String(e.target.name) : ""
   }
}

class ChangeController extends ViewController {
   /**
    * @typedef {ViewControllerOptions&_ChangeControllerOptions} ChangeControllerOptions
    * @typedef _ChangeControllerOptions
    * @property {EventTarget[]} [group] Group of elements to be listened for change events instead of a single htmlElem.
    *
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|ChangeControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      super(elem, options, action)
      const onChange = this.onChange.bind(this)
      for (const elem of options?.group ?? [this.htmlElem])
         this.addListener(elem,"change",onChange)
   }
   /**
    * @param {Event} e
    */
   onChange(e) {
      this.callAction(new ChangeControllerEvent("change", this, e))
   }
}

class FocusControllerEvent extends ControllerEvent {
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {Event} e
    * @param {boolean} focus
    */
   constructor(type, controller, e, focus) {
      super(type, controller, e)
      this.focus = focus
   }
}

class FocusController extends ViewController {
   /**
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|ViewControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      super(elem, options, action)
      this.addListener(this.htmlElem,"focusin",this.onFocus.bind(this))
      this.addListener(this.htmlElem,"focusout",this.onFocus.bind(this))
   }
   /**
    * @param {FocusEvent} e
    */
   onFocus(e) { this.callAction(new FocusControllerEvent("focus",this,e,true)) }
   /**
    * @param {FocusEvent} e
    */
   onBlur(e) { this.callAction(new FocusControllerEvent("blur",this,e,false)) }
}

class KeyControllerEvent extends ControllerEvent {
   /** @type {string} */
   key
   /** @type {number} */
   acmsMask
   /** @type {boolean} */
   altKey
   /** @type {boolean} */
   ctrlKey
   /** @type {boolean} */
   metaKey
   /** @type {boolean} */
   shiftKey
   /**
    * @param {string} type
    * @param {ViewController} controller
    * @param {KeyboardEvent} e
    */
   constructor(type, controller, e) {
      super(type, controller, e)
      this.key = e.key
      this.acmsMask = (e.altKey ? 1 : 0) + (e.ctrlKey ? 2 : 0)
                    + (e.metaKey ? 4 : 0) + (e.shiftKey ? 8 : 0)
      this.altKey = e.altKey
      this.ctrlKey = e.ctrlKey
      this.metaKey = e.metaKey
      this.shiftKey = e.shiftKey
   }
   acmsKey() { return String.fromCharCode(65/*A*/+this.acmsMask) + this.key }
}

class KeyController extends ViewController {
   static KEYS=ViewListener.KEYS
   /////////////
   // ACTIONS //
   /////////////
   static KEYDOWN=ViewListener.KEYDOWN
   static KEYUP=ViewListener.KEYUP

   /**
    * @param {KeyControllerEvent} e
    * @return {boolean} *true* in case a lock could be acquired for the pressed key
    */
   static lockKeyFromEvent(e) {
      const lock = VMX.documentVM.getVMActionLock("key-"+e.acmsKey())
      VMX.listeners.onceEndEventLoop(e, () => lock.unlock())
      return lock.lock()
   }
   /**
    * @param {Document|HTMLElement|ViewElem} elem
    * @param {null|undefined|ViewControllerOptions} options
    * @param {ViewControllerAction} [action]
    */
   constructor(elem, options, action) {
      super(elem, options, action)
      this.addListener(this.htmlElem,ViewListener.KEYDOWN,this.onKeyDown.bind(this))
      this.addListener(this.htmlElem,ViewListener.KEYUP,this.onKeyUp.bind(this))
   }
   /**
    * @param {KeyboardEvent} e
    */
   onKeyDown(e) { this.callAction(new KeyControllerEvent(KeyController.KEYDOWN,this,e)) }
   /**
    * @param {KeyboardEvent} e
    */
   onKeyUp(e) { this.callAction(new KeyControllerEvent(KeyController.KEYUP,this,e)) }
}

class ViewNode {
   /**
    * @param {any} child
    * @return {HTMLElement|Text|Comment}
    */
   static assertChild(child) {
      if (child instanceof ViewElem)
         return child.htmlElem
      else if (child instanceof ViewComment)
         return child.htmlNode
      else if (child instanceof ViewText)
         return child.htmlNode
      else if (typeof child === "string")
         return document.createTextNode(child)
      return VMX.throwError(this, `Expect child of type ViewElem,ViewComment,ViewText or string and not »${String(VMX.typeof(child))}«.`)
   }
   ////////////
   // Helper //
   ////////////
   assertConnected() { this.connected() || VMX.throwError(this, `Expect this text node to be connected.\ntext=${this.text()}`) }
   /////////////
   // Content //
   /////////////
   /** @return {Node} */
   get htmlNode() { return VMX.throwError(this, `Overwrite htmlNode in subtype.`) }
   /** @return {string} */
   text() { return this.htmlNode.textContent ?? "" }
   ////////////////////////////////////////////////
   // Inserting, Removing, Replacing in Document //
   ////////////////////////////////////////////////
   connected() { return this.htmlNode.isConnected }
   connect() { if (!this.connected()) document.body.append(this.htmlNode) }
   disconnect() { this.remove() }
   remove() { this.htmlNode.parentNode?.removeChild(this.htmlNode) }
   /**
    * Replaces this ViewElem with another one.
    * @param {ViewElem|ViewNode|string} elem The new element with which this ViewElem is replaced.
    */
   replaceWith(elem) {
      this.assertConnected()
      this.htmlNode.parentNode?.replaceChild(ViewNode.assertChild(elem),this.htmlNode)
   }
}

class ViewComment extends ViewNode { // implements ViewNode
   /** @type {Comment} */
   #commentNode

   /** @param {Comment} commentNode */
   constructor(commentNode) {
      super()
      this.#commentNode = ViewComment.assertCommentNode(commentNode)
   }
   /**
    * @param {string} textString The text content of a newly created HTML Comment node.
    * @return {ViewComment} Encapsulates Comment node containing specified text.
    */
   static fromText(textString) { return new ViewComment(document.createComment(textString)) }
   ////////////
   // Helper //
   ////////////
   /**
    * @param {Node|null|undefined} commentNode
    * @return {Comment} The given commentNode in case it is of type Comment.
    */
   static assertCommentNode(commentNode) {
      if (commentNode instanceof Comment)
         return commentNode
      return VMX.throwError(this, `Expect type Comment and not »${String(VMX.typeof(commentNode))}«.`)
   }
   ////////////////////////
   // Overwrite ViewNode //
   ////////////////////////
   /** @return {Comment} */
   get htmlNode() { return this.#commentNode }
   remove() { this.#commentNode.remove() }
}

class ViewText extends ViewNode { // implements ViewNode
   /** @type {Text} */
   #textNode

   /** @param {Text} textNode */
   constructor(textNode) {
      super()
      this.#textNode = ViewText.assertTextNode(textNode)
   }
   /**
    * @param {string} textString The text content of a newly created HTML Text node.
    * @return {ViewText} Encapsulates HTML Text node containing specified text.
    */
   static fromText(textString) {
      return new ViewText(document.createTextNode(textString))
   }
   ////////////
   // Helper //
   ////////////
   /**
    * @param {Node|null|undefined} textNode
    * @return {Text} The given textNode in case it is of type Text.
    */
   static assertTextNode(textNode) {
      if (textNode instanceof Text)
         return textNode
      return VMX.throwError(this, `Expect type Text and not »${String(VMX.typeof(textNode))}«.`)
   }
   ////////////////////////
   // Overwrite ViewNode //
   ////////////////////////
   /** @return {Text} */
   get htmlNode() { return this.#textNode }
   remove() { this.#textNode.remove() }
}

class ViewElem extends ViewNode { // implements ViewNode
   /** Call to {@link nextID} increases number (possibly more than once). */
   static NEXT_ID = 1

   /**
    * @param {EventTarget|ViewElem} elem
    * @return {EventTarget}
    */
   static toEventTarget(elem) { return elem instanceof ViewElem ? elem.#htmlElem : elem }
   /**
    * @param {Document|HTMLElement|ViewElem} elem
    * @return {Document|HTMLElement}
    */
   static toHTMLElement(elem) { return elem instanceof ViewElem ? elem.#htmlElem : elem }

   /**
    * @typedef {{[style:string]:undefined|number|string}} ViewStyles
    */

   /** @type {HTMLElement} */
   #htmlElem
   /** @type {undefined|ViewElemStylesManager} */
   #stylesManager

   ////////////////////////
   // Overwrite ViewNode //
   ////////////////////////
   /**
    * @param {HTMLElement} htmlElem
    */
   constructor(htmlElem) {
      super()
      this.#htmlElem = ViewElem.assertHTMLElement(htmlElem)
   }
   /** @return {HTMLElement} */
   get htmlNode() { return this.#htmlElem }
   ////////////////
   // Properties //
   ////////////////
   get htmlElem() { return this.#htmlElem }
   /**
    * Name of data attribute of HTML node to mark it as a slot (container). The value of the attribute is the name of the slot.
    * @return {"data-slot"}
    */
   get slotattr() { return "data-slot" }
   get stylesManager() { return (this.#stylesManager ??= new ViewElemStylesManager(this.deleteStylesManager.bind(this))) }
   deleteStylesManager() { if (this.#stylesManager?.isFree) this.#stylesManager = undefined }
   isStylesManager() { return this.#stylesManager !== undefined }
   ////////////
   // Create //
   ////////////
   /**
    * Clones this ViewElem. The cloned node is not connected to the Document.
    * @return {ViewElem} A new ViewElem cloned from this one.
    */
   clone() {
      const cloned = this.#htmlElem.cloneNode(true)
      if (cloned instanceof HTMLElement) {
         return new ViewElem(cloned)
      }
      return VMX.throwError(this, `Can only clone HTMLElement nodes.`)
   }
   /**
    * @param {{[style:string]:string|number}} styles
    * @return {ViewElem}
    */
   static fromDiv(styles) {
      const viewElem = new ViewElem(document.createElement("div"))
      viewElem.updateStyles(styles)
      return viewElem
   }
   /**
    * @param {string} htmlString The HTML string which describes a single HTML element with any number of childs.
    * @return {ViewElem} The encapsulated HTML element specified by a string.
    */
   static fromHtml(htmlString) {
      return new ViewElem(this.newHTMLElement(htmlString))
   }
   /**
    * @param {string} textString The text content of a newly created HTML Text node.
    * @return {ViewElem} Encapsulates HTML span node which contains text as its single child.
    */
   static fromText(textString) {
      const div = document.createElement("div")
      div.appendChild(document.createTextNode(textString))
      return new ViewElem(div)
   }
   /**
    * @param {string} htmlString
    * @return {HTMLElement}
    */
   static newHTMLElement(htmlString) {
      htmlString = String(htmlString??"").trim()
      const needsTable = (htmlString.startsWith("<td") || htmlString.startsWith("<tr"))
      const div = document.createElement(needsTable ? "table" : "div")
      div.innerHTML = htmlString
      if (div.childNodes.length !== 1)
         return VMX.throwError(this, `HTML string describes not a single child node.\nhtml=${htmlString}`)
      const htmlElem = needsTable ? div.querySelector(htmlString.substring(1,3)) : div.childNodes[0]
      return htmlElem instanceof HTMLElement
               ? (htmlElem.remove(), htmlElem)
               : VMX.throwError(this, `HTML string describes no HTMLElement but »${VMX.typeof(htmlElem)}«.\nhtml=${htmlString}`)
   }
   // TODO:
   // static createNodeListfromHtml(htmlString) {
   //   const div = document.createElement("div")
   //   div.innerHTML = htmlString
   //   const list = document.createDocumentFragment()
   //   list.append(...div.childNodes)
   //   return list
   // }
   ////////////
   // Helper //
   ////////////
   /**
    * @param {Node|null|undefined} htmlElem
    * @return {HTMLElement} The given htmlElem in case it is of type HTMLElement or Document.
    */
   static assertHTMLElement(htmlElem) {
      if (htmlElem instanceof HTMLElement)
         return htmlElem
      return VMX.throwError(this, `Expect type HTMLElement and not »${String(VMX.typeof(htmlElem))}«.`)
   }
   /**
    * @param {any} viewElem
    * @return {ViewElem}
    */
   static assertType(viewElem) {
      if (viewElem instanceof ViewElem)
         return viewElem
      return VMX.throwError(this, `Expect type ViewElem and not »${String(VMX.typeof(viewElem))}«.`)
   }
   ///////////////////
   // Query Content //
   ///////////////////
   /** @return {string} */
   text() { return this.#htmlElem.textContent ?? "" }
   /** @return {string} */
   html() { return this.#htmlElem.innerHTML }
   /** @return {string} */
   outerHTML() { return this.#htmlElem.outerHTML }
   /**
    * @param {string} cssSelector
    * @return {ViewElem}
    */
   static query(cssSelector) {
      const htmlElem = document.querySelector(cssSelector)
      if (!(htmlElem instanceof HTMLElement))
         return VMX.throwError(this, `Found no HTMLElement matching '${cssSelector}'.`)
      if (htmlElem instanceof HTMLTemplateElement) {
         const child = htmlElem.content.children[0]
         if (!(child instanceof HTMLElement))
            return VMX.throwError(this, `Found no child node inside matching template '${cssSelector}'.`)
         return new ViewElem(child)
      }
      return new ViewElem(htmlElem)
   }
   /**
    * Calls first {@link query} and returns a clone of the queried ViewElem.
    * @param {string} cssSelector
    * @return {ViewElem}
    */
   static clone(cssSelector) {
      const viewElem = this.query(cssSelector)
      return viewElem?.clone() ?? VMX.throwError(this, `Found no HTMLElement matching '${cssSelector}'.`)
   }
   /**
    * @param {string} cssSelector
    * @return {undefined|ViewElem}
    */
   query(cssSelector) {
      const node = this.#htmlElem.querySelector(cssSelector) ?? (this.#htmlElem.matches(cssSelector) && this.#htmlElem || null)
      return node instanceof HTMLElement ? new ViewElem(node) : undefined
   }
   /** @return {undefined|ViewElem|ViewText} */
   child() { return this.childAt(0) }
   /** @param {number} index @return {undefined|ViewElem|ViewText} */
   childAt(index) {
      const child = this.#htmlElem.childNodes[index]
      return child instanceof HTMLElement ? new ViewElem(child)
             : child instanceof Text ? new ViewText(child)
             : undefined
   }
   /** @param {number} index @return {undefined|ViewElem} */
   childElemAt(index) {
      const child = this.#htmlElem.childNodes[index]
      return child instanceof HTMLElement ? new ViewElem(child)
             : undefined
   }
   /** @return {number} */
   nrChilds() { return this.#htmlElem.childNodes.length }
   ////////////////////////////////////////////////
   // Inserting, Removing, Replacing in Document //
   ////////////////////////////////////////////////
   remove() { this.#htmlElem.remove() }
   /**
    * Replaces this ViewElem with another one.
    * @param {ViewNode|string} elem The new element with which this ViewElem is replaced.
    */
   replaceWith(elem) {
      this.assertConnected()
      this.#htmlElem.replaceWith(ViewNode.assertChild(elem))
   }
   ////////////////////
   // Change Content //
   ////////////////////
   /**
    * Copy childs from element to this element
    * @param {ViewElem} fromElem
    */
   copyFrom(fromElem) { this.#htmlElem.innerHTML = fromElem.#htmlElem.innerHTML }
   /**
    * Appends given childs to end of list of children of this element.
    * If any argument is of type string it is converted to a text node before adding
    * it as a child.
    * @param  {...ViewNode|string} childs List of new childs appended to end of list of children.
    */
   appendChildren(...childs) {
      const htmlElems = childs.map(child => ViewNode.assertChild(child))
      this.#htmlElem.append(...htmlElems)
   }
   /** @param {undefined|null|ViewNode|string} child */
   setChild(child) {
      if (child)
         this.replaceChildren(child)
      else
         this.removeChildren()
   }
   /** @param {ViewNode} child @param {number} index */
   setChildAt(child, index) {
      const oldNode = this.#htmlElem.childNodes[index]
      oldNode ? this.#htmlElem.replaceChild(child.htmlNode,oldNode)
              : this.#htmlElem.appendChild(child.htmlNode)
   }
   /**
    * Removes all childs of this element and adds all given childs as new childs in the same order.
    * If any argument is of type string it is converted to a text node before adding
    * it as a child.
    * @param  {...ViewNode|string} childs The new childs which replaces the old ones.
    */
   replaceChildren(...childs) {
      const htmlElems = childs.map(child => ViewNode.assertChild(child))
      this.#htmlElem.replaceChildren(...htmlElems)
   }
   /** Removes all childs of this element.*/
   removeChildren() { this.#htmlElem.replaceChildren() }
   /** @param {number} index */
   removeChildAt(index) { this.#htmlElem.childNodes[index] && this.#htmlElem.removeChild(this.#htmlElem.childNodes[index]) }
   ////////////////////
   // Input Elements //
   ////////////////////
   /**
    * @param {undefined|null|Node|EventTarget} htmlElem
    * @return {undefined|boolean} Value of checked attribute, if htmlElem is either a radio button or a checkbox else undefined.
    */
   static getChecked(htmlElem) {
      return htmlElem instanceof HTMLInputElement && (htmlElem.type === "radio" || htmlElem.type === "checkbox")
               ? htmlElem.checked : undefined
   }
   /**
    * @return {undefined|boolean} Value of checked attribute, if ViewElem is either a radio button or a checkbox else undefined.
    */
   checked() { return ViewElem.getChecked(this.#htmlElem) }
   /** Node names of interactive elements. */
   static INTERACTIVE = new Set(["INPUT","LABEL","SUMMARY","SELECT","TEXTAREA"])
   /**
    * @param {undefined|null|EventTarget} target
    * @return {boolean} *true*, if element has focus, is a node from {@link INTERACTIVE}.
    */
   static isInteractiveElement(target) {
      const nodeName = target instanceof Node ? target.nodeName : ""
      return target === document.activeElement || this.INTERACTIVE.has(nodeName)
   }
   ///////////////////////
   // Position and Size //
   ///////////////////////
   /** @param {()=>any} onConnected @return {any} */
   ensureConnected(onConnected) {
      const isConnected = this.connected()
      isConnected || this.connect()
      const result = onConnected()
      isConnected || this.disconnect()
      return result
   }
   /**
    * Position of element read from computed styles.
    * @return {{width:number, height:number, top:number, left:number, position:string}}
    */
   computedPos() {
      const pos = { width:100, height:100, top:0, left:0, position:"static" }
      return this.ensureConnected( () => {
         const cs = this.computedStyle()
         pos.position = cs.position
         pos.height = ViewElem.parseFloatDefault(cs.height, pos.height)
         pos.width = ViewElem.parseFloatDefault(cs.width, pos.width)
         const isStatic = cs.position === "static"
         if (!isStatic) {
            pos.top = ViewElem.parseFloatDefault(cs.top, pos.top)
            pos.left = ViewElem.parseFloatDefault(cs.left, pos.left)
         }
         return pos
      })
   }
   /**
    * Position styles read from computed styles.
    * @return {{width:string, height:string, top:string, left:string, bottom:string, right:string }}
    */
   computedPosStyles() {
      return this.ensureConnected(() => {
         const cs = this.computedStyle()
         return { width:cs.width, height:cs.height, top:cs.top, left:cs.left, bottom:cs.bottom, right:cs.right }
      })
   }
   viewportPosStyles() {
      const vvp = visualViewport
      const width = vvp ? vvp.width*vvp.scale : document.documentElement.clientWidth
      const height = vvp ? vvp.height*vvp.scale : document.documentElement.clientHeight
      const vp = this.viewportRect()
      return { width:vp.width+"px", height:vp.height+"px", top:vp.top+"px", left:vp.left+"px", bottom:(height-vp.bottom).toFixed(3)+"px", right:(width-vp.right).toFixed(3)+"px" }
   }
   /**
    * @return {boolean} *true*, if element could be positioned with "left" and "top" styles.
    */
   moveable() {
      return this.ensureConnected(() => {
         const isMoveable = ["fixed","absolute","relative"].includes(this.computedStyle().position)
         return isMoveable
      })
   }
   /**
    * The position relative to the viewport. A value of (0,0) means top/left starting point.
    * An increasing value (> 0) of x,left,right means more to the right of the viewport.
    * An increasing value (> 0) of y,top,bottom means more to the bottom of the viewport.
    * The width and height are the difference between right-left and bottom-top.
    * @return {{x:number,y:number,left:number,top:number,bottom:number,right:number,width:number,height:number}}
    */
   viewportRect() { return this.#htmlElem.getBoundingClientRect() }
   /**
    * @param {{width?:number, height?:number, top?:number, left?:number, transition?:boolean }} pos
    * @return {void}
    */
   setPos(pos) { this.stylesManager.setPos(this, pos, pos.transition) }
   /** @return {{width:number, height:number, top:number, left:number }} */
   pos() {
      const elemStyle = this.#htmlElem.style
      const pfd = ViewElem.parseFloatDefault
      return { width:pfd(elemStyle.width,0), height:pfd(elemStyle.height,0), top:pfd(elemStyle.top,0), left:pfd(elemStyle.left,0) }
   }
   /**
    * @param {{width?:number|undefined, height?:number|undefined, top?:number|undefined, left?:number|undefined }} pos
    * @return {undefined|{width?:number, height?:number, top?:number, left?:number}}
    */
   diffPos(pos) {
      const elemStyle = this.#htmlElem.style
      const changed = {}
      for (const prop of ["width","height","top","left"]) {
         const value = pos[prop]
         if (typeof value === "number" && isFinite(value)) {
            const styleValue = ViewElem.parseFloatDefault(elemStyle[prop],0)
            if (styleValue !== value) {
               changed[prop] = value
            }
         }
      }
      return Object.keys(changed).length > 0 ? changed : undefined
   }
   //////////////////////////
   // CSS Style Management //
   //////////////////////////
   /**
    * @param {string} className The CSS class name.
    * @return {boolean} *true*, if element was assigned given CSS class name.
    */
   hasClass(className) {
      return this.#htmlElem.classList.contains(className)
   }
   /**
    * Assigns CSS class name to element.
    * @param {string} className The CSS class name.
    */
   addClass(className) {
      this.#htmlElem.classList.add(className)
   }
   /**
    * Removes CSS class name from element.
    * @param {string} className The CSS class name.
    */
   removeClass(className) {
      this.#htmlElem.classList.remove(className)
   }
   /**
    * Adds or removes CSS class name to/from element.
    * @param {boolean} isAdd *true*, if class name should be added else it is removed.
    * @param {string} className The CSS class name.
    */
   switchClass(isAdd, className) {
      if (isAdd)
         this.#htmlElem.classList.add(className)
      else
         this.#htmlElem.classList.remove(className)
   }
   show() {
      this.#htmlElem.hidden = false
   }
   hide() {
      this.#htmlElem.hidden = true
   }
   get hidden() { return this.#htmlElem.hidden }
   /**
    * @param {boolean} hidden *true*, if view should be hidden (same as {@link hide}), else *false* {same as {@link show}}.
    */
   set hidden(hidden) { this.#htmlElem.hidden = hidden }
   /** Makes element (in-)visible without changing the layout. @param {boolean} visible */
   set visibility(visible) { this.#htmlElem.style.visibility = visible ? "visible" : "hidden" }
   /**
    * @param {string} str
    * @param {number} defaultValue
    * @return {number}
    */
   static parseFloatDefault(str, defaultValue) {
      const nr = parseFloat(str)
      return isNaN(nr) ? defaultValue : nr
   }
   /**
    * @param {string|number} value
    * @return {string} String(value)+"px". Value is rounded to 3 places after decimal point.
    */
   static toPixel(value) {
      return Number(value).toFixed(3) + "px"
   }
   /**
    * @return {CSSStyleDeclaration} A live object, whose values change automatically whenever styles of the (HTML-) element changes.
    */
   computedStyle() { return getComputedStyle(this.#htmlElem) }
   /**
    * Set new styles but do not return old values.
    * @param {ViewStyles} newStyles
    */
   updateStyles(newStyles) {
      const hStyle = this.#htmlElem.style
      for (const key in newStyles)
         hStyle[key] = newStyles[key]
   }
   /**
    * @param {ViewStyles} newStyles
    * @return {object} Old values of set style values.
    */
   setStyles(newStyles) {
      const oldStyles = {}
      const hStyle = this.#htmlElem.style
      for (const key in newStyles) {
         oldStyles[key] = hStyle[key]
         hStyle[key] = newStyles[key]
      }
      return oldStyles
   }
   /**
    * @param {string[]|({[style:string]:any})} [styles]
    * @return {object} Values of read styles. If style is undefined the values of all available styles are returned.
    */
   styles(styles) {
      const readValues = {}
      const hStyle = this.#htmlElem.style
      if (Array.isArray(styles)) {
         for (const key of styles)
            readValues[key] = hStyle[key]
      }
      else {
         for (const key in (styles ?? hStyle))
            readValues[key] = hStyle[key]
      }
      return readValues
   }
   /////////////////
   // Transitions //
   /////////////////
   /**
    * @return {CSSTransition[]}
    */
   getTransitions() {
      return this.#htmlElem.getAnimations(/*includes no childs*/).filter(animation => animation instanceof CSSTransition)
   }
   /**
    * @return {boolean} True in case transition animations are running.
    */
   isTransition() {
      return this.#htmlElem.getAnimations(/*includes no childs*/).some(animation => animation instanceof CSSTransition)
   }
   /**
    * @param {null|undefined|boolean|ViewTransitionStyle} transitionStyle If undefined,null,or false, updateAttrCallback is called without a transition. If *true*, the default transition styles are used.
    * @param {()=>void} updateAttrCallback Callback to update view attributes. Updating is done with a transition if stylesManager is set else without one.
    */
   startTransition(transitionStyle, updateAttrCallback) {
      this.stylesManager.startTransition(this, false, transitionStyle, updateAttrCallback)
   }
   /**
    * Finishes any running transition.
    */
   endTransition() {
      this.getTransitions().forEach( transition => transition.finish())
   }
   /**
    * Allows to wait for end of transition.
    * Notification is done either by callback or by waiting for the returned promise.
    * If no transition is running callback is called immediately or promise is resolved immediately.
    * @param {()=>void} [onTransitionEnd] Callback to signal end of transition (all computed view attributes has reached their final value).
    * @return {undefined|Promise<null>} Promise if onTransitionEnd is undefined else undefined.
    */
   onceOnTransitionEnd(onTransitionEnd) {
      if (onTransitionEnd) {
         new TransitionController(this.#htmlElem, null, (e) => {
            if (!e.isTransition)
               onTransitionEnd()
         }).ensureInTransitionElseEnd()
      }
      else {
         const promiseHolder = new PromiseHolder()
         new TransitionController(this.#htmlElem, null, (e) => {
            if (!e.isTransition)
               promiseHolder.resolve(null)
         }).ensureInTransitionElseEnd()
         return promiseHolder.promise
      }
   }
   ///////////
   // Slots //
   ///////////
   /**
    * @param {string} slotName The name of a slot.
    * @return {boolean} *true*, if a slot of the given name exists.
    */
   hasSlot(slotName) {
      return this.trySlot(slotName) != null
   }
   /**
    * @param {string} slotName The name of an existing slot.
    * @return {ViewElem} Container of slot. Use setChild to assign content.
    */
   slot(slotName) {
      const slot = this.trySlot(slotName)
      if (!slot)
         return VMX.throwError(this, `Missing slot »${this.slotattr}=${slotName}«.\nhtml=${this.outerHTML()}`)
      return slot
   }
   /**
    * @param {string} slotName The name of a slot.
    * @return {undefined|ViewElem} Container of slot or undefined, if slot does not exist. Use setChild to assign content.
    */
   trySlot(slotName) {
      return this.query("["+this.slotattr+"='"+slotName+"']")
   }
   /**
    * @param {string} slotName The name of an existing slot.
    * @param {null|undefined|ViewText|ViewElem} child The new content set into the slot.
    */
   setSlot(slotName, child) {
      const slot = this.slot(slotName)
      slot.setChild(child)
   }
   ////////////////
   // Attributes //
   ////////////////
   static nextID(prefix) {
      let id=""
      do { id = prefix+"_"+ViewElem.NEXT_ID++ } while (document.getElementById(id))
      return id
   }
}

class ViewElemDocument extends ViewElem {

   constructor() {
      super(document.documentElement)
   }
   connect() {
      if (!this.connected())
         document.append(this.htmlElem)
   }
   /**
    * @param {string} cssSelector
    * @return {undefined|ViewElem}
    */
   query(cssSelector) {
      const htmlElem = document.querySelector(cssSelector)
      if (htmlElem instanceof HTMLElement) {
         if (htmlElem instanceof HTMLTemplateElement) {
            const child = htmlElem.content.children[0]
            if (child instanceof HTMLElement)
               return new ViewElem(child)
            return undefined
         }
         return new ViewElem(htmlElem)
      }
   }

   /** @type {number} Size of one CSS pixel to the size of one physical pixel. Or number of device's pixels used to draw a single CSS pixel. */
   static get devicePixelRatio() { return window.devicePixelRatio }
   /** @type {{width:number, height:number}} Screen size in css pixel. */
   static get screen() { return { width:window.screen.width, height:window.screen.height } }
   /** @type {{width:number, height:number}} Screen size in physical device pixel. */
   static get deviceScreen() {
      const screen = window.screen, dpr = window.devicePixelRatio
      return { width:screen.width*dpr, height:screen.height*dpr }
   }
   /** @type {{width:number, height:number}} Size of viewport showing the document (not including size of scroll bars). */
   static get viewport() { return { width:document.documentElement.clientWidth, height:document.documentElement.clientHeight } }
}

class ViewElemStylesManager {
   /**
    * @typedef {{[style:string]:undefined|string, "transition-delay":string, "transition-duration":string, "transition-property":string, "transition-timing-function":string }} ViewTransitionStyle
    * @typedef {{position?:string, width?:string, height?:string, top?:string, left?:string }} ViewPositionStyle
    */
   /**
    * Default transition style for resize and change position animation.
    * @type {{"transition-delay":"0s",  "transition-duration":"0.2s", "transition-property":"all", "transition-timing-function":"ease-out"}}
    */
   static TransitionStyle={ "transition-delay":"0s", "transition-duration":"0.2s", "transition-property":"all", "transition-timing-function":"ease-out" }
   /**
    * @param {{width?:number, height?:number, top?:number, left?:number }} pos
    * @return {ViewPositionStyle} The position encoded as styles values.
    */
   static convertPos(pos) {
      const posStyles = {}
      for (const prop of ["width","height","top","left"]) {
         const value = pos[prop]
         if (typeof value === "number" && isFinite(value)) {
            posStyles[prop] = value + "px"
         }
      }
      return posStyles
   }
   /**
    * Delay needed to make changes of style effective.
    * @param {ViewElem} viewElem
    */
   static delay(viewElem) { viewElem.computedStyle().display !== "none" }

   /** The old styles of the HTMLElement before the transition styles were applied. @type {{position?:ViewPositionStyle,transition?:ViewTransitionStyle}} */
   #oldStyles
   /** The class which was set in last call to {@link setFixedPos}. @type {{position?:string}} */
   #setClass
   /** Called whenever this manager is no more in use (isFree === true). @type {undefined|(()=>void)} */
   #onFree
   /** *True* in case position should be reset after transition ends. @type {boolean} */
   #doResetPosition

   /**
    * @param {()=>void} [onFree] Called whenever this manager is no more in use (isFree === true).
    */
   constructor(onFree) {
      this.#oldStyles = {}
      this.#setClass = {}
      this.#onFree = onFree
      this.#doResetPosition = false
   }
   /** @param {ViewElem} viewElem */
   free(viewElem) {
      for (const propName of Object.keys(this.#oldStyles))
         this.#resetStyle(viewElem, propName, this.#oldStyles[propName])
      this.tryCallOnFree()
   }
   tryCallOnFree() { this.#onFree && this.isFree && (this.#onFree(), this.#onFree=undefined) }
   get isFree() { return Object.keys(this.#oldStyles).length === 0 }
   get isPosition() { return this.#oldStyles.position !== undefined }
   get isTransition() { return this.#oldStyles.transition !== undefined }
   /**
    * @param {ViewElem} viewElem
    * @param {boolean} doResetPosition
    * @param {null|undefined|boolean|ViewTransitionStyle} transitionStyle If undefined,null,or false, updateAttrCallback is called without a transition. If *true*, the default transition styles are used.
    * @param {()=>void} onTransitionStart Callback to update view attributes. Updating is done with a transition.
    */
   startTransition(viewElem, doResetPosition, transitionStyle, onTransitionStart) {
      ViewElemStylesManager.delay(viewElem) // add delay so that transitions work after switching to hidden=false or position="fixed"
      if (transitionStyle)
         this.#oldStyles.transition = { ...viewElem.setStyles((typeof transitionStyle === "object" && transitionStyle || ViewElemStylesManager.TransitionStyle)), ...this.#oldStyles.transition }
      else
         this.#resetStyle(viewElem, "transition")
      const unchanged = this.#oldStyles.transition
      this.#doResetPosition = doResetPosition
      onTransitionStart()
      const resetOnTransitionEnd = () => {
         if (this.#oldStyles.transition !== unchanged) return
         if (viewElem.isTransition()) {
            new ViewListener(viewElem.htmlElem, ViewListener.TRANSITIONEND, resetOnTransitionEnd, { once:true })
         }
         else {
            this.#resetStyle(viewElem, "transition")
            if (this.#doResetPosition) {
               this.#doResetPosition = false
               this.#resetStyle(viewElem, "position")
            }
            this.tryCallOnFree()
         }
      }
      resetOnTransitionEnd()
   }
   /**
    * @param {ViewElem} viewElem
    */
   switchPositionToFixed(viewElem) {
      if (!this.#oldStyles.position) {
         const posStyles = viewElem.viewportPosStyles()
         this.#oldStyles.position = viewElem.setStyles({...posStyles, position:"fixed"})
      }
   }
   /**
    * Restores old styles previously changed with call to {@link switchPositionToFixed}.
    * @param {ViewElem} viewElem
    * @param {null|undefined|boolean|ViewTransitionStyle} [transitionStyle]
    */
   resetPosition(viewElem, transitionStyle) {
      if (this.#oldStyles.position) {
         // get current pos and stop transition
         viewElem.updateStyles(viewElem.computedPosStyles())
         this.#setClass.position && viewElem.removeClass(this.#setClass.position)
         delete this.#setClass.position
         ViewElemStylesManager.delay(viewElem)
         this.#oldStyles.transition && viewElem.updateStyles(this.#oldStyles.transition)
         // get old pos in fixed position coordinates (could be changed => not cached)
         const switchedStyles = viewElem.setStyles(this.#oldStyles.position)
         const oldPos = viewElem.viewportPosStyles()
         viewElem.updateStyles(switchedStyles)
         // transition to old position and reset position after transition
         this.startTransition(viewElem, true, transitionStyle, () => {
            viewElem.updateStyles(oldPos)
         })
      }
   }
   /**
    * Sets new position immediately (if not in {@link switchPositionToFixed} not called before).
    * If in "fixed position mode" the new position merge into the old remembered position before switching.
    * If a transition is active to restore the old position (switch ouf of fixed position mode)
    * the transition coordinates are updated to reflect the updated old position.
    * @param {ViewElem} viewElem
    * @param {{width?:number, height?:number, top?:number, left?:number}} pos
    * @param {null|undefined|boolean|ViewTransitionStyle} [transitionStyle]
    */
   setPos(viewElem, pos, transitionStyle) {
      const posStyles = ViewElemStylesManager.convertPos(pos)
      if (this.#oldStyles.position) {
         // update pos of "previous position mode"
         Object.assign(this.#oldStyles.position, posStyles)
         if (this.#oldStyles.transition && this.#doResetPosition) {
            // update running transition with new coordinates
            const transitionStyles = transitionStyle ?? viewElem.styles(this.#oldStyles.transition)
            this.resetPosition(viewElem, transitionStyles)
         }
      }
      else {
         this.startTransition(viewElem, false, transitionStyle, () => {
            viewElem.updateStyles(posStyles)
         })
      }
   }
   /**
    * @param {ViewElem} viewElem
    * @param {ViewPositionStyle} pos
    * @param {undefined|null|string} setClass
    * @param {null|undefined|boolean|ViewTransitionStyle} [transitionStyle]
    */
   setFixedPos(viewElem, pos, setClass, transitionStyle) {
      if (!this.#oldStyles.position) return VMX.throwError(this,`call switchPositionToFixed before setFixedPos.`)
      this.startTransition(viewElem, false, transitionStyle, () => {
         this.#setClass.position && viewElem.removeClass(this.#setClass.position)
         if (setClass) {
            viewElem.addClass(setClass)
            this.#setClass.position = setClass
         }
         else
            delete this.#setClass.position
         viewElem.updateStyles(pos)
      })
   }
   /**
    * @param {ViewElem} viewElem
    * @param {string} stylesName
    * @param {undefined|ViewStyles} [unchanged]
    */
   #resetStyle(viewElem, stylesName, unchanged) {
      if (this.#oldStyles[stylesName] && (!unchanged || this.#oldStyles[stylesName] === unchanged)) {
         viewElem.updateStyles(this.#oldStyles[stylesName])
         delete this.#oldStyles[stylesName]
         delete this.#setClass[stylesName]
      }
   }
}

class ViewElemPositionControl {
   /** @type {ViewElem} */
   viewElem
   /** @type {boolean|ViewTransitionStyle} */
   transitionStyle
   /**
    * @param {ViewElem} viewElem
    * @param {boolean|ViewTransitionStyle} transitionStyle
    */
   constructor(viewElem, transitionStyle) {
      this.viewElem = viewElem
      this.transitionStyle = transitionStyle
   }
   restore() {
      const viewElem = this.viewElem
      viewElem.stylesManager.resetPosition(viewElem, this.transitionStyle)
   }
   /**
   * @param {{[name:string]:string}} posStyle
   * @param {string} posClass
   * @param {()=>void} [onChanged] Called at end of position change transition.
   */
   changePos(posStyle, posClass, onChanged) {
      const viewElem = this.viewElem
      viewElem.stylesManager.switchPositionToFixed(viewElem)
      viewElem.stylesManager.setFixedPos(viewElem, posStyle, posClass, this.transitionStyle)
      onChanged && viewElem.onceOnTransitionEnd(onChanged)
   }
}

class ViewModelActionInterface {
   /**
    * @param {any} o
    * @return {o is ViewModelActionInterface}
    */
   static isType(o) {
      return   (o && typeof o === "object")
            && typeof o.isStarted === "function"
            && typeof o.doAction === "function"
            && typeof o.startAction === "function"
            && typeof o.endAction === "function"
   }
   constructor() { return VMX.throwError(this, `ViewModelActionInterface is abstract.`) }
   /**
    * @return {boolean} *true*, if action was succesfully started (is locked).
    */
   isStarted() { return true }
   /**
    * Action is called if {@link startAction} was successfully called and all constraints are fulfilled.
    * @param {{[name:string]: any}} [args]
    * @return {void}
    */
   doAction(args) { return VMX.throwError(this, `ViewModelActionInterface.doAction not implemented.`) }
   /**
    * @return {boolean} *true* in case of action could be locked.
    */
   startAction() { return true }
   /**
    * Releases action lock acquired by a call to {@link startAction}.
    */
   endAction() {}
}

class ViewModelActionWrapper /*implements ViewModelActionInterface*/ {
   /** @type {(args:undefined|{[name:string]:any})=>any} */
   action
   /**
    * @param {(args:undefined|{[name:string]:any})=>any} action
    */
   constructor(action) {
      if (typeof action !== "function")
         return VMX.throwError(this, `Expect argument »action« of type function.`)
      this.action = action
   }
   isStarted() { return true }
   /**
    * Action is always called.
    * @param {{[name:string]: any}} [args]
    */
   doAction(args) { this.action.call(null,args) }
   startAction() { return true }
   endAction() {}
}

/**
 * Combines an action of a ViewModel with a lock.
 * Returned from calling {@link ViewModel.bindAction}
 */
class ViewModelAction /*implements ViewModelActionInterface*/ {
   /** @type {(this:object,args:undefined|{[name:string]:any})=>any} */
   action
   /** @type {object} */
   model
   /** @type {ActionLockInterface} */
   lock
   /** @type {undefined|((args:object) => object)} */
   mapArgs

   /**
    * @param {ActionLockInterface} lock
    * @param {object} model
    * @param {(this:object,args:{[name:string]:any})=>void} vmAction
    * @param {(args:object) => object} [mapArgs]
    */
   constructor(lock, model, vmAction, mapArgs) {
      if (typeof vmAction !== "function")
         return VMX.throwError(this, `Expect argument »vmAction« of type function.`)
      this.action = vmAction
      this.model = model
      this.lock = lock
      this.mapArgs = mapArgs
   }
   isStarted() { return this.lock.isLocked() }
   /**
    * Action is called if {@link startAction} was successfully called and all constraints are fulfilled.
    * @param {{[name:string]: any}} [args]
    */
   doAction(args) {
      if (this.lock.isContextCondition() && this.isStarted())
         this.action.call(this.model,this.mapArgs?.(args)??args)
   }
   /**
    * @return {boolean} *true* in case of locking succeeded.
    */
   startAction() {
      if (this.lock.lock())
         VMX.logInfo(this.model??this,`startAction: lock=${VMX.stringOf(this.lock.getName())} OK << TODO: remove >>`)
      else
         VMX.logInfo(this.model??this,`startAction: lock=${VMX.stringOf(this.lock.getName())} FAILED << TODO: remove >>`)
      return this.lock.isLocked()
   }
   /**
    * Unlocks action.
    */
   endAction() {
      if (this.lock.isLocked()) {
         VMX.logInfo(this.model??this,`Unlock ${VMX.stringOf(this.lock.getName())}`)
         this.lock.unlock()
      }
   }
}

class ActionLockInterface {
   /**
    * @return {boolean} *true*, if defined constraints on other locks are fulfilled.
    */
   isContextCondition() { return true }
   /**
    * @return {boolean} *true*, if this lock was acquired ({@link lock} was called successfully).
    */
   isLocked() { return false }
   /**
    * @return {string}
    */
   getName() { return "" }
   /**
    * Tries to acquire lock. If lock is already acquired nothing is done.
    * @return {boolean} *true*, if lock is acquired.
    */
   lock() { return false }
   /**
    * Removes lock or undos changes of a partially acquired lock in case of an error.
    * After return {@link isLocked} return *false*.
    */
   unlock() {}
}

class ActionLockState {
   /** @type {number} Number of times the lock was acquired [0 ... (shared ? 100 : 1)]. */
   lockCounter = 0
   /** @type {boolean} *true*, if lock was acquired in shared mode. This value could be true even if configShared is false (parent nodes are locked shared). */
   shared = false
   /** @type {string} */
   name
   /** @type {boolean} *true*, if this lock is configured as shared lock. */
   configShared
   /** Closure of all parents ordered by depth. @type {ActionLockState[]} */
   parents
   /** Closure of all nodes which must be unlocked to satisfy context condition. @type {ActionLockState[]} */
   contextUnlockedNodes

   /**
    * @param {ActionLockState[]} parents
    * @return {ActionLockState[]}
    */
   static getContextUnlockNodesClosure(parents) {
      const unlockedNodes = new Set()
      for (const parent of parents)
         parent.contextUnlockedNodes.forEach(node => unlockedNodes.add(node))
      return [...unlockedNodes]
   }

   /**
    * @param {ActionLockState[]} parents
    * @return {ActionLockState[]}
    */
   static getParentsClosure(parents) {
      const parentClosure = new Set(parents)
      for (const parent of parents)
         parent.parents.forEach(pparent => parentClosure.add(pparent))
      const sortedClosure = new Set()
      while (parentClosure.size) {
         const increment = []
         for (const parent of parentClosure)
            if (parent.parents.every(pparent => sortedClosure.has(pparent)))
               increment.push(parent)
         for (const parent of increment) {
            parentClosure.delete(parent)
            sortedClosure.add(parent)
         }
      }
      return [...sortedClosure]
   }

   /**
    * @param {string} name
    * @param {boolean} shared *true*, if this lock is a shared lock.
    * @param {ActionLockState[]} parents
    */
   constructor(name, shared, ...parents) {
      this.name = name
      this.lockIsShared = shared
      this.parents = ActionLockState.getParentsClosure(parents)
      this.contextUnlockedNodes = ActionLockState.getContextUnlockNodesClosure(parents)
   }
   /** @param {ActionLockState[]} addedUnlockedNodes */
   addContextUnlockedNodes(...addedUnlockedNodes) {
      const unlockedNodes = new Set(this.contextUnlockedNodes)
      for (const node of addedUnlockedNodes)
         unlockedNodes.add(node)
      this.contextUnlockedNodes = [...unlockedNodes]
   }
   ///////////
   // Query //
   ///////////
   isContextCondition() { return this.contextUnlockedNodes.every(n => n.isNodeLockable(false)) }
   /** @param {boolean} shared */
   isNodeLockable(shared) { return this.lockCounter === 0 || (shared && this.shared) }
   /** @return {boolean} */
   isPathLockable() { return this.isNodeLockable(this.configShared) && this.parents.every(parent => parent.isNodeLockable(true)) }
   ////////////
   // Update //
   ////////////
   lock() { return this.isPathLockable() && (this.#increasePathCounter(), true) }
   unlock() { this.#decreasePathCounter() }
   /////////////
   // Private //
   /////////////
   /** @param {boolean} shared */
   #increaseNodeCounter(shared) {
      if (this.lockCounter && (!shared || !this.shared)) throw VMX.throwError(this, `Lock already locked (name:${this.name}).`)
      if (this.lockCounter >= 100) throw VMX.throwError(this, `Lock already locked 100 times (name:${this.name}).`)
      ++ this.lockCounter
      this.shared = shared
   }
   #decreaseNodeCounter() {
      if (this.lockCounter <= 0) throw VMX.throwError(this, `Lock already unlocked (name:${this.name}).`)
      if (-- this.lockCounter === 0) this.shared = false
   }
   #increasePathCounter() {
      this.#increaseNodeCounter(this.configShared)
      this.parents.every(parent => parent.#increaseNodeCounter(true))
   }
   #decreasePathCounter() {
      this.#decreaseNodeCounter()
      this.parents.every(parent => parent.#decreaseNodeCounter())
   }
}

class ActionLockHolder /*implements ActionLockInterface*/ {
   /** @type {boolean} */
   holdsLock = false
   /** @type {ActionLockState} */
   lockState
   /**
    * @param {ActionLockState} lockState
    */
   constructor(lockState) {
      this.lockState = lockState
   }
   ///////////
   // Query //
   ///////////
   isContextCondition() { return this.lockState.isContextCondition() }
   isLocked() { return this.holdsLock }
   getName() { return this.lockState.name }
   ////////////
   // Update //
   ////////////
   /** @return {boolean} *true* if lock could be acquired or is already acquired. */
   lock() { return (this.holdsLock ||= this.lockState.lock()) }
   /**
    * Removes lock or undos changes of a partially acquired lock in case of an error.
    * After return {@link isLocked} return *false*.
    */
   unlock() { this.holdsLock &&= (this.lockState.unlock(), false) }
}

class ViewExtension {
   /**
    * Frees resources and possibly undo changes.
    * @param {View} view
    * @return {void}
    */
   free(view) { return VMX.throwError(this,`free not implemented.`) }
}

class ViewDecorator {
   static ClassConfig = {
      /** @type {{[name:symbol]:never}} */
      InitOptions: {}
   }
   /**
    * @param {View} view
    * @param {ViewModel} viewModel
    * @param {object} options
    * @param {ViewRestrictions} restrictions
    * @return {undefined|ViewDecorator} Returns created ViewDecorator object or undefined if initOptions prevents creation.
    */
   static init(view, viewModel, options, restrictions) { return VMX.throwError(this,`init not implemented.`) }
   /**
    * Frees resources and possibly undo changes.
    * @param {View} view
    * @return {void}
    */
   free(view) { return VMX.throwError(this,`free not implemented.`) }
   /**
    * @param {View} view
    * @param {{[action:string]:ViewController|null|undefined|false}} controllers
    */
   setActions(view, controllers) {
      for (const [action,controller] of Object.entries(controllers)) {
         if (controller instanceof ViewController) {
            controller.setAction(view.getAction(action))
         }
      }
   }
}

class ViewConfig {
   static Category = VMXConfig.VIEW
   /** @type {{[option:symbol]:never}} ViewConfigInitOptions */
   static InitOptions = {}
   // TODO: not used in transitions (instead ViewElemStylesManager.TransitionStyle is used)
   /** Transition style for resize and change position animation. @type {{"transition-delay":string, "transition-duration":string, "transition-property":string, "transition-timing-function":string }} */
   static TransitionStyle = { "transition-delay":"0s", "transition-duration":"0.2s", "transition-property":"all", "transition-timing-function":"ease-out" }
}

class View { // implements ViewModelObserver
   //////////////////////
   // implement Config //
   //////////////////////
   /** Returns config values which are used for all instances of this class (if not overwritten in instance). @return {typeof ViewConfig} */
   get ClassConfig() { return ViewConfig }
   /** Returns single config value valid for a single instance. @param {string} config @return any */
   getUserConfig(config) { return this.#config?.[config] ?? this.ClassConfig[config] }
   /** Overwrites config for a single instance. @param {string} config @param {any} value */
   setUserConfig(config, value) {
      if (!(config in this.ClassConfig)) return VMX.throwError(this, `Can not set unknown config »${String(config)}«.`)
      const defaultValue = this.ClassConfig[config]
      if (typeof value !== typeof defaultValue || value === null) return VMX.throwError(this, `Expect config »${String(config)}« of type ${typeof defaultValue} instead of ${value === null ? "null" : typeof value}.`)
      this.#config ??= {}
      this.#config[config] = value
      if (typeof value === "object")
         this.#config[config] = Object.assign({},this.#config[config],value)
   }

   //////////////////////////
   // implement LogIDOwner //
   //////////////////////////
   getLogID() { return this.#logid }
   getLogIDParent() { return this.#viewModel }

   /**
    * @template {View} T
    * @this {new(...args:any[])=>T} Either View or a derived subtype.
    * @param {string} htmlString
    * @return {T} A View (or derived type) which wraps the supplied HTML Element created from the provided HTML string.
    */
   static fromHtml(htmlString) {
      return new this(ViewElem.fromHtml(htmlString))
   }
   /**
    * @template T extends View
    * @this {new(...args:any[])=>T} Either View or a derived subtype.
    * @param {string} textString
    * @return {T} A View (or derived type) which wraps the supplied HTML Text node created from the provided text string.
    */
   static fromText(textString) {
      return new this(ViewElem.fromText(textString))
   }
   /**
    * @param {HTMLElement|ViewElem} viewElem
    * @param {{viewModel:ViewModel} & ViewInitOptions} [viewOptions]
    */
   static create(viewElem, { viewModel, ...viewOptions }={}) {
      return new this(viewElem, viewModel).init(viewOptions)
   }

   /** @type {ViewElem} */
   #viewElem
   /** @type {(ViewExtension|ViewDecorator)[]} */
   #extensions
   /** @type {ViewModel} */
   #viewModel
   /** @type {LogID} */
   #logid
   /** @type {undefined|{[config:string]:any}} */
   #config

   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /**
    * Initializes value properties.
    * @param {HTMLElement|ViewElem} viewElem
    * @param {null|undefined|ViewModel} [viewModel]
    */
   constructor(viewElem, viewModel) {
      viewModel ??= new ViewModel()
      if (!(viewModel instanceof ViewModel)) return VMX.throwError(this, BasicTypeChecker.expectNameValueOfType("argument »viewModel«",viewModel,"ViewModel"))
      viewElem = viewElem instanceof HTMLElement ? new ViewElem(viewElem) : viewElem
      this.#viewElem = ViewElem.assertType(viewElem)
      this.#extensions = []
      this.#viewModel = viewModel
      this.#logid = new LogID(this, viewElem.htmlElem)
   }
   /**
    * @typedef {object} ViewInitOptions
    * @typedef {{[property:string]:any}} ViewRestrictions
    */
   /**
    * Connects view (if not already connected) and validates viewOptions.
    * Must be overwriteen in subtype.
    * The init of the subtype should provide default values for the options
    * and initialize the default values of the model if taken from the view.
    * After that it should connect the model to the view and set all view
    * values to the connected model values (if not done already).
    * @param {ViewInitOptions} viewOptions
    * @return {View}
    */
   init(viewOptions) {
      this.validateInitOptions(viewOptions)
      this.connect()
      this.#logid.init(this, this.viewElem.htmlElem)
      if (this.constructor === View)
         this.viewModel.connectView(this,{})
      return this
   }
   /**
    * Frees bound resources. Call does nothing if already freed before.
    * @param {()=>void} [onFree] Callback which is called if free is executed (not ignored).
    */
   free(onFree) {
      if (!this.#logid.isFree()) {
         this.#logid.free()
         onFree?.()
         this.freeExtensions()
         this.#viewElem.disconnect()
         this.#viewModel.free()
      }
   }
   /////////////////
   // Init Helper //
   /////////////////
   /**
    * @param {{[option:string]:any}} options
    */
   validateInitOptions(options) {
      const allowedOptions = this.ClassConfig.InitOptions
      BasicTypeChecker.validateOptions(this, options, allowedOptions)
   }
   ////////////////
   // Properties //
   ////////////////
   // TODO: rename htmlElements -> viewElements with type ViewElem[]
   /** @type {(HTMLElement)[]} */
   get htmlElements() { return [ this.#viewElem.htmlElem ] }
   /** @return {boolean} True in case view is connected to the document. */
   get connected() { return this.#viewElem.connected() }
   /** @return {boolean} */
   get hidden() { return this.#viewElem.hidden }
   /** @param {boolean} hidden *true*, if view should be hidden. *false*, if be shown. */
   set hidden(hidden) { this.#viewElem.hidden = hidden }
   /** @return {ViewElem} */
   get viewElem() { return this.#viewElem }
   /** @return {ViewElem} */
   get elem() { return this.#viewElem }
   /** Returns {@link ViewModel} attached to view. @return {ViewModel} */
   get viewModel() { return this.#viewModel }
   /** Returns {@link ViewModel} attached to view. Should be overwritten in inheriting type. @return {ViewModel} */
   get model() { return this.#viewModel }
   /** Returns {@link ViewModel} attached to view. @return {any} */
   get _unsafe_model() { return this.#viewModel }
   /**
    * @param {string} slot
    * @return {undefined|ViewElem}
    */
   trySlot(slot) { return this.viewElem.trySlot(slot) }
   ////////////
   // update //
   ////////////
   /** Appends view to document if not connected. On return {@link connected} returns true. */
   connect() { this.#viewElem.connect() }
   /** Replaces connected view with other node. On return {@link connected} is false. @param {ViewNode} viewNode */
   replaceWith(viewNode) { this.#viewElem.replaceWith(viewNode) }
   ///////////////////////
   // Extension Support //
   ///////////////////////
   /**
    * @template T
    * @param {T&(ViewExtension|ViewDecorator)} extension
    * @return {T}
    */
   extend(extension) {
      const extensionClass = extension.constructor
      if (typeof extensionClass !== "function")
         return VMX.throwError(this, `extension has no constructor.`)
      if (this.#extensions.some(e => e instanceof extensionClass))
         return VMX.throwError(this, `extension already exists.`)
      return this.#extensions.push(extension), extension
   }
   /**
    * @template {ViewDecorator} T
    * @param {{name:string, init:(view:View,viewModel:ViewModel,initOptions:object,restrictions:ViewRestrictions)=>undefined|T}} decoratorClass
    * @param {ViewModel} viewModel
    * @param {object} initOptions
    * @param {ViewRestrictions} restrictions Decorator could add additional properties which restricts the view (describes the view configuration).
    */
   decorate(decoratorClass, viewModel, initOptions, restrictions) {
      const decorator = decoratorClass.init(this, viewModel, initOptions, restrictions)
      if (decorator) this.extend(decorator)
   }
   freeExtensions() {
      for (let i=this.#extensions.length; (--i)>=0;)
         this.#extensions[i].free(this)
      this.#extensions = []
   }
   /**
    * @template {ViewExtension|ViewDecorator} T
    * @param {new(...args:any[])=>T} extensionClass
    * @return {undefined|T}
    */
   getExtension(extensionClass) {
      for (const extension of this.#extensions)
         if (extension instanceof extensionClass)
            return extension
   }
   /**
    * @template {ViewExtension|ViewDecorator} T
    * @param {new(...args:any[])=>T} extensionClass
    * @return {undefined|T}
    */
   removeExtension(extensionClass) {
      for (let i=this.#extensions.length; (--i)>=0;) {
         const extension = this.#extensions[i]
         if (extension instanceof extensionClass) {
            this.#extensions.splice(i,1)
            return extension
         }
      }
   }

   /////////////
   // Actions //
   /////////////

   /**
    * @param {string} action
    * @param {(args: object)=>object} [mapArgs]
    * @return {ViewModelAction} Bound action which calls the correct view model method (or view method).
    */
   getAction(action, mapArgs) { return this.#viewModel.getVMAction(action,mapArgs) }

   ///////////////////////
   // ViewModelObserver //
   ///////////////////////

   /**
    * Updates view properties with values from view model if they are changed.
    * View properties which are synced by this method must have the same same
    * as the view model property but prefixed with a "v_".
    * @param {VMPropertyUpdateNotification} notification
    * @return {void}
    */
   onVMUpdate(notification) {
      const viewProperty = "v_" + notification.property
      if (!VMX.getPropertyDescriptor(this,viewProperty)?.set)
         return VMX.throwError(this, `Unknown view property ${String(viewProperty)}.`)
      this[viewProperty] = notification.value
   }

   ////////////////////////
   // Transition & Style //
   ////////////////////////

   /**
    * Allows to wait for end of transition.
    * Notification is done either by callback or by waiting for the returned promise.
    * If no transition is running notification is done immediately.
    * @param {()=>void} [onTransitionEnd] Callback to signal end of transition (all computed view attributes has reached their final value).
    * @return {undefined|Promise<null>} Promise if onTransitionEnd is undefined else undefined.
    */
   onceOnTransitionEnd(onTransitionEnd) { return this.viewElem.onceOnTransitionEnd(onTransitionEnd) }

   /**
    * @param {ViewStyles} newStyles
    * @return {object} Old values of set style values.
    */
   setStyles(newStyles) { return this.#viewElem.setStyles(newStyles) }

}

class ReframeViewDecorator extends ViewDecorator {
   /**
    * @typedef ReframeViewDecorator_Imports
    * @property {ViewElem} viewElem
    * @property {undefined|ViewText|ViewElem} content
    *
    * @typedef ReframeViewDecoratorInitOptions
    * @property {BasicTypeChecker} content The ViewElem which is connected to the document. Its position within the document is replaced by the window frame.
    * @property {BasicTypeChecker} reframe True activates reframing extension.
    */
   static ClassConfig = {
      /** @type {ReframeViewDecoratorInitOptions} */
      InitOptions: {
         content: new BasicTypeChecker( (value,typeCheckerContext) =>
            Boolean(typeCheckerContext?.reframe) &&
            (  (value === undefined) && BasicTypeChecker.requiredValue()
            || !(value instanceof ViewElem) && BasicTypeChecker.expectValueOfType(value, "ViewElem")
            || !(value.connected()) && BasicTypeChecker.expectSomething("to be a ViewElem connected to the document")
            )).makeRequired(),
         reframe: BasicTypeChecker.booleanType
      },
   }
   /** @type {undefined|object} */
   oldContentStyles
   /**
    * @param {View&ReframeViewDecorator_Imports} view
    * @param {ViewModel} viewModel
    * @param {{reframe:boolean, content:ViewElem}} options
    * @return {undefined|ReframeViewDecorator}
    */
   static init(view, viewModel, { reframe, content }) {
      if (reframe) {
         return new ReframeViewDecorator(view, content)
      }
   }
   /**
    * @param {ReframeViewDecorator_Imports} view
    * @param {ViewElem} content
    */
   constructor(view, content) {
      super()
      const pos = content.computedPos()
      this.oldContentStyles = content.setStyles({ width:"100%", height:"100%", position:"static" })
      view.viewElem.setStyles({ position: pos.position })
      content.replaceWith(view.viewElem)
      view.viewElem.setPos({ left:pos.left, top:pos.top, width:pos.width, height:pos.height })
   }
   /**
    * @param {View&ReframeViewDecorator_Imports} view
    */
   free(view) {
      if (this.oldContentStyles) {
         this.oldContentStyles = undefined
         // default is to not unframe in case of close window
      }
   }
   /**
    * @param {View&ReframeViewDecorator_Imports} view
    * @return {undefined|ViewElem} *ViewElem* in case of success else content was not reframed.
    */
   unframeContent(view) {
      if (this.oldContentStyles) {
         const content = view.content
         if (content instanceof ViewElem) {
            view.viewElem.replaceWith(content)
            content.updateStyles(this.oldContentStyles)
            this.oldContentStyles = undefined
            return content
         }
      }
   }
}

class MoveResizeViewDecorator extends ViewDecorator {
   /**
    * @typedef MoveResizeViewDecorator_CalledActions
    * @property {(e:MoveControllerEvent)=>void} move moveable set to true.
    * @property {(e:TouchResizeControllerEvent)=>void} resize resizeable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeTop resizeable and moveable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeTopRight resizeable and moveable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeRight resizeable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeBottomRight resizeable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeBottom resizeable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeBottomLeft resizeable and moveable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeLeft resizeable and moveable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeTopLeft resizeable and moveable set to true.
    *
    * @typedef MoveResizeViewDecoratorInitOptions
    * @property {BasicTypeChecker} moveable *true*, if user is allowed to to move view.
    * @property {BasicTypeChecker} resizeable *true*, if user is allowed to resize view.
    */
   static ClassConfig = {
      /** @type {MoveResizeViewDecoratorInitOptions} */
      InitOptions: {
         moveable: BasicTypeChecker.booleanType,
         resizeable: BasicTypeChecker.booleanType,
      },
   }
   /**
    * @param {View} view
    * @param {ViewModel} viewModel
    * @param {{moveable?:boolean, resizeable?:boolean, [o:string]:any}} options
    * @param {ViewRestrictions} restrictions
    * @return {undefined|MoveResizeViewDecorator}
    */
   static init(view, viewModel, {moveable, resizeable}, restrictions) {
      restrictions.moveable = moveable = Boolean(moveable) && view.viewElem.moveable()
      restrictions.resizeable = resizeable = Boolean(resizeable)
      if (moveable || resizeable) {
         return new MoveResizeViewDecorator(view, viewModel, moveable, resizeable)
      }
   }
   /** @type {(false|ViewElem)[]} */
   borders
   /** @type {ViewElem} */
   moveElem
   /** @type {null|{[name:string]:false|ViewController}} */
   controllers
   /** @type {null|{[name:string]:string|number}} */
   oldStyles
   /**
    * @param {View} view
    * @param {ViewModel} viewModel
    * @param {boolean} moveable
    * @param {boolean} resizeable
    */
   constructor(view, viewModel, moveable, resizeable) {
      super()
      const viewElem=view.viewElem
      const [borderpx, edgepx, outerpx] = ["15px","20px","-10px"]
      const both = resizeable && moveable
      this.borders = [
         both && ViewElem.fromDiv({top:outerpx, left:0, right:0, height:borderpx, cursor:"n-resize", position:"absolute"}),
         resizeable && ViewElem.fromDiv({right:outerpx, top:0, bottom:0, width:borderpx, cursor:"e-resize", position:"absolute"}),
         resizeable && ViewElem.fromDiv({bottom:outerpx, left:0, right:0, height:borderpx, cursor:"s-resize", position:"absolute"}),
         both && ViewElem.fromDiv({left:outerpx, top:0, bottom:0, width:borderpx, cursor:"w-resize", position:"absolute"}),
         both && ViewElem.fromDiv({top:outerpx, right:outerpx, width:edgepx, height:edgepx, cursor:"ne-resize", position:"absolute"}),
         resizeable && ViewElem.fromDiv({bottom:outerpx, right:outerpx, width:edgepx, height:edgepx, cursor:"se-resize", position:"absolute"}),
         both && ViewElem.fromDiv({bottom:outerpx, left:outerpx, width:edgepx, height:edgepx, cursor:"sw-resize", position:"absolute"}),
         both && ViewElem.fromDiv({top:outerpx, left:outerpx, width:edgepx, height:edgepx, cursor:"nw-resize", position:"absolute"}),
      ]
      viewElem.appendChildren(...this.borders.filter(b => b!==false))
      this.moveElem = view.trySlot(WindowView.Slots.move) ?? view.viewElem
      this.controllers = {
         move: moveable && new MoveController(this.moveElem.htmlElem,{view,viewModel}),
         resize: resizeable && new TouchResizeController(viewElem.htmlElem,{view,viewModel}),
         resizeTop: this.borders[0] && new MoveController(this.borders[0].htmlElem,{view,viewModel}),
         resizeRight: this.borders[1] && new MoveController(this.borders[1].htmlElem,{view,viewModel}),
         resizeBottom: this.borders[2] && new MoveController(this.borders[2].htmlElem,{view,viewModel}),
         resizeLeft: this.borders[3] && new MoveController(this.borders[3].htmlElem,{view,viewModel}),
         resizeTopRight: this.borders[4] && new MoveController(this.borders[4].htmlElem,{view,viewModel}),
         resizeBottomRight: this.borders[5] && new MoveController(this.borders[5].htmlElem,{view,viewModel}),
         resizeBottomLeft: this.borders[6] && new MoveController(this.borders[6].htmlElem,{view,viewModel}),
         resizeTopLeft: this.borders[7] && new MoveController(this.borders[7].htmlElem,{view,viewModel}),
      }
      this.setActions(view, this.controllers)
      this.oldStyles = null
      this.#setMoveable(moveable)
   }
   /**
    * @param {View} view
    */
   free(view) {
      if (this.controllers) {
         this.enable(false)
         for (const border of this.borders)
            if (border) border.remove()
         for (const controller of Object.values(this.controllers))
            if (controller) controller.free()
         this.controllers = null
      }
   }
   /**
    * @param {boolean} enabled
    */
   enable(enabled) {
      this.#setMoveable(enabled)
      this.#setResizeable(enabled)
   }
   /**
    * Changes cursor style of decorated element to "move" or restores old style.
    * @param {boolean} isMoveable
    */
   #setMoveable(isMoveable) {
      if (this.controllers?.move && (this.oldStyles !== null) !== isMoveable) {
         if (this.oldStyles) {
            this.moveElem.updateStyles(this.oldStyles)
            this.oldStyles=null
            this.controllers.move.muted = true
         }
         else {
            this.oldStyles = this.moveElem.setStyles({cursor:"move"})
            this.controllers.move.muted = false
         }
      }
   }
   /**
    * @param {boolean} isResizeable *true*, if window frame should support resizing else feature is turned off
    */
   #setResizeable(isResizeable) {
      if (this.controllers?.resize && this.controllers.resize.muted === isResizeable) {
         if (isResizeable) {
            for (const border of this.borders)
               if (border) border.show()
         }
         else {
            for (const border of this.borders)
               if (border) border.hide()
         }
         this.controllers.resize.muted = !isResizeable
      }
   }
}

class WindowControlsDecorator extends ViewDecorator {
   /**
    * @typedef WindowControlsDecorator_CalledActions
    * @property {(e:TouchControllerEvent&{type:TouchController.CLICK})=>void} close
    * @property {(e:TouchControllerEvent&{type:TouchController.CLICK})=>void} minimize
    * @property {(e:TouchControllerEvent&{type:TouchController.CLICK})=>void} toggleMaximize
    *
    * @typedef WindowControlsDecoratorInitOptions
    * @property {BasicTypeChecker} controls *true*, if view support window controls.
    */
   static ClassConfig = {
      /** @type {WindowControlsDecoratorInitOptions}} */
      InitOptions: {
         controls: BasicTypeChecker.booleanType
      },
   }
   /**
    * @param {View} view
    * @param {ViewModel} viewModel
    * @param {{controls?:boolean, [o:string]:any}} options
    * @param {ViewRestrictions} restrictions
    * @return {undefined|WindowControlsDecorator}
    */
   static init(view, viewModel, {controls}, restrictions) {
      if (controls) {
         const [ closeSlot, minSlot, maxSlot ] = [ view.trySlot(WindowView.Slots.close), view.trySlot(WindowView.Slots.min), view.trySlot(WindowView.Slots.max) ]
         if (closeSlot || minSlot || maxSlot)
            return new WindowControlsDecorator(view,viewModel,closeSlot,minSlot,maxSlot)
      }
   }
   /** @type {null|{[name:string]:undefined|ClickController}} */
   controllers
   /**
    * @param {View} view
    * @param {ViewModel} viewModel
    * @param {undefined|ViewElem} closeSlot
    * @param {undefined|ViewElem} minSlot
    * @param {undefined|ViewElem} maxSlot
    */
   constructor(view, viewModel, closeSlot, minSlot, maxSlot) {
      super()
      this.controllers = {
         close: closeSlot && new ClickController(closeSlot.htmlElem, {view,viewModel}),
         minimize: minSlot && new ClickController(minSlot.htmlElem, {view,viewModel}),
         toggleMaximize: maxSlot && new ClickController(maxSlot.htmlElem, {view,viewModel}),
      }
      this.setActions(view, this.controllers)
   }
   /**
    * @param {View} view
    */
   free(view) {
      if (this.controllers) {
         for (const controller of Object.values(this.controllers))
            if (controller) controller.free()
         this.controllers = null
      }
   }
}

class WindowViewConfig extends ViewConfig {
   /**
    * Checks that content option is of correct type.
    * @param {any} value
    * @return {false|((name:string)=>string)}
    */
   static isContentType(value) { return !(value instanceof ViewElem || value instanceof ViewText) && BasicTypeChecker.expectValueOfType(value, "ViewElem|ViewText") }

   /**
    * @typedef WindowViewConfigInitOptions
    * @property {BasicTypeChecker} content The ViewElem or ViewText which is set as child of the content slot.
    * @property {BasicTypeChecker} controls *true*, if view support window controls.
    * @property {BasicTypeChecker} moveable *true*, if user is allowed to to move view.
    * @property {BasicTypeChecker} reframe *true*, if content should be replaced by window frame. content must be connected to the document and of type ViewElem.
    * @property {BasicTypeChecker} resizeable *true*, if user is allowed to resize view.
    * @property {BasicTypeChecker} top Top distance (0: top, > 0 going down) of view in (css) pixel. Real on screen position depends on position style.
    * @property {BasicTypeChecker} left Left distance (0: left, > 0 going right) of view in (css) pixel. Real on screen position depends on position style.
    * @property {BasicTypeChecker} width Width of view in (css) pixel.
    * @property {BasicTypeChecker} height Height of view in (css) pixel.
    * @property {BasicTypeChecker} hide Set value to true if window should be hidden initially.
    * @property {BasicTypeChecker} show Set value to true if window should be shown initially.
    * @property {BasicTypeChecker} title Text string which is shown in the title bar of the window.
    */

   /** @type {WindowViewConfigInitOptions}. */
   static InitOptions = {...ViewConfig.InitOptions,
      ...ReframeViewDecorator.ClassConfig.InitOptions,
      ...MoveResizeViewDecorator.ClassConfig.InitOptions,
      ...WindowControlsDecorator.ClassConfig.InitOptions,
      content: new BasicTypeChecker( (value,typeCheckerContext) =>
         ReframeViewDecorator.ClassConfig.InitOptions.content.validate(value,typeCheckerContext)
         || (value !== undefined) && !(value instanceof ViewElem || value instanceof ViewText) && BasicTypeChecker.expectValueOfType(value, "ViewElem|ViewText")
         ).makeRequired(ReframeViewDecorator.ClassConfig.InitOptions.content.required),
      top: BasicTypeChecker.numberType,
      left: BasicTypeChecker.numberType,
      width: BasicTypeChecker.numberType,
      height: BasicTypeChecker.numberType,
      hide: BasicTypeChecker.booleanType,
      show: BasicTypeChecker.booleanType,
      title: BasicTypeChecker.stringType,
   }

   /** Class to mark window frame as maximized. */
   static MaximizedClass = "maximized"
   /** Class to mark window frame as minimized. */
   static MinimizedClass = "minimized"
   /** Styles to resize window into maximized state. */
   static MaximizedStyle = { width:"100%", height:"100%", top:"0px", left:"0px" }
   /** Styles to resize window into minimized state. */
   static MinimizedStyle = { width:"32px", height:"16px", top:"unset", bottom:"32px", left:"5px" }
}

class WindowView extends View {
   //////////////////////
   // implement Config //
   //////////////////////
   /** @return {typeof WindowViewConfig} */
   get ClassConfig() { return WindowViewConfig }
   /** @template {keyof typeof WindowViewConfig} K @param {K} config @return {typeof WindowViewConfig[K]} */
   getUserConfig(config) { return super.getUserConfig(config) }

   /**
    * Optional slot names which could be exported in the window frame by "data-slot=<name>".
    * @type {{content:"content", title:"title", close:"close", min:"min", max:"max", move:"move"}}
    */
   static Slots = { content:"content", title:"title", close:"close", min:"min", max:"max", move:"move" }

   /**
    * WindowView_CalledActions enumerates all called actions.
    * @typedef {MoveResizeViewDecorator_CalledActions & WindowControlsDecorator_CalledActions} WindowView_CalledActions
    */

   ////////////////////
   // Overwrite View //
   ////////////////////
   /**
    * @param {ViewElem} viewElem
    * @param {null|undefined|WindowVM} [viewModel]
    */
   constructor(viewElem, viewModel) {
      super(viewElem, viewModel ?? new WindowVM())
      if (!(this.viewModel instanceof WindowVM)) return VMX.throwError(this, BasicTypeChecker.expectNameValueOfType("argument »viewModel«",viewModel,"WindowVM"))
   }
   /**
    * @typedef WindowViewInitOptions
    * @property {ViewElem|ViewText} [content] ViewElem which is set as child of the content slot.
    * @property {boolean} [reframe] *true*, window frame replaces content within the document. The content must be if type ViewElem and connected to the document.
    * @property {boolean} [moveable] *true*, window frame can be moved if ViewElem supports it (HTML position is "absolute","fixed",or "relative").
    * @property {boolean} [resizeable] *true*, window frame can be resized.
    * @property {boolean} [controls] *true*, if view support window controls.
    * @property {number} [top] Distance from top of screen in css pixel.
    * @property {number} [left] Distance from left of screen in css pixel.
    * @property {number} [height] Height of view in css pixel.
    * @property {number} [width] Width of view in css pixel.
    * @property {boolean} [hide] True if view is hidden initially.
    * @property {boolean} [show] True if view is shown initially.
    * @property {string} [title] Title (text string/no html) shown in window title bar.
    */
   /**
    * @see {@link View.init}
    * @param {WindowViewInitOptions} viewOptions
    * @return {WindowView}
    */
   init(viewOptions) {
      const model = this.model, restrictions = {}
      viewOptions = this.completeOptions(viewOptions)
      super.init(viewOptions)
      this.decorate(ReframeViewDecorator, model, viewOptions, restrictions)
      this.decorate(MoveResizeViewDecorator, model, viewOptions, restrictions)
      this.decorate(WindowControlsDecorator, model, viewOptions, restrictions)
      restrictions.hasTitle = this.trySlot(WindowView.Slots.title) !== undefined
      if (viewOptions.content)
         this.content = viewOptions.content
      // position and title is initially taken from view if not set in options
      const pos = this.viewElem.computedPos()
      if (viewOptions.top !== undefined) pos.top = viewOptions.top
      if (viewOptions.left !== undefined) pos.left = viewOptions.left
      if (viewOptions.height !== undefined) pos.height = viewOptions.height
      if (viewOptions.width !== undefined) pos.width = viewOptions.width
      model.pos = pos
      if (viewOptions.hide)
         model.windowState = WindowVM.HIDDEN
      if (viewOptions.show)
         model.windowState = WindowVM.SHOWN
      if (typeof viewOptions.title === "string")
         model.title = viewOptions.title
      else if (restrictions.hasTitle && !model.title && this.v_title)
         model.title = this.v_title
      if (this.constructor === WindowView)
         model.connectView(this,restrictions)
      // == initial update ==
      this.v_title = model.title
      this.v_pos = model.pos
      this.v_windowState = model.windowState
      return this
   }
   /** Returns the {@link WindowVM} attached to view. @return {WindowVM} */
   get model() { return this._unsafe_model }
   /////////////////
   // Init Helper //
   /////////////////
   /**
    * @param {WindowViewInitOptions} [options]
    * @return {WindowViewInitOptions}
    */
   completeOptions(options) {
      const o = { ...options }
      if (o.content instanceof HTMLElement) o.content = new ViewElem(o.content)
      if (o.controls === undefined) o.controls = true
      if (o.moveable === undefined) o.moveable = true
      if (o.reframe === undefined && o.content instanceof ViewElem && o.content.connected()) o.reframe = true
      if (o.resizeable === undefined) o.resizeable = true
      if (o.moveable && o.reframe && o.content instanceof ViewElem && !o.content.moveable()) o.moveable = false
      return o
   }
   /////////////
   // Actions //
   /////////////
   /**
    * This method succeeds only if content was reframed (option.reframe=true) by this window.
    * In case of failure nothing is done. In case of success content
    * is removed from window frame and replaces frame in the document
    * and the window is freed.
    * @return {ViewElem|undefined} *ViewElem* in case of success.
    */
   unframeContent() { const content = this.getExtension(ReframeViewDecorator)?.unframeContent(this); return content && this.free(), content }
   ////////////////
   // Properties //
   ////////////////
   /** @param {true} closed *true* if windowState will be set to WindowVM.CLOSED. */
   set v_closed(closed) { if (closed) VMX.extpoint.closeWindowView(this) }
   /** @type {{content:"content", title:"title", close:"close", min:"min", max:"max", move:"move"}} */
   get slots() { return WindowView.Slots }
   /**
    * @return {undefined|ViewText|ViewElem}
    */
   get content() { return this.viewElem.slot(this.slots.content).child() }
   /**
    * @param {undefined|ViewText|ViewElem} view
    */
   set content(view) { this.viewElem.setSlot(this.slots.content, view) }
   /**
    * @return {WindowVMPos}
    */
   get v_pos() { return this.viewElem.pos() }
   /**
    * @param {WindowVMPosArgs} pos
    */
   set v_pos(pos) { this.viewElem.setPos(pos) }
   /** @return {string} */
   get v_title() { return this.model.hasTitle ? this.viewElem.slot(this.slots.title).text() : "" }
   /** @param {string} title */
   set v_title(title) { this.model.hasTitle && this.viewElem.setSlot(this.slots.title, ViewText.fromText(title)) }
   /**
    * @param {WindowVMState} state
    */
   set v_windowState(state) {
      const withTransition = !this.model.lastWindowStateHidden
      const pc = new ViewElemPositionControl(this.viewElem, withTransition)
      this.hidden = (state === WindowVM.HIDDEN)
      this.getExtension(MoveResizeViewDecorator)?.enable(state !== WindowVM.MAXIMIZED)
      switch(state) {
         case WindowVM.MAXIMIZED:
            pc.changePos(this.getUserConfig("MaximizedStyle"),this.getUserConfig("MaximizedClass"))
            break
         case WindowVM.MINIMIZED:
            pc.changePos(this.getUserConfig("MinimizedStyle"),this.getUserConfig("MinimizedClass"),() => this.model.windowState === WindowVM.MINIMIZED && (this.hidden=true))
            break
         case WindowVM.CLOSED:
            pc.restore()
            this.free()
            break
         case WindowVM.HIDDEN:
         case WindowVM.SHOWN:
            pc.restore()
            break
      }
   }
}

class ViewModelObserver {
   /**
    * @typedef VMPropertyUpdateNotification
    * @property {"VMPropertyUpdate"} type
    * @property {ViewModel} from
    * @property {string} property
    * @property {any} value
    *
    * @typedef {(notification:VMPropertyUpdateNotification)=>void} VMPropertyUpdateFunction
    */

   /**
    * Creates an update notification message for an ViewModel property value.
    * @param {ViewModel} viewModel ViewModel whose property has changed.
    * @param {string} property Name of property
    * @param {any} value New value of property.
    * @return {VMPropertyUpdateNotification}
    */
   static VMPropertyUpdateNotification(viewModel,property,value) {
      return { type:"VMPropertyUpdate", from:viewModel, property, value}
   }

   /**
    * @template T
    * @param {T} owner Object which owns the method.
    * @param {(this:T,arg:object)=>void} method Method which is called whenever a ViewModel property changes.
    */
   static createProxy(owner, method) { return new this.Proxy(owner,method) }

   static Proxy = class ViewModelObserverProxy { // implements ViewModelObserver
      /**
       * @param {object} owner Object which owns the method.
       * @param {(this:object,arg:VMPropertyUpdateNotification)=>void} method Method which is called whenever a ViewModel property changes.
       */
      constructor(owner, method) {
         this.owner = owner
         this.method = method
      }

      /** @param {object} o */
      equals(o) { return o instanceof ViewModelObserver.Proxy && o.owner == this.owner && o.method == this.method }

      /** @type {VMPropertyUpdateFunction} */
      onVMUpdate(notification) { this.method.call(this.owner,notification) }
   }

   /** @type {VMPropertyUpdateFunction} */
   onVMUpdate(notification) { return VMX.throwError(this, `onVMUpdate not implemented.`) }
}

// XXXXX

class ViewModelObserverHub {
   /**
    * @typedef {(VMPropertyUpdateFunction|ViewModelObserver)[]} ViewModelObservers
    * @typedef {Map<string,ViewModelObservers>} PropertyObservers
    * @typedef {typeof ViewModel|ViewModel} ViewModelObservers_VMType
    * @typedef {Map<ViewModelObservers_VMType,PropertyObservers>} VMTypeObservers
    * @type {VMTypeObservers}
    */
   modelObservers
   /** @type {(()=>void)[]|undefined} */
   onUpdateEndCallbacks

   constructor() {
      this.modelObservers = new Map()
      this.onUpdateEndCallbacks = undefined
   }

   /** @return {(typeof ViewModel|ViewModel)[]} */
   get vmtypes() { return [...this.modelObservers.keys()] }
   /** @return {Set<String>} */
   get properties() {
      const result = new Set()
      this.modelObservers.forEach( (propertyObservers) => propertyObservers.forEach( (_v,property) => result.add(property)))
      return result
   }
   /**
    * @return {{vmtype:typeof ViewModel|ViewModel, property:string, observers:ViewModelObservers}[]}
    */
   get observers() {
      const result = []
      for (const [ vmtype, propertyObservers ] of this.modelObservers) {
         for (const [ property, observers ] of propertyObservers) {
            result.push({vmtype, property, observers:[...observers]})
         }
      }
      return result
   }

   /**
    * @param {typeof ViewModel|ViewModel} vmtype
    * @return {{}|{[property:string]:ViewModelObservers}}
    */
   observersByVM(vmtype) {
      const result = {}
      const propertyObservers = this.modelObservers.get(vmtype)
      if (propertyObservers) {
         for (const [ property, observers ] of propertyObservers) {
            result[property] = [...observers]
         }
      }
      return result
   }

   /**
    * @param {string} property
    * @return {{vmtype: typeof ViewModel|ViewModel, observers:(VMPropertyUpdateFunction|ViewModelObserver)[]}[]}
    */
   observersByProperty(property) {
      const result = []
      for (const [ vmtype, propertyObservers ] of this.modelObservers) {
         const observers = propertyObservers.get(property)
         if (observers) {
            result.push({vmtype, observers:[...observers]})
         }
      }
      return result
   }

   /** @param {typeof ViewModel|ViewModel} vmtype */
   vmtypeName(vmtype) { return typeof vmtype === "function" ? "class "+vmtype.name : "object "+vmtype.getLogID().name(vmtype) }

   /**
    * @param {typeof ViewModel|ViewModel} vmtype
    * @return {PropertyObservers}
    */
   registerType(vmtype) {
      const propertyObservers = this.modelObservers.get(vmtype)
      if (propertyObservers) return propertyObservers
      const emptyObservers = new Map()
      this.modelObservers.set(vmtype,emptyObservers)
      return emptyObservers
   }

   /**
    * Registers an observer which is notified for change of a property value.
    * @param {typeof ViewModel|ViewModel} vmtype
    * @param {string} property
    * @param {VMPropertyUpdateFunction|ViewModelObserver} observer
    */
   addPropertyObserver(vmtype, property, observer) {
      const isFunction = typeof observer === "function"
      if (!(isFunction || typeof observer === "object" && typeof observer?.onVMUpdate === "function"))
         return VMX.throwError(this, `Expect argument »observer« of type function or ViewModelObserver instead of ${VMX.typeof(observer)}.`)
      const isProxy = !isFunction && observer instanceof ViewModelObserver.Proxy
      const propertyObservers = this.registerType(vmtype)
      const observers = propertyObservers.get(property) ?? []
      if (observers.length === 0) propertyObservers.set(property,observers)
      if (observers.findIndex(o => o === observer || (isProxy && observer.equals(o))) === -1) {
         observers.push(observer)
      }
   }

   /**
    * @param {typeof ViewModel|ViewModel} vmtype
    * @param {string} property
    * @param {VMPropertyUpdateFunction|ViewModelObserver} observer
    */
   removePropertyObserver(vmtype, property, observer) {
      const propertyObservers = this.modelObservers.get(vmtype)
      if (!propertyObservers) return
      const isProxy = observer instanceof ViewModelObserver.Proxy
      const observers = propertyObservers.get(property)
      if (!observers) return
      const index = observers.findIndex(o => o === observer || (isProxy && observer.equals(o)))
      if (index === -1) return
      if (observers.length === 1)
         propertyObservers.delete(property)
      else
         observers.splice(index,1)
   }

   /** @param {()=>void} callback*/
   onceOnUpdateEnd(callback) {
      if (typeof callback === "function") this.onUpdateEndCallbacks?.push(callback)
   }

   ///////////////////////
   // ViewModelObserver //
   ///////////////////////

   /**
    * @param {VMPropertyUpdateNotification} notification
    * @return {void}
    */
   onVMUpdate(notification) {
      const oldonUpdateEndCallbacks = this.onUpdateEndCallbacks
      this.onUpdateEndCallbacks = []
      this.#sendNotification(notification,notification.from)
      this.#sendNotification(notification,notification.from.constructor)
      this.onUpdateEndCallbacks.forEach(cb=>cb())
      this.onUpdateEndCallbacks = oldonUpdateEndCallbacks
   }
   /**
    * @param {VMPropertyUpdateNotification} notification
    * @param {Function|ViewModel} vmtype
    * @return {void}
    */
   #sendNotification(notification, vmtype) {
      /** @type {any} remove warning from typescript compiler */
      const _vmtype = vmtype
      const observers = this.modelObservers.get(_vmtype)?.get(notification.property)
      if (observers) {
         for (const observer of [...observers]) {
            (typeof observer === "function") ? observer(notification) : observer.onVMUpdate(notification)
         }
      }
   }
}

/**
 * Basic view model configuration.
 * Should be copied in subtype and extended or overwritten accordingly.
 */
class ViewModelConfig {
   /** @type {{[option:symbol]:never}} */
   static ConnectOptions = {}
   /**
    * @typedef ViewModelConfigLock
    * @property {string[]} parents
    * @property {boolean} [shared]
    * @property {string} [contextUnlockedNode]
    */
   /** @type {{[name:string]:ViewModelConfigLock}} */
   static Locks = {}
}

class ViewModel {
   //////////////////////
   // implement Config //
   //////////////////////
   /** Returns config values which are used for all instances of this class. @return {typeof ViewModelConfig} */
   get ClassConfig() { return ViewModelConfig }
   //////////////////////////
   // implement LogIDOwner //
   //////////////////////////
   getLogID() { return this.#logid }
   getLogIDParent() { return undefined }

   /**
    * @param {any} viewModel
    * @return {ViewModel}
    */
   static assertType(viewModel) {
      if (viewModel instanceof ViewModel)
         return viewModel
      return VMX.throwError(this, `Expect type ViewModel and not »${VMX.typeof(viewModel)}«.`)
   }

   /** @type {undefined|View} */
   #view
   /** @type {LogID} */
   #logid
   /** @type {{[lock:string]:ActionLockState}} */
   #lockStates


   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /**
    * Initializes value properties.
    */
   constructor() {
      this.#logid = new LogID(this)
      this.#lockStates = {}
   }
   /**
    * @param {View} view
    * @param {ViewRestrictions} connectOptions
    * @return {ViewModel}
    */
   connectView(view, connectOptions) {
      if (this.#view)
         return VMX.throwError(this,`connectView called twice.`)
      this.#view = view
      this.#logid.init(this, view.htmlElements)
      this.validateConnectOptions(connectOptions)
      return this
   }
   /**
    * Frees resources. Call is ignored if already freed before.
    * @param {()=>void} [onFree] Callback which is called if free is executed (not ignored).
    */
   free(onFree) {
      if (!this.#logid.isFree()) {
         this.#logid.free()
         onFree?.()
         this.#view?.free()
         this.#view = undefined
      }
   }
   // overwrite also:
   // --- getVMAction

   /////////////////
   // Init Helper //
   /////////////////
   /**
    * @param {{[option:string]:any}} options
    */
   validateConnectOptions(options) {
      const allowedOptions = this.ClassConfig.ConnectOptions
      BasicTypeChecker.validateOptions(this, options, allowedOptions)
   }
   ////////////////
   // Properties //
   ////////////////
   /** @return {View} */
   get view() { return this.#view || VMX.throwError(this,`view only valid after call to connectView.`) }

   ////////////////////
   // Action Support //
   ////////////////////
   /**
    * @param {string} action
    * @param {(args: object)=>object} [mapArgs]
    * @return {ViewModelAction} Bound action which calls the correct view model method.
    */
   getVMAction(action, mapArgs) { return VMX.throwError(this, `Unknown action ${String(action)}.`) }
   /** @param {string} name @return {ActionLockState} */
   getVMActionLockState(name) {
      return (this.#lockStates[name] ??= (() => {
         const config = this.ClassConfig.Locks[name]
         if (!config) return VMX.throwError(this, `Unknown lock configuration ${name}.`)
         const parents = config.parents.map(parent => this.getVMActionLockState(parent))
         const lockState = new ActionLockState(name, Boolean(config.shared), ...parents)
         if (config.contextUnlockedNode)
            lockState.addContextUnlockedNodes(this.getVMActionLockState(config.contextUnlockedNode))
         return lockState
      })())
   }
   /** @param {string} name @return {ActionLockHolder} */
   getVMActionLock(name) { return new ActionLockHolder(this.getVMActionLockState(name)) }

   ///////////////////
   // Notifications //
   ///////////////////
   /**
    * @param {string} property
    * @param {any} value
    */
   notifyPropertyUpdate(property, value) {
      const notification = ViewModelObserver.VMPropertyUpdateNotification(this,property,value)
      VMX.vmObserver.onVMUpdate(notification)
      this.#view?.onVMUpdate(notification)
   }
}

class WindowVMConfig extends ViewModelConfig {
   /**
    * @typedef WindowVMConnectOptions
    * @property {BasicTypeChecker} moveable Could the window be moved.
    * @property {BasicTypeChecker} resizeable Could the window be resized.
    * @property {BasicTypeChecker} hasTitle Could title be set (and displayed in view).
    */
   /** @type {WindowVMConnectOptions} */
   static ConnectOptions = {...ViewModelConfig.ConnectOptions,
      moveable: BasicTypeChecker.booleanType,
      resizeable: BasicTypeChecker.booleanType,
      hasTitle: BasicTypeChecker.booleanType
   }
   static Locks = {...ViewModelConfig.Locks,
      click: { parents:[], shared:true },
      move: { parents:[], contextUnlockedNode: "click" },
      top: { parents:["move"] },
      topRight: { parents:["top","right"] },
      right: { parents:["move"] },
      bottomRight: { parents:["bottom","right"] },
      bottom: { parents:["move"] },
      bottomLeft: { parents:["bottom","left"] },
      left: { parents:["move"] },
      topLeft: { parents:["top","left"] },
   }
}

class WindowVM extends ViewModel {
   //////////////////////
   // implement Config //
   //////////////////////
   /** @return {typeof WindowVMConfig} */
   get ClassConfig() { return WindowVMConfig }

   /**
    * @typedef WindowVMPos
    * @property {number} width
    * @property {number} height
    * @property {number} top
    * @property {number} left
    */
   /**
    * @typedef WindowVMPosArgs
    * @property {number|undefined} [width]
    * @property {number|undefined} [height]
    * @property {number|undefined} [top]
    * @property {number|undefined} [left]
    * @property {boolean|undefined} [transition]
    */
   /**
    * @typedef {"closed"|"hidden"|"maximized"|"minimized"|"shown"} WindowVMState
    */
   /** @type {"closed"} */
   static CLOSED = "closed"
   /** @type {"hidden"} */
   static HIDDEN = "hidden"
   /** @type {"maximized"} */
   static MAXIMIZED = "maximized"
   /** @type {"minimized"} */
   static MINIMIZED = "minimized"
   /** @type {"shown"} */
   static SHOWN = "shown"
   /**
    * @param {any} viewModel
    * @return {WindowVM}
    */
   static assertType(viewModel) {
      if (viewModel instanceof WindowVM)
         return viewModel
      return VMX.throwError(this, `Expect type WindowVM and not »${VMX.typeof(viewModel)}«.`)
   }

   /** @type {WindowVMState} */
   #windowState = WindowVM.SHOWN
   /** @type {WindowVMPos} The current position of the window frame (no matter which state). */
   #pos = { width:0, height:0, top:0, left:0 }
   /** @type {number} */
   #flags = 1/*MOVEABLE*/ | 2/*RESIZEABLE*/ | 4/*HASTITLE*/ // | 8/*LASTWINDOWSTATEHIDDEN*/
   /** @type {string} */
   #title = ""

   /////////////////////////
   // Overwrite ViewModel //
   /////////////////////////

   /**
    * @param {View} view
    * @param {ViewRestrictions} connectOptions
    * @return {ViewModel}
    */
   connectView(view, connectOptions) {
      super.connectView(view,connectOptions)
      this.#setMoveable(connectOptions.moveable)
      this.#setResizeable(connectOptions.resizeable)
      this.#setHasTitle(typeof connectOptions.hasTitle !== "boolean" || connectOptions.hasTitle)
      return this
   }

   //////////////
   // WindowVM //
   //////////////

   //// Restrictions ////

   /** @return {boolean} *true*, if changing left/top is supported. */
   get moveable() { return Boolean(this.#flags&1) }
   /** @param {boolean} flag */
   #setMoveable(flag) { this.#flags = flag ? this.#flags|1 : this.#flags&(~1) }

   /** @return {boolean} *true*, if changing width/height is supported. */
   get resizeable() { return Boolean(this.#flags&2) }
   /** @param {boolean} flag */
   #setResizeable(flag) { this.#flags = flag ? this.#flags|2 : this.#flags&(~2) }

   /** @return {boolean} *true*, if setting title is supported. */
   get hasTitle() { return Boolean(this.#flags&4) }
   /** @param {boolean} flag */
   #setHasTitle(flag) { this.#flags = flag ? this.#flags|4 : this.#flags&(~4) }

   /** @return {boolean} flag */
   get lastWindowStateHidden() { return Boolean(this.#flags&8); }
   /** @param {boolean} flag */
   #setLastWindowStateHidden(flag) { this.#flags = flag ? this.#flags|8 : this.#flags&(~8) }

   //// Pos Helper ////

   /**
    * @param {string} property
    * @param {object} newValue
    * @param {object} oldValue
    * @param {object} changed
    * @return {boolean} Attribute value differs.
    */
   diffNumberProperty(property, newValue, oldValue, changed) {
      const value = newValue[property]
      if (isFinite(value) && oldValue[property] !== value) {
         changed[property] = value
         return true
      }
      return false
   }
   /**
    * @param {WindowVMPosArgs} newPos
    * @return {undefined|WindowVMPosArgs} Valid value, if any value changed else undefined.
    */
   diffPos(newPos) {
      const changed = {}
      let hasChanged = false
      if (this.moveable) {
         hasChanged = this.diffNumberProperty("top", newPos, this.#pos, changed) || hasChanged
         hasChanged = this.diffNumberProperty("left", newPos, this.#pos, changed) || hasChanged
      }
      if (this.resizeable) {
         hasChanged = this.diffNumberProperty("width", newPos, this.#pos, changed) || hasChanged
         hasChanged = this.diffNumberProperty("height", newPos, this.#pos, changed) || hasChanged
      }
      return hasChanged ? changed : undefined
   }
   /**
    * Notifes view of pos changes if there are any.
    * @param {WindowVMPosArgs} [newPos]
    */
   notifyPosUpdate(newPos) { this.notifyPropertyUpdate("pos",newPos ?? this.#pos) }

   //// Properties ////

   /**
    * @return {WindowVMPos}
    */
   get pos() { return { ...this.#pos } }
   /**
    * @param {WindowVMPosArgs} pos
    */
   set pos(pos) {
      const changedPos = this.diffPos(pos)
      if (changedPos) {
         Object.assign(this.#pos,changedPos)
         pos.transition && (changedPos.transition = true)
         this.notifyPosUpdate(changedPos)
      }
   }
   get title() { return this.#title }
   set title(title) { this.notifyPropertyUpdate("title", this.#title = String(title)) }
   get windowState() { return this.#windowState }
   set windowState(newState) {
      const oldState = this.#windowState
      switch (newState) {
      case WindowVM.CLOSED:
      case WindowVM.HIDDEN:
      case WindowVM.MAXIMIZED:
      case WindowVM.MINIMIZED:
      case WindowVM.SHOWN:
         this.#windowState = newState
         break
      default:
         break
      }
      if (this.#windowState === oldState) return
      this.#setLastWindowStateHidden(oldState === WindowVM.HIDDEN)
      this.notifyPropertyUpdate("windowState",newState)
      newState === WindowVM.CLOSED && this.notifyPropertyUpdate("closed",true)
   }
   get hidden() { return this.#windowState === WindowVM.HIDDEN }
   get minimized() { return this.#windowState === WindowVM.MINIMIZED }
   /////////////
   // Actions //
   /////////////
   /**
    * @param {string} action
    * @param {(args: object)=>object} [mapArgs]
    * @return {ViewModelAction} Bound action which calls the correct view model method.
    */
   getVMAction(action, mapArgs) {
      switch (action) {
      case "move": return new ViewModelAction(this.getVMActionLock("move"), this, this.move, mapArgs)
      case "resize": return new ViewModelAction(this.getVMActionLock("move"), this, this.resize, mapArgs)
      case "resizeTop": return new ViewModelAction(this.getVMActionLock("top"), this, this.resizeTop, mapArgs)
      case "resizeTopRight": return new ViewModelAction(this.getVMActionLock("topRight"), this, this.resizeTopRight, mapArgs)
      case "resizeRight": return new ViewModelAction(this.getVMActionLock("right"), this, this.resizeRight, mapArgs)
      case "resizeBottomRight": return new ViewModelAction(this.getVMActionLock("bottomRight"), this, this.resizeBottomRight, mapArgs)
      case "resizeBottom": return new ViewModelAction(this.getVMActionLock("bottom"), this, this.resizeBottom, mapArgs)
      case "resizeBottomLeft": return new ViewModelAction(this.getVMActionLock("bottomLeft"), this, this.resizeBottomLeft, mapArgs)
      case "resizeLeft": return new ViewModelAction(this.getVMActionLock("left"), this, this.resizeLeft, mapArgs)
      case "resizeTopLeft": return new ViewModelAction(this.getVMActionLock("topLeft"), this, this.resizeTopLeft, mapArgs)
      case "close": return new ViewModelAction(this.getVMActionLock("click"), this, this.close, mapArgs)
      case "minimize": return new ViewModelAction(this.getVMActionLock("click"), this, this.minimize, mapArgs)
      case "toggleMaximize": return new ViewModelAction(this.getVMActionLock("click"), this, this.toggleMaximize, mapArgs)
      }
      return super.getVMAction(action,mapArgs)
   }
   /**
    * Set windowState to {@link CLOSED}
    */
   close() { this.windowState = WindowVM.CLOSED }
   /**
    * Set windowState to {@link MAXIMIZED}.
    */
   maximize() { this.windowState = WindowVM.MAXIMIZED }
   /**
    * Set windowState to {@link MINIMIZED}.
    */
   minimize() { this.windowState = WindowVM.MINIMIZED }
   /**
    * Set windowState to {@link HIDDEN}.
    */
   hide() { this.windowState = WindowVM.HIDDEN }
   /**
    * Set windowState to {@link SHOWN}.
    */
   show() { this.windowState = WindowVM.SHOWN }
   /**
    * Switches windowState to {@link MAXIMIZED} if not maximized else {@link SHOWN}.
    */
   toggleMaximize() { this.windowState = this.windowState === WindowVM.MAXIMIZED ? WindowVM.SHOWN : WindowVM.MAXIMIZED }
   /**
    * Move window position dx pixels horizontal and dy pixels vertical.
    * If (dx > 0) in the right direction else left. If (dy > 0) in the bottom direction else top.
    * @this {WindowVM}
    * @param {{dx?:number, dy?:number}} args
    */
   move({dx, dy}) {
      const { top, left } = this.pos
      this.pos = { top:top+(dy?dy:0), left:left+(dx?dx:0) }
   }
   /** @this {WindowVM} @param {{dy:number}} e */
   resizeTop(e) { return this.moveBorder({dtop:e.dy}) }
   /** @this {WindowVM} @param {{dx:number,dy:number}} e */
   resizeTopRight(e) { return this.moveBorder({dtop:e.dy,dright:e.dx}) }
   /** @this {WindowVM} @param {{dx:number}} e */
   resizeRight(e) { return this.moveBorder({dright:e.dx}) }
   /** @this {WindowVM} @param {{dx:number,dy:number}} e */
   resizeBottomRight(e) { return this.moveBorder({dbottom:e.dy,dright:e.dx}) }
   /** @this {WindowVM} @param {{dy:number}} e */
   resizeBottom(e) { return this.moveBorder({dbottom:e.dy}) }
   /** @this {WindowVM} @param {{dx:number,dy:number}} e */
   resizeBottomLeft(e) { return this.moveBorder({dbottom:e.dy,dleft:e.dx}) }
   /** @this {WindowVM} @param {{dx:number}} e */
   resizeLeft(e) { return this.moveBorder({dleft:e.dx}) }
   /** @this {WindowVM} @param {{dx:number,dy:number}} e */
   resizeTopLeft(e) { return this.moveBorder({dtop:e.dy,dleft:e.dx}) }
   /**
    * Moves a horizontal border (dright,dleft) to the left if value < 0 else to the right.
    * Moves a vertical border (dtop,dbottom) to the top if value < 0 else to the bottom.
    * @this {WindowVM}
    * @param {{dtop?:number, dright?:number, dbottom?:number, dleft?:number}} args
    */
   moveBorder({dtop=0, dright=0, dbottom=0, dleft=0}) {
      const { top, left, width, height } = this.pos
      this.pos = { top:top+dtop, left:left+dleft, width:width-dleft+dright, height:height-dtop+dbottom }
   }
   /**
    * @this {WindowVM}
    * @param {{dx?:number, dy?:number}} args
    */
   resize({dx=0, dy=0}) {
      if (!this.resizeable) return
      const { top, left, width, height } = this.pos
      this.pos = this.moveable
                  ? { top:top-dy, left:left-dx, width:width+2*dx, height:height+2*dy }
                  : { width:width+2*dx, height:height+2*dy }
   }
}

// XXXXX

class WindowManagerVM extends ViewModel {
   /** @type {[ViewComment,View][]} */
   #closed = []
   /** @type {WindowVM[]} */
   #windowVMs = []
   /** @type {Set<Number>} */
   #winIDs = new Set()

   /////////////////////////
   // Overwrite ViewModel //
   /////////////////////////
   /** */
   constructor() {
      super()
      VMX.vmObserver.addPropertyObserver(WindowVM,"windowState",this)
   }
   /** @param {()=>void} [onFree] */
   free(onFree) {
      super.free(() => {
         onFree?.()
         VMX.vmObserver.removePropertyObserver(WindowVM,"windowState",this)
      })
   }
   /** @param {WindowManagerView} view */
   connectView(view) { return super.connectView(view,{}) }

   ///////////////////////
   // ViewModelObserver //
   ///////////////////////

   /** @return {ViewComment[]} */
   closedPlaceHolders() { return this.#closed.map(([placeHolder])=>placeHolder) }

   /** @type {VMPropertyUpdateFunction} */
   onVMUpdate(notification) {
      const windowVM = notification.from
      if (!(windowVM instanceof WindowVM)) return
      if (notification.property === "windowState") {
         if (notification.value === WindowVM.MINIMIZED)
            this.addWindow(windowVM)
         else {
            this.removeWindow(windowVM)
            if (notification.value === WindowVM.CLOSED)
               this.storeClosed(windowVM)
         }
      }
   }

   ////////////
   // Action //
   ////////////

   /** @typedef {{closeable:boolean, index:number, title:string, winID:number}} WindowManagerVM_addWindowNotification */
   /** @typedef {{index:number, winID:number}} WindowManagerVM_removeWindowNotification */

   /** @param {WindowVM} windowVM */
   storeClosed(windowVM) {
      if (windowVM.windowState !== WindowVM.CLOSED) return
      if (windowVM.view.connected) {
         const placeHolder = ViewComment.fromText(`Closed Window * ${windowVM.getLogID().name(windowVM)}`)
         windowVM.view.replaceWith(placeHolder)
         this.#closed.push([placeHolder,windowVM.view])
      }
   }

   restoreClosed() {
      const closed = this.#closed.shift()
      if (closed) {
         const [ placeHolder, view ] = closed
         placeHolder.replaceWith(view.viewElem)
         view.init({show:true})
      }
   }

   /** @param {WindowVM} windowVM */
   removeClosed(windowVM) {
      const i = this.#closed.findIndex(([ placeHolder, view ]) => view.viewModel === windowVM )
      if (i === -1) return
      const [ placeHolder, view ] = this.#closed[i]
      placeHolder.disconnect()
      this.#closed.splice(i,1)
   }

   /** @param {WindowVM} windowVM */
   addWindow(windowVM) {
      const winID = windowVM.getLogID().id
      if (this.#winIDs.has(winID)) return
      const closeable = Boolean(windowVM.view.trySlot("close"))
      const newIndex = this.#windowVMs.length
      this.#winIDs.add(winID)
      this.#windowVMs.push(windowVM)
      /** @type {WindowManagerVM_addWindowNotification} */
      const notification = {closeable, index:newIndex, title:windowVM.title, winID:winID}
      this.notifyPropertyUpdate("addWindow",notification)
   }

   /** @param {WindowVM} windowVM */
   removeWindow(windowVM) {
      const winID = windowVM.getLogID().id
      if (!this.#winIDs.has(winID)) return
      this.#winIDs.delete(winID)
      const index = this.#windowVMs.findIndex(vm => vm === windowVM)
      if (index === -1) return
      this.#windowVMs.splice(index,1)
      /** @type {WindowManagerVM_removeWindowNotification} */
      const notification = {index:index, winID:winID}
      this.notifyPropertyUpdate("removeWindow",notification)
   }

   /** @param {number} winID */
   closeWindow(winID) {
      if (!this.#winIDs.has(winID)) return
      const windowVM = this.#windowVMs.find(vm => vm.getLogID().id === winID)
      if (!windowVM) return
      windowVM.close()
   }

   /** @param {number} winID */
   showWindow(winID) {
      if (!this.#winIDs.has(winID)) return
      const windowVM = this.#windowVMs.find(vm => vm.getLogID().id === winID)
      if (!windowVM) return
      windowVM.show()
   }
}

class WindowManagerViewConfig extends ViewConfig {
   /**
    * @typedef WindowManagerViewConfigInitOptions
    * @property {BasicTypeChecker} minimizedTemplate
    */
   /** @type {WindowManagerViewConfigInitOptions} */
   static InitOptions = {...ViewConfig.InitOptions,
      minimizedTemplate: BasicTypeChecker.simple((value)=>value instanceof ViewElem,"ViewElem")
   }
}

class WindowManagerView extends View {
   /////////////////////////
   // implement Config //
   /////////////////////////
   /** @return {typeof WindowManagerViewConfig} */
   get ClassConfig() { return WindowManagerViewConfig }

   /** @type {ViewElem} */
   #template
   /** State to reference added window templates. @type {{child:ViewElem,controllers:(false|ViewController)[]}[]} */
   #state

   ////////////////////
   // Overwrite View //
   ////////////////////
   /**
    * @param {ViewElem} viewElem
    * @param {null|undefined|WindowManagerVM} [viewModel]
    */
   constructor(viewElem, viewModel) {
      super(viewElem, viewModel ?? new WindowManagerVM())
      if (!(this.viewModel instanceof WindowManagerVM)) return VMX.throwError(this, BasicTypeChecker.expectNameValueOfType("argument »viewModel«",viewModel,"WindowManagerVM"))
      this.setMinimizedTemplate(null)
      this.#state = []
   }
   /**
    * @see {@link View.init}
    * @param {{minimizedTemplate?:ViewElem}} viewOptions
    * @return {WindowManagerView}
    */
   init(viewOptions) {
      const model = this.model
      super.init(viewOptions)
      this.setMinimizedTemplate(viewOptions.minimizedTemplate)
      const restoreButton = this.viewElem.query("button")
      restoreButton && new ClickController(restoreButton, null, ()=>this.model.restoreClosed())
      model.connectView(this)
      return this
   }
   /** Returns the {@link WindowManagerVM Model} attached to view. @return {WindowManagerVM} */
   get model() { return this._unsafe_model }

   /////////////////
   // Init Helper //
   /////////////////
   /** @param {null|undefined|ViewElem} [template] */
   setMinimizedTemplate(template) { this.#template = template ?? ViewElem.fromHtml("<div style='cursor:pointer; padding:2px; border:2px black solid'><div data-slot='title'></div></div>") }

   ////////////////
   // Properties //
   ////////////////
   /** @param {WindowManagerVM_addWindowNotification} arg */
   set v_addWindow({closeable, index, title, winID}) {
      if (index > this.#state.length) return
      const childs = this.viewElem.trySlot("childs") ?? this.viewElem
      const child = this.#state[index]?.child ?? this.#template.clone()
      child.slot("title").setChild(title)
      if (index < this.#state.length)
         this.#freeController(this.#state[index])
      else
         childs.appendChildren(child)
      if (!closeable) child.slot("close").visibility = false
      const controllers = [
         closeable && new ClickController(child.slot("close"), null, (e) => this.model.closeWindow(winID)),
         new ClickController(child.slot("title"), null, (e) => this.model.showWindow(winID))
      ]
      this.#state[index] = {child,controllers}
   }

   /** @param {WindowManagerVM_removeWindowNotification} arg */
   set v_removeWindow({index}) {
      if (index < this.#state.length) {
         const state = this.#state[index]
         this.#state.splice(index,1)
         this.#freeController(state)
         state.child.updateStyles(state.child.computedPosStyles())
         state.child.setPos({height:0,transition:true})
         state.child.onceOnTransitionEnd(()=>{ state.child.remove() })
      }
   }

   /** @param {{controllers:(false|ViewController)[]}} state */
   #freeController(state) { state.controllers.forEach( (controller) => { if (controller) controller.free() }) }
}



   // TODO: integrate into ViewElemDocument
/*
class DocumentObserver {

   constructor() {
      const observer = new ResizeObserver((entries, observer) => {
   // borderBoxSize   -> includes padding and border size ?
   // contentBoxSize  -> includes no padding
         console.log(entries[0].borderBoxSize)
         console.log(entries[0].contentBoxSize)
      })
      observer.observe(document.documentElement)
   }

}
*/


class DocumentView extends View {

   /** @type {number} Size of one CSS pixel to the size of one physical pixel. Or number of device's pixels used to draw a single CSS pixel. */
   devicePixelRatio
   /** @type {{width:number, height:number}} Screen size in css pixel. */
   screen
   /** @type {{width:number, height:number}} Size of viewport(window) which shows the document. */
   viewport

   // TODO: visualViewport

}


// ViewElemDocument <-> DocumentView <-> DocumentVM
// supports different sub-models/sub-views

class DocumentVM extends ViewModel {
   #keyLockStates = []

   constructor() {
      super()
   }

   /** @param {string} name @return {ActionLockState} */
   getVMActionLockState(name) {
      if (name.startsWith("key-")) {
         const i = this.#keyLockStates.findIndex(ls => ls.name === name)
         const lockState = i === -1 ? new ActionLockState(name,false) : this.#keyLockStates.splice(i, 1)[0]
         if (this.#keyLockStates.length > 5)
            this.#keyLockStates.pop()
         this.#keyLockStates.unshift(lockState)
         return lockState
      }
      return super.getVMActionLockState(name)
   }

}

const VMX = new ViewModelContext()
VMX.init()