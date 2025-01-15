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
 * * ViewUpdateListener
 * === Wrapping HTML
 * (not fully, htmlElem is accessed within ViewModel,Controller,and Decorator.)
 * * DOM
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

/**
 * @typedef HasTypeClass
 * @property {TypeClass} typeClass
 */
/**
 * @typedef ImplementsCategory
 * @property {{category:ObjectCategory}} typeClass
 */

/**
 * Types which implement TypeClass uses marker {@link HasTypeClass}.
 */
class TypeClass {
   /** @typedef {"c"|"l"|"v"|"vm"} ObjectCategory */
   /** @type {"c"} */
   static CONTROLLER = "c"
   /** @type {"l"} */
   static LISTENER = "l"
   /** @type {"v"} */
   static VIEW = "v"
   /** @type {"vm"} */
   static VIEWMODEL = "vm"
   /** @type {ObjectCategory} */
   category
   /** @type {Function&{typeClass:TypeClass}} */
   typeconstructor
   /** @type {string} */
   typename

   // Additional implemented interfaces
   // Interface<name1>, ...

   /**
    * @param {Function&{typeClass:TypeClass}} typeconstructor
    * @param {ObjectCategory} category
    */
   constructor(typeconstructor, category) {
      this.typeconstructor = typeconstructor
      this.typename = typeconstructor.name
      this.category = category
   }
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
    * @returns {[string,any][]} An array of all top level option names and their value.
    */
   entries() { return Object.entries(this.#options) }
   /**
    * @param {string} name
    * @returns {boolean} True in case option is defined.
    */
   has(name) { return name in this.#options }
   /**
    * @param {string} name
    * @returns {any} Value of option.
    */
   value(name) { return this.#options[name] }
   /**
    * @returns {string[]} An array of all top level option names.
    */
   names() { return Object.keys(this.#options) }
   /**
    * @param {string} inputStr
    * @returns {object} An object which contains all options with their values.
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

/**
 * Manages relationship between LogIDs and their owners and their attached HTML elements.
 * Allows to search for elements which are bound to viewmodels, controllers, or listeners.
 * And also retrieves the objects which are bound to a certain element.
 */
class LogIDBinding {
   /** @typedef {HasTypeClass&ImplementsCategory&ImplementsInterfaceLogID} LogIDOwner */
   /** @type {Map<number,LogIDOwner>} ! This mapping prevents garbage collection ! */
   index2Owner = new Map()
   /** @type {WeakMap<Document|Element,LogIDOwner[]>} */
   elem2LIDOwner = new WeakMap()
   /** @type {WeakMap<LogIDOwner,(Document|Element)[]>} */
   LIDOwner2Elem = new WeakMap()

   /**
    * @param {LogID} logid
    * @param {null|undefined|LogIDOwner} logidOwner
    */
   bindOwner(logid, logidOwner) {
      if (logidOwner) {
         this.index2Owner.set(logid.index,logidOwner)
      }
   }
   /**
    * @param {LogID} logid
    * @param {undefined|LogIDOwner} logidOwner
    */
   unbind(logid, logidOwner) {
      if (!logidOwner) return
      this.unbindOwner(logid)
      this.unbindAllViewElem(logidOwner)
   }
   /**
    * @param {LogID} logid
    */
   unbindOwner(logid) {
      this.index2Owner.delete(logid.index)
   }
   /**
    * @param {LogIDOwner} logidOwner
    * @param {undefined|Document|Element} htmlElem
    */
   bindViewElem(logidOwner, htmlElem) {
      if (!htmlElem) return
      const elems = this.LIDOwner2Elem.get(logidOwner)
      if (elems) {
         if (elems.some((el) => el === htmlElem))
            VMX.throwErrorObject(this, `Object ${VMX.log.objectName(logidOwner)} bound twice to elem ${htmlElem.nodeName}.`, {logidOwner, htmlElem})
         elems.push(htmlElem)
      }
      else {
         this.LIDOwner2Elem.set(logidOwner, [htmlElem])
      }
      const owners = this.elem2LIDOwner.get(htmlElem)
      if (owners) {
         owners.push(logidOwner)
      }
      else {
         this.elem2LIDOwner.set(htmlElem, [logidOwner])
      }
   }
   /**
    * @param {LogIDOwner} logidOwner
    */
   unbindAllViewElem(logidOwner) {
      const elems = this.LIDOwner2Elem.get(logidOwner)
      if (elems) {
         this.LIDOwner2Elem.delete(logidOwner)
         for (const elem of elems) {
            const owners = this.elem2LIDOwner.get(elem)
            if (owners) {
               const i = owners.indexOf(logidOwner)
               i >= 0 && (owners.length > 1 ? owners.splice(i,1) : this.elem2LIDOwner.delete(elem))
            }
         }
      }
   }
   /**
    * @param {{index?:number,logID?:LogID,owner?:LogIDOwner,rootElem?:Element|Document}} searchOptions
    * @returns {undefined|Element|Document}
    */
   getElementBy({index,logID,owner,rootElem}) {
      const byIndex = typeof index === "number" ? index : logID instanceof LogID ? logID.index : undefined
      if (byIndex) {
         const byOwner = this.index2Owner.get(byIndex)
         if (byOwner) return this.getElementBy({owner:byOwner,rootElem})
      }
      const elemsByOwner = owner ? this.LIDOwner2Elem.get(owner) : undefined
      if (elemsByOwner) {
         return rootElem ? elemsByOwner.find((el) => rootElem.contains(el)) : elemsByOwner[0]
      }
      const result = []
      if (rootElem)
         this.traverseNodes([rootElem], (node) => this.elem2LIDOwner.get(node) && result.push(node) > 0 )
      return result[0]
   }
   /**
    * @param {{index?:number,logID?:LogID,owner?:LogIDOwner,rootElem?:Element|Document}} searchOptions
    * @returns {(Element|Document)[]}
    */
   getElementsBy({index, logID, owner, rootElem}) {
      const byIndex = typeof index === "number" ? index : logID instanceof LogID ? logID.index : undefined
      if (byIndex) {
         const byOwner = this.index2Owner.get(byIndex)
         if (byOwner) return this.getElementsBy({owner:byOwner,rootElem})
      }
      const elemsByOwner = owner ? this.LIDOwner2Elem.get(owner) : undefined
      if (elemsByOwner) {
         return rootElem ? elemsByOwner.filter((el) => rootElem.contains(el)) : elemsByOwner
      }
      const result = []
      if (rootElem)
         this.traverseNodes([rootElem], (node) => { this.elem2LIDOwner.get(node) && result.push(node) })
      return result
   }
   /**
    * @param {{index?:number}} searchOptions
    * @returns {undefined|LogIDOwner}
    */
   getOwnerBy({index}) {
      const ownerByIndex = index ? this.index2Owner.get(index) : undefined
      return ownerByIndex
   }
   /**
    * @template {LogIDOwner} T
    * @param {{elem?:Element|Document,type:new(...args:any[])=>T}} searchOptions
    * @returns {T[]}
    */
   getTypedOwnersBy({elem, type}) {
      const ownersByElem = elem ? this.elem2LIDOwner.get(elem) : undefined
      const typedResult = []
      for (const owner of (ownersByElem ? ownersByElem : this.index2Owner.values()))
         if (owner instanceof type)
            typedResult.push(owner)
      return typedResult
   }
   /**
    * Breadth first search over a given set of nodes.
    * @param {(Node)[]} nodes
    * @param {(node:Element|Document)=>void|boolean} visitNode Computes result of given node and adds it to result. Returns *true* if no more nodes should be processed (processinng is done,result complete).
    */
   traverseNodes(nodes, visitNode) {
      for(var i=0; i<nodes.length; ++i) {
         const node = nodes[i]
         if (node instanceof Element || node instanceof Document) {
            if (visitNode(node))
               break
            node.hasChildNodes() && nodes.push(...node.childNodes)
         }
      }
   }
   /**
    * If this function returns a non empty set then disconnected html elements
    * exist with undetached (not freed) ViewModels.
    * @returns {Set<LogIDOwner>} All owners which contain a LogID but are not bound to connected HTML elements.
    */
   disconnectedOwners() {
      const disconnected = new Set()
      for (const owner of this.index2Owner.values()) {
         if (!this.getElementsBy({owner}).some((el) => el.isConnected))
            disconnected.add(owner)
      }
      return disconnected
   }
   freeDisconnectedOwners() {
      for (const owner of this.disconnectedOwners()) {
         // TODO: add support for other views ... (does viewmodel2 free all views ???)
         if (owner instanceof ViewModel && owner.free)
            owner.free()
      }
      if (this.disconnectedOwners().size)
         return VMX.throwErrorObject(this,`Not all disconnected owners could be freed.`)
   }
}

class Logger {
   /** @type {undefined|Logger} */
   static defaultLog
   /** @typedef {0} LogLevel_OFF */
   /** @typedef {1} LogLevel_FATAL */
   /** @typedef {2} LogLevel_ERROR */
   /** @typedef {3} LogLevel_DEBUG */
   /** @typedef {4} LogLevel_WARNING */
   /** @typedef {5} LogLevel_INFO */
   /** @typedef {LogLevel_FATAL|LogLevel_ERROR|LogLevel_DEBUG|LogLevel_WARNING|LogLevel_INFO} LogLevel */
   /** @type {LogLevel_OFF}  */
   static OFF=0
   /** @type {LogLevel_FATAL} */
   static FATAL=1
   /** @type {LogLevel_ERROR} */
   static ERROR=2
   /** @type {LogLevel_DEBUG} */
   static DEBUG=3
   /** @type {LogLevel_WARNING} */
   static WARNING=4
   /** @type {LogLevel_INFO} */
   static INFO=5
   /** @type {[string,string,string,string,string,string]} Names off all known log levels. */
   static LEVELNAME=[ "off", "fatal", "error", "debug", "warning", "info" ]

   /**
    * @returns {Logger}
    */
   static getDefault() {
      this.defaultLog ??= new Logger()
      return this.defaultLog
   }

   /** @type {LogLevel_OFF|LogLevel} */
   level
   /**
    * @param {LogLevel_OFF|LogLevel} [level]
    */
   constructor(level) {
      this.level = level ?? Logger.WARNING
   }
   /**
    * @param {LogLevel_OFF|LogLevel} level
    * @returns {string}
    */
   levelName(level) {
      return (Logger.LEVELNAME[level] ?? String(level))
   }
   /**
    * @param {{name:string}} classConstructor
    */
   className(classConstructor) {
      return classConstructor?.name ? String(classConstructor.name) : "<missing type name>"
   }
   /**
    * @param {any} value
    * @returns {string}
    */
   typeof(value) {
      const type = typeof value
      if (type === "object") {
         if (value === null) return "null"
         const className = value.constructor?.name
         if (className && className !== "Object") return className
      }
      return type
   }
   /**
    * @param {{constructor:{name:string}}|ImplementsInterfaceLogID} obj
    * @returns {string}
    */
   objectName(obj) {
      if (obj && "typeClass" in obj) { // TODO: typeClass does not ensure of type InterfaceLogID
         const ilogid = InterfaceLogID.assertInterface(obj)
         const logid = ilogid.getLogID(obj)
         return logid.toString(ilogid.assertOwner(obj))
      }
      return this.typeof(obj)
   }
   /**
    * Converts provided value to a string.
    * @param {any} v Value which is converted to a string.
    * @returns {string} The string representation of *v*.
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
    * Converts provided value to a string.
    * @param {any} v Value which is converted to a string.
    * @returns {string} The string representation of *v*.
    */
   stringOf(v) { return Logger.stringOf(v) }
   /**
    * Sets log level. The value {@link LogLevel_OFF} turns off logging.
    * @param {LogLevel_OFF|LogLevel} level
    */
   setLevel(level) { this.level = level }
   /**
    * @param {LogLevel} level
    * @param {string} context
    * @param {string|Error} msgOrException
    */
   writeFormattedLog(level, context, msgOrException) {
      const message = msgOrException instanceof Error
               ? new Error(`Catched exception ${String(msgOrException)}.`,{cause:msgOrException})
               : level <= Logger.ERROR ? new Error(msgOrException) : msgOrException
      this.writeLog(level, context, message)
   }
   /**
    * @param {LogLevel} level
    * @param {string} context
    * @param {string|Error} msgOrException
    */
   formattedError(level, context, msgOrException) {
      if (msgOrException instanceof Error)
         return Error(`${context} ${Logger.LEVELNAME[level] ?? level}: Catched exception ${msgOrException}.`,{cause:msgOrException})
      return Error(`${context} ${Logger.LEVELNAME[level] ?? level}: ${msgOrException}`)
   }
   /**
    * @param {LogLevel} level
    * @param {{name:string}} classConstructor
    * @param {string|Error} message
    */
   logStatic(level, classConstructor, message) {
      if (level > this.level) return
      this.writeFormattedLog(level, this.className(classConstructor), message)
   }
   /**
    * @param {LogLevel} level
    * @param {{constructor:{name:string}}} obj
    * @param {string|Error} message
    */
   log(level, obj, message) {
      if (level > this.level) return
      this.writeFormattedLog(level, this.objectName(obj), message)
   }
   /**
    * @callback LogWithObjectContext
    * @param {{constructor:{name:string}}} obj
    * @param {string|Error} message
    * @returns {void}
    */
   /** @type {LogWithObjectContext} */
   debug(obj, message) { this.log(Logger.DEBUG, obj, message) }
   /** @type {LogWithObjectContext} */
   error(obj, message) { this.log(Logger.ERROR, obj, message) }
   /** @type {LogWithObjectContext} */
   info(obj, message) { this.log(Logger.INFO, obj, message) }
   /** @type {LogWithObjectContext} */
   warning(obj, message) { this.log(Logger.WARNING, obj, message) }
   /**
    * @param {{constructor:{name:string}}} obj
    * @param {string|Error} message
    * @param {object} [args] Additional log arguments written to console.
    * @returns {never}
    */
   throwError(obj, message, args) {
      this.error(obj, message)
      args && this.writeArgs(message,args)
      throw this.formattedError(Logger.ERROR,this.objectName(obj),message)
   }
   /**
    * @callback LogWithStaticContext
    * @param {{name:string}} classConstructor
    * @param {string|Error} message
    * @returns {void}
    */
   /** @type {LogWithStaticContext} */
   debugStatic(classConstructor, message) { this.logStatic(Logger.DEBUG, classConstructor, message) }
   /** @type {LogWithStaticContext} */
   errorStatic(classConstructor, message) { this.logStatic(Logger.ERROR, classConstructor, message) }
   /** @type {LogWithStaticContext} */
   infoStatic(classConstructor, message) { this.logStatic(Logger.INFO, classConstructor, message) }
   /** @type {LogWithStaticContext} */
   warningStatic(classConstructor, message) { this.logStatic(Logger.WARNING, classConstructor, message) }
   /**
    * @param {{name:string}} classConstructor
    * @param {string|Error} message
    * @param {object} [args] Additional log arguments written to console.
    * @returns {never}
    */
   throwErrorStatic(classConstructor, message, args) {
      this.errorStatic(classConstructor, message)
      args && this.writeArgs(message,args)
      throw this.formattedError(Logger.ERROR,this.className(classConstructor),message)
   }

   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /**
    * @param {LogLevel} level
    * @param {string} context
    * @param {string|Error} message
    */
   writeLog(level, context, message) {
      console.log(context, this.levelName(level)+":", message)
   }
   /**
    * @param {string|Error} message
    * @param {object} args Additional log arguments written to console.
    */
   writeArgs(message, args) {
      console.log(`Console output for »${message}«:`,args)
   }
}

/**
 * Allocated at bottom of source and assigned to global variable »VMX«.
 */
class ViewModelContext {
   /** Stores references from logIDs to their owners and attached HTML elements. */
   logidBinding = new LogIDBinding()
   /** All ViewListener are managed by this single instance. */
   listeners = new ViewListeners()
   /** Manages callbacks which are called before the View is rendered to the screen the next time. */
   viewUpdateListener = new ViewUpdateListener()
   /** Supports logging. */
   log = Logger.getDefault()
   /** @type {ViewModelActionLocking} */
   vmActionLocking
   /** @type {DocumentVM} The view model for the whole document.documentElement. */
   documentVM

   init() {
      this.documentVM = new DocumentVM()
      this.vmActionLocking = new ViewModelActionLocking()
   }

   ////////////
   // Facade //
   ////////////

   /**
    * @returns {ViewModel[]} All {@link ViewModel} having HTML elements connected to the document.
    */
   getConnectedViewModels() {
      const elements = this.logidBinding.getElementsBy({rootElem:document})
      return elements.flatMap( el =>
         this.logidBinding.getTypedOwnersBy({elem:el, type:ViewModel})
      )
   }
   /**
    * @param {Document|HTMLElement} rootElem The root and all its childs are queried for listeners.
    * @param {string} [eventType] Optional event type a listener is listening on.
    * @returns {(Document|HTMLElement)[]} An array of HTML elements who has attached listeners.
    */
   getListenedElements(rootElem, eventType) {
      return VMX.listeners.getListenedElements(rootElem,eventType)
   }

   //////////////////////////////////
   // JavaScript Low Level Helpers //
   //////////////////////////////////

   isConstructor(value) {
      try {
         return (Reflect.construct(String,[],value), true)
      }
      catch(e) {
         return false
      }
   }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @returns {string}
    */
   objectName(obj) { return this.log.objectName(obj) }
   /**
    * @param {any} value
    * @returns {string} Either name of constructor or name of primitive type.
    */
   typeof(value) { return this.log.typeof(value) }
   /**
    * Converts provided value to a string.
    * @param {any} value Value which is converted to a string.
    * @returns {string} The string representation of *v*.
    */
   stringOf(value) { return this.log.stringOf(value) }
   /**
    * @param {object} obj
    * @param {string} name
    * @returns {undefined|PropertyDescriptor}
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
    * @param {function} classConstructor
    * @param {string} msg
    * @returns {never}
    */
   throwErrorStatic(classConstructor, msg) { return this.log.throwErrorStatic(classConstructor,msg) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    * @param {object} [args]
    * @returns {never}
    */
   throwErrorObject(obj, msg, args) { return this.log.throwError(obj,msg,args) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    */
   logErrorObject(obj, msg) { this.log.error(obj, msg) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {any} exception
    */
   logExceptionObject(obj, exception) { this.log.error(obj, exception) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    */
   logInfoObject(obj, msg) { this.log.info(obj, msg) }
   /**
    * @param {{constructor:{name:string}}|{LID:LogID, constructor:{name:string}}} obj
    * @param {string} msg
    */
   logDebugObject(obj, msg) { this.log.debug(obj, msg) }
}

/**
 * @typedef ImplementsInterfaceLogID
 * @property {{InterfaceLogID:InterfaceLogID}} typeClass
 */

/**
 * Allows to access LogID of object and a parent to produce a chain of log IDs.
 * Use {@link ImplementsInterfaceLogID} to mark a type implementing this interface.
 */
class InterfaceLogID {
   /** @type {(owner:ImplementsInterfaceLogID)=>LogID} */
   getLogID
   /** @type {(owner:ImplementsInterfaceLogID)=>undefined|object} */
   getParent
   /**
    * @param {ImplementsInterfaceLogID} owner
    * @returns {LogID}
    */
   static getLogID(owner) { return this.assertInterface(owner).getLogID(owner) }
   /**
    * @param {ImplementsInterfaceLogID} owner
    * @returns {undefined|object}
    */
   static getParent(owner) { return this.assertInterface(owner).getParent(owner) }
   /**
    * @param {{getLogID?:(owner:ImplementsInterfaceLogID)=>LogID,getParent?:(owner:ImplementsInterfaceLogID)=>undefined|object}} adapter
    */
   constructor({getLogID, getParent}) {
      this.getLogID=getLogID ?? this.defaultGetLogID
      this.getParent=getParent ?? this.defaultGetParent
   }
   /**
    * @param {ImplementsInterfaceLogID} owner
    * @returns {undefined|InterfaceLogID}
    */
   static getInterface(owner) {
      const ilogid = owner?.typeClass?.InterfaceLogID
      return ilogid instanceof InterfaceLogID ? ilogid : undefined
   }
   /**
    * @param {ImplementsInterfaceLogID} owner
    * @returns {InterfaceLogID}
    */
   static assertInterface(owner) {
      return this.getInterface(owner) ?? VMX.throwErrorObject(this, `owner »${VMX.typeof(owner)}« does not implement InterfaceLogID.`)
   }
   /**
    * @param {any} owner
    * @returns {ImplementsInterfaceLogID}
    */
   assertOwner(owner) {
      const ilogid = owner?.typeClass?.InterfaceLogID
      if (this === ilogid)
         return owner
      return VMX.throwErrorObject(this, `owner »${VMX.typeof(owner)}« does not implement InterfaceLogID.`)
   }
   /**
    * @param {ImplementsInterfaceLogID} owner
    * @returns {LogID}
    */
   defaultGetLogID(owner) {
      if ("LID" in owner && owner.LID instanceof LogID)
         return owner.LID
      return VMX.throwErrorObject(this, `owner »${VMX.typeof(owner)}« does not have property LID:LogID.`)
   }
   /**
    * @param {ImplementsInterfaceLogID} owner
    * @returns {undefined}
    */
   defaultGetParent(owner) { return undefined }
}

/**
 * Assigns a log-ID to a component which produces log entries.
 */
class LogID {
   /**  @type {number} */
   #index
   //
   // Generate unique index
   //
   static INDEX = 1
   static nextIndex() {
      const index = this.INDEX ++
      return index
   }
   /**
    * @param {LogIDOwner} owner
    * @param {Document|HTMLElement|EventTarget} [htmlElem]
    */
   constructor(owner, htmlElem) {
      this.#index = LogID.nextIndex()
      VMX.logidBinding.bindOwner(this, owner)
      if (VMX.logidBinding.getOwnerBy({index:this.#index}) !== owner)
         throw Error("... owner differs ... (TODO: remove)")
      this.bindElem(owner, htmlElem)
   }
   /**
    * @param {LogIDOwner} owner
    */
   unbind(owner) { VMX.logidBinding.unbind(this, owner) }
   /**
    * @param {LogIDOwner} owner
    * @param {Document|HTMLElement|EventTarget|undefined} htmlElem
    */
   bindElem(owner, htmlElem) {
      // TODO: rename into bindViewElem (htmlElem -> viewElem) !!!
      if (htmlElem && htmlElem instanceof Document || htmlElem instanceof HTMLElement)
         VMX.logidBinding.bindViewElem(owner, htmlElem)
   }
   /**
    * @param {LogIDOwner} owner
    * @param {Document|Element|(Document|Element)[]} elems
    */
   bindElems(owner, elems) {
      // TODO: rename bindViewElems ; replace type elems -> ViewElem|ViewElem[]
      VMX.logidBinding.unbindAllViewElem(owner)
      for (const elem of Array.isArray(elems)?elems:[elems]) {
         if (elem instanceof Document || elem instanceof HTMLElement)
            VMX.logidBinding.bindViewElem(owner, elem)
      }
   }

   get index() {
      return this.#index
   }
   /**
    * @type {undefined|LogIDOwner}
    */
   get owner() { return VMX.logidBinding.getOwnerBy({index:this.#index}) }
   /**
    * Returns chain of parent LogIDs.
    * @param {ImplementsInterfaceLogID} owner
    * @param {number} depth The number of maximum parent.LID in the chained path of LogIDs.
    * @returns {string} Returns parent IDs like '['+parent.parent.LID+']/['+parent.LID+']'
    */
   parentChain(owner, depth=2) {
      if (depth > 0) {
         const parent = this.parent(owner)
         if (parent) {
            const parentLID = InterfaceLogID.getLogID(parent)
            return parentLID.parentChain(parent, depth-1)+"["+parentLID.index+"]/"
         }
      }
      return ""
   }
   /**
    * Returns name if LogID owner.
    * @param {ImplementsInterfaceLogID} owner
    * @returns {string} Name of owner which is '['+parent.LID+']/'+className(owner)+'-['+owner.LID+']'
    */
   toString(owner) {
      if (InterfaceLogID.assertInterface(owner).getLogID(owner) !== this)
         VMX.throwErrorObject(this, "... owner missing ... (TODO: remove)")
      return `${this.parentChain(owner)}${this.name(owner)}-[${this.#index}]`
   }
   /**
    * Returns name of object.
    * @param {any} owner
    * @returns {string} Return type name of owner. See {@link className}.
    */
   name(owner) { return VMX.typeof(owner) }
   /**
    * Returns parent of owner only if parent has also a LogID.
    * @param {any|{LID: LogID}} owner
    * @returns {undefined|ImplementsInterfaceLogID} The parent of the owner which has also an assigned LogID else undefined for no parent.
    */
   parent(owner) {
      const ilogid = InterfaceLogID.assertInterface(owner)
      const parent = ilogid.getParent(owner)
      const pilogid = parent?.typeClass?.InterfaceLogID
      if (pilogid instanceof InterfaceLogID)
         return parent
   }
}

/**
 * Stores callbacks which are called before the
 * View is rendered to the screen.
 * Implemented with help of requestAnimationFrame.
 */
class ViewUpdateListener {
   /** @type {((timestamp:number)=>void)[]} */
   #callbacks = []
   /** The number of the next requested animation frame
    * @type {null|number} */
   #frameID = null
   constructor(delayCount, callback) {
      callback && this.onceOnViewUpdate(delayCount, callback)
   }
   /**
    * @param {number} timestamp
    */
   #onUpdate(timestamp) {
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
   onceOnViewUpdate(delayCount, callback) {
      if (typeof callback !== "function") return
      this.#callbacks.push( 0 < delayCount && delayCount < 300
         ? () => this.onceOnViewUpdate(delayCount-1,callback)
         : callback
      )
      if (this.#frameID == null)
         this.#frameID = requestAnimationFrame(this.#onUpdate.bind(this))
   }
}

class ControllerEvent {
   /** @type {string} */
   type
   /** @type {{currentTarget:Document|HTMLElement, target:null|EventTarget, timeStamp:number, type:string}} */
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
      this.event = { currentTarget:controller.htmlElem, target:e.target, timeStamp:e.timeStamp, type }
      this.oevent = e
   }
   get eventPhaseCapture() { return this.oevent.eventPhase === 1 }
   get eventPhaseTarget() { return this.oevent.eventPhase === 2 }
   get eventPhaseBubbling() { return this.oevent.eventPhase === 3 }
   get eventTarget() { return this.event.target }
   get eventCurrentTarget() { return this.event.currentTarget }
}

class ViewListenerTypeClass extends TypeClass {
   InterfaceLogID=new InterfaceLogID({ getParent: (owner) => owner instanceof ViewListener ? owner.owner : undefined })
}

/**
 * Stores state of event listener added to HTML view.
 */
class ViewListener {
   static #typeClass = new ViewListenerTypeClass(ViewListener,TypeClass.LISTENER)
   static get typeClass() { return ViewListener.#typeClass }
   get typeClass() { return ViewListener.#typeClass }
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
   /** @type {3} Mask with all phase values bits set. */
   static ALL_PHASES = 3
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
    * @returns
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
    * @returns {boolean} *true* if either touch event or mouse event with main button
    */
   static isMainButton(e) { return this.isButton(e, 0) }
   /**
    * @param {undefined|null|Event} e
    * @param {number} button The number of the button (0==main(left),1==wheel(middle),2==second(right))
    * @returns {boolean} *true* if button is pressed or released
    */
   static isButton(e, button) {
      return Boolean(e) && button === (e instanceof MouseEvent ? e.button : 0)
   }
   /**
    * @param {undefined|null|Event} e
    * @returns {e is MouseEvent}
    */
   static isMouseEvent(e) { return e instanceof MouseEvent }
   /**
    * @param {undefined|null|Event} e
    * @returns {e is TouchEvent}
    */
   static isTouchEvent(e) { return "TouchEvent" in window && e instanceof TouchEvent }

   /**
    * Contains added listener state.
    * Use {@link ViewListeners.addListener} of object VMX.listeners to create a new listener and add it to an html element.
    *
    * @param {EventTarget} htmlElem - The HTML element event handler is added to.
    * @param {string} eventType - The event type ("click","touchstart",...).
    * @param {(e:Event)=>void} eventHandler - Callback which is called to handle events of the given type. The first argument is the HTML event.
    * @param {{owner:LogIDOwner|null, phase?:number, once?:boolean}} options - Owner of the listener should be set.
    */
   constructor(htmlElem, eventType, eventHandler, {owner,phase,once}) {
      this.htmlElem = htmlElem
      this.eventType = eventType
      this.eventHandler = eventHandler
      this.owner = owner
      this.phase = phase || ViewListener.DEFAULT_PHASE
      this.once = Boolean(once)
      this.LID = new LogID(this, this.htmlElem)
      this.#add()
   }
   free() {
      this.#remove()
      this.LID.unbind(this)
   }
   isBubblePhase() { return Boolean(this.phase & ViewListener.BUBBLE_PHASE) }
   isCapturePhase() { return Boolean(this.phase & ViewListener.CAPTURE_PHASE) }
   #add() {
      VMX.listeners.addListener(this)
      if (this.isBubblePhase())
         this.htmlElem.addEventListener(this.eventType,this.eventHandler,{capture:false,passive:false})
      if (this.isCapturePhase())
         this.htmlElem.addEventListener(this.eventType,this.eventHandler,{capture:true,passive:false})
   }
   #remove() {
      VMX.listeners.removeListener(this)
      if (this.isBubblePhase())
         this.htmlElem.removeEventListener(this.eventType,this.eventHandler,{capture:false})
      if (this.isCapturePhase())
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
   /** @type {WeakMap<Event,{callbacks:((e:Event)=>void)[], holder:PromiseHolder}>} */
   endEventLoop = new WeakMap()
   /**
    * Initializes a new ViewListener. There should be only a single one per window.
    */
   constructor() {
      this.boundOnEndEventLoop = this.onEndEventLoop.bind(this)
   }
   /**
    * @param {ControllerEvent|Event} ce
    * @param {undefined|((e:Event) => void)} callback
    * @returns {Promise<Event>}
    */
   onceEndEventLoop(ce, callback) {
      const e = ce instanceof ControllerEvent ? ce.oevent : ce
      const registered = this.endEventLoop.get(e) ?? { callbacks:[], holder:new PromiseHolder() }
      registered.callbacks.length || this.endEventLoop.set(e, registered)
      if (typeof callback === "function")
         registered.callbacks.push(callback)
      return registered.holder.promise
   }
   /**
    * @param {Event} e
    */
   onEndEventLoop(e) {
      const registered = this.endEventLoop.get(e)
      if (registered) {
         this.endEventLoop.delete(e)
         registered.callbacks.forEach( callback => callback(e))
         registered.holder.resolve(e)
      }
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
         return VMX.throwErrorObject(this,`Can not add same event handler twice for type ${listener.eventType}.`)
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
      const eventType = listener.eventType
      if (eventTypes) {
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
    * @returns {Iterator<ViewListener>&Iterable<ViewListener>}
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
    * @returns {boolean}
    */
   isListener(htmlElem, eventType) {
      return !this.get(htmlElem,eventType).next().done
   }
   /**
    * @param {Document|HTMLElement} rootElem The root HTML and all its child elements are queried for listeners.
    * @param {string} [eventType] Optional type of the event the listener must match else listeners with any event type are matched.
    * @returns {(Document|HTMLElement)[]}
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

class ViewControllerTypeClass extends TypeClass {
   InterfaceLogID = new InterfaceLogID({ getParent: (owner) => { return (owner instanceof ViewController ? owner.view ?? owner.viewModel : undefined) } })
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
 * only works if the lock is hold.
 * - To change the state of the lock use {@link trySwitchLock}.
 * - To prevent acquiring the lock during {@link activate} set option tryLock to false.
 * - The lock is always removed during {@link deactivate}.
 */
class ViewController {
   static #typeClass = new ViewControllerTypeClass(ViewController,TypeClass.CONTROLLER)
   static get typeClass() { return ViewController.#typeClass }
   get typeClass() { return ViewController.#typeClass }
   static preventDefault = (e) => e.preventDefault()
   static Options(options) {
      if (typeof options === "object") {
         options = { ...options }
         if ("group" in options) {
            if (!(Symbol.iterator in Object(options.group)))
               return VMX.throwErrorStatic(this, "options.group is not iterable")
            options.group = [...options.group].filter( elem => typeof elem?.addEventListener == "function")
         }
         if ("phase" in options && !(typeof options.phase === "number" && (options.phase&ViewListener.ALL_PHASES)))
            return VMX.throwErrorStatic(this, "options.phase is not of type number")
         if ("enableClick" in options)
            options.enableClick = Boolean(options.enableClick)
         if ("view" in options && !(options.view instanceof View2))
            return VMX.throwErrorStatic(this, "options.view is not of type View2")
         if ("viewModel" in options && !(options.viewModel instanceof ViewModel))
            return VMX.throwErrorStatic(this, "options.viewModel is not of type ViewModel")
         if ("viewModel2" in options && !(options.viewModel2 instanceof ViewModel2))
            return VMX.throwErrorStatic(this, "options.viewModel2 is not of type ViewModel2")
      }
      else
         options = {}
      return options
   }

   /** @type {boolean} */
   #isActive = false
   /** @type {boolean} */
   #muted = false
   /** @type {ViewListener[]} */
   #listeners = []
   /** @type {ViewListener[]} */
   #activeListeners = []
   /** @type {Document|HTMLElement} htmlElem */
   #htmlElem
   /** @type {undefined|ViewModelActionInterface} */
   #action
   /** @type {undefined|ViewModel} */
   #viewModel // TODO: remove
   /** @type {View2} */
   #view
   /** @type {undefined|number} */
   #activationDelay
   /** @type {undefined|number} */
   #autoDeactivation
   /** @type {object} */
   options
   /** @type {LogID} */
   LID

   /** @typedef {undefined|null|ViewModelActionInterface|((e:ControllerEvent)=>void)} ViewControllerAction */

   /**
    * @typedef ViewControllerOptions
    * @property {View2} [view]
    * @property {ViewModel} [viewModel] TODO: remove
    */

   /**
    * @param {Document|HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {ViewControllerOptions} [options]
    */
   constructor(htmlElem, action, options) {
      this.LID = new LogID(this, htmlElem)
      this.options = ViewController.Options(options)
      this.#htmlElem = htmlElem
      action && this.connectToAction(action)
      if (this.options.view)
         this.#view = this.options.view
      if (this.options.viewModel)
         this.#viewModel = this.options.viewModel
   }
   /**
    * @param {ViewModelActionInterface|((e:ControllerEvent)=>void)} [action]
    */
   connectToAction(action) {
      if (action) {
         if (!(typeof action === "function" || action instanceof ViewModelActionInterface))
            return VMX.throwErrorObject(this, `Argument action is not of type ViewModelActionInterface.`)
         this.#action = action instanceof ViewModelActionInterface ? action : new ViewModelActionInterface(action)
      }
      else
         this.#action = undefined
   }
   free() {
      this.deactivate()
      this.removeListeners(this.#listeners)
      this.removeListeners(this.#activeListeners)
      this.LID.unbind(this)
      this.#htmlElem = null
      this.#action?.endAction()
      return this
   }
   /**
    * @type {undefined|View2}
    */
   get view() { return this.#view }
   /**
    * @type {undefined|ViewModel}
    */
   get viewModel() { return this.#viewModel }
   get htmlElem() {
      return this.#htmlElem
   }
   get isActive() {
      return this.#isActive
   }
   get isLocked() {
      return this.#action ? this.#action.isLocked() : true
   }
   get isActivationDelay() {
      return this.#activationDelay !== undefined
   }
   get isAutoDeactivation() {
      return this.#autoDeactivation !== undefined
   }
   /**
    * @returns {number} Number of calls to {@link addActiveListener}. Calling {@link removeActiveListenerForElement} resets this value to 0.
    */
   get nrActiveListeners() {
      return this.#activeListeners.length
   }
   get muted() { return this.#muted }
   /**
    * @param {boolean} muted Set value to *true* in case you want to prevent action callbacks.
    */
   set muted(muted) { this.#muted = muted }
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @returns {boolean} *true* if an listener is listening on htmlElem for event of type eventType.
    */
   hasListenerFor(htmlElem, eventType) {
      return this.#matchListener(htmlElem, eventType, this.#listeners)
   }
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @returns {boolean} *true* if an listener in active state is listening on htmlElem for event of type eventType.
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
   /**
    * @param {ControllerEvent} ce
    */
   callAction(ce) {
      if (!this.#muted && this.#action) {
         try { this.#action.doAction(ce) } catch(x) { VMX.logExceptionObject(this, x) }
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
    * @param {ViewListener[]} listenerArray
    */
   removeListeners(listenerArray) {
      listenerArray.forEach(listener => listener && listener.free())
      listenerArray.length = 0
   }
   /**
    * @param {boolean} isLock
    */
   trySwitchLock(isLock) {
      const action = this.#action
      if (action && action.isLocked() !== isLock) {
         if (isLock)
            action.startAction()
         else
            action.endAction()
      }
   }
   /**
    * Tries to acquire lock and calls callback if locking succeeded. After return the lock state is the same as before the call.
    * @param {()=>void} onLocked Callback is called if action could be locked.
    */
   withLock(onLocked) {
      const action = this.#action
      if (action) {
         const isLocked = action.isLocked()
         if (isLocked || action.startAction())
            onLocked()
         isLocked || action.endAction()
      }
      else {
         onLocked()
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
         this.trySwitchLock(false)
         onDeactivateCallback?.()
      }
   }
   /**
    * @param {{autoDeactivation?:number, delay?:number, noLock?:boolean}} options
    * @param {()=>void} [onActivateCallback]
    */
   activate({autoDeactivation, delay, noLock}, onActivateCallback) {
      if (!this.#isActive) {
         this.#isActive = true
         noLock || this.trySwitchLock(true)
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
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @param {(e:Event)=>void} eventHandler
    * @returns {ViewListener}
    */
   #addListener(htmlElem, eventType, eventHandler) {
      return new ViewListener(htmlElem, eventType, eventHandler, { owner:this, phase:this.options.phase })
   }
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @param {ViewListener[]} listeners
    * @returns {boolean} *true* if in listeners exists a listener which is listening on htmlElem for event of type eventType.
    */
   #matchListener(htmlElem, eventType, listeners) {
      return listeners.some( listener => listener.htmlElem == htmlElem && listener.eventType == eventType)
   }
   /**
    * @param {Document|HTMLElement} htmlElem
    * @param {string} eventType
    * @param {(e:Event)=>void} eventHandler
    * @returns {undefined|ViewListener} A value of undefined means a listener for the given element and event type already exists.
    */
   addListener(htmlElem, eventType, eventHandler) {
      if (!this.#matchListener(htmlElem, eventType, this.#listeners)) {
         const listener = this.#addListener(htmlElem, eventType, eventHandler)
         this.#listeners.push(listener)
         return listener
      }
   }
   /**
    * @param {EventTarget} htmlElem
    * @param {string} eventType
    * @param {(e:Event)=>void} eventHandler
    * @returns {undefined|ViewListener} A value of undefined means a listener for the given element and event type already exists.
    */
   addActiveListener(htmlElem, eventType, eventHandler) {
      if (!this.#matchListener(htmlElem, eventType, this.#activeListeners)) {
         const listener = this.#addListener(htmlElem, eventType, eventHandler)
         this.#activeListeners.push(listener)
         return listener
      }
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
   /**
    * Called if delay option timed out (if activate called with option set to a value > 0).
    * Does nothing. Should be overwritten in a subtype if any action should be taken.
    */
   onActivationDelay() { }
   /**
    * Called if autoDeactivation option timed out (if activate called with option set to a value > 0).
    * Deactivates the controller. Should be overwritten in a subtype if additional action should be taken.
    */
   onAutoDeactivation() { this.deactivate() }
}

class TouchSupportController extends ViewController {
   /**
    * Identifiers of touches which originated within htmlElem or one of its childs.
    */
   #targetIDs=new Set()
   ///////////////
   // overwrite //
   ///////////////
   /**
    * @param {()=>void} [onDeactivateCallback]
    */
   deactivate(onDeactivateCallback) {
      super.deactivate( () => {
         this.#targetIDs.clear()
         onDeactivateCallback?.()
      })
   }
   ///////////////////////
   // add touch support //
   ///////////////////////
   /**
    * @param {Event} e
    * @returns {string}
    */
   touchIDs(e) {
      let all="", target="", changed="", active=""
      for (const id of this.#targetIDs ?? [])
         active += (active.length?",":"") + id
      if (ViewListener.isTouchEvent(e)) {
         for (const t of e.touches)
            all += (all.length?",":"") + t.identifier
         for (const t of e.targetTouches)
            target += (target.length?",":"") + t.identifier
         for (const t of e.changedTouches)
            changed += (changed.length?",":"") + t.identifier
      }
      return "touchids:["+all+"] tg:["+target+"] ch:["+changed+"] act:["+active+"]"
   }
   /**
    * @returns {number} The number of currently started touches but not ended or canceled.
    */
   nrActiveTouches() { return this.#targetIDs.size }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @param {number} [index] Index of active touch. Indices could change
    * @returns {undefined|Touch}
    */
   getActiveTouch(e, index=0) {
      if (ViewListener.isTouchEvent(e))
         for (const touch of e.touches)
            if (this.#targetIDs.has(touch.identifier) && 0 === index--)
               return touch
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @returns {undefined|Touch}
    */
   getChangedTouch(e) {
      return ViewListener.isTouchEvent(e) ? e.changedTouches[0] : undefined
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @returns {{x:number, y:number}}
    */
   extractActivePos(e) {
      const pos = this.getActiveTouch(e) ?? e
      if (pos && "clientX" in pos && "clientY" in pos)
         return { x:pos.clientX, y:pos.clientY }
      return VMX.throwErrorObject(this, "extractActivePos with wrong parameter")
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    * @returns {{x:number, y:number}}
    */
   extractChangedPos(e) {
      const pos = this.getChangedTouch(e) ?? e
      if (pos && "clientX" in pos && "clientY" in pos)
         return { x:pos.clientX, y:pos.clientY }
      return VMX.throwErrorObject(this, "extractChangedPos with wrong parameter")
   }
   /**
    * Adds changed touches to the list of active touches.
    * @param {Event} e
    */
   addActiveTouch(e) {
      if (ViewListener.isTouchEvent(e))
         for (const touch of e.changedTouches)
            this.#targetIDs.add(touch.identifier)
   }
   /**
    * Removes changed touches from the list of active touches.
    * @param {Event} e
    */
   removeActiveTouch(e) {
      if (ViewListener.isTouchEvent(e))
         for (const touch of e.changedTouches)
            this.#targetIDs.delete(touch.identifier)
   }
   //////////////////////
   // listener support //
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
    * @typedef TouchController_Options
    * @property {boolean} [enableClick]
    * @property {boolean} [enableDoubleClick]
    * @property {boolean} [nomouse]
    */
   /** @typedef {ViewControllerOptions&TouchController_Options} TouchControllerOptions */

   /**
    * @param {HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {TouchControllerOptions} [options] Optional object which contains additional options. Set *enableClick* to true if click events should be enabled. Set *enableDoubleClick* to true if doubleclick should be enabled. Set *nomouse* to true if you want to support only touch devices.
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, options)
      this.addStartListener(this.onStart, this.options.nomouse)
      const onClick = this.onClick.bind(this)
      if (this.options.enableClick) this.addListener(this.htmlElem,ViewListener.CLICK,onClick)
      if (this.options.enableDoubleClick) this.addListener(this.htmlElem,ViewListener.DBLCLICK,onClick)
   }
   /**
    * @param {Event} e
    * @param {undefined|{timeStamp:number, type:string}} lastEvent
    * @returns {undefined|boolean} *true* in case last event was a touch event and e is a mouse event at same time
    */
   isSimulatedMouse(e, lastEvent) {
      return lastEvent?.type.startsWith("t") && e.type.startsWith("m")
             && (lastEvent.timeStamp-e.timeStamp) <= 10
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onStart(e) {
      VMX.logInfoObject(this, `onStart ${e.type} ${e.eventPhase} ${this.touchIDs(e)}`)
      this.addActiveTouch(e)
      if (ViewListener.isMainButton(e)) {
         this.activate({}, () => {
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
      VMX.logInfoObject(this, `onClick ${e.type} ${e?.detail}`)
      // e.detail === 0|undefined (custom event); e.detail === 1 (single click)
      // before DBLCLICK: CLICK is sent twice with e.detail === 2 (e.detail === 3 in case of TRIPPLE click)
      if (e.type === ViewListener.CLICK && !(e.detail >= 2))
         this.callAction(new TouchControllerEvent(TouchController.CLICK, this, e, true, false))
      else if (e.type === ViewListener.DBLCLICK)
         this.callAction(new TouchControllerEvent(TouchController.DOUBLECLICK, this, e, true, false))
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onEnd(e) {
      VMX.logInfoObject(this, `${this.LID.toString(this)} onEnd ${e.type} ${this.touchIDs(e)}`)
      this.removeActiveTouch(e)
      if (this.nrActiveTouches() === 0 && ViewListener.isMainButton(e)) {
         const isSimulatedMouse = this.isSimulatedMouse(e, this.#lastEvent)
         this.#lastEvent = { type:e.type, timeStamp:e.timeStamp }
         if (!isSimulatedMouse) {
            const pos = this.extractChangedPos(e)
            const contained = isFinite(pos.x) && isFinite(pos.y) && this.htmlElem.contains(document.elementFromPoint(pos.x,pos.y))
            const cancel = e.type.endsWith("l") // true => 'touchcancel' (false => 'mouseup'|'touchend')
            VMX.logInfoObject(this, `TOUCHEND call action contained=${contained} lock=${this.isLocked}`)
            this.callAction(new TouchControllerEvent(TouchController.TOUCHEND, this, e, contained, cancel))
         }
         this.deactivate()
      }
   }
}

class ClickController extends TouchController {
   /**
    * @param {HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {TouchControllerOptions} [options] Optional object which contains additional options. Set *enableDoubleClick* to true if you want to receive doubleclick events. Set *enableClick* to false if you do not want to receive click events.
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, { enableClick:true, ...options })
   }
   /**
    * Filter all actions except CLICK actions (also DOUBLECLICK if enableDoubleClick set to true).
    * @param {ControllerEvent} ce
    */
   callAction(ce) {
      if (ce.type.endsWith(ClickController.CLICK)) {
         VMX.logInfoObject(this, "click action")
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
    *
    * @param {HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {ViewControllerOptions} options
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, options)
      this.addStartListener(this.onStart)
   }
   /**
    * @param {Event} e
    * @returns {boolean} *true* in case target where event originated should not be considered as start of touch/moudesdown operation
    */
   blockTarget(e) {
      return DOM.isInputOrFocusedElement(e.target)
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onStart(e) {
      VMX.logDebugObject(this, `onStart ${e.type} ${this.touchIDs(e)}`)
      if (this.blockTarget(e)) return
      this.addActiveTouch(e)
      this.activate({delay:30, autoDeactivation:400}, () => {
         this.#pos = this.extractChangedPos(e)
         this.#totalxy = { x:0, y:0 }
         this.addActiveEndListener(e, this.onEnd)
      })
      if (this.nrActiveTouches() > 1) {
         this.trySwitchLock(false)
         this.removeActiveMoveListener()
      }
   }
   onActivationDelay() {
      this.addActiveMoveListener(this.onMove)
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onMove(e) {
      // VMX.logDebugObject(this, `onMove ${e.type} ${this.touchIDs(e)}`)
      if (this.nrActiveTouches() <= 1) {
         e.preventDefault()
         this.clearAutoDeactivation()
         const pos = this.extractActivePos(e)
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
      VMX.logInfoObject(this, `onEnd ${e.type} ${this.touchIDs(e)}`)
      this.removeActiveTouch(e)
      if (this.nrActiveTouches() === 0)
         this.deactivate()
      else if (this.nrActiveTouches() === 1) {
         this.#pos = this.extractActivePos(e)
         this.trySwitchLock(true)
         this.addActiveMoveListener(this.onMove)
      }
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
    * @param {HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {ViewControllerOptions} options
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, options)
      this.addStartListener(this.onStart, /*nomouse*/true)
   }

   blockTarget(e) {
      return DOM.isInputOrFocusedElement(e.target)
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   extractActivePos2(e) {
      const pos1 = this.getActiveTouch(e, 0)
      const pos2 = this.getActiveTouch(e, 1)
      if (pos1 && pos2)
         return { x:Math.abs(pos1.clientX - pos2.clientX), y:Math.abs(pos1.clientY - pos2.clientY) }
      return VMX.throwErrorObject(this, "extractActivePos2 with wrong parameter")
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onStart(e) {
      VMX.logInfoObject(this, `onStart ${e.type} ${this.touchIDs(e)}`)
      if (this.blockTarget(e)) return
      this.addActiveTouch(e)
      this.activate({noLock:true}, () => this.addActiveEndListener(e, this.onEnd))
      const isNrActiveTouchesEQ2 = this.nrActiveTouches() === 2
      this.trySwitchLock(isNrActiveTouchesEQ2)
      if (isNrActiveTouchesEQ2) {
         this.#pos = this.extractActivePos2(e)
         this.#totalxy = { x:0, y:0 }
         e.preventDefault()
         this.addActiveMoveListener(this.onMove)
      }
      else
         this.removeActiveMoveListener()
   }
   /**
    * @param {MouseEvent|TouchEvent} e
    */
   onMove(e) {
      if (this.nrActiveTouches() === 2) {
         e.preventDefault()
         const pos = this.extractActivePos2(e)
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
      VMX.logInfoObject(this, `onEnd ${e.type} ${this.touchIDs(e)}`)
      this.removeActiveTouch(e)
      if (this.nrActiveTouches() === 2) {
         this.trySwitchLock(true)
         this.#pos = this.extractActivePos2(e)
         this.addActiveMoveListener(this.onMove)
      }
      else if (this.nrActiveTouches() === 0)
         this.deactivate()
      else {
         this.trySwitchLock(false)
         this.removeActiveMoveListener()
      }
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
    * @param {HTMLElement} htmlElem
    * @param {ViewControllerAction&((e:TransitionControllerEvent)=>void)} action
    * @param {ViewControllerOptions} [options]
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, options)
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
         this.activate({}, () => {
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
    * @returns {Animation[]}
    */
   static getTransitions(htmlElem) {
      return htmlElem.getAnimations(/*includes no childs*/).filter(animation => animation instanceof CSSTransition)
   }
   /**
    * @param {Document|HTMLElement} htmlElem
    * @returns {boolean} True in case transition animations are running.
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
    * @param {Document|HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {ViewControllerOptions} [options]
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, options)
      this.addListener(htmlElem,"dragenter",this.onStart.bind(this))
      // enable executing drop handler
      this.addListener(htmlElem,"dragover",DropController.preventDefault)
      // prevent opening of images if dropped not into target zone
      this.addListener(document,"drop",DropController.preventDefault)
   }
   /**
    * @param {DragEvent} e
    */
   onStart(e) {
      VMX.logInfoObject(this, `onStart ${e.type}`)
      ++ this.#enterCount // dragenter for a child is sent before dragleave of parent
      if (! this.isActive) {
         this.activate({}, () => {
            this.callAction(new DropControllerEvent("dropstart",this,e))
            this.addActiveListener(this.htmlElem,"drop",this.onDrop.bind(this)),
            this.addActiveListener(this.htmlElem,"dragleave",this.onEnd.bind(this))
         })
      }
   }
   /**
    * @param {DragEvent} e
    */
   onDrop(e) {
      VMX.logInfoObject(this, `onDrop ${e.type}`)
      e.preventDefault()
      this.callAction(new DropControllerEvent("drop",this,e))
      this.onEnd(e)
   }
   /**
    * @param {DragEvent} e
    */
   onEnd(e) {
      VMX.logInfoObject(this, `onEnd ${e.type}`)
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
    * @param {Document|HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {ViewControllerOptions} [options]
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, options)
      const onChange = this.onChange.bind(this)
      for (const elem of this.options.group ?? [htmlElem])
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
    * @param {Document|HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {ViewControllerOptions} [options]
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, options)
      const onFocus = this.onFocus.bind(this)
      const onBlur = this.onFocus.bind(this)
      this.addListener(htmlElem,"focusin",onFocus)
      this.addListener(htmlElem,"focusout",onBlur)
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
    * @returns {boolean} *true* in case a lock could be acquired for the pressed key
    */
   static lockKeyFromEvent(e) {
      const keyAction = VMX.documentVM.bindAction(e.type, [[e.acmsKey()]])
      VMX.listeners.onceEndEventLoop(e, () => keyAction.endAction())
      return keyAction.startAction()
   }
   /**
    * @param {Document|HTMLElement} htmlElem
    * @param {ViewControllerAction} action
    * @param {ViewControllerOptions} [options]
    */
   constructor(htmlElem, action, options) {
      super(htmlElem, action, options)
      this.addListener(htmlElem,ViewListener.KEYDOWN,this.onKeyDown.bind(this))
      this.addListener(htmlElem,ViewListener.KEYUP,this.onKeyUp.bind(this))
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

class ViewElemText {
   /**
    * @type {Text}
    */
   #textNode
   /**
    * @param {Text} textNode
    */
   constructor(textNode) {
      this.#textNode = ViewElemText.assertTextNode(textNode)
   }
   get textNode() {
      return this.#textNode
   }
   /**
    * @param {string} textString The text content of a newly created HTML Text node.
    * @returns {ViewElemText} Encapsulates HTML Text node containing specified text.
    */
   static fromText(textString) {
      return new ViewElemText(document.createTextNode(textString))
   }
   ////////////
   // Helper //
   ////////////
   assertConnected() {
      this.connected() || VMX.throwErrorObject(this, `Expect this text node to be connected.\ntext=${this.text()}`)
   }
   /**
    * @param {Node|null|undefined} textNode
    * @returns {Text} The given textNode in case it is of type Text.
    */
   static assertTextNode(textNode) {
      if (textNode instanceof Text)
         return textNode
      return VMX.throwErrorStatic(this, `Expect type Text and not »${String(VMX.typeof(textNode))}«.`)
   }
   /////////////
   // Content //
   /////////////
   /**
    * @returns {null|string}
    */
   text() {
      return this.#textNode.textContent
   }
   ////////////////////////////////////////////////
   // Inserting, Removing, Replacing in Document //
   ////////////////////////////////////////////////
   connected() {
      return this.#textNode.isConnected
   }
   connect() {
      if (!this.connected())
         document.body.append(this.#textNode)
   }
   disconnect() {
      this.remove()
   }
   remove() {
      this.#textNode.remove()
   }
}

class ViewElem {
   /**
    * Data attribute of HTML node to export view element as slot container.
    * @type {"data-slot"}
    */
   static SlotAttr="data-slot"
   /**
    * @type {HTMLElement}
    */
   #htmlElem
   /**
    * @type {undefined|ViewElemStylesManager}
    */
   #stylesManager
   /**
    * @param {HTMLElement} htmlElem
    */
   constructor(htmlElem) {
      this.#htmlElem = ViewElem.assertHTMLElement(htmlElem)
   }
   get stylesManager() { return (this.#stylesManager ??= new ViewElemStylesManager(this.deleteStylesManager.bind(this))) }
   deleteStylesManager() { if (this.#stylesManager?.isFree) this.#stylesManager = undefined }
   get htmlElem() {
      return this.#htmlElem
   }
   get slotattr() {
      return ViewElem.SlotAttr
   }
   /**
    * Clones this ViewElem. The cloned node is not connected to the Document.
    * @returns {ViewElem} A new ViewElem cloned from this one.
    */
   clone() {
      const cloned = this.#htmlElem.cloneNode(true)
      if (cloned instanceof HTMLElement) {
         return new ViewElem(cloned)
      }
      return VMX.throwErrorObject(this, `Can only clone HTMLElement nodes.`)
   }
   /**
    * @param {{[style:string]:string|number}} styles
    * @returns {ViewElem}
    */
   static fromDiv(styles) {
      const viewElem = new ViewElem(document.createElement("div"))
      viewElem.updateStyles(styles)
      return viewElem
   }
   /**
    * @param {string} htmlString The HTML string which describes a single HTML element with any number of childs.
    * @returns {ViewElem} The encapsulated HTML element specified by a string.
    */
   static fromHtml(htmlString) {
      return new ViewElem(this.createElemFromHtml(htmlString))
   }
   /**
    * @param {string} htmlString
    * @returns {HTMLElement}
    */
   static createElemFromHtml(htmlString) {
      const div = document.createElement("div")
      div.innerHTML = htmlString
      if (div.childNodes.length !== 1)
         return VMX.throwErrorStatic(this, `HTML string describes not a single child node.\nhtml=${htmlString}`)
      const htmlElem = div.childNodes[0]
      return htmlElem instanceof HTMLElement
               ? (htmlElem.remove(), htmlElem)
               : VMX.throwErrorStatic(this, `HTML string describes no HTMLElement but »${VMX.typeof(htmlElem)}«.\nhtml=${htmlString}`)
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
   assertConnected() {
      this.connected() || VMX.throwErrorObject(this, `Expect this element to be connected.\nhtml=${this.outerHTML()}`)
   }
   /**
    * @param {Node|null|undefined} htmlElem
    * @returns {HTMLElement} The given htmlElem in case it is of type HTMLElement or Document.
    */
   static assertHTMLElement(htmlElem) {
      if (htmlElem instanceof HTMLElement)
         return htmlElem
      return VMX.throwErrorStatic(this, `Expect type HTMLElement and not »${String(VMX.typeof(htmlElem))}«.`)
   }
   /**
    * @param {any} viewElem
    * @returns {ViewElem}
    */
   static assertType(viewElem) {
      if (viewElem instanceof ViewElem)
         return viewElem
      return VMX.throwErrorStatic(this, `Expect type ViewElem and not »${String(VMX.typeof(viewElem))}«.`)
   }
   /**
    * @param {any} child
    * @returns {HTMLElement|string}
    */
   static assertChild(child) {
      if (child instanceof ViewElem)
         return child.#htmlElem
      else if (child instanceof ViewElemText)
         return child.text() ?? ""
      else if (typeof child === "string")
         return child
      return VMX.throwErrorStatic(this, `Expect child of type ViewElem, ViewElemText or string and not »${String(VMX.typeof(child))}«.`)
   }
   ///////////////////
   // Query Content //
   ///////////////////
   text() {
      return this.#htmlElem.textContent
   }
   get html() {
      return this.#htmlElem.innerHTML
   }
   outerHTML() {
      return this.#htmlElem.outerHTML
   }
   /**
    * @param {string} cssSelector
    * @returns {ViewElem}
    */
   static query(cssSelector) {
      const htmlElem = document.querySelector(cssSelector)
      if (!(htmlElem instanceof HTMLElement))
         return VMX.throwErrorStatic(this, `Found no HTMLElement matching '${cssSelector}'.`)
      if (htmlElem instanceof HTMLTemplateElement) {
         const child = htmlElem.content.children[0]
         if (!(child instanceof HTMLElement))
            return VMX.throwErrorStatic(this, `Found no child node inside matching template '${cssSelector}'.`)
         return new ViewElem(child)
      }
      return new ViewElem(htmlElem)
   }
   /**
    * Calls first {@link query} and returns a clone of the queried ViewElem.
    * @param {string} cssSelector
    * @returns {ViewElem}
    */
   static clone(cssSelector) {
      return this.query(cssSelector).clone()
   }
   /**
    * @param {string} cssSelector
    * @returns {undefined|ViewElem}
    */
   query(cssSelector) {
      const node = this.#htmlElem.querySelector(cssSelector)
      if (node instanceof HTMLElement)
         return new ViewElem(node)
   }
   /**
    * @returns {undefined|ViewElem|ViewElemText}
    */
   child() {
      const child = this.#htmlElem.childNodes[0]
      if (child instanceof HTMLElement)
         return new ViewElem(child)
      else if (child instanceof Text)
         return new ViewElemText(child)
   }
   ////////////////////////////////////////////////
   // Inserting, Removing, Replacing in Document //
   ////////////////////////////////////////////////
   connected() {
      return this.#htmlElem.isConnected
   }
   connect() {
      if (!this.connected())
         document.body.append(this.#htmlElem)
   }
   disconnect() {
      this.remove()
   }
   remove() {
      this.#htmlElem.remove()
   }
   /**
    * Replaces this ViewElem with another one.
    * @param {ViewElemText|ViewElem|string} elem The new element with which this ViewElem is replaced.
    */
   replaceWith(elem) {
      this.assertConnected()
      this.#htmlElem.replaceWith(ViewElem.assertChild(elem))
   }
   ////////////////////
   // Change Content //
   ////////////////////
   /**
    * Sets inner HTML of this element.
    * @deprecated
    * @type {string} html
    */
   set html(html) { this.#htmlElem.innerHTML = html }
   /**
    * Copy childs from element to this element
    * @param {ViewElem} fromElem
    */
   copyFrom(fromElem) { this.#htmlElem.innerHTML = fromElem.#htmlElem.innerHTML }
   /**
    * Appends given childs to end of list of children of this element.
    * If any argument is of type string it is converted to a text node before adding
    * it as a child.
    * @param  {...ViewElemText|ViewElem|string} childs List of new childs appended to end of list of children.
    */
   appendChildren(...childs) {
      const htmlElems = childs.map(child => ViewElem.assertChild(child))
      this.#htmlElem.append(...htmlElems)
   }
   /**
    * @param {undefined|null|ViewElemText|ViewElem|string} child
    */
   setChild(child) {
      if (child)
         this.replaceChildren(child)
      else
         this.removeChildren()
   }
   /**
    * @deprecated // TODO: remove
    * @param {undefined|null|Node} child
    */
   setChildNode(child) {
      if (child)
         this.#htmlElem.replaceChildren(child)
      else
         this.removeChildren()
   }
   /**
    * Removes all childs of this element and adds all given childs as new childs in the same order.
    * If any argument is of type string it is converted to a text node before adding
    * it as a child.
    * @param  {...ViewElemText|ViewElem|string} childs The new childs which replaces the old ones.
    */
   replaceChildren(...childs) {
      const htmlElems = childs.map(child => ViewElem.assertChild(child))
      this.#htmlElem.replaceChildren(...htmlElems)
   }
   /**
    * Removes all childs of this element.
    */
   removeChildren() { this.#htmlElem.replaceChildren() }
   ////////////////////
   // Input Elements //
   ////////////////////
   /**
    * @param {undefined|null|Node|EventTarget} htmlElem
    * @returns {undefined|boolean} Value of checked attribute, if htmlElem is either a radio button or a checkbox else undefined.
    */
   static getChecked(htmlElem) {
      return htmlElem instanceof HTMLInputElement && (htmlElem.type === "radio" || htmlElem.type === "checkbox")
               ? htmlElem.checked : undefined
   }
   /**
    * @returns {undefined|boolean} Value of checked attribute, if ViewElem is either a radio button or a checkbox else undefined.
    */
   checked() { return ViewElem.getChecked(this.#htmlElem) }
   ///////////////////////
   // Position and Size //
   ///////////////////////
   /**
    * @typedef {{width:number, height:number, top:number, left:number, position:string}} ViewElem_InitialPos
    * @returns {ViewElem_InitialPos}
    */
   initialPos() {
      const isConnected = this.connected()
      isConnected || this.connect()
      const cstyle = this.computedStyle()
      const position = cstyle.position
      const height = ViewElem.parseFloatDefault(cstyle.height, 100)
      const width = ViewElem.parseFloatDefault(cstyle.width, 100)
      const isStatic = position === "static"
      const top = isStatic ? 0 : ViewElem.parseFloatDefault(cstyle.top, 0)
      const left = isStatic ? 0 : ViewElem.parseFloatDefault(cstyle.left, 0)
      isConnected || this.disconnect()
      return { width, height, top, left, position }
   }
   /**
    * @returns {boolean} *true*, if element could be positioned with "left" and "top" styles.
    */
   moveable() {
      const isConnected = this.connected()
      isConnected || this.connect()
      const cstyle = this.computedStyle()
      isConnected || this.disconnect()
      return ["fixed","absolute","relative"].includes(cstyle["position"])
   }
   /**
    * The position relative to the viewport. A value of (0,0) means top/left starting point.
    * An increasing value (> 0) of x,left,right means more to the right of the viewport.
    * An increasing value (> 0) of y,top,bottom means more to the bottom of the viewport.
    * The width and height are the difference between right-left and bottom-top.
    * @returns {{x:number,y:number,left:number,top:number,bottom:number,right:number,width:number,height:number}}
    */
   viewportRect() {
      return this.#htmlElem.getBoundingClientRect()
   }
   /**
    * @param {{width?:number|undefined, height?:number|undefined, top?:number|undefined, left?:number|undefined }} pos
    * @return {undefined|{width?:number, height?:number, top?:number, left?:number}}
    */
   setPos(pos) {
      const elemStyle = this.#htmlElem.style
      const changed = {}
      for (const prop of ["width","height","top","left"]) {
         const value = pos[prop]
         if (typeof value === "number" && isFinite(value)) {
            const styleValue = ViewElem.parseFloatDefault(elemStyle[prop],0)
            if (styleValue !== value) {
               changed[prop] = value
               elemStyle[prop] = value + "px"
            }
         }
      }
      return Object.keys(changed).length > 0 ? changed : undefined
   }
   /** @returns {{width:number, height:number, top:number, left:number }} */
   pos() {
      const elemStyle = this.#htmlElem.style
      const pfd = ViewElem.parseFloatDefault
      return { width:pfd(elemStyle.width,0), height:pfd(elemStyle.height,0), top:pfd(elemStyle.top,0), left:pfd(elemStyle.left,0) }
   }
   //////////////////////////
   // CSS Style Management //
   //////////////////////////
   /**
    * @param {string} className The CSS class name.
    * @returns {boolean} *true*, if element was assigned given CSS class name.
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
   /**
    * @param {string} str
    * @param {number} defaultValue
    * @returns {number}
    */
   static parseFloatDefault(str, defaultValue) {
      const nr = parseFloat(str)
      return isNaN(nr) ? defaultValue : nr
   }
   /**
    * @param {string|number} value
    * @returns {string} String(value)+"px". Value is rounded to 3 places after decimal point.
    */
   static toPixel(value) {
      return Number(value).toFixed(3) + "px"
   }
   /**
    * @returns {CSSStyleDeclaration} A live object, whose values change automatically whenever styles of the (HTML-) element changes.
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
    * @returns {object} Old values of set style values.
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
    * @returns {object} Values of read styles. If style is undefined the values of all available styles are returned.
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
    * @returns {CSSTransition[]}
    */
   getTransitions() {
      return this.#htmlElem.getAnimations(/*includes no childs*/).filter(animation => animation instanceof CSSTransition)
   }
   /**
    * @returns {boolean} True in case transition animations are running.
    */
   isTransition() {
      return this.#htmlElem.getAnimations(/*includes no childs*/).some(animation => animation instanceof CSSTransition)
   }
   /**
    * @param {null|undefined|boolean|ViewTransitionStyle} transitionStyle If undefined,null,or false, updateAttrCallback is called without a transition. If *true*, the default transition styles are used.
    * @param {()=>void} updateAttrCallback Callback to update view attributes. Updating is done with a transition if stylesManager is set else without one.
    */
   startTransition(transitionStyle, updateAttrCallback) {
      if (transitionStyle)
         this.stylesManager.startTransition(this, typeof transitionStyle === "object" ? transitionStyle : null, updateAttrCallback)
      else
         updateAttrCallback()
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
    * @returns {undefined|Promise<null>} Promise if onTransitionEnd is undefined else undefined.
    */
   onceOnTransitionEnd(onTransitionEnd) {
      if (onTransitionEnd) {
         new TransitionController(this.#htmlElem, (e) => {
            if (!e.isTransition)
               onTransitionEnd()
         }).ensureInTransitionElseEnd()
      }
      else {
         const promiseHolder = new PromiseHolder()
         new TransitionController(this.#htmlElem, (e) => {
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
    * @returns {boolean} *true*, if a slot of the given name exists.
    */
   hasSlot(slotName) {
      return this.trySlot(slotName) != null
   }
   /**
    * @param {string} slotName The name of an existing slot.
    * @returns {ViewElem} Container of slot. Use setChild to assign content.
    */
   slot(slotName) {
      const slot = this.trySlot(slotName)
      if (!slot)
         return VMX.throwErrorObject(this, `Missing slot »${ViewElem.SlotAttr}=${slotName}«.\nhtml=${this.outerHTML()}`)
      return slot
   }
   /**
    * @param {string} slotName The name of a slot.
    * @returns {undefined|ViewElem} Container of slot or undefined, if slot does not exist. Use setChild to assign content.
    */
   trySlot(slotName) {
      return this.query("["+ViewElem.SlotAttr+"='"+slotName+"']")
   }
   /**
    * @param {string} slotName The name of an existing slot.
    * @param {null|undefined|ViewElemText|ViewElem} child The new content set into the slot.
    */
   setSlot(slotName, child) {
      const slot = this.slot(slotName)
      slot.setChild(child)
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
    * @returns {undefined|ViewElem}
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
    */
   /**
    * @typedef {{[style:string]:undefined|string, position?:string, width:string, height:string, top:string, left:string }} ViewPositionStyle
    */
   /**
    * @typedef {{[style:string]:undefined|number|string}} ViewStyles
    */
   /**
    * Default transition style for resize and change position animation.
    * @type {{"transition-delay":"0s",  "transition-duration":"0.2s", "transition-property":"all", "transition-timing-function":"ease-out"}}
    */
   static TransitionStyle={ "transition-delay":"0s", "transition-duration":"0.2s", "transition-property":"all", "transition-timing-function":"ease-out" }
   /**
    * @type {{position?:ViewPositionStyle,transition?:ViewTransitionStyle}} The old styles of the HTMLElement before the transition styles were applied.
    */
   #oldStyles
   /**
    * @type {undefined|(()=>void)} Called whenever this manager is no more in use (isFree === true).
    */
   #onFree
   /**
    * @param {()=>void} [onFree] Called whenever this manager is no more in use (isFree === true).
    */
   constructor(onFree) {
      this.#oldStyles = {}
      this.#onFree = onFree
   }
   /**
    * @param {ViewElem} viewElem
    */
   free(viewElem) {
      for (const propName of Object.keys(this.#oldStyles))
         this.#resetStyle(viewElem, propName, this.#oldStyles[propName])
   }
   get isFree() { return Object.keys(this.#oldStyles).length === 0 }
   get isPosition() { return this.#oldStyles.position !== undefined }
   get isTransition() { return this.#oldStyles.transition !== undefined }
   /**
    * @param {ViewElem} viewElem
    * @param {null|undefined|ViewTransitionStyle} transitionStyle
    * @param {()=>void} onTransitionStart Callback to update view attributes. Updating is done with a transition.
    */
   startTransition(viewElem, transitionStyle, onTransitionStart) {
      this.#resetTransition(viewElem, null)
      const preTransitionStyle = viewElem.setStyles(transitionStyle ?? ViewElemStylesManager.TransitionStyle)
      this.#oldStyles.transition = preTransitionStyle
      onTransitionStart()
      VMX.viewUpdateListener.onceOnViewUpdate(1, () => this.#resetTransition(viewElem, preTransitionStyle))
   }
   /**
    * @param {ViewElem} viewElem
    * @param {ViewPositionStyle} newPos
    */
   setPosition(viewElem, newPos) {
      this.resetPosition(viewElem)
      this.#oldStyles.position = viewElem.setStyles(newPos)
   }
   /**
    * @param {ViewElem} viewElem
    * @param {ViewPositionStyle} newPos
    */
   updatePosition(viewElem, newPos) {
      if (this.#oldStyles.position) {
         viewElem.updateStyles(newPos)
      }
   }
   /**
    * Restores old styles previously changed with call to {@link setPosition}.
    * @param {ViewElem} viewElem
    */
   resetPosition(viewElem) {
      this.#resetStyle(viewElem, "position", null)
   }
   /**
    * @param {ViewElem} viewElem
    * @param {null|ViewTransitionStyle} preTransitionStyle
    */
   #resetTransition(viewElem, preTransitionStyle) {
      this.#resetStyle(viewElem, "transition", preTransitionStyle)
   }
   /**
    * @param {ViewElem} viewElem
    * @param {string} oldPropName
    * @param {null|ViewStyles} oldStyles
    */
   #resetStyle(viewElem, oldPropName, oldStyles) {
      if (this.#oldStyles[oldPropName] && (!oldStyles || this.#oldStyles[oldPropName] === oldStyles)) {
         viewElem.updateStyles(this.#oldStyles[oldPropName])
         delete this.#oldStyles[oldPropName]
         this.isFree && this.#onFree?.()
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
    * @param {ViewTransitionStyle} [transitionStyle]
    */
   constructor(viewElem, transitionStyle) {
      this.viewElem = viewElem
      this.transitionStyle = transitionStyle ?? true/*default transition style*/
   }
   free() { this.restore() }
   /**
    * @param {()=>void} onSwitched
    */
   switchFixedPosition(onSwitched) {
      const viewElem = this.viewElem
      if (viewElem.stylesManager.isPosition) {
         return (onSwitched?.(), undefined)
      }
      const pos = viewElem.viewportRect()
      viewElem.stylesManager.setPosition(viewElem, {position:"fixed",width:pos.width+"px",height:pos.height+"px",top:pos.top+"px",left:pos.left+"px"})
      const oldTransStyle = this.transitionStyle
      if (pos.width === 0 && pos.height === 0)
         this.transitionStyle = false // hidden => turn off transition
      onSwitched()
      this.transitionStyle = oldTransStyle
   }
   restore() {
      const viewElem = this.viewElem
      viewElem.startTransition(this.transitionStyle, () => {
         viewElem.stylesManager.resetPosition(viewElem)
      })
   }
   maximize() {
      const viewElem = this.viewElem
      this.switchFixedPosition(() =>
         viewElem.startTransition(this.transitionStyle, () => {
            viewElem.stylesManager.updatePosition(viewElem, { width:"100%", height:"100%", top:"0px", left:"0px" })
         }))
   }
   minimize() {
      const viewElem = this.viewElem
      this.switchFixedPosition(() =>
         viewElem.startTransition(this.transitionStyle, () => {
            viewElem.stylesManager.updatePosition(viewElem, { width:"32px", height:"16px", top:"calc(100vh - 32px)", left:"5px" })
         }))
   }
}

class DOM {
   /**
    * Selects an HTML element from within the document or a given node.
    * If a template node is selected its first child is returned instead.
    * @param {string} cssSelector A CSS selector.
    * @param {Document|Element} [node] The node which serves as root. If no value is provided document is used.
    * @returns {HTMLElement} The queried HTML node or its first child in case of a template.
    **/
   static query(cssSelector, node=document) {
      if (!(node === document || node instanceof Element))
         return VMX.throwErrorStatic(this, `Can not query Node for '${cssSelector}'.`)
      const htmlElem = node.querySelector(cssSelector)
      if (!(htmlElem instanceof HTMLElement))
         return VMX.throwErrorStatic(this, `Found no HTMLElement matching '${cssSelector}'.`)
      if (htmlElem instanceof HTMLTemplateElement) {
         const child = htmlElem.content.children[0]
         if (!(child instanceof HTMLElement))
            return VMX.throwErrorStatic(this, `Found no child node inside matching template '${cssSelector}'.`)
         return child
      }
      return htmlElem
   }
   /**
    * First queries a node from the document and then clones it.
    * The cloned node is not connected to the Document.
    * @param {string} cssSelector A CSS selector.
    * @returns {HTMLElement} A deep clone of the node returned from calling {@link query}.
    */
   static clone(cssSelector) {
      const htmlElem = this.query(cssSelector)
      const cloneNode = htmlElem.cloneNode(true)
      if (!(cloneNode instanceof HTMLElement))
         return VMX.throwErrorStatic(this, `Found no HTMLElement matching '${cssSelector}'.`)
      return cloneNode
   }

   static getStyles(htmlElem, styles) {
      const elemStyle = htmlElem.style
      const styleValues = {}
      if (Array.isArray(styles)) {
         for (const key of styles)
            styleValues[key] = elemStyle.getPropertyValue(key)
      }
      else {
         for (const key in styles)
            styleValues[key] = elemStyle.getPropertyValue(key)
      }
      return styleValues
   }
   static setStyles(htmlElem, newStyles) {
      const styles = htmlElem.style
      const oldStyles = {}
      for (const key in newStyles) {
         oldStyles[key] = styles[key]
         styles[key] = newStyles[key]
      }
      return oldStyles
   }
   static restoreStyles(htmlElem, oldStyles) {
      const styles = htmlElem.style
      for (const key in oldStyles)
         styles[key] = oldStyles[key]
   }
   static newElement(htmlString) {
      const div = document.createElement("div")
      div.innerHTML = htmlString
      const htmlElem = div.children[0]
      htmlElem?.remove()
      return htmlElem
   }
   static newElementList(htmlString) {
      const div = document.createElement("div")
      div.innerHTML = htmlString
      const fragment = document.createDocumentFragment()
      fragment.append(...div.childNodes)
      return fragment
   }
   static newDiv(styles) {
      const htmlElem = document.createElement("div")
      this.setStyles(htmlElem, styles)
      return htmlElem
   }
   static replaceWith(replacedHtmlElem, replacingHtmlElem) {
      replacedHtmlElem.replaceWith(replacingHtmlElem)
   }
   static getSingleChild(parentHtmlElem) {
      return parentHtmlElem.childNodes[0]
   }
   static setSingleChild(parentHtmlElem, childHtmlElem) {
      if (childHtmlElem)
         parentHtmlElem.replaceChildren(childHtmlElem)
      else
         parentHtmlElem.replaceChildren()
   }
   static nextUnusedID(prefix) {
      let id=""
      for (let i=1; (id=prefix+"-"+i, document.getElementById(id)); ++i) {
      }
      return id
   }
   static focus(htmlElem) {
      return htmlElem.matches(":focus")
   }
   static focusWithin(htmlElem) {
      return htmlElem.matches(":focus-within")
   }
   static hasCheckedAttr(htmlElem) {
      const hasChecked = htmlElem.nodeName === "INPUT"
                        && htmlElem.type == "radio" || htmlElem.type == "checkbox"
      return hasChecked
   }
   static isInputOrFocusedElement(htmlElem) {
      const nodeName = htmlElem.nodeName
      return nodeName === "INPUT"
            || nodeName === "SELECT"
            || htmlElem === document.activeElement
   }
   static show(htmlElem) {
      htmlElem.removeAttribute("hidden")
   }
   static hide(htmlElem) {
      htmlElem.setAttribute("hidden","")
   }
   /**
    * @param {Node} htmlElem
    */
   static connected(htmlElem) {
      return htmlElem.isConnected
   }
   /**
    * @param {Document|HTMLElement|Text} htmlElem
    */
   static connect(htmlElem) {
      if (!htmlElem.isConnected)
         document.body.append(htmlElem)
   }
   /**
    * @param {HTMLElement|Text} htmlElem
    */
   static disconnect(htmlElem) {
      if (htmlElem.isConnected)
         htmlElem.remove()
   }
}

class View {
   #htmlElem
   constructor(htmlElem) {
      this.#htmlElem = htmlElem
   }
   get htmlElem() {
      return this.#htmlElem
   }
   get html() {
      return this.#htmlElem.innerHTML
   }
   set html(htmlString) {
      this.#htmlElem.innerHTML = htmlString
   }
   append(node) {
      this.#htmlElem.append(node)
      return this
   }
   get node() {
      return this.#htmlElem.childNodes[0]
   }
   set node(node) {
      node
      ? this.#htmlElem.replaceChildren(node)
      : this.#htmlElem.replaceChildren()
   }
   /**
    * @param {string} name
    * @return {boolean}
    */
   hasSlot(name) {
      return this.trySlot(name) != null
   }
   /**
    * @param {string} name
    * @return {undefined|ViewElem}
    */
   trySlot(name) {
      return new ViewElem(this.#htmlElem).trySlot(name)
   }
   /**
    * @param {string} name
    * @return {ViewElem}
    */
   slot(name) {
      return new ViewElem(this.#htmlElem).slot(name)
   }
   getComputedStyle() {
      return getComputedStyle(this.#htmlElem)
   }
   getStyles(styles) {
      return DOM.getStyles(this.#htmlElem, styles)
   }
   setStyles(styles) {
      return DOM.setStyles(this.#htmlElem, styles)
   }
   hasClass(className) {
      return this.#htmlElem.classList.contains(className)
   }
   addClass(className) {
      this.#htmlElem.classList.add(className)
   }
   removeClass(className) {
      this.#htmlElem.classList.remove(className)
   }
   switchClass(isAdd, className) {
      new ViewElem(this.#htmlElem).switchClass(isAdd,className)
   }
   get initialPos() {
      return new ViewElem(this.#htmlElem).initialPos()
   }
   get connected() {
      return this.#htmlElem.isConnected
   }
   get displayed() {
      return this.connected && this.getComputedStyle().display !== "none"
   }
   get focus() {
      return DOM.focus(this.#htmlElem)
   }
   get focusWithin() {
      return DOM.focusWithin(this.#htmlElem)
   }
   get hidden() {
      return Boolean(this.#htmlElem.hidden)
   }
   set hidden(isHidden) {
      this.#htmlElem.hidden = isHidden
   }
}

/**
 * Stores state of an applied decorator.
 * Useful for debugging and/or undecorating HTML elements.
 */
class ViewDecorator {
   /** @type {ViewModel} */
   viewModel
   /** A subtype stores its controllers under this.controllers.subtypeName={}.
    * The name of a subtype is its class name without suffix Decorator.
    * Additional state is stored as this.subtypeNameState = {}.
    * All stored controllers are removed (freed) during undecorate() operation. */
   controllers = {}
   /**
    * Constructor which decorates an HTML element (creates ViewController and binds their action callbacks).
    * @param {ViewModel} viewModel - The view-model which contains the root view.
    * @param {object} options - Additional options.
    */
   constructor(viewModel, options={}) {
      this.decorate(viewModel, options)
   }
   /**
    * @param {ViewModel} viewModel
    * @param {object} [options]
    */
   decorate(viewModel, options={}) {
      if (! this.viewModel) {
         !(viewModel instanceof ViewModel) && VMX.throwErrorObject(this, "Argument viewModel is not of type ViewModel.")
         this.viewModel = viewModel
         this.decorateOnce(viewModel, options)
      }
   }
   undecorate() {
      const viewModel = this.viewModel
      if (viewModel) {
         this.undecorateOnce(viewModel)
         for (const controllers of Object.values(this.controllers)) {
            for (const controller of Object.values(controllers)) {
               if (controller instanceof ViewController)
                  controller.free()
            }
         }
         this.controllers = {}
         this.viewModel = null
      }
   }
   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /**
    * @param {ViewModel} viewModel
    * @param {object} [options]
    */
   decorateOnce(viewModel, options) { VMX.throwErrorObject(this, "decorateOnce(viewModel,options) not implemented in subtype.") }
   /**
    * @param {ViewModel} viewModel
    */
   undecorateOnce(viewModel) { VMX.throwErrorObject(this, "undecorateOnce(viewModel) not implemented in subtype.") }
}

class ResizeDecorator extends ViewDecorator {
   /**
    * @param {ViewModel} viewModel
    * @param {object} options
    */
   decorateOnce(viewModel, { moveable }) {
      const htmlElem = viewModel.htmlElem
      const width = { outer:10, border:15, edge:20 }
      const size = { border: width.border+"px", edge:width.edge+"px", outer:"-"+width.outer+"px" }
      const borders = [
         moveable && DOM.newDiv({top:size.outer, left:0, right:0, height:size.border, cursor:"n-resize", position:"absolute"}),
         DOM.newDiv({right:size.outer, top:0, bottom:0, width:size.border, cursor:"e-resize", position:"absolute"}),
         DOM.newDiv({bottom:size.outer, left:0, right:0, height:size.border, cursor:"s-resize", position:"absolute"}),
         moveable && DOM.newDiv({left:size.outer, top:0, bottom:0, width:size.border, cursor:"w-resize", position:"absolute"}),
         moveable && DOM.newDiv({top:size.outer, left:size.outer, width:size.edge, height:size.edge, cursor:"nw-resize", position:"absolute"}),
         moveable && DOM.newDiv({top:size.outer, right:size.outer, width:size.edge, height:size.edge, cursor:"ne-resize", position:"absolute"}),
         DOM.newDiv({bottom:size.outer, right:size.outer, width:size.edge, height:size.edge, cursor:"se-resize", position:"absolute"}),
         moveable && DOM.newDiv({bottom:size.outer, left:size.outer, width:size.edge, height:size.edge, cursor:"sw-resize", position:"absolute"}),
      ]
      htmlElem.append(...borders.filter(border=>Boolean(border)))
      const moveHtmlElem = viewModel.view.trySlot("move")?.htmlElem ?? htmlElem
      /** @type {null|{borders:any[], moveHtmlElem:HTMLElement, oldStyles:object|null, isResizeable:boolean }} */
      this.resizeState = { borders, moveHtmlElem, oldStyles:null, isResizeable:true }
      const moveAction = viewModel.bindAction("move",null,(e) => ({ dx:e.dx, dy:e.dy }))
      const resizeAction = viewModel.bindAction("resize",null,
         moveable ? (e) => ({ dTop:e.dy, dRight:e.dx, dBottom:e.dy, dLeft:e.dx })
                  : (e) => ({ dTop:0, dRight:2*e.dx, dBottom:2*e.dy, dLeft:0 }))
      const topAction = viewModel.bindAction("resize",[["top"]],(e)=>({ dTop:-e.dy }))
      const rightAction = viewModel.bindAction("resize",[["right"]],(e)=>({ dRight:e.dx }))
      const bottomAction = viewModel.bindAction("resize",[["bottom"]],(e)=>({ dBottom:e.dy }))
      const leftAction = viewModel.bindAction("resize",[["left"]],(e)=>({ dLeft:-e.dx }))
      const topleftAction = viewModel.bindAction("resize",[["top","left"]],(e)=>({ dTop:-e.dy, dLeft:-e.dx }))
      const toprightAction = viewModel.bindAction("resize",[["top","right"]],(e)=>({ dTop:-e.dy, dRight:e.dx }))
      const bottomleftAction = viewModel.bindAction("resize",[["bottom","left"]],(e)=>({ dBottom:e.dy, dLeft:-e.dx }))
      const bottomrightAction = viewModel.bindAction("resize",[["bottom","right"]],(e)=>({ dBottom:e.dy, dRight:e.dx }))
      this.controllers.resize = {
         move: moveable && new MoveController(moveHtmlElem,moveAction,{viewModel:viewModel}),
         resize: new TouchResizeController(htmlElem,resizeAction,{viewModel}),
         topResize: moveable && new MoveController(borders[0],topAction,{viewModel}),
         rightResize: new MoveController(borders[1],rightAction,{viewModel}),
         bottomResize: new MoveController(borders[2],bottomAction,{viewModel}),
         leftResize: moveable && new MoveController(borders[3],leftAction,{viewModel}),
         topLeftResize: moveable && new MoveController(borders[4],topleftAction,{viewModel}),
         topRightResize: moveable && new MoveController(borders[5],toprightAction,{viewModel}),
         bottomRightResize:  new MoveController(borders[6],bottomrightAction,{viewModel}),
         bottomLeftResize: moveable && new MoveController(borders[7],bottomleftAction,{viewModel}),
      }
      this.switchStyles()
   }

   undecorateOnce(viewModel) {
      if (this.resizeState) {
         this.switchStyles()
         for (const border of this.resizeState.borders)
            border && border.remove()
         this.resizeState = null
      }
   }

   /**
    * Changes cursor style of decorated element to "move" or restores old style (every call switches).
    */
   switchStyles() {
      if (this.resizeState) {
         if (this.resizeState.oldStyles)
            (DOM.setStyles(this.resizeState.moveHtmlElem, this.resizeState.oldStyles), this.resizeState.oldStyles=null)
         else if (this.controllers.resize.move)
            this.resizeState.oldStyles = DOM.setStyles(this.resizeState.moveHtmlElem, {cursor:"move"})
      }
   }
   setResizeable(isResizeable) {
      if (this.resizeState && this.resizeState.isResizeable !== Boolean(isResizeable)) {
         this.resizeState.isResizeable = Boolean(isResizeable)
         if (isResizeable) {
            for (const border of this.resizeState.borders)
               border && DOM.show(border)
         }
         else {
            for (const border of this.resizeState.borders)
               border && DOM.hide(border)
         }
         for (const controller of Object.values(this.controllers.resize)) {
            if (controller instanceof ViewController)
               controller.muted = !isResizeable
         }
         this.switchStyles()
      }
   }

}

class WindowDecorator extends ResizeDecorator {

   onMaximized({value:vWindowState}) {
      this.setResizeable(vWindowState != WindowVM.MAXIMIZED)
   }
   /**
    * @param {ViewModel} viewModel
    * @param {object} options
    */
   decorateOnce(viewModel, options) {
      if (viewModel instanceof WindowVM) {
         this.windowState = { onMaximized:this.onMaximized.bind(this) }
         super.decorateOnce(viewModel, options)
         const closeSlot=viewModel.view.trySlot("close")
         const minSlot=viewModel.view.trySlot("min")
         const maxSlot=viewModel.view.trySlot("max")
         this.controllers.window = {
            close: closeSlot && new ClickController(closeSlot.htmlElem, viewModel.bindAction("close"), {viewModel}),
            min: minSlot && new ClickController(minSlot.htmlElem, viewModel.bindAction("minimize"), {viewModel}),
            max: maxSlot && new ClickController(maxSlot.htmlElem, viewModel.bindAction("toggleMaximize"), {viewModel}),
         }
         viewModel.addPropertyListener("vWindowState",this.windowState.onMaximized)
      }
   }

   undecorateOnce(viewModel) {
      if (this.windowState) {
         viewModel.removePropertyListener("vWindowState",this.windowState.onMaximized)
         super.undecorateOnce(viewModel)
         this.windowState = null
      }
   }
}

class ViewModelActionInterface {
   /** @type {(args:undefined|{[name:string]:any})=>any} */
   action
   /**
    * @param {(args:undefined|{[name:string]:any})=>any} action
    */
   constructor(action) {
      this.action = action
      if (typeof action !== "function")
         return VMX.throwErrorObject(this, `Argument action is not of type function.`)
   }
   /**
    * @returns {boolean} *true*, if action was succesfully started (is locked).
    */
   isLocked() { return true }
   /**
    * @returns {boolean} *true*, if action is available in general.
    */
   isAvailable() { return true }
   /**
    * Action is called if lock could be acquired and action is available.
    * If {@link startAction} is not called previously the lock is acquired and released automatically (if possible).
    * @param {{[name:string]: any}} [args]
    */
   doAction(args) {
      if (!this.isAvailable()) return
      const isLocked = this.isLocked()
      if (isLocked || this.startAction()) {
         this.action(args)
         isLocked || this.endAction()
      }
   }
   /**
    * @returns {boolean} *true* in case of action could be locked.
    */
   startAction() {
      return true
   }
   /**
    * Releases action lock acquired by a call to {@link startAction}.
    */
   endAction() {
   }
}

// TODO: remove ViewActionAdapter
class ViewActionAdapter extends ViewModelActionInterface {
   /** @type {ViewModelActionInterface} vmAction */
   vmAction
   /**
    * @param {(args:undefined|{[name:string]:any})=>undefined|{[name:string]:any}} [mapArgs]
    * @param {ViewModelActionInterface} [vmAction]
    */
   constructor(mapArgs, vmAction) {
      super(mapArgs ? (args) => this.vmAction.doAction(mapArgs(args)) : (args) => this.vmAction.doAction(args))
      this.vmAction = vmAction ?? new ViewModelActionInterface(()=>undefined)
   }
   /**
    * @returns {boolean} *true*, if action was succesfully started.
    */
   isLocked() { return this.vmAction.isLocked() }
   /**
    * @param {{[name:string]: any}} [args]
    */
   doAction(args) { return this.vmAction.doAction(args) }
   /**
    * @returns {boolean} *true* in case of action could be locked.
    */
   startAction() { return this.vmAction.startAction() }
   /**
    * Releases action lock acquired by a call to {@link startAction}.
    */
   endAction() { this.vmAction.endAction() }
}



/**
 * Combines an action of a ViewModel with a lock.
 * Returned from calling {@link ViewModel.bindAction}
 */
class ViewModelAction extends ViewModelActionInterface {
   /** @type {ActionLock} */
   lock
   /** @type {undefined|ActionLock} */
   notLocked

   /**
    * @param {object} scope
    * @param {string[][]} lockPath
    * @param {undefined|boolean} shared *true*, if this is a shared lock. Else exclusive.
    * @param {(args:{[name:string]:any})=>void} vmAction
    */
   constructor(scope, lockPath, shared, vmAction) {
      super(vmAction)
      this.lock = new ActionLock(scope, lockPath, shared)
   }
   /**
    * @param {undefined|string[][]} lockPath
    * @returns {ViewModelAction}
    */
   setNotLocked(lockPath) {
      if (lockPath)
         this.notLocked = new ActionLock(this.lock.scope, lockPath)
      else if (this.notLocked)
         delete this.notLocked
      return this
   }
   isLocked() { return this.lock.isLocked() }
   isAvailable() { return !Boolean(this.notLocked?.isPathLocked()) }
   /**
    * @returns {boolean} *true* in case of locking succeeded.
    */
   startAction() {
      if (this.lock.lock())
         VMX.logInfoObject(this.lock.scope,`startAction: lock=${VMX.stringOf(this.lock.path)} OK << TODO: remove >>`)
      else
         VMX.logInfoObject(this.lock.scope,`startAction: lock=${VMX.stringOf(this.lock.path)} FAILED << TODO: remove >>`)
      return this.lock.isLocked()
   }
   /**
    * Unlocks action.
    */
   endAction() {
      if (this.lock.isLocked()) {
         VMX.logInfoObject(this.lock.scope,`Unlock ${VMX.stringOf(this.lock.path)}`)
         this.lock.unlock()
      }
   }
}

class ActionLock {
   /** @type {"-"} */
   static SUBPATH_SEPARATOR="-"
   /** @type {object} */
   scope
   /** @type {string[][]} */
   path
   /** @type {[ActionLockingPathNode,number][]} */
   changedPathNodes
   /** @type {boolean} */
   error
   /** @type {boolean} */
   shared
   /**
    * @param {object} scope
    * @param {string[][]} lockPath The contained arrays are not allowed to change afterwards (no copy is made).
    * @param {boolean} [shared]
    */
   constructor(scope, lockPath, shared) {
      this.scope = scope
      this.path = [...lockPath]
      this.changedPathNodes = []
      this.error = false
      this.shared = Boolean(shared)
   }
   signalError() { this.error = true }
   isError() { return this.error }
   /**
    * @returns {boolean} *true*, if this lock was acquired ({@link lock} was called successfully).
    */
   isLocked() { return this.changedPathNodes.length !== 0 }
   /**
    * @returns {boolean} *true*, if path of this lock is locked by any lock.
    */
   isPathLocked() { return VMX.vmActionLocking.isPathLocked(this) }
   /**
    * @param {ActionLockingPathNode} node
    * @param {number} nrAdded
    */
   addChange(node, nrAdded) {
      this.changedPathNodes.push([node, nrAdded])
   }
   lock() {
      return VMX.vmActionLocking.lock(this)
   }
   unlock() {
      for (const tuple of this.changedPathNodes) {
         tuple[0].undoChange(this.scope, tuple[1])
      }
      this.changedPathNodes = []
      this.error = false
   }
   pathAsString() { return this.path.map(sp=>sp.join()).join(ActionLock.SUBPATH_SEPARATOR) }
}

class ActionLockingPathNode {
   /** @type {number} */
   level
   /** @type {Map<string,ActionLockingPathNode|null>} */
   subNodes
   /** @type {Map<object,number>} Number 0 means this and all subNodes are locked for this scope. Number < 0 means abs(number) different shared locks hold a lock on this and all subNodes for this scope. No entry means nothing is locked. A number > 0 means there exists this number of subnodes where this scope holds a lock. */
   lockedScopes
   /**
    * @param {number} level
    */
   constructor(level) {
      this.level = level
      this.subNodes = new Map()
      this.lockedScopes = new Map()
   }
   /**
    * @param {ActionLock} lock Contains scope, path, and changed node. If an error occurred somewhere in the path the error flag is set. But changes are not undone.
    * @return {number} The number of locks added to this node and its subnodes.
    */
   tryLock(lock) {
      const last = (lock.path.length <= this.level)
      const nrSubNodes = this.lockedScopes.get(lock.scope)
      let nrAdded = 0
      if (last && (nrSubNodes === undefined || (lock.shared && nrSubNodes < 0))) {
         const change = lock.shared ? -1 : 0
         this.lockedScopes.set(lock.scope, (nrSubNodes??0)+change)
         lock.addChange(this,change)
         nrAdded = 1
      }
      else if (!last && (nrSubNodes === undefined || nrSubNodes > 0)) {
         for (const action of lock.path[this.level]) {
            let subNode = this.subNodes.get(action)
            if (!subNode) {
               subNode = new ActionLockingPathNode(this.level+1)
               this.subNodes.set(action, subNode)
            }
            nrAdded += subNode.tryLock(lock)
            if (lock.isError()) break
         }
         this.lockedScopes.set(lock.scope, (nrSubNodes??0)+nrAdded)
         lock.addChange(this,nrAdded)
      }
      else
         lock.signalError()
      return nrAdded
   }
   /**
    * @param {object} scope
    * @param {number} nrAdded
    */
   undoChange(scope, nrAdded) {
      const nrSubNodes = this.lockedScopes.get(scope)
      if (nrSubNodes === undefined || (nrAdded<0)!==(nrSubNodes<0)) return VMX.throwErrorObject(this, "Invalid node state")
      if (nrAdded !== nrSubNodes)
         this.lockedScopes.set(scope, nrSubNodes-nrAdded)
      else
         this.lockedScopes.delete(scope)
   }
   /**
    * @param {ActionLock} lock Contains scope and path to test for.
    * @return {boolean} *true*, if an exclusive or shared lock is hold on any subpath.
    */
   isPathLocked(lock) {
      const last = (lock.path.length <= this.level)
      const nrSubNodes = this.lockedScopes.get(lock.scope)
      if (nrSubNodes !== undefined) {
         if (last || nrSubNodes <= 0) return true
         for (const action of lock.path[this.level]) {
            if (this.subNodes.get(action)?.isPathLocked(lock))
               return true
         }
      }
      return false
   }
   /**
    * @param {ActionLock} lock Contains scope and path to test for.
    * @return {boolean} *true*, if this lock could be acquired for all subpath.
    */
   isLockable(lock) {
      const last = (lock.path.length <= this.level)
      const nrSubNodes = this.lockedScopes.get(lock.scope)
      if (nrSubNodes !== undefined) {
         if (last) return (lock.shared && nrSubNodes < 0)
         for (const action of lock.path[this.level]) {
            const subNode = this.subNodes.get(action)
            if (subNode && !subNode.isLockable(lock))
               return false
         }
      }
      return true
   }
}

class ViewModelActionLocking {
   /** @type {ActionLockingPathNode} */
   rootNode

   constructor() {
      this.rootNode = new ActionLockingPathNode(0)
   }
   /**
    * Tries to acquire the given lock. If lock is already acquired nothing is done.
    * @param {ActionLock} lock The lock to acquire.
    * @returns {boolean} *true*, if lock is acquired.
    */
   lock(lock) {
      if (!lock.isLocked()) {
         this.rootNode.tryLock(lock)
         if (lock.isError())
            lock.unlock()
      }
      return lock.isLocked()
   }
   /**
    * @param {object} lock
    * @returns {boolean} *true*, if any subpath of given lock is hold by a shared or exclusive lock.
    */
   isPathLocked(lock) { return this.rootNode.isPathLocked(lock) }
}


class ViewModelTypeClass extends TypeClass {
   InterfaceLogID = new InterfaceLogID({})
}

/**
 * Model which is bound to a single view and manages its state.
 */
class ViewModel /* implements ViewModelActionInterface */ {
   static #typeClass = new ViewModelTypeClass(ViewModel,TypeClass.VIEWMODEL)
   static get typeClass() { return ViewModel.#typeClass }
   get typeClass() { return ViewModel.#typeClass }
   /**
    * @typedef ViewModelConfig Default configurations (for all instances) which could be overwritten in subclass.
    * @property {null|(new(vm:ViewModel,options:object)=>ViewDecorator)} decorator Constructor of decorator which adds additional elements and controllers to the view model.
    * @property {Set<string>} listenableProperties Defines set of valid property names which could be observed for change.
    */
   /** @type {ViewModelConfig} */
   static Config = { decorator: null, listenableProperties: new Set(["hidden","connected"]) }
   /** @type {HTMLElement} */
   #htmlElem
   /** @type {View} */
   #view
   /** @type {null|undefined|ViewDecorator} */
   #decorator
   #propertyListeners = {}
   /** @type {LogID} */
   LID
   /**
    *
    * @param {HTMLElement} htmlElem
    * @param {{decorator?:null|(new(vm:ViewModel,options:object)=>ViewDecorator),[o:string]:any}} options
    * @returns
    */
   static create(htmlElem, { decorator, ...initOptions }={}) {
      const config = this.Config
      const vmodel = new this(htmlElem)
      const decoOptions = vmodel.init(initOptions)
      vmodel.decorate(decorator ?? config.decorator, decoOptions)
      return vmodel
   }
   /**
    * @param {HTMLElement} htmlElem
    */
   constructor(htmlElem) {
      this.#htmlElem = htmlElem
      this.#view = new View(htmlElem)
      this.LID = new LogID(this, htmlElem)
   }
   free() {
      const htmlElem = this.htmlElem
      this.#decorator?.undecorate?.()
      this.LID.unbind(this)
      this.#htmlElem = null
      this.#view = null
      this.#decorator = null
      this.#propertyListeners = {}
      return htmlElem
   }
   /**
    *
    * @param {null|(new(vm:ViewModel,options:object)=>ViewDecorator)} decorator
    * @param {object} decoratorOptions
    */
   decorate(decorator, decoratorOptions) {
      if (decorator != null && !this.#decorator) {
         if (typeof decorator !== "function" || !VMX.isConstructor(decorator) || !(decorator.prototype instanceof ViewDecorator))
            VMX.logErrorObject(this, `Option »decorator« expects subclass of ViewDecorator not »${String(typeof decorator === "function" ? decorator.name : decorator).substring(0,30)}«.`)
         else
            this.#decorator = new decorator(this, decoratorOptions)
      }
   }
   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /**
    * Initializes view model after constructor has been run.
    * Sets config and option values.
    *
    * @param {{[o:string]:any}} initOptions
    */
   init(initOptions) {
      for (const [ name, value ] of Object.entries(initOptions)) {
         this.setOption(name, value)
      }
   }
   /**
    * Sets certain value of this ViewModel.
    * Overwrite setOption in subclass to support more options.
    * Supported names are one of ["connected","hidden"].
    * @param {string} name Name of option (or config value).
    * @param {any} value Value of option.
    */
   setOption(name, value) {
      switch (name) {
         case "connected": this.connected = value; break;
         case "hidden": this.hidden = value; break;
         default: VMX.logErrorObject(this, `Unsupported option »${String(name)}«.`); break; // in subclass: super.setOption(name, value)
      }
   }
   /**
    * @param {string} action
    * @param {null|string[][]} [subactions]
    * @param {(args: object)=>object} [mapArgs]
    * @returns {ViewModelAction} Bound action which calls the correct view model method.
    */
   bindAction(action, subactions, mapArgs) { return VMX.throwErrorObject(this, "bindAction not implemented") }
   //
   // Config & Options (could also change state)
   //
   /**
    * @template {keyof ViewModelConfig} T
    * @param {T} name
    * @returns {ViewModelConfig[T]} The configured value.
    */
   config(name) {
      if (ViewModel.Config[name])
         return ViewModel.Config[name]
      return VMX.throwErrorObject(this, `Unsupported config »${name}«.`)
   }
   /**
    * @template {keyof ViewModelConfig} T
    * @param {T} name
    * @returns {undefined|ViewModelConfig[T]} The configured value.
    */
   tryConfig(name) {
      if (ViewModel.Config[name])
         return ViewModel.Config[name]
   }
   setOptions(options={}) {
      for (const name in options)
         this.setOption(name, options[name])
      return this
   }
   //
   // State
   //
   get name() {
      return this.LID.toString(this)
   }
   set name(name) {
      VMX.throwErrorObject(this, "Not supported (TODO: remove).")
   }
   get htmlElem() {
      return this.#htmlElem
   }
   get view() {
      return this.#view
   }
   get decorator() {
      return this.#decorator
   }
   /**
    * @type {{parent:null|Node, nextSibling:null|Node}}
    */
   get connectedTo() {
      // !htmlElem.isConnected ==> parent == nextSibling == null
      const htmlElem = this.htmlElem
      return { parent: htmlElem.parentNode, nextSibling: htmlElem.nextSibling }
   }
   /**
    * @param {undefined|null|{parent:null|Node, nextSibling:null|Node}} connectTo
    */
   set connectedTo(connectTo) {
      const htmlElem = this.htmlElem
      const isConnected = htmlElem.isConnected
      if (!connectTo?.parent) {
         if (htmlElem instanceof HTMLElement)
            htmlElem.remove()
      }
      else if (htmlElem.parentNode !== connectTo.parent || htmlElem.nextSibling !== connectTo.nextSibling)
         connectTo.parent.insertBefore(htmlElem, connectTo.parent === connectTo.nextSibling?.parentNode ? connectTo.nextSibling : null )
      if (isConnected != htmlElem.isConnected)
         this.notifyPropertyListener("connected",htmlElem.isConnected)
   }
   get connected() {
      return this.htmlElem.isConnected
   }
   set connected(isConnect) {
      const htmlElem = this.htmlElem
      if (isConnect != htmlElem.isConnected) {
         if (isConnect)
            document.body.append(htmlElem)
         else if (htmlElem instanceof HTMLElement)
            htmlElem.remove()
         this.notifyPropertyListener("connected",isConnect)
      }
   }
   get hidden() {
      return this.view.hidden
   }
   set hidden(isHidden) {
      if (isHidden != this.hidden) {
         this.view.hidden = isHidden
         this.notifyPropertyListener("hidden",isHidden)
      }
   }
   onceOnViewUpdate(delayCount, callback) {
      VMX.viewUpdateListener.onceOnViewUpdate(delayCount, callback)
   }
   //
   // Property Change Listener
   //
   listenableNames() {
      return [...this.config("listenableProperties").keys()]
   }
   /**
    * @param {string} name
    */
   assertListenableName(name) {
      if (!this.config("listenableProperties").has(name))
         VMX.throwErrorObject(this, `Unsupported listenable '${name}'.`)
   }
   /**
    * Notifies all listeners of a property that its value was changed.
    * Every caller must ensure that cleaning up code is run in a finally clause
    * to ensure that an exception thrown by a listener does not mess up internal state.
    * @param {string} name - Name of listenable property
    * @param {any} newValue - New value of property after it has changed.
    */
   notifyPropertyListener(name, newValue) {
      this.assertListenableName(name)
      const event = { vm:this, value:newValue, name }
      this.#propertyListeners[name]?.forEach( callback => callback(event))
   }
   addPropertyListener(name, callback) {
      this.assertListenableName(name)
      this.#propertyListeners[name] ??= []
      this.#propertyListeners[name].push(callback)
   }
   removePropertyListener(name, callback) {
      this.assertListenableName(name)
      const listeners = this.#propertyListeners[name]?.filter( cb => cb !== callback)
      if (listeners)
         this.#propertyListeners[name] = listeners
   }
}

class ResizeVM extends ViewModel {
   /** @type {ViewModelConfig} */
   static Config = { ...super.Config, decorator: ResizeDecorator, }

   constructor(htmlElem) {
      super(htmlElem)
   }

   /**
    * Initializes object after constructor has been completed.
    * Set moveable to false if moving window is to be disabled (resizing is supported).
    * Set moveable to true if moving window should be enabled for style="position:relative".
    * @param {{moveable?:Node|string, [o:string]:any}} options
    * @returns {object} Options for decorator
    */
   init({moveable, ...initOptions}) {
      const pos = this.view.initialPos
      super.init({ left:pos.left, top:pos.top, width:pos.width, height:pos.height, ...initOptions})
      const decoOptions = { moveable: moveable ?? this.moveable(pos.position) }
      return decoOptions
   }

   config(name) {
      if (ResizeVM.Config[name])
         return ResizeVM.Config[name]
      return super.config(name)
   }

   setOption(name, value) {
      switch (name) {
         case "left": this.vLeft = value; break;
         case "height": this.vHeight = value; break;
         case "top": this.vTop = value; break;
         case "width": this.vWidth = value; break;
         default: super.setOption(name, value); break;
      }
   }

   /**
    * @param {string} name
    * @param {number} value
    */
   setPosAttr(name, value) {
      if (isFinite(value) && this.htmlElem instanceof HTMLElement)
         this.htmlElem.style[name] = this.roundPosValue(value)+"px"
   }
   /**
    * @param {string} name
    * @returns {undefined|number}
    */
   getPosAttr(name) {
      if (this.htmlElem instanceof HTMLElement)
         return ViewElem.parseFloatDefault(this.htmlElem.style[name],0)
   }
   roundPosValue(value) {
      return Math.round(value*1000)/1000
   }
   get pos() {
      return { width:this.getPosAttr("width"), height:this.getPosAttr("height"), top:this.getPosAttr("top"), left:this.getPosAttr("left") }
   }
   set pos(pos) {
      for (const name of ["width","height","top","left"]) {
         isFinite(pos[name]) && this.setPosAttr(name, pos[name])
      }
   }
   /**
    * @param {number} value
    */
   set vHeight(value/*pixels*/) {
      this.setPosAttr("height", (value < 0 ? 0 : value))
   }
   /**
    * @returns {number}
    */
   get vHeight() {
      return this.getPosAttr("height") ?? -1
   }
   /**
    * @param {number} value
    */
   set vWidth(value/*pixels*/) {
      this.setPosAttr("width", (value < 0 ? 0 : value))
   }
   /**
    * @returns {number}
    */
   get vWidth() {
      return this.getPosAttr("width") ?? -1
   }
   /**
    * @param {number} value
    */
   set vTop(value/*pixels*/) {
      this.setPosAttr("top", value)
   }
   /**
    * @returns {number}
    */
   get vTop() {
      return this.getPosAttr("top") ?? -1
   }
   /**
    * @param {number} value
    */
   set vLeft(value/*pixels*/) {
      this.setPosAttr("left", value)
   }
   /**
    * @returns {number}
    */
   get vLeft() {
      return this.getPosAttr("left") ?? -1
   }
   /**
    * @param {string} [position]
    * @returns {boolean} True in case "position" attribute of style could be positioned with "left" and "top" styles.
    */
   moveable(position) {
      return ["fixed","absolute"].includes(position ?? getComputedStyle(this.htmlElem)["position"])
   }
   /**
    * @param {number} deltax
    * @param {number} deltay
    */
   move(deltax,deltay) { this.vLeft += deltax; this.vTop += deltay; }
   /**
    * @param {undefined|number} dTop Resizes top by dTop pixel. A negative value shrinks the size.
    * @param {undefined|number} dRight Resizes right by dRight pixel. A negative value shrinks the size.
    * @param {undefined|number} dBottom Resizes bottom by dBottom pixel. A negative value shrinks the size.
    * @param {undefined|number} dLeft Resizes left by dLeft pixel. A negative value shrinks the size.
    */
   resize(dTop, dRight, dBottom, dLeft) {
      if (dTop) { this.vTop -= dTop; this.vHeight += dTop }
      if (dRight) { this.vWidth += dRight }
      if (dBottom) { this.vHeight += dBottom }
      if (dLeft)  { this.vLeft -= dLeft; this.vWidth += dLeft }
   }
}

class ViewExtension {
   /**
    * Frees resources and possibly undo changes.
    * @param {View2} view
    * @returns {void}
    */
   free(view) { return VMX.throwErrorObject(this,`free not implemented.`) }
}

class ViewDecorator2 {
   /**
    * @typedef ViewDecorator2Extension
    * @property {{[option:string]:string}} initOptions
    */
   /** @type {ViewDecorator2Extension} */
   static Extension = {
      initOptions: {}
   }
   /**
    * @param {View2} view
    * @param {object} options
    * @param {ViewRestrictions} restrictions
    * @returns {undefined|ViewDecorator2} Returns created ViewDecorator2 object or undefined if initOptions prevents creation.
    */
   static init(view, options, restrictions) { return VMX.throwErrorStatic(this,`init not implemented.`) }
   /**
    * Frees resources and possibly undo changes.
    * @param {View2} view
    * @returns {void}
    */
   free(view) { return VMX.throwErrorObject(this,`free not implemented.`) }
   /**
    * @param {View2} view
    * @param {{[action:string]:ViewController|null|undefined|false}} controllers
    */
   connectActions(view, controllers) {
      const connector = view.modelConnector
      for (const [action,controller] of Object.entries(controllers)) {
         if (controller instanceof ViewController)
            controller.connectToAction(connector.getVMAction(action))
      }
   }
}

class View2TypeClass extends TypeClass {
   InterfaceLogID = new InterfaceLogID({ getParent: (owner) => owner instanceof View2 ? owner.viewParent : undefined })
}

class View2 {
   static #typeClass = new View2TypeClass(View2,TypeClass.VIEW)
   static get typeClass() { return View2.#typeClass }
   get typeClass() { return View2.#typeClass }
   /**
    * @typedef View2Config
    * @property {{connection:string,[option:string]:string}} initOptions Supported init options.
    * @property {{[style:string]:string, "transition-property":string, "transition-duration":string }} transitionStyle Transition style for resize and change position animation.
    */
   /**
    * Basic view Configuration. Should be copied in subtype and extended or overwritten accordingly.
    * @type {View2Config}
    */
   static Config = {
      initOptions:{
         "connection":"Defines connections between properties of view to properties of view-model.",
         "connector":"Optional implementation of ViewViewModelConnectorInterface which connects view and view-model.",
         "viewModel":"Optional implementation of ViewModel2 which is used to create a default implementation of ViewViewModelConnectorInterface.",
      },
      transitionStyle:{ "transition-delay":"0s", "transition-duration":"0.2s", "transition-property":"all", "transition-timing-function":"ease-out" }
   }
   /**
    * @template {View2} T
    * @this {new(...args:any[])=>T} Either View2 or a derived subtype.
    * @param {string} htmlString
    * @returns {T} A View2 (or derived type) which wraps the supplied HTML Element created from the provided HTML string.
    */
   static fromHtml(htmlString) {
      return new this(ViewElem.fromHtml(htmlString))
   }
   /**
    * @template T extends View2
    * @this {new(...args:any[])=>T} Either View2 or a derived subtype.
    * @param {string} textString
    * @returns {T} A View2 (or derived type) which wraps the supplied HTML Text node created from the provided text string.
    */
   static fromText(textString) {
      return new this(ViewElemText.fromText(textString))
   }
   /** @type {ViewElem} */
   #viewElem
   /** @type {undefined|ViewViewModelConnectorInterface} */
   #modelConnector
   /** @type {{[name: string]: ViewExtension|ViewDecorator2}} */
   #extensions
   /** @type {LogID} */
   LID

   /**
    * @param {ViewElem} viewElem
    */
   constructor(viewElem) {
      this.#viewElem = ViewElem.assertType(viewElem)
      this.#extensions = {}
      this.LID = new LogID(this, viewElem.htmlElem)
   }
   /**
    * @param {{connector?:ViewViewModelConnectorInterface,viewModel?:ViewModel2,connection:ViewViewModelConnectionLiteral,[option:string]:any}} initOptions
    * @return {View2}
    */
   init(initOptions) {
      const connector = this.#modelConnector ?? this.createConnector(initOptions)
      this.validateInitOptions(initOptions,this.config.initOptions)
      connector.connect(initOptions.connection,() => this.subinit(initOptions))
      return this
   }
   /**
    * Frees bound resources.
    */
   free() {
      this.LID.unbind(this)
      // TODO: this.#viewModel.unbindView(this)
   }
   /**
    * @param {{[option:string]:any}} initOptions
    * @param {{[option:string]:string}} allowedOptions
    */
   validateInitOptions(initOptions, allowedOptions) {
      for (const name in initOptions) {
         if (!(name in allowedOptions))
            VMX.throwErrorObject(this, `Unknown init option »${String(name)}«.`)
      }
   }
   /**
    * @param {ViewViewModelConnectorInterface} connector
    */
   connectToModel(connector) {
      this.#modelConnector = ViewViewModelConnectorInterface.assertType(connector,"connector")
   }
   /**
    * @param {{connector?:ViewViewModelConnectorInterface,viewModel?:ViewModel2}} initOptions
    * @returns {ViewViewModelConnectorInterface}
    */
   createConnector(initOptions) {
      const connector = (initOptions.connector instanceof ViewViewModelConnectorInterface && initOptions.connector)
                        || (initOptions.viewModel instanceof ViewModel2 && new ViewViewModelConnector(this, initOptions.viewModel))
      if (!connector)
         return VMX.throwErrorObject(this, `Expect initOption connector of type ViewViewModelConnectorInterface or initOption vm of type ViewModel2 instead of »${VMX.typeof(initOptions.connector)}«,»${VMX.typeof(initOptions.viewModel)}«.`)
      return connector
   }
   // TODO: rename htmlElements -> viewElements with type ViewElem[]
   /** @type {(HTMLElement)[]} */
   get htmlElements() {
      return [ this.#viewElem.htmlElem ]
   }
   /**
    * @returns {View2.Config}
    */
   get config() { return View2.Config }
   /**
    * @returns {boolean} True in case view is connected to the document.
    */
   get connected() { return this.#viewElem.connected() }
   /**
    * @returns {ViewViewModelConnectorInterface}
    */
   get modelConnector() { return this.#modelConnector || VMX.throwErrorObject(this, `Call connectToModel first.`) }
   /**
    * @return {boolean}
    */
   get hidden() { return this.#viewElem.hidden }
   /**
    * @param {boolean} hidden *true*, if view should be hidden. *false*, if be shown.
    */
   set hidden(hidden) { this.#viewElem.hidden = hidden }
   /**
    * @returns {ViewElem}
    */
   get viewElem() { return this.#viewElem }
   /**
    * @returns {undefined|ImplementsInterfaceLogID}
    */
   get viewParent() { return this.#modelConnector?.viewParent() }
   /**
    * @param {string} slot
    * @returns {undefined|ViewElem}
    */
   trySlot(slot) { return this.viewElem.trySlot(slot) }
   /**
    * @template V
    * @param {string} property
    * @param {V} value
    */
   notifyUpdate(property, value) {
      this.modelConnector.onUpdateNotification({from:this, property, value})
   }
   ///////////////////////
   // Extension Support //
   ///////////////////////
   /**
    * @template T
    * @param {T&(ViewExtension|ViewDecorator2)} extension
    * @return {T}
    */
   extend(extension) {
      const name = extension.constructor.name
      if (typeof name !== "string")
         return VMX.throwErrorObject(this, `extension has no name.`)
      if (this.#extensions[name])
         return VMX.throwErrorObject(this, `extension already exists.`)
      return this.#extensions[name] = extension
   }
   /**
    * @template {ViewDecorator2} T
    * @param {{name:string, init:(view:View2,initOptions:object,restrictions:ViewRestrictions)=>undefined|T}} decoratorClass
    * @param {object} initOptions
    * @param {ViewRestrictions} restrictions Decorator could add additional properties which restricts the view (describes the view configuration).
    */
   decorate(decoratorClass, initOptions, restrictions) {
      const decorator = decoratorClass.init(this, initOptions, restrictions)
      if (decorator) this.extend(decorator)
   }
   /**
    * @template {ViewExtension|ViewDecorator2} T
    * @param {new(...args:any[])=>T} extensionClass
    * @returns {undefined|T}
    */
   getExtension(extensionClass) {
      const name = extensionClass.name
      if (typeof name !== "string")
         return VMX.throwErrorObject(this, `extensionClass has no name.`)
      if (this.#extensions[name] instanceof extensionClass)
         return this.#extensions[name]
   }
   /**
    * @template {ViewExtension|ViewDecorator2} T
    * @param {new(...args:any[])=>T} extensionClass
    * @returns {undefined|T}
    */
   removeExtension(extensionClass) {
      const extension = this.getExtension(extensionClass)
      if (extension) {
         delete this.#extensions[extensionClass.name]
         return extension
      }
   }

   /////////////////
   // Transitions //
   /////////////////

   // TODO: implement

   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /** @typedef {{[property:string]:any}} ViewRestrictions */
   /**
    * @param {{[option:string]:any}} initOptions
    * @return {ViewRestrictions} Restricted properties which are of fixed value which forces the view-model to adapt accordingly.
    */
   subinit(initOptions) { return VMX.throwErrorObject(this, "subinit(initOptions) not implemented in subtype.") }

}

class ReframeViewDecorator2 extends ViewDecorator2 {
   /**
    * @typedef ReframeViewDecorator2_Imports
    * @property {ViewElem} viewElem
    * @property {undefined|ViewElemText|ViewElem} content
    */
   static Extension = {
      initOptions: {
         content: "The ViewElem whose position within the document is replaced by the window frame.",
         reframe: "True activates extension which means window frame replaces the content within the document during init. The content must be connected to the document."
      },
   }
   /** @type {undefined|ViewElem_InitialPos} */
   initialPos
   /** @type {undefined|object} */
   oldContentStyles
   /** @type {undefined|object} */
   oldViewStyles
   /**
    * @param {any} content
    * @returns {content is ViewElem}
    */
   static assertContentSupported(content) {
      if (!(content instanceof ViewElem && content.connected()))
         return VMX.throwErrorObject(this, `Expect »content« to be of type ViewElem and connected to the document.`)
      return true
   }
   /**
    * @param {View2&ReframeViewDecorator2_Imports} view
    * @param {{reframe:boolean, content:ViewElem}} options
    * @returns {undefined|ReframeViewDecorator2}
    */
   static init(view, { reframe, content }) {
      if (reframe) {
         return new ReframeViewDecorator2(view, content)
      }
   }
   /**
    * @param {ReframeViewDecorator2_Imports} view
    * @param {ViewElem} content
    */
   constructor(view, content) {
      super()
      ReframeViewDecorator2.assertContentSupported(content)
      const pos = content.initialPos()
      this.initialPos = pos
      this.oldContentStyles = content.setStyles({ width:"100%", height:"100%", position:"static" })
      this.oldViewStyles = view.viewElem.setStyles({ position:pos.position })
      content.replaceWith(view.viewElem)
      view.viewElem.setPos({ left:pos.left, top:pos.top, width:pos.width, height:pos.height })
   }
   /**
    * @param {View2&ReframeViewDecorator2_Imports} view
    */
   free(view) {
      if (this.initialPos) {
         const content = view.content
         if (ReframeViewDecorator2.assertContentSupported(content)) {
            view.viewElem.replaceWith(content)
            content.updateStyles(this.oldContentStyles)
            view.viewElem.updateStyles(this.oldViewStyles)
            content.setPos(this.initialPos)
         }
         this.initialPos = undefined
      }
   }
}

class MoveResizeViewDecorator2 extends ViewDecorator2 {
   /**
    * @typedef MoveResizeViewDecorator2_CalledActions
    * @property {(e:MoveControllerEvent)=>void} move moveable set to true.
    * @property {(e:TouchResizeControllerEvent)=>void} resize resizeable and moveable set to true.
    * @property {(e:TouchResizeControllerEvent)=>void} resizeNotMoveable resizeable set to true and moveable set to false.
    * @property {(e:MoveControllerEvent)=>void} resizeTop resizeable and moveable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeTopRight resizeable and moveable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeRight resizeable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeRightBottom resizeable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeBottom resizeable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeBottomLeft resizeable and moveable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeLeft resizeable and moveable set to true.
    * @property {(e:MoveControllerEvent)=>void} resizeLeftTop resizeable and moveable set to true.
    */
   static Extension = {
      initOptions: {
         moveable: "boolean value. Set to true, if view should support moving around.",
         resizeable: "boolean value. Set to true, if view should support resizing around.",
      },
   }
   /**
    * @param {View2} view
    * @param {{moveable?:boolean, resizeable?:boolean, [o:string]:any}} options
    * @param {ViewRestrictions} restrictions
    * @returns {undefined|MoveResizeViewDecorator2}
    */
   static init(view, {moveable, resizeable}, restrictions) {
      moveable = Boolean(moveable) && view.viewElem.moveable()
      restrictions.moveable = moveable
      restrictions.resizeable = Boolean(resizeable)
      if (moveable || resizeable) {
         return new MoveResizeViewDecorator2(view, Boolean(moveable), Boolean(resizeable))
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
    * @param {View2} view
    * @param {boolean} moveable
    * @param {boolean} resizeable
    */
   constructor(view, moveable, resizeable) {
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
      this.moveElem = view.trySlot("move") ?? view.viewElem
      this.controllers = {
         move: moveable && new MoveController(this.moveElem.htmlElem,null,{view}),
         resize: resizeable && moveable && new TouchResizeController(viewElem.htmlElem,null,{view}),
         resizeNotMoveable: resizeable && !moveable && new TouchResizeController(viewElem.htmlElem,null,{view}),
         resizeTop: this.borders[0] && new MoveController(this.borders[0].htmlElem,null,{view}),
         resizeRight: this.borders[1] && new MoveController(this.borders[1].htmlElem,null,{view}),
         resizeBottom: this.borders[2] && new MoveController(this.borders[2].htmlElem,null,{view}),
         resizeLeft: this.borders[3] && new MoveController(this.borders[3].htmlElem,null,{view}),
         resizeTopRight: this.borders[4] && new MoveController(this.borders[4].htmlElem,null,{view}),
         resizeRightBottom: this.borders[5] && new MoveController(this.borders[5].htmlElem,null,{view}),
         resizeBottomLeft: this.borders[6] && new MoveController(this.borders[6].htmlElem,null,{view}),
         resizeLeftTop: this.borders[7] && new MoveController(this.borders[7].htmlElem,null,{view}),
      }
      this.connectActions(view, this.controllers)
      this.oldStyles = null
      this.#setMoveable(moveable)
      // TODO: implement notification interface
      // viewModel.addPropertyListener("isMoveResizeable",this.setEnable.bind(this))
   }
   /**
    * @param {View2} view
    */
   free(view) {
      if (this.controllers) {
         this.setEnable(false)
         for (const border of this.borders)
            if (border) border.remove()
         for (const controller of Object.values(this.controllers))
            if (controller) controller.free()
         this.controllers = null
      }
   }
   /**
    * @param {boolean} isEnabled
    */
   setEnable(isEnabled) {
      this.#setMoveable(isEnabled)
      this.#setResizeable(isEnabled)
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
      if (this.controllers?.resize && this.controllers.resize.muted !== isResizeable) {
         if (isResizeable) {
            for (const border of this.borders)
               if (border) border.show()
         }
         else {
            for (const border of this.borders)
               if (border) border.hide()
         }
         this.controllers.resize.muted = isResizeable
      }
   }
}

class WindowControlsDecorator2 extends ViewDecorator2 {
   /**
    * @typedef WindowControlsDecorator2_CalledActions
    * @property {(e:TouchControllerEvent&{type:TouchController.CLICK})=>void} close
    * @property {(e:TouchControllerEvent&{type:TouchController.CLICK})=>void} minimize
    * @property {(e:TouchControllerEvent&{type:TouchController.CLICK})=>void} toggleMaximize
    */
   static Extension = {
      initOptions: {
         controls: "boolean value. Set to true, if view should support window controls.",
      },
   }
   /**
    * @param {View2} view
    * @param {{controls?:boolean, [o:string]:any}} options
    * @param {ViewRestrictions} restrictions
    * @returns {undefined|WindowControlsDecorator2}
    */
   static init(view, {controls}, restrictions) {
      if (controls && ["close","min","max"].some(slotName => view.trySlot(slotName)))
         return new WindowControlsDecorator2(view)
   }
   /** @type {null|{[name:string]:undefined|ClickController}} */
   controllers
   /**
    * @param {View2} view
    */
   constructor(view) {
      super()
      const [ closeSlot, minSlot, maxSlot ] = [ view.trySlot("close"), view.trySlot("min"), view.trySlot("max") ]
      this.controllers = {
         close: closeSlot && new ClickController(closeSlot.htmlElem, null, {view}),
         minimize: minSlot && new ClickController(minSlot.htmlElem, null, {view}),
         toggleMaximize: maxSlot && new ClickController(maxSlot.htmlElem, null, {view}),
      }
      this.connectActions(view, this.controllers)
   }
   /**
    * @param {View2} view
    */
   free(view) {
      if (this.controllers) {
         for (const controller of Object.values(this.controllers))
            if (controller) controller.free()
         this.controllers = null
      }
   }
}

class WindowView extends View2 {
   /**
    * WindowView_CalledActions enumerates all called actions.
    * @typedef {MoveResizeViewDecorator2_CalledActions & WindowControlsDecorator2_CalledActions} WindowView_CalledActions
    */
   /**
    * @typedef WindowViewConfig
    * @property {string} maximizedClass Class name which is set on frame if window is maximized.
    * @property {{content:string, controls:string, moveable:string, reframe:string, resizeable:string}} initOptions
    */
   /** @type {View2Config&WindowViewConfig}. */
   static Config = { ...super.Config,
      maximizedClass: "maximized",
      initOptions: {...super.Config.initOptions,
         ...ReframeViewDecorator2.Extension.initOptions,
         ...MoveResizeViewDecorator2.Extension.initOptions,
         ...WindowControlsDecorator2.Extension.initOptions,
         content: "The ViewElem which is set as child of the content slot."
      }
   }
   /** Optional slot names which could be exported in the window frame by "data-slot=<name>". */
   static Slots = { content:"content", title:"title", close:"close", min:"min", max:"max"}
   /////////////////////
   // Overwrite View2 //
   /////////////////////
   /**
    * @param {ViewElem} viewElem
    */
   constructor(viewElem) {
      super(viewElem)
   }
   /**
    * @typedef WindowViewInitOptions
    * @property {null|ViewElem|ViewElemText} content ViewElem which is set as child of the content slot.
    * @property {boolean} reframe *true*, window frame replaces content within the document. The content must be set and connected to the document.
    * @property {boolean} moveable *true*, window frame can be moved if ViewElem supports it (HTML position is "absolute","fixed",or "relative")
    * @property {boolean} resizeable
    * @property {boolean} controls
    */
   /**
    * @param {WindowViewInitOptions} initOptions
    * @return {ViewRestrictions} Restricted properties which are of fixed value which forces the view-model to adapt accordingly.
    */
   subinit(initOptions) {
      const restrictions={}
      this.decorate(ReframeViewDecorator2, initOptions, restrictions)
      this.decorate(MoveResizeViewDecorator2, initOptions, restrictions)
      this.decorate(WindowControlsDecorator2, initOptions, restrictions)
      if (initOptions.content && (initOptions.content instanceof ViewElem || initOptions.content instanceof ViewElemText))
         this.content = initOptions.content
      return restrictions
   }
   /**
    * Content is removed from window frame and replaces frame in the document.
    */
   unframeContent() { this.removeExtension(ReframeViewDecorator2)?.free(this) }
   ///////////////////////////
   // WindowView Properties //
   ///////////////////////////
   /**
    * @returns {View2Config&WindowViewConfig}
    */
   get config() { return WindowView.Config }
   /**
    * @returns {undefined|ViewElemText|ViewElem}
    */
   get content() { return this.viewElem.slot(WindowView.Slots.content).child() }
   /**
    * @param {undefined|ViewElemText|ViewElem} view
    */
   set content(view) { this.viewElem.setSlot(WindowView.Slots.content, view) }
   /**
    * @returns {WindowVM2Pos}
    */
   get pos() { return this.viewElem.pos() }
   /**
    * @param {WindowVM2PosArgs} pos
    */
   set pos(pos) {
      const changed = this.viewElem.setPos(pos)
      if (changed)
         this.notifyUpdate("pos", changed)
   }
   // XXXXX
   /**
    * @param {WindowVM2State} state
    */
   set windowState(state) {
      const pc = this.getExtension(ViewElemPositionControl) ?? this.extend(new ViewElemPositionControl(this.viewElem))
      switch(state) {
         case WindowVM2.MAXIMIZED:
            pc.maximize()
            this.hidden = false
            break;
         case WindowVM2.MINIMIZED:
            pc.minimize()
            this.hidden = false
            break;
         case WindowVM2.CLOSED:
         case WindowVM2.HIDDEN:
         case WindowVM2.VISIBLE:
            pc.restore()
            this.removeExtension(ViewElemPositionControl)
            this.hidden = (state !== WindowVM2.VISIBLE)
            break;
      }
   }
}

class ViewViewModelConnection {
   /**
    * @typedef ViewViewModelConnectionLiteral
    * @property {{[viewAction:string]:VVMConnectionLock}} locks
    * @property {{[viewAction:string]:VVMConnectionAction}} actions
    * @property {VVMConnectionRestriction[]} restrictions
    * @property {VVMConnectionProperty[]} properties
    */
   //////////////////////////////////////////////////
   // Types used in ViewViewModelConnectionLiteral //
   /**
    * @typedef VVMConnectionLock
    * @property {string[][]} path
    * @property {string} notlocked
    * @property {boolean} shared
    */
   /**
    * @typedef VVMConnectionAction
    * @property {string} method
    * @property {string} lock
    * @property {(...actionArgs:any)=>any} mapArgs
    */
   /**
    * @typedef VVMConnectionProperty
    * @property {"view"|"vm"|"both"|undefined} update Defines view or view-model as the one which receives updates from the other side. In case update is set to "both" both sides sides (vm or view) receives updates from the other side. If not set or undefined neither side is updated if the other side changes.
    * @property {"view"|"vm"|undefined} init Defines view or view-model as the one whose property is set during initialization from the read value of the other side. If not set or undefined neither side is set during intialization.
    * @property {string} view Name of connected view property.
    * @property {string} vm Name of connected view-model property.
    * @property {null|((...viewValues:any)=>any)} [mapToVM] Maps view value to a format the view-model property expects.
    * @property {null|((...vmValues:any)=>any)} [mapToView] Maps view-model value to a format the view property expects.
    */
   /**
    * @typedef VVMConnectionRestriction
    * @property {string} view Name of restriction property of view (read).
    * @property {string} vm Name of view-model restriction property (set during init).
    * @property {null|((...viewValues:any)=>any)} [mapToVM] Maps read value to a format the view-model expects.
    */
   /////////////////////////////////////////////////////
   // Types used in validated ViewViewModelConnection //
   /**
    * @typedef ViewViewModelConnectionLock
    * @property {string[][]} path
    * @property {undefined|ViewViewModelConnectionLock} notlocked
    * @property {boolean} shared
    */
   /** @type {{[lock:string]:ViewViewModelConnectionLock}} */
   locks={}
   /**
    * @typedef ViewViewModelConnectionAction
    * @property {string} method
    * @property {undefined|ViewViewModelConnectionLock} lock
    * @property {(...actionArgs:any)=>any} mapArgs
    */
   /** @type {{[action:string]:ViewViewModelConnectionAction}} */
   actions={}
   /**
    * @typedef ViewViewModelConnectionProperty
    * @property {"view"|"vm"|"both"|undefined} update
    * @property {"view"|"vm"|undefined} init
    * @property {string} view
    * @property {string} vm
    * @property {(...viewValues:any)=>any} mapToVM
    * @property {(...vmValues:any)=>any} mapToView
    */
   /** @type {ViewViewModelConnectionProperty[]} */
   properties=[]
   /**
    * @typedef ViewViewModelConnectionRestriction
    * @property {string} view
    * @property {string} vm
    * @property {(...viewValues:any)=>any} mapToVM
    */
   /** @type {ViewViewModelConnectionRestriction[]} */
   restrictions=[]
   /** @type {(val:any)=>any} */
   mapid=((value) => value)

   /**
    * @param {ViewViewModelConnectionLiteral} literal
    */
   constructor(literal) {
      if (literal instanceof ViewViewModelConnection)
         return literal
      // == locks ==
      for (const key of Object.keys(literal.locks)) {
         const lock = literal.locks[key]
         if (!Array.isArray(lock.path))
            return VMX.throwErrorObject(this,`Expect locks.${key}.path of type string[][].`)
         this.locks[key] = { path:new Array(literal.locks[key].path.length), notlocked:undefined, shared:Boolean(lock.shared) }
         for (var si=0; si<lock.path.length; ++si) {
            if (!Array.isArray(lock.path[si]))
               return VMX.throwErrorObject(this,`Expect locks.${key}.path[${si}] of type string[].`)
            this.locks[key].path[si] = new Array(lock.path[si].length)
            for (var ni=0; ni<lock.path[si].length; ++ni) {
               if (typeof lock.path[si][ni] !== "string")
                  return VMX.throwErrorObject(this,`Expect locks.${key}.path[${si}][${ni}] of type string.`)
               this.locks[key].path[si][ni] = lock.path[si][ni]
            }
         }
         if (lock.notlocked !== undefined) {
            if (typeof lock.notlocked !== "string")
               return VMX.throwErrorObject(this,`Expect locks.${key}.notlocked of type string.`)
            if (!(lock.notlocked in literal.locks))
               return VMX.throwErrorObject(this,`Expect locks.${key}.notlocked:'${lock.notlocked}' to refer to defined lock.`)
         }
      }
      for (const key of Object.keys(literal.locks)) {
         if (literal.locks[key].notlocked !== undefined)
            this.locks[key].notlocked = this.locks[literal.locks[key].notlocked]
      }
      // == actions ==
      for (const key of Object.keys(literal.actions)) {
         const action = literal.actions[key]
         if (typeof action.method !== "string")
            return VMX.throwErrorObject(this,`Expect actions.${key}.method of type string.`)
         if (action.lock !== undefined) {
            if (typeof action.lock !== "string")
               return VMX.throwErrorObject(this,`Expect actions.${key}.lock of type string.`)
            if (!(action.lock in this.locks))
               return VMX.throwErrorObject(this,`Expect actions.${key}.lock:'${action.lock}' to refer to defined lock.`)
         }
         if (action.mapArgs && typeof action.mapArgs !== "function")
            return VMX.throwErrorObject(this,`Expect actions.${key}.mapArgs of type function.`)
         this.actions[key] = { method:action.method, lock:action.lock?this.locks[action.lock]:undefined, mapArgs:action.mapArgs??this.mapid }
      }
      // == restrictions ==
      for (let key=0; key<literal.restrictions.length; ++key) {
         const prop = literal.restrictions[key]
         if (typeof prop.view !== "string")
            return VMX.throwErrorObject(this, `Expect restrictions.${key}.view of type string.`)
         if (typeof prop.vm !== "string")
            return VMX.throwErrorObject(this, `Expect restrictions.${key}.vm of type string.`)
         if (prop.mapToVM && typeof prop.mapToVM !== "function")
            return VMX.throwErrorObject(this, `Expect restrictions.${key}.mapToVM of type function.`)
         this.restrictions[key] = { view:prop.view, vm:prop.vm, mapToVM:prop.mapToVM??this.mapid }
      }
      // == properties ==
      const validUpdate = ["vm","view","both"]
      const validInit = ["vm","view"]
      for (let key=0; key<literal.properties.length; ++key) {
         const prop = literal.properties[key]
         if (prop.update !== undefined && !validUpdate.includes(prop.update))
            return VMX.throwErrorObject(this, `Expect properties[${key}].update:'${String(prop.update)}' a value out of [${validUpdate}] or undefined.`)
         if (prop.init !== undefined && !validInit.includes(prop.init))
            return VMX.throwErrorObject(this, `Expect properties[${key}].init:'${String(prop.init)}' a value out of [${validInit}] or undefined.`)
         if (prop.update === undefined && prop.init === undefined)
            return VMX.throwErrorObject(this, `Expect properties[${key}].update (or init) to be not undefined.`)
         if (typeof prop.view !== "string")
            return VMX.throwErrorObject(this, `Expect properties[${key}].view of type string.`)
         if (typeof prop.vm !== "string")
            return VMX.throwErrorObject(this, `Expect properties[${key}].vm of type string.`)
         if (prop.mapToView && typeof prop.mapToView !== "function")
            return VMX.throwErrorObject(this, `Expect properties[${key}].mapToView of type function.`)
         if (prop.mapToVM && typeof prop.mapToVM !== "function")
            return VMX.throwErrorObject(this, `Expect properties[${key}].mapToVM of type function.`)
         this.properties[key] = { update:prop.update, init:prop.init, view:prop.view, vm:prop.vm, mapToView:prop.mapToView??this.mapid, mapToVM:prop.mapToVM??this.mapid }
      }
   }
}

class ViewViewModelConnectorInterface {
   /**
    * @typedef UpdateNotification
    * @property {View2|ViewModel2} from
    * @property {string} property
    * @property {any} value
    */

   /**
    * @param {any} connector Argument value which is tested to be of type ViewViewModelConnectorInterface.
    * @param {string} argument Name of tested argument.
    * @returns {ViewViewModelConnectorInterface}
    */
   static assertType(connector, argument) {
      if (connector instanceof ViewViewModelConnectorInterface)
         return connector
      return VMX.throwErrorObject(this, `Expect argument ${argument} of type ViewViewModelConnectorInterface instead of »${VMX.typeof(connector)}«.`)
   }
   /**
    * @returns {boolean} *true*, if initConnection was called before.
    */
   isConnected() { return VMX.throwErrorObject(this, `isConnected not implemented.`)}
   /**
    * @param {ViewViewModelConnectionLiteral} connection
    * @param {()=>ViewRestrictions} getViewRestrictions
    * @returns {void}
    */
   connect(connection, getViewRestrictions) { return VMX.throwErrorObject(this, `connect not implemented.`)}
   /**
    * @returns {(HTMLElement)[]}
    */
   htmlElements() { return VMX.throwErrorObject(this, `htmlElements not implemented.`)}
   /**
    * @returns {ImplementsInterfaceLogID}
    */
   viewParent()  { return VMX.throwErrorObject(this, `viewParent not implemented.`)}
   /**
    * @param {string} action
    * @return {ViewModelAction}
    */
   getVMAction(action) { return VMX.throwErrorObject(this, `getVMAction not implemented.`) }
   /**
    * @param {UpdateNotification} updateNotification
    * @returns {void}
    */
   onUpdateNotification(updateNotification) { return VMX.throwErrorObject(this, `onUpdateNotification not implemented.`)}
}

class ViewViewModelConnector extends ViewViewModelConnectorInterface {

   /** @type {View2} */
   view
   /** @type {ViewModel2} */
   viewModel
   /** @type {undefined|ViewViewModelConnection} */
   connection
   /** @type {{[vprop:string]:{vm:string, mapToVM:(...args:any)=>any}}} */
   viewProperties
   /** @type {{[vmprop:string]:{view:string, mapToView:(...args:any)=>any}}} */
   vmProperties
   /** @type {Set<string>} */
   viewLocks
   /** @type {Set<string>} */
   vmLocks
   /** @type {boolean} *false*, onUpdateNotification will transfer updates between connected properties. */
   ignoreConnectedUpdates

   /**
    * @param {View2} view
    * @param {ViewModel2} viewModel
    */
   constructor(view, viewModel) {
      super()
      this.view = view
      this.viewModel = viewModel
      this.viewProperties = {}
      this.vmProperties = {}
      this.viewLocks = new Set()
      this.vmLocks = new Set()
      this.ignoreConnectedUpdates = true
      view.connectToModel(this)
      viewModel.connectToView(this)
   }
   ignoreUpdates() { this.ignoreConnectedUpdates = true }
   enableUpdates() { this.ignoreConnectedUpdates = false }
   setVMRestrictions(vmrestrictions) { this.viewModel.init(vmrestrictions) }
   ////////////////////////////////////////////////
   // Implements ViewViewModelConnectorInterface //
   ////////////////////////////////////////////////
   /**
    * @returns {boolean}
    */
   isConnected() { return this.connection !== undefined }
   /**
    * @param {ViewViewModelConnectionLiteral} connection
    * @param {()=>ViewRestrictions} getViewRestrictions
    */
   connect(connection, getViewRestrictions) {
      if (!connection || typeof connection !== "object" || typeof connection.actions !== "object" || typeof connection.properties !== "object" || typeof connection.restrictions !== "object")
         return VMX.throwErrorObject(this, `Expect argument connection of type ViewViewModelConnectionLiteral instead of »${VMX.typeof(connection)}«.`)
      if (this.isConnected())
         return VMX.throwErrorObject(this,`connect already called.`)
      this.connection = new ViewViewModelConnection(connection)
      // XXXXX
      // == restrictions ==
      const viewrestrictions = getViewRestrictions()
      const vmrestrictions = {}
      for (const prop of this.connection.restrictions) {
         if (!(prop.view in viewrestrictions))
            return VMX.throwErrorObject(this, `Missing viewrestriction ${prop.view}.`)
         vmrestrictions[prop.vm] = prop.mapToVM(viewrestrictions[prop.view])
      }
      this.setVMRestrictions(vmrestrictions)
      // == properties ==
      for (const prop of this.connection.properties) {
         // update flow
         if ("view" === prop.update || "both" === prop.update) {
            this.vmProperties[prop.vm] = prop
         }
         if ("vm" === prop.update || "both" === prop.update) {
            this.viewProperties[prop.view] = prop
         }
         // init flow
         if ("vm" === prop.init)
            this.viewModel[prop.vm] = prop.mapToVM(this.view[prop.view])
         else if ("view" === prop.init)
            this.view[prop.view] = prop.mapToView(this.viewModel[prop.vm])
      }
      this.enableUpdates()
   }
   /**
    * @returns {(HTMLElement)[]}
    */
   htmlElements() { return this.view.htmlElements }
   /**
    * @returns {ImplementsInterfaceLogID}
    */
   viewParent()  { return this.viewModel }
   /**
    * @param {string} viewAction
    * @return {ViewModelAction}
    */
   getVMAction(viewAction) {
      if (!this.connection)
         return VMX.throwErrorObject(this,`Call connect first.`)
      const conn = this.connection.actions[viewAction]
      if (!conn)
         return VMX.throwErrorObject(this, `Action »${viewAction}« is unknown.`)
      const { method, mapArgs } = conn
      const lock = conn.lock ?? { path:[[method]], notlocked:undefined, shared:false }
      const notlockedPath = lock.notlocked ? lock.notlocked.path : undefined
      if (!(method in this.viewModel) || typeof this.viewModel[method] !== "function")
         return VMX.throwErrorObject(this, `Action »${viewAction}« can not access ViewModel method »${method}«.`)
      return new ViewModelAction(this.viewModel, lock.path, lock.shared, (args) => this.viewModel[method](mapArgs(args))).setNotLocked(notlockedPath)
   }
   /**
    * @param {UpdateNotification} updateNotification
    */
   onUpdateNotification(updateNotification) {
      if (this.ignoreConnectedUpdates) return
      if (updateNotification.from === this.view) {
         if (this.viewLocks.has(updateNotification.property)) return
         const connectedProperty = this.viewProperties[updateNotification.property]
         if (connectedProperty) {
            this.vmLocks.add(connectedProperty.vm)
            this.viewModel[connectedProperty.vm] = connectedProperty.mapToVM(updateNotification.value)
            this.vmLocks.delete(connectedProperty.vm)
         }
      }
      else if (updateNotification.from === this.viewModel) {
         if (this.vmLocks.has(updateNotification.property)) return
         const connectedProperty = this.vmProperties[updateNotification.property]
         if (connectedProperty) {
            this.viewLocks.add(connectedProperty.view)
            this.view[connectedProperty.view] = connectedProperty.mapToView(updateNotification.value)
            this.viewLocks.delete(connectedProperty.view)
         }
      }
   }
}

class ViewModel2TypeClass extends TypeClass {
   InterfaceLogID = new InterfaceLogID({})
}

class ViewModel2 {
   static #typeClass = new ViewModel2TypeClass(ViewModel2,TypeClass.VIEWMODEL)
   static get typeClass() { return ViewModel2.#typeClass }
   get typeClass() { return ViewModel2.#typeClass }
   /**
    * @param {any} viewModel
    * @returns {ViewModel2}
    */
   static assertType(viewModel) {
      if (viewModel instanceof ViewModel2)
         return viewModel
      return VMX.throwErrorStatic(this, `Expect type ViewModel and not »${VMX.typeof(viewModel)}«.`)
   }

   /** @type {undefined|ViewViewModelConnectorInterface} */
   #viewConnector
   /** @type {LogID} */
   LID
   /**
    *
    */
   constructor() {
      this.LID = new LogID(this)
   }
   /**
    * @param {{[vmrestriction:string]:any}} vmrestrictions
    */
   init(vmrestrictions) { }
   /**
    *
    */
   free() { this.LID.unbind(this) }
   /**
    * @param {ViewViewModelConnectorInterface} connector
    */
   connectToView(connector) {
      this.#viewConnector = ViewViewModelConnectorInterface.assertType(connector,"connector")
      this.LID.bindElems(this, connector.htmlElements())
   }
   get viewConnector() { return this.#viewConnector || VMX.throwErrorObject(this, `Call connectToView first.`) }
   /**
    * @template V
    * @param {string} property
    * @param {V} value
    */
   notifyUpdate(property, value) {
      this.viewConnector.onUpdateNotification({from:this, property, value})
   }
}

class WindowVM2 extends ViewModel2 {
   // static #typeClass = new ViewModel2TypeClass(WindowVM2,TypeClass.VIEWMODEL)
   // static get typeClass() { return WindowVM2.#typeClass }
   // get typeClass() { return WindowVM2.#typeClass }
   /**
    * @typedef WindowVM2Pos
    * @property {number} width
    * @property {number} height
    * @property {number} top
    * @property {number} left
    */
   /**
    * @typedef WindowVM2PosArgs
    * @property {number|undefined} [width]
    * @property {number|undefined} [height]
    * @property {number|undefined} [top]
    * @property {number|undefined} [left]
    */
   /**
    * @typedef {"closed"|"hidden"|"maximized"|"minimized"|"visible"} WindowVM2State
    */
   /** @type {"closed"} */
   static CLOSED = "closed"
   /** @type {"hidden"} */
   static HIDDEN = "hidden"
   /** @type {"maximized"} */
   static MAXIMIZED = "maximized"
   /** @type {"minimized"} */
   static MINIMIZED = "minimized"
   /** @type {"visible"} */
   static VISIBLE = "visible"
   /**
    * @param {any} viewModel
    * @returns {WindowVM2}
    */
   static assertType(viewModel) {
      if (viewModel instanceof WindowVM2)
         return viewModel
      return VMX.throwErrorStatic(this, `Expect type WindowVM2 and not »${VMX.typeof(viewModel)}«.`)
   }

   /** @type {WindowVM2State} */
   #windowState = WindowVM2.HIDDEN
   /** @type {WindowVM2Pos} The current position of the window frame (no matter which state). */
   #pos = { width:0, height:0, top:0, left:0 }
   /** @type {number} */
   #flags = 3

   //////////////////////////
   // Overwrite ViewModel2 //
   //////////////////////////

   /**
    * @param {{moveable:boolean,resizeable:boolean,[vmrestriction:string]:any}} vmrestrictions
    */
   init(vmrestrictions) {
      this.#setMoveable(vmrestrictions.moveable)
      this.#setResizeable(vmrestrictions.resizeable)
   }

   /////////////////////
   // WindowViewModel //
   /////////////////////

   /**
    * @param {string[]} posAttributes
    * @param {WindowVM2PosArgs} newPos
    * @param {WindowVM2Pos} updatedPos
    * @return {undefined|WindowVM2PosArgs} Valid value, if any value changed else undefined.
    */
   updatePos(posAttributes, newPos, updatedPos) {
      const changed = {}
      let hasChanged = false
      for (const name of posAttributes) {
         const value = newPos[name]
         if (isFinite(value) && updatedPos[name] !== value) {
            hasChanged = true
            updatedPos[name] = changed[name] = value
         }
      }
      return hasChanged ? changed : undefined
   }
   /**
    * @return {WindowVM2Pos}
    */
   get pos() { return { ...this.#pos } }
   /**
    * @param {WindowVM2PosArgs} pos
    */
   set pos(pos) {
      if (!this.moveable && !this.resizeable) return
      const changed = this.updatePos([...(this.resizeable ? ["width","height"] : []), ...(this.moveable ? ["top","left"] : [])], pos, this.#pos)
      if (changed) {
         if (this.isNotifyForUpdatedPos)
            this.notifyUpdate("pos", changed)
         else
            this.#setPosChanged(true)
      }
   }
   /**
    * @returns {boolean} *true*, if changing left/top is supported.
    */
   get moveable() { return Boolean(this.#flags&1) }
   /**
    * @returns {boolean} *true*, if changing width/height is supported.
    */
   get resizeable() { return Boolean(this.#flags&2) }
   /**
    * @returns {boolean} *true*, if pos changed but {@link isNotifyForUpdatedPos} returned false.
    */
   get posChanged() { return Boolean(this.#flags&4) }
   /**
    * @returns {boolean} *true*, if view is notfied of pos updates immediately.
    */
   get isNotifyForUpdatedPos() {
      return (this.#windowState === WindowVM2.VISIBLE || this.#windowState === WindowVM2.HIDDEN)
   }
   /**
    * Notifes view of pos changes if there are any.
    */
   notifyUpdatedPos() {
      if (this.posChanged) {
         this.notifyUpdate("pos", this.#pos)
         this.#setPosChanged(false)
      }
   }
   /**
    * @param {boolean} flag
    */
   #setMoveable(flag) { this.#flags = flag ? this.#flags|1 : this.#flags&(~1) }
   /**
    * @param {boolean} flag
    */
   #setResizeable(flag) { this.#flags = flag ? this.#flags|2 : this.#flags&(~2) }
   /**
    * @param {boolean} flag
    */
   #setPosChanged(flag) { this.#flags = flag ? this.#flags|4 : this.#flags&(~4) }
   get windowState() { return this.#windowState }
   set windowState(newState) {
      const oldState = this.#windowState
      switch (newState) {
      case WindowVM2.CLOSED:
      case WindowVM2.HIDDEN:
      case WindowVM2.MAXIMIZED:
      case WindowVM2.MINIMIZED:
      case WindowVM2.VISIBLE:
         this.#windowState = newState
         break
      default:
         break
      }
      if (this.#windowState === oldState) return
      this.notifyUpdate("windowState", newState)
      this.isNotifyForUpdatedPos && this.notifyUpdatedPos()
   }
   /////////////
   // Actions //
   /////////////
   /**
    * Set windowState to CLOSED.
    */
   close() { this.windowState = WindowVM2.CLOSED }
   /**
    * Set windowState to MINIMIZED.
    */
   minimize() { this.windowState = WindowVM2.MINIMIZED }
   /**
    * Switches windowState to MAXIMIZED if not maximized else VISIBLE.
    */
   toggleMaximize() { this.windowState = this.windowState === WindowVM2.MAXIMIZED ? WindowVM2.VISIBLE : WindowVM2.MAXIMIZED }
   /**
    * @param {{dx?:number, dy?:number}} args
    */
   move({dx, dy}) {
      console.log(`move-action(dx:${dx}, dy:${dy})`)
      const { top, left } = this.pos
      this.pos = { top:top+(dy?dy:0), left:left+(dx?dx:0) }
   }
   /**
    * @param {{dtop?:number, dright?:number, dbottom?:number, dleft?:number}} args
    */
   resize({dtop=0, dright=0, dbottom=0, dleft=0}) {
      console.log(`resize-action(dtop:${dtop}, dright:${dright}, dbottom:${dbottom}, dleft:${dleft})`)
      const { top, left, width, height } = this.pos
      this.pos = { top:top-dtop, left:left-dleft, width:width+dleft+dright, height:height+dtop+dbottom }
   }
}

class WindowVM extends ResizeVM {
   /**
    * @typedef WindowViewModelConfig
    * @property {string} maximizedClass
    */
   /** @type {ViewModelConfig&WindowViewModelConfig} */
   static Config = { ...super.Config, decorator: WindowDecorator,
      listenableProperties: new Set(["vWindowState", ...super.Config.listenableProperties]),
      /** Class name which sets display to none if set on htmlElement. Additional property! */
      maximizedClass: "maximized",
   }
   /// window states ///
   static HIDDEN = "hidden"; static VISIBLE = "visible"; static MINIMIZED = "minimized"; static MAXIMIZED = "maximized"; static CLOSED = "closed";
   /** Transition style for resize and change position animation. */
   static TRANSITION_STYLE = { "transition-delay":"0s", "transition-duration":"0.2s", "transition-property":"width,height,top,left", "transition-timing-function":"ease-out" }

   #windowState
   #windowStateBefore = WindowVM.VISIBLE
   #visiblePos={ width:"0px", height:"0px", top:"0px", left:"0px" }
   #transitionPos
   #oldTransitionStyle
   #replacedOldStyles

   #initWindowState() {
      this.#windowState = (this.view.displayed ? WindowVM.VISIBLE : WindowVM.HIDDEN)
   }

   constructor(htmlElem) {
      super(htmlElem)
      this.#initWindowState()
   }

   /**
    * Initializes object after constructor has been completed.
    * @param {{content?:Node|string}} options
    * @returns {object} Options for decorator
    */
   init({ content, ...initOptions }) {
      if (content)
         this.vContent = content
      const decoOptions = super.init({ ...initOptions })
      return decoOptions
   }

   config(name) {
      if (WindowVM.Config[name])
         return WindowVM.Config[name]
      return super.config(name)
   }

   setOption(name, value) {
      switch (name) {
         case "content": this.vContent = value; break;
         case "title": this.vTitle = value; break;
         default: super.setOption(name, value); break;
      }
   }

   /**
    * @param {string} action
    * @param {null|string[][]} [subactions]
    * @param {(args: object)=>object} [mapArgs]
    * @returns {ViewModelAction} Bound action which calls the correct view model method.
    */
   bindAction(action, subactions, mapArgs) {
      let lockAction = [action]
      let vmAction
      switch(action) {
         case "close": lockAction = ["move"]; vmAction = () => this.close(); break
         case "minimize": lockAction = ["move"]; vmAction = () => this.minimize(); break
         case "move": lockAction = ["move","resize"]; vmAction = (args) => this.move(args.dx,args.dy); break
         case "resize": vmAction = (args) => this.resize(args.dTop,args.dRight,args.dBottom,args.dLeft); break
         case "toggleMaximize": lockAction = ["move"]; vmAction = () => this.toggleMaximize(); break
         default: return super.bindAction(action, subactions, mapArgs)
      }
      return new ViewModelAction(this, [lockAction, ...(subactions?subactions:[])], false, mapArgs ? (args) => vmAction(mapArgs(args)) : vmAction)
   }

   replaceWithContent() {
      const content = this.vContent
      if (content) {
         this.htmlElem.replaceWith(content)
         if (this.#replacedOldStyles)
            DOM.setStyles(content, this.#replacedOldStyles)
         this.#replacedOldStyles = undefined
      }
      return content
   }

   get vTitle() {
      return this.view.slot("title").html
   }
   set vTitle(html) {
      if (typeof html === "string")
         this.view.slot("title").html = html
   }

   needReplaceContent(htmlElem) {
      return htmlElem instanceof Element && htmlElem.isConnected && htmlElem.parentElement
   }

   /** @type {undefined|Node} */
   get vContent() {
      const child = this.view.slot("content").child()
      return child instanceof ViewElem ? child.htmlElem : child ? child.textNode : undefined
   }
   /**
    * Assigns content as child to the window frame.
    * If content is connected a Node connected to the document
    * it is replaced with the WindowVM.htmlElem therefore the window takes
    * up the same space as the content and the content is inserted as child
    * to the window.
    * Use {@link replaceWithContent} to move the content back to DOM location
    * where {@link htmlElem} is currently placed.
    * @param {undefined|string|Node} content An HTML string (resulting in a single HTML node) or a single html element.
    */
   set vContent(content) {
      const slot = this.view.slot("content")
      if (typeof content === "string")
         slot.html = content
      else {
         const needReplace = this.needReplaceContent(content)
         if (needReplace && content instanceof HTMLElement) {
            const pos = new ViewElem(content).initialPos()
            content.replaceWith(this.htmlElem)
            this.#replacedOldStyles = DOM.setStyles(content, { width:"100%", height:"100%", position:"static" })
            this.view.setStyles({ position: pos.position })
            this.#initWindowState()
            this.setOptions({ left:pos.left, top:pos.top, width:pos.width, height:pos.height })
         }
         slot.setChildNode(content)
      }
   }

   get useVisiblePos() {
      return this.vWindowState === WindowVM.VISIBLE || this.vWindowState === WindowVM.HIDDEN
   }
   setPosAttr(name, value) {
      value = this.roundPosValue(value)
      if (isFinite(value)) {
         if (this.useVisiblePos)
            this.#visiblePos[name] = value+"px"
         if (this.#transitionPos)
            this.#transitionPos[name] = value+"px"
         else
            super.setPosAttr(name, value)
      }
   }

   get visiblePos() {
      const pos = this.#visiblePos
      return { width:parseFloat(pos.width), height:parseFloat(pos.height), top:parseFloat(pos.top), left:parseFloat(pos.left) }
   }
   /**
    * Sets new position of visible (or hidden) window.
    * Setting flag transition starts a transition to animate the window movement to the new position if it is visible.
    * @param {{width:number,height:number,top:number,left:number,transition?:boolean}} pos The new position in css pixel coordinates.
    */
   set visiblePos(pos) {
      for (const name of ["width","height","top","left"]) {
         const value = this.roundPosValue(pos[name])
         if (isFinite(value))
            this.#visiblePos[name] = value+"px"
      }
      if (this.useVisiblePos) {
         const useRunningTransition = (this.#transitionPos && pos.transition)
         this.#transitionPos = this.#visiblePos
         if (!useRunningTransition)
            this.#startTransition(pos.transition)
      }
   }

   #startTransition(useTransition, onTransitionEnd) {
      const htmlElem = this.htmlElem
      if (this.#transitionPos && useTransition) {
         this.#oldTransitionStyle ??= DOM.setStyles(htmlElem, WindowVM.TRANSITION_STYLE)
         DOM.setStyles(htmlElem, this.#transitionPos)
         new TransitionController(htmlElem, (e) => {
            if (! e.isTransition) {
               this.disableTransition()
               onTransitionEnd?.()
            }
         }).ensureInTransitionElseEnd()
      }
      else {
         this.disableTransition()
         this.#transitionPos && DOM.setStyles(htmlElem, this.#transitionPos)
         onTransitionEnd?.()
      }
      this.#transitionPos = undefined
   }
   disableTransition() {
      if (this.#oldTransitionStyle) {
         DOM.setStyles(this.htmlElem, this.#oldTransitionStyle)
         this.#oldTransitionStyle = undefined
         TransitionController.endTransition(this.htmlElem)
      }
   }
   isTransition() {
      return this.#transitionPos || this.#oldTransitionStyle || TransitionController.isTransition(this.htmlElem)
   }
   /**
    * Allows to wait for end of transition.
    * Notification is done either by callback or by waiting for the returned promise.
    * If no transition is running notification is done immediately.
    * @param {()=>void} [onTransitionEnd] Optional callback called after transition ends.
    * @returns {Promise<boolean>} Resolved after transition ends with flag set to true if transition was running at the time this function was called.
    */
   onceOnTransitionEnd(onTransitionEnd) {
      const promiseHolder = new PromiseHolder()
      if (this.isTransition())
         new TransitionController(this.htmlElem, (e) => {
            if (!e.isTransition) {
               promiseHolder.resolve(true)
               onTransitionEnd?.()
            }
         })
      else {
         promiseHolder.resolve(false)
         onTransitionEnd?.()
      }
      return promiseHolder.promise
   }

   close() {
      this.vWindowState = WindowVM.CLOSED
   }
   toggleMaximize() {
      this.vWindowState === WindowVM.MAXIMIZED
      ? this.show()
      : this.maximize()
   }
   maximize() {
      this.vWindowState = WindowVM.MAXIMIZED
   }
   minimize() {
      this.vWindowState = WindowVM.MINIMIZED
   }
   show() {
      this.vWindowState = WindowVM.VISIBLE
   }
   hide() {
      this.vWindowState = WindowVM.HIDDEN
   }
   get hidden() {
      return super.hidden
   }
   set hidden(isHidden) {
      if (Boolean(isHidden) !== (this.vWindowState === WindowVM.HIDDEN)) {
         this.vWindowState = isHidden ? WindowVM.HIDDEN : this.#windowStateBefore
      }
   }
   get vWindowState() {
      return this.#windowState
   }
   set vWindowState(state) {
      const oldState = this.#windowState
      if (oldState === state)
         return
      if (oldState === WindowVM.CLOSED) {
         this.connectedTo = this.closedState?.connectedTo
         delete this.closedState
      }
      this.connected = true
      const htmlElem = this.htmlElem
      if (state === WindowVM.VISIBLE) {
         super.hidden = false
         this.#transitionPos = this.#visiblePos
         this.onceOnViewUpdate(1, () => {  // switching display from "none" (hidden true->false) needs one frame waiting to make transitions work
            if (this.#windowState === WindowVM.VISIBLE) {
               this.#startTransition(true, () => {
                  if (this.#windowState === WindowVM.VISIBLE)
                     htmlElem.scrollIntoView?.({block:"nearest"})
               })
            }
         })
      }
      else if (state === WindowVM.HIDDEN) {
         super.hidden = true
         this.#transitionPos = undefined
      }
      else if (state === WindowVM.MINIMIZED) {
         super.hidden = false
         const cstyle = getComputedStyle(htmlElem)
         const pos = htmlElem.getBoundingClientRect()
         const top = window.innerHeight-32, left = 5
         this.#transitionPos = { width:"32px", height:"16px", top:(top-pos.top+parseFloat(cstyle.top))+"px", left:(left-pos.left+parseFloat(cstyle.left))+"px" }
         super.hidden = (oldState === WindowVM.HIDDEN)
         this.onceOnViewUpdate(1, () => {
            if (this.#windowState === WindowVM.MINIMIZED) {
               this.#startTransition(true, () => {
                  if (this.#windowState === WindowVM.MINIMIZED)
                     super.hidden = true
               })
            }
         })
      }
      else if (state === WindowVM.MAXIMIZED) {
         super.hidden = false
         const pos = htmlElem.getBoundingClientRect()
         const cstyle = getComputedStyle(htmlElem)
         this.#transitionPos = { width:"100vw", height:"100vh", top:(parseFloat(cstyle.top)-pos.top)+"px", left:(parseFloat(cstyle.left)-pos.left)+"px" }
         this.onceOnViewUpdate(1, () => {
            if (this.#windowState === WindowVM.MAXIMIZED) {
               this.#startTransition(true, () => {
                  if (this.#windowState === WindowVM.MAXIMIZED)
                     htmlElem.scrollIntoView?.({block:"start"})
               })
            }
         })
      }
      else if (state == WindowVM.CLOSED) {
         this.closedState = { connectedTo: this.connectedTo }
         this.connected = false
      }
      else
         VMX.throwErrorObject(this, `Unsupported window state: ${state}`)
      this.view.switchClass(state == WindowVM.MAXIMIZED, this.config("maximizedClass"))
      if (oldState === WindowVM.HIDDEN && this.#transitionPos)
         DOM.setStyles(htmlElem, this.#transitionPos)
      this.#windowStateBefore = this.#windowState
      this.#windowState = state
      this.notifyPropertyListener("vWindowState",state)
   }
}

class WindowStackVM extends ViewModel {
   #onUpdate = this.onUpdate.bind(this)
   /** @type {Map<WindowVM, { windowVM: WindowVM, vWindowState:string, minimized:null|{htmlElem:HTMLElement, controllers:(false|ViewController)[]} }>} */
   #windowVMs = new Map()
   /** @type {HTMLElement} */
   #template
   /**
    * @param {HTMLElement} htmlElem
    */
   constructor(htmlElem) {
      super(htmlElem)
   }
   /**
    * @param {{minimizedTemplate:undefined|null|HTMLElement, [o: string]: any}} options
    */
   init({ minimizedTemplate, ...initOptions }) {
      super.init(initOptions)
      this.setMinimizedTemplate(minimizedTemplate)
   }
   /**
    * @returns {HTMLElement}
    */
   defaultTemplate() {
      const htmlElem = DOM.newElement("<div data-slot='title' style='cursor:pointer; padding:2px; border:2px black solid'></div>")
      if (htmlElem instanceof HTMLElement)
         return htmlElem
      return VMX.throwErrorObject(this, "Default template is no HTMLElement")
   }
   /**
    * @param {undefined|null|HTMLElement} htmlElem
    */
   setMinimizedTemplate(htmlElem) {
      this.#template = htmlElem ?? this.defaultTemplate()
   }
   /**
    * @param {WindowVM} windowVM
    * @returns {{windowVM: WindowVM, vWindowState:string, minimized:null|{htmlElem:HTMLElement, controllers:(false|ViewController)[]}}}
    */
   getState(windowVM) {
      const state = this.#windowVMs.get(windowVM)
      return state ?? VMX.throwErrorObject(this, "received update event for unregistered Window-View-Model.")
   }
   /**
    * @param {{windowVM: WindowVM, vWindowState:string, minimized:null|{htmlElem:HTMLElement, controllers:(false|ViewController)[]}}} state
    * @param {WindowVM} windowVM
    */
   addMinimized(state, windowVM) {
      if(! state.minimized) {
         const elem = new ViewElem(this.#template).clone()
         const htmlElem = elem.htmlElem
         elem.slot("title").copyFrom(windowVM.view.slot("title"))
         const controllers = [
            elem.hasSlot("close") && new ClickController(elem.slot("close").htmlElem, (e) => {
               windowVM.vWindowState = WindowVM.CLOSED
            }),
            new ClickController(elem.slot("title").htmlElem, (e) => {
               this.resetWindowState(state, windowVM)
            }),
         ]
         this.htmlElem.append(htmlElem)
         state.minimized = { htmlElem, controllers }
      }
   }
   /**
    * @param {{windowVM: WindowVM, vWindowState:string, minimized:null|{htmlElem:HTMLElement, controllers:(false|ViewController)[]}}} state
    * @param {WindowVM} windowVM
    */
   removeMinimized(state, windowVM) {
      const minimized = state.minimized
      if (minimized) {
         minimized.htmlElem.remove()
         minimized.controllers.forEach( cntrl => { if (cntrl instanceof ViewController) cntrl.free() })
         state.minimized = null
      }
      state.vWindowState = windowVM.vWindowState
   }
   /**
    * @param {{windowVM: WindowVM, vWindowState:string, minimized:null|{htmlElem:HTMLElement, controllers:(false|ViewController)[]}}} state
    * @param {WindowVM} windowVM
    */
   resetWindowState(state, windowVM) {
      windowVM.vWindowState = state.vWindowState
   }
   /**
    * @param {WindowVM} windowVM
    */
   unregister(windowVM) {
      const state = this.getState(windowVM)
      this.removeMinimized(state, windowVM)
      this.#windowVMs.delete(windowVM)
      windowVM.removePropertyListener("vWindowState", this.#onUpdate)
   }
   register(windowVM) {
      if (!this.#windowVMs.has(windowVM)) {
         this.#windowVMs.set(windowVM, { windowVM, vWindowState:windowVM.vWindowState, minimized:null })
         windowVM.addPropertyListener("vWindowState", this.#onUpdate)
      }
   }

   onUpdate({vm: windowVM, value: vWindowState}) {
      const state = this.getState(windowVM)
      if (vWindowState === WindowVM.MINIMIZED || vWindowState === WindowVM.HIDDEN)
         this.addMinimized(state, windowVM)
      else if (vWindowState === WindowVM.CLOSED)
         this.unregister(windowVM)
      else if (vWindowState === WindowVM.MAXIMIZED || vWindowState === WindowVM.VISIBLE)
         this.removeMinimized(state, windowVM)
      else
         VMX.throwErrorObject(this, `Unhandled WindowVM.State »${String(vWindowState)}«.`)
   }

}

// XXXXX


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


class DocumentView extends View2 {

   /** @type {number} Size of one CSS pixel to the size of one physical pixel. Or number of device's pixels used to draw a single CSS pixel. */
   devicePixelRatio
   /** @type {{width:number, height:number}} Screen size in css pixel. */
   screen
   /** @type {{width:number, height:number}} Size of viewport(window) which shows the document. */
   viewport

   // TODO: support update notifications <-> same as viewmodel
   // n2mObserving twoway dataflow

}


// TODO:
//  ==== log message : Notify View of ViewModel updates (v2)  ====
// ViewElemDocument <-> DocumentView <-> DocumentVM
// !!! Model receives updates from view !!!
//                     change --> set change
//                     update <-- change
// binding yyy <-converter-> xxx
// supports different sub-models/sub-views

class DocumentVM extends ViewModel2 {

   constructor() {
      super()
   }

   /**
    * @param {string} action
    * @param {null|string[][]} [subactions]
    * @param {(args: object)=>object} [mapArgs]
    * @returns {ViewModelAction} Bound action which calls the correct view model method.
    */
   bindAction(action, subactions, mapArgs) {
      VMX.logInfoObject(this,`bindAction ${action} ${VMX.stringOf(subactions)}`)
      switch(action) {
         case KeyController.KEYUP: break
         case KeyController.KEYDOWN: break
         default: return VMX.throwErrorObject(this, `bindAction does not support action ${action}`)
      }
      return new ViewModelAction(this, [[action], ...(subactions?subactions:[])], false, (args)=>mapArgs?.(args))
   }
}

const VMX = new ViewModelContext()
VMX.init()