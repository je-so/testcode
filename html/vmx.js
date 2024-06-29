/**
 * Class-Types for handling HTML-Views.
 *
 * === Helper Classes ===
 * * PromiseHolder
 * * // TODO: ClientLog
 * * // TODO: HTTP
 * === module Context ===
 * * ViewModelBinding
 * * ViewModelContext (stored in »const VMX = new ViewModelContext()«)
 * * LogIDs, LogID
 * === Wrapping of event listeners
 * * ViewListener
 * * ViewListeners
 * * ViewController
 * * ViewUpdateListener
 * === Wrapping HTML
 * (not fully, htmlElem is accessed within ViewModel,Controller,and Decorator.)
 * * HTMLView
 * * View
 * * ViewConfig
 * * ViewSlots
 * === Binding of View, ViewModel, and Controllers
 * * ViewUpdater
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
 * Simplifies creating a new promise.
 */
class PromiseHolder {
   promise
   resolve
   reject
   constructor() {
      this.promise = new Promise( (resolve, reject) => (this.resolve = resolve, this.reject = reject))
   }
   bindResolve(value) {
      return this.resolve.bind(this/*dummy*/,value)
   }
   bindReject(reason) {
      return this.reject.bind(this/*dummy*/,reason)
   }
}

/**
 * Manages relationship between HTMLElement and viewmodel objects and companions.
 * Stores in attribute »data-vmx-binding« the abbreviated type with the LogID
 * of objects (i.e. ' m:21 c:22 l:23 ').
 * which are bound to this HTMLElement.
 * Allows to search for elements which are bound to viewmodels, controllers, or listeners.
 * And also to retrieve the objects which are bound to a certain element.
 */
class ViewModelBinding {
   // known object types
   static MODEL="model"; static LISTENER="listener"; static CONTROLLER="controller"

   bindings = new WeakMap(/*htmlElem -> LogID[] */)

   /**
    * Returns type of supported object type (ViewController,ViewListener, and ViewModel).
    * @param {Object} object The object whose type is determined.
    * @returns {"controller"|"listener"|"model"|"?"} Value "?" in case of unknown type.
    */
   type(object) {
      if (object instanceof ViewController)
         return "controller"
      else if (object instanceof ViewListener)
         return "listener"
      else if (object instanceof ViewModel)
         return "model"
      return "?"
   }

   bind(htmlElem, LID) {
      if (!htmlElem)
         return
      const binding = this.bindings.get(htmlElem)
      const object = LID.owner
      const type = this.type(object)
      if (!binding) {
         this.bindings.set(htmlElem,[LID])
      }
      else {
         binding.some(id => {
            if (id.owner === object)
               throw Error(`ViewModelBinding error: bind object twice ${object.constructor?.name}-[${LID.index}]-old-[${id.index}]`)
         })
         binding.push(LID)
      }
      if (htmlElem.dataset) {
         const vmxBinding = htmlElem.dataset.vmxBinding
         htmlElem.dataset.vmxBinding = (vmxBinding ?? " ") + type[0]+":"+LID.index+" "
      }
   }

   unbind(htmlElem, LID) {
      if (!htmlElem)
         return
      const binding = this.bindings.get(htmlElem)
      const object = LID.owner
      const type = this.type(object)
      if (binding) {
         const i = binding.indexOf(LID)
         i >= 0 && binding.splice(i,1)
      }
      if (htmlElem.dataset) {
         let vmxBinding = htmlElem.dataset.vmxBinding
         if (vmxBinding) {
            vmxBinding = vmxBinding.replace(" "+type[0]+":"+LID.index+" "," ")
            vmxBinding === " "
               ? delete htmlElem.dataset.vmxBinding
               : htmlElem.dataset.vmxBinding = vmxBinding
         }
      }
   }

   getIDs(htmlElem, type) {
      const ids = []
      const binding = this.bindings.get(htmlElem)
      for (const id of binding ?? [])
         (!type || type === this.type(id.owner)) && ids.push(id)
      return ids
   }

   getObjects(htmlElem, type) {
      const ids = this.getIDs(htmlElem, type)
      return ids.map(id => id.owner)
   }

   getIndex(htmlElem, type) {
      const ids = this.getIDs(htmlElem, type)
      return ids.map(id => id.index)
   }

   isElementBound(htmlElem, type) {
      const binding = this.bindings.get(htmlElem)
      return (binding ?? []).some( id => !type || this.type(id.owner) === type)
   }

   getBoundElements(root, type) {
      const elements = []
      if (this.isElementBound(root,type))
         elements.push(root)
      for (const e of type ? root.querySelectorAll(`[data-vmx-binding*=' ${type[0]}:']`) : root.querySelectorAll("[data-vmx-binding]"))
         elements.push(e)
      return elements
   }

   getElement(index) {
      if ((this.bindings.get(document) ?? []).some( id => id.index === index))
         return document
      return document.querySelector(`[data-vmx-binding*=':${index} ']`)
   }
}

/**
 * Allocated at bottom of source and assigned to global variable »VMX«.
 */
class ViewModelContext {
   /** Stores references from all HTMLElement to bound objects. */
   binding = new ViewModelBinding()
   /** All LogID are managed by this single instance. */
   LogIDs = new LogIDs()
   /** All ViewListener are managed by this single instance. */
   Listeners = new ViewListeners()

   ////////////
   // Facade //
   ////////////

   /**
    * @returns {ViewModel[]} The {@link ViewModel ViewModels} attached to connected HTML elements.
    */
   getBoundViewModels() {
      const elements = this.binding.getBoundElements(document,ViewModelBinding.MODEL)
      return elements.flatMap( htmlElem => this.binding.getObjects(htmlElem,ViewModelBinding.MODEL))
   }

}

// LogIDs -> IndexID IID

/**
 * Bind an HTMLElement to a set of LogID.
 * Allows to query for a LogID by its index.
 */
class LogIDs {
   INDEX = 1
   index2ID = new Map(/*index->LogID*/)
   //
   // Index management
   //
   nextIndex() {
      const index = this.INDEX ++
      return index
   }
   //
   // Binding HTMLElement <-> LogID
   //
   onNew(LID) {
      this.index2ID.set(LID.index,LID)
   }
   onFree(LID) {
      this.index2ID.delete(LID.index)
   }
   getID(index) {
      return this.index2ID.get(index)
   }
   getElement(index) {
      return VMX.binding.getElement(index)
   }
   /**
    * If this function returns a non empty set then disconnected html elements
    * exist with undetached (not freed) ViewModels.free
    * @returns {Set<LogID>} All ids which are no more bound to connected HTML elements.
    */
   unboundIDs() {
      const elements = VMX.binding.getBoundElements(document)
      const boundids = new Set()
      for (const elem of elements) {
         for (const id of VMX.binding.getIDs(elem))
            boundids.add(id)
      }
      const unbound = new Set()
      for (const id of this.index2ID.values())
         !boundids.has(id) && unbound.add(id)
      return unbound
   }
   removeUnboundIDs() {
      const unbound = this.unboundIDs()
      for (const id of unbound)
         id.free()
   }
}

/**
 * Assigns a log-ID to a component which produces log entries.
 */
class LogID {
   #owner   // reference to context object this one is part of
   #name    // name of individual object
   #parent  // reference to parent LogID
   #index

   constructor(owner, name, parent) {
      const type = owner.constructor?.name ?? `[${typeof owner}]`
      this.#owner = owner
      this.#name = name != null ? String(name) : type
      this.#parent = parent instanceof LogID ? parent : null
      this.#index = VMX.LogIDs.nextIndex()
      VMX.LogIDs.onNew(this)
   }
   free() {
      if (this.#index != null) {
         VMX.LogIDs.onFree(this)
         this.#owner = null
         this.#name = null
         this.#parent = null
         this.#index = null
      }
   }

   parentChain() {
      return this.#parent
            ? this.#parent.parentChain()+"["+this.#parent.#index+"]/"
            : ""
   }

   toString() {
      return `${this.parentChain()}${this.#name}-[${this.#index}]`
   }

   get name() {
      return this.#name
   }
   set name(name) {
      this.#name = name
   }
   get index() {
      return this.#index
   }
   get owner() {
      return this.#owner
   }
}

/**
 * Stores callbacks which are called before the
 * View is rendered to the screen.
 * Implemented with help of requestAnimationFrame.
 */
class ViewUpdateListener {
   #callbacks = []
   #frameID = null
   constructor(callback, delayCounter) {
      callback && this.onceOnViewUpdate(callback, delayCounter)
   }
   #onUpdate(timestamp) {
      const callbacks = this.#callbacks
      this.#callbacks = []
      this.#frameID = null
      for (const callback of callbacks)
         callback(timestamp)
   }
   onceOnViewUpdate(callback, delayCounter) {
      this.#callbacks.push( 0 < delayCounter && delayCounter < 300
         ? () => this.onceOnViewUpdate(callback, delayCounter-1)
         : callback
      )
      if (this.#frameID == null)
         this.#frameID = requestAnimationFrame(this.#onUpdate.bind(this))
   }
}

/**
 * Stores state of event listener added to HTML view.
 */
class ViewListener {
   /////////////
   // Options //
   /////////////
   static TARGET_ONLY = true

   /**
    * Stores added listener state.
    *
    * Listeners are reference counted so adding the same event handler
    * more than once (with same event type on same HTML element)
    * increments the count.
    *
    * === Background info
    * HTML events know a target and a currentTarget.
    * The target is where the event originates and the currentTarget
    * denotes an HTML element for which the executed listener has been registered.
    * Events bubble therefore html elements which are nested within each other all receive the same event.
    * ===
    *
    * @param {HTMLElement} htmlElem - The HTML element event handler is added to.
    * @param {string} eventType - The event type ("click","touchstart",...).
    * @param {(e:Event)=>void} eventHandler - Function which is called to handle events. The first argument is the HTML event.
    * @param {object|null} owner - Owner of the listener (used as reference in other listeners to block this one).
    * @property {object[]} blockedOwners - An array of owners which have lower priority. Listeners owned by them are suppressed if an event for this listener is received.
    * @property {boolean} TARGET_ONLY - If set to true all other event handlers not registered for this specific target are ignored (listeners registered for child or parent nodes).
    *                                   And also this listener is ignored if the event targets not the same HTML element as this listener is registered for.
    */
   constructor(htmlElem, eventType, eventHandler, owner=null, blockedOwners=[], TARGET_ONLY=false) {
      this.htmlElem = htmlElem
      this.eventType = eventType
      this.eventHandler = eventHandler
      this.owner = owner
      this.blockedOwners = blockedOwners.filter(o => o != null)
      this.referenceCount = 1
      this.TARGET_ONLY = TARGET_ONLY
      this.LID = new LogID(this, null, owner?.LID)
   }
   free() {
      this.LID.free()
      this.htmlElem = null
   }
   addReference() {
      return ++ this.referenceCount
   }
   removeReference() {
      return -- this.referenceCount
   }
}

/**
 * Manages event listeners and allows to prioritize them.
 * One listener could block one or more other listeners.
 * So if two listeners would be executed, and one is blocked by the other
 * only one is executed. Higher priority of a listener is expresses by
 * blocking the one with lower priority.
 */
class ViewListeners {
   htmlElements = new WeakMap()
   bubbleEvent = null
   captureEvent = null
   blockedOwners = null
   capturedTargets = null
   capturedListeners = null
   processedListener = null
   TARGET_ONLY = false
   onCaptureBind = null
   onBubbleBind =  null
   DEBUG = true
   constructor() {
      this.onCaptureBind = this.onCapture.bind(this)
      this.onBubbleBind = this.onBubble.bind(this)
   }
   error(msg) {
      throw Error(`ViewListeners error: ${msg}`)
   }
   onCapture(e) {
      if (!this.captureEvent) {
         this.captureEvent = e
         this.bubbleEvent = null
         this.blockedOwners = new Set()
         this.capturedTargets = new WeakSet()
         this.capturedListeners = this.DEBUG && new Set() || null
         this.processedListeners = this.DEBUG && new Set() || null
         this.TARGET_ONLY = false
      }
      else if (e !== this.captureEvent)
         this.error("Internal: Event loop processes two capture events at same time.")
      if (this.capturedTargets.has(e.currentTarget)) {
         console.warn("Internal: onCapture called with same target more than once.")
      }
      else {
         this.capturedTargets.add(e.currentTarget)
         const listeners = this.get(e.currentTarget,e.type)
         for (const listener of listeners) {
            for (const owner of listener.blockedOwners) {
               this.blockedOwners.add(owner)
            }
            if (listener.TARGET_ONLY && e.target === e.currentTarget)
               this.TARGET_ONLY = true
            this.DEBUG && this.capturedListeners.add(listener)
         }
      }
   }
   onBubble(e) {
      if (!this.bubbleEvent) {
         this.bubbleEvent = e
         this.captureEvent = null
         if (this.DEBUG && !this.capturedListeners.size)
            console.warn("Internal: capture phase found no listener(s).")
      }
      else if (e !== this.bubbleEvent)
         this.error("Internal: Event loop processes two bubble events at same time.")
      if (  this.capturedTargets.has(e.currentTarget)
         && (!this.TARGET_ONLY || e.currentTarget === e.target)) {
         const listeners = this.get(e.currentTarget,e.type)
         for (const listener of listeners) {
            if (!this.blockedOwners.has(listener.owner)
               && (!listener.TARGET_ONLY || e.currentTarget === e.target)) {
               try { listener.eventHandler(e) } catch(exc) { logException(exc) }
               this.DEBUG && this.processedListeners.add(listener)
            }
         }
      }
      this.capturedTargets.delete(e.currentTarget)
      if (this.DEBUG && !this.capturedTargets.size && !this.processedListeners.size)
         console.warn("Internal: bubble phase processed no listener (all blocked).")
   }
   addListener(htmlElem, eventType, eventHandler, {blockedOwners, owner, TARGET_ONLY}={}) {
      const eventTypes = this.htmlElements.get(htmlElem) || {}
      const evHandlers = eventTypes[eventType] || new Map()
      if (!evHandlers.has(eventHandler)) {
         const listener = new ViewListener(htmlElem, eventType, eventHandler, owner, blockedOwners, TARGET_ONLY)
         if (evHandlers.size === 0) {
            eventTypes[eventType] = evHandlers
            this.htmlElements.set(htmlElem,eventTypes)
            listener.htmlElem.addEventListener(listener.eventType,this.onCaptureBind,{capture:true,passive:false})
            listener.htmlElem.addEventListener(listener.eventType,this.onBubbleBind,{capture:false,passive:false})
         }
         evHandlers.set(eventHandler,listener)
         VMX.binding.bind(listener.htmlElem,listener.LID)
         return listener
      }
      else {
         const listener = evHandlers.get(eventHandler)
         listener.addReference()
         return listener
      }
   }
   removeListener(listener) {
      const eventTypes = this.htmlElements.get(listener?.htmlElem)
      const evHandlers = eventTypes && eventTypes[listener.eventType]
      const isListener = evHandlers && evHandlers.get(listener.eventHandler)
      if (isListener && listener.removeReference() <= 0) {
         evHandlers.delete(listener.eventHandler)
         if (evHandlers.size === 0) {
            listener.htmlElem.removeEventListener(listener.eventType,this.onCaptureBind,{capture:true,passive:false})
            listener.htmlElem.removeEventListener(listener.eventType,this.onBubbleBind,{capture:false,passive:false})
         }
         VMX.binding.unbind(listener.htmlElem,listener.LID)
         listener.free()
      }
   }
   /**
    * Returns iterator for iterating over all matching listeners.
    * @param {HTMLElement} htmlElem The HTML element whose listeners are queried.
    * @param {string=} eventType Optional type of the event the listener must match else listeners with any event type are matched.
    * @param {(e:Event)=>void} eventHandler Optional event handler function the listener must match else all listeners with any event handler are matched.
    */
   get(htmlElem, eventType, eventHandler) {
      const eventTypes = this.htmlElements.get(htmlElem)
      if (!eventTypes)
         return [].values()
      if (eventType)
         if (eventHandler)
            return eventTypes[eventType] && eventTypes[eventType].get(eventHandler)
               ? [eventTypes[eventType].get(eventHandler)].values()
               : [].values()
         else
            return (eventTypes[eventType] || []).values()
      return (function*() {
         for (const key in eventTypes) {
            const evHandlers = eventTypes[key]
            if (evHandlers) {
               if (!eventHandler)
                  yield* evHandlers.values()
               else {
                  const listener = evHandlers.get(eventHandler)
                  if (listener)
                     yield listener
               }
            }
         }
      })()
   }
   isListener(htmlElem, eventType, eventHandler) {
      return !this.get(htmlElem,eventType,eventHandler).next().done
   }
   getListenedElements(rootElem, eventType, eventHandler) {
      const result = []
      if (this.isListener(rootElem,eventType,eventHandler))
         result.push(rootElem)
      for (const htmlElem of rootElem.querySelectorAll("*")) {
         if (this.isListener(htmlElem,eventType,eventHandler))
            result.push(htmlElem)
      }
      return result
   }
   static getListenedElements(rootElem, eventType, eventHandler) {
      return VMX.Listeners.getListenedElements(rootElem,eventType,eventHandler)
   }
}

class ActionBlocking {
   ///////////
   // Types //
   ///////////
   /**
    * Describes a named action delivered by a ViewController
    * as reaction to one or more event callbacks.
    */
   static ActionContext = class ActionContext {
      name
      param = {}
      controller
      target
      targetContainer
      constructor(name, param, controller, target, targetContainer/*currentTarget*/) {
         this.name = name
         this.param = param
         this.controller = controller
         this.target = target // htmlElem
         this.targetContainer = targetContainer // htmlElem (target or parent of target where listener was added)
      }
   }
   /**
    * Base class for implementing different blocking strategies.
    */
   static BlockingStrategy = class BlockingStrategy {
      #callback
      constructor(callback) {
         this.#callback = callback
      }
      isActionBlocked(actionContext) {
         return this.#callback(actionContext)
      }
   }
   static ExcludeInputElements = new ActionBlocking.BlockingStrategy( (ac) => {
         const target = ac.target
         const nodeName = target.nodeName
         return ( nodeName === "INPUT"
                  || nodeName === "SELECT"
                  || target === document.activeElement) // ac.target.matches(":focus")
   })
   static AlwaysBlock = new ActionBlocking.BlockingStrategy( (ac) => {
      return true
   })

   #namedBlockings = new Map()

   constructor() {
   }
   error(msg) {
      throw Error(`ActionBlocking error: ${msg}`)
   }

   #isActionBlocked(blockings, actionContext) {
      if (blockings) {
         for (const blk of blockings) {
            if (blk.isActionBlocked(actionContext))
               return true
         }
      }
      return false
   }

   isActionBlocked(name, controller, target/*htmlElem*/, targetContainer/*htmlElem containing target*/) {
      if (typeof name !== "string")
         this.error("expect »name« of type string")
      if (!(target instanceof Node))
         this.error("expect »target« of type Node")
      if (!(targetContainer instanceof HTMLElement))
         this.error("expect targetContainer« of type HTMLElement")
      const actionContext = new ActionBlocking.ActionContext(name, controller, target, targetContainer)
      const blockings = this.#namedBlockings.get(name)
      return this.#isActionBlocked(blockings, actionContext)
   }

   addBlocking(name, ...blockingStrategies) {
      const blockings = this.#namedBlockings.get(name)
      if (blockings)
         blockings.push(blockingStrategies)
      else
         this.#namedBlockings.set(name, blockingStrategies)
   }
   alwaysBlock(name) {
      this.#namedBlockings.set(name, [ActionBlocking.AlwaysBlock])
   }
}

/**
 * Supports event listeners and blocking of other controllers.
 */
class ViewController {
   #isActive = false
   #listeners = []
   #activeListeners = []
   #blockedControllers = []
   #blockingCount = 0
   #othersBlocked = false
   #htmlElem
   #callback
   options
   LID
   static preventDefault = (e) => e.preventDefault()
   static Options(options) {
      return typeof options === "object" && !(options instanceof ViewController)
            ? options
            : {}
   }

   constructor(htmlElem, callback, ...blockedControllers) {
      const options = ViewController.Options(blockedControllers[0])
      this.#htmlElem = htmlElem
      this.#callback = callback
      this.#blockedControllers = blockedControllers.flat(Infinity).filter(cntrl => cntrl instanceof ViewController)
      this.options = { ...options, TARGET_ONLY:Boolean(options.TARGET_ONLY) }
      this.LID = new LogID(this, this.options.name, this.options.parentLID)
      VMX.binding.bind(htmlElem,this.LID)
   }
   // TODO: change remove() into free()
   remove() {
      this.deactivate()
      this.removeListeners(this.#listeners)
      this.removeListeners(this.#activeListeners)
      VMX.binding.unbind(this.#htmlElem,this.LID)
      this.LID.free()
      this.#htmlElem = null
      this.#callback = null
      this.#blockedControllers = []
      return this
   }
   get htmlElem() {
      return this.#htmlElem
   }
   get callback() {
      return this.#callback
   }
   get isActive() {
      return this.#isActive
   }
   get isBlocked() {
      return Boolean(this.#blockingCount)
   }
   set callback(callback) {
      this.#callback = callback
   }
   castEvent(type, e, optional) {
      const target = !e || e.target==document ? document.documentElement : e.target
      const timeStamp = e?.timeStamp
      return optional
               ? {type, target, currentTarget:this.htmlElem, timeStamp, orig:e,
                  ...optional}
               : {type, target, currentTarget:this.htmlElem, timeStamp, orig:e}
   }
   callAction(type, e, optional) {
      if (!this.isBlocked && this.#callback)
         this.#callback(this.castEvent(type,e,optional))
   }
   callActionOnce(type, e, optional) {
      const callback = this.#callback
      if (!this.isBlocked && callback) {
         this.#callback = null
         callback(this.castEvent(type,e,optional))
      }
   }
   removeListeners(listenerArray) {
      listenerArray.forEach(listener => listener && VMX.Listeners.removeListener(listener))
      listenerArray.length = 0
   }
   block() {
      (1 === ++this.#blockingCount) && this.onBlock()
   }
   unblock() {
      (0 === --this.#blockingCount) && this.onUnblock()
   }
   blockOthers(isBlocked) {
      if (this.#othersBlocked !== Boolean(isBlocked)) {
         this.#othersBlocked = Boolean(isBlocked)
         if (this.#othersBlocked)
            for (const contr of this.#blockedControllers)
               contr.block()
         else
            for (const contr of this.#blockedControllers)
               contr.unblock()
      }
   }
   deactivate(onDeactivateCallback) {
      if (this.#isActive) {
         this.#isActive = false
         this.removeListeners(this.#activeListeners)
         this.blockOthers(false)
         onDeactivateCallback?.()
      }
   }
   activate(onActivateCallback) {
      if (!this.#isActive) {
         this.#isActive = true
         this.blockOthers(true)
         onActivateCallback?.()
      }
   }
   #addListener(htmlElem, eventType, eventHandler, TARGET_ONLY) {
      return VMX.Listeners.addListener(htmlElem, eventType, eventHandler,
         { owner: this, blockedOwners: this.#blockedControllers, TARGET_ONLY }
      )
   }
   addListener(htmlElem, eventType, eventHandler) {
      this.#listeners.push(this.#addListener(htmlElem,eventType,eventHandler,this.options.TARGET_ONLY))
      return this.#listeners.at(-1)
   }
   addActiveListener(htmlElem, eventType, eventHandler) {
      this.#activeListeners.push(this.#addListener(htmlElem,eventType,eventHandler,false))
      return this.#activeListeners.at(-1)
   }
   removeActiveListenerForElement(htmlElem) {
      const removedListeners = []
      this.#activeListeners = this.#activeListeners.filter( listener =>
         (listener.htmlElem === htmlElem
            ? (removedListeners.push(listener), false)
            : true)
      )
      this.removeListeners(removedListeners)
   }
   hasActiveListenerForElement(htmlElem) {
      return this.#activeListeners.some( listener => listener.htmlElem === htmlElem)
   }
   get nrActiveListeners() {
      return this.#activeListeners.length
   }
   /////////////////////////////
   // overwritten in subclass //
   /////////////////////////////
   onBlock() {
      // do nothing
   }
   onUnblock() {
      // do nothing
   }
   ////////////
   // Helper //
   ////////////
   touchIDs(e, active) {
      let strActive = ""
      for (const id of active ?? [])
         strActive += (strActive.length?",":"") + id
      if (e.touches) {
         let touches=[...e.touches], targetTouches=[...e.targetTouches], changedTouches=[...e.changedTouches]
         let ids = "touchids all:["
         touches.forEach( (t) => {
            ids += (ids.at(-1)!="["?",":"") + t.identifier
         })
         ids += "] tg:["
         targetTouches.forEach( t => {
            ids += (ids.at(-1)!="["?",":"") + t.identifier
         })
         ids += "] c:["
         changedTouches.forEach( t => {
            ids += (ids.at(-1)!="["?",":"") + t.identifier
         })
         return ids+"] a:["+strActive+"]"
      }
      return "touchids all:[] tg:[] c:[] a:["+strActive+"]"
   }
}

class TouchController extends ViewController {
   /**
    * Stores end of last touch (or mouse up) and allows to recognize a simulated mouse click (in case of touchstart/touchend).
    */
   #lastEvent
   /**
    * Stores touch identifiers of touches which originated within the listened element or its childs.
    */
   #targetIDs=new Set()

   /**
    *
    * @param {HTMLElement} htmlElem
    * @param {({type:string,target:HTMLElement,currentTarget:HTMLElement})=>void} clickCallback
    * @param {{nomouse:boolean}} [options] Optional object which contains additional options.
    *                                      Set nomouse to true if you want to support only touch devices.
    * @param  {...any} blockedControllers
    */
   constructor(htmlElem, clickCallback, ...blockedControllers) {
      super(htmlElem, clickCallback, ...blockedControllers)
      const onStart = this.onStart.bind(this)
      !this.options.nomouse && this.addListener(htmlElem,"mousedown",onStart)
      this.addListener(htmlElem,"touchstart",onStart)
   }

   touchIDs(e) {
      return super.touchIDs(e, this.#targetIDs)
   }

   deactivate(onDeactivateCallback) {
      super.deactivate( () => {
         this.#targetIDs.clear()
         onDeactivateCallback?.()
      })
   }

   addActiveTouch(e) {
      if (e.changedTouches)
         for (const touch of e.changedTouches)
            this.#targetIDs.add(touch.identifier)
   }

   removeActiveTouch(e) {
      if (e.changedTouches)
         for (const touch of e.changedTouches)
            this.#targetIDs.delete(touch.identifier)
   }

   nrActiveTouches(e) {
      return this.#targetIDs.size
   }

   getActiveTouch(e) {
      if (e.touches)
         for (const touch of e.touches)
            if (this.#targetIDs.has(touch.identifier))
               return touch
   }

   getActiveTouches(e) {
      const activeTouches = []
      if (e.touches) {
         for (const touch of e.touches) {
            if (this.#targetIDs.has(touch.identifier))
               activeTouches.push(touch)
         }
         if (activeTouches.length !== this.#targetIDs.size) {
            console.warn("this.#targetIDs contains invalid values")
            this.#targetIDs.clear()
            for (const touch of activeTouches)
               this.#targetIDs.add(touch.identifier)
         }
      }
      return activeTouches
   }

   extractPos(e) {
      return { x:e.clientX, y:e.clientY }
   }

   isSimulatedMouse(e, lastEvent) {
      return lastEvent?.type.startsWith("t") && e.type.startsWith("m")
             && (lastEvent.timeStamp-e.timeStamp) <= 400
   }

   castEvent(type, e, optional) {
      return super.castEvent(type, e, { device:e.type[0]=="t" ? "touch" : "mouse", ...optional })
   }

   onActivate(e) {
      const isMove = this.onMove !== undefined
      e.touches
      ? ( isMove && this.addActiveListener(e.currentTarget,"touchmove",this.onMove.bind(this)),
          this.addActiveListener(e.currentTarget,"touchend",this.onEnd.bind(this)),
          this.addActiveListener(e.currentTarget,"touchcancel",this.onEnd.bind(this)))
      : ( isMove && this.addActiveListener(document,"mousemove",this.onMove.bind(this)),
          this.addActiveListener(document,"mouseup",this.onEnd.bind(this)))
   }

   onStart(e) {
      logError(`${this.LID} onStart ${e.type} ${this.touchIDs(e)}`)
      this.addActiveTouch(e)
      this.activate( () => {
         this.onActivate(e)
         if (!this.isSimulatedMouse(e, this.#lastEvent))
            this.callAction("touchstart", e)
      })
   }

   /*
   onMove(e) {
      logError(`${this.LID} onMove ${e.type} ${this.touchIDs(e)}`)
   }
   */

   onEnd(e) {
      logError(`${this.LID} onEnd ${e.type} ${this.touchIDs(e)}`)
      this.removeActiveTouch(e)
      if (this.nrActiveTouches())
         return
      this.deactivate()
      const isSimulatedMouse = this.isSimulatedMouse(e, this.#lastEvent)
      this.#lastEvent = { type:e.type, timeStamp:e.timeStamp }
      if (!isSimulatedMouse) {
         const pos = this.extractPos(e.changedTouches?.[0] ?? e)
         const contained = isFinite(pos.x) && isFinite(pos.y) && this.htmlElem.contains(document.elementFromPoint(pos.x,pos.y))
         logError(this.LID+" call action "+pos.x+","+pos.y+","+contained)
         if (!e.type.endsWith("l")) // mouseup|touchend
            this.callAction("touchend", e, {contained})
         else
            this.callAction("touchcancel", e, {contained})
      }
   }
}

class ClickController extends TouchController {
   callAction(type, e, optional) {
      if (type.endsWith("d") && optional.contained) { // touchend
         logError(this.LID+" click action")
         super.callAction("click", e, optional)
      }
   }
}

class MoveController extends TouchController {
   #pos
   #totalxy
   #negateX
   #negateY

   constructor(htmlElem, moveCallback, ...blockedControllers) {
      super(htmlElem, moveCallback, ...blockedControllers)
      this.#negateX = Boolean(this.options.negateX) ? -1 : +1
      this.#negateY = Boolean(this.options.negateY) ? -1 : +1
   }

   blockTarget(e) {
      return ( e.target.nodeName === "INPUT"
               || e.target.nodeName === "SELECT"
               || e.target.matches?.(":focus")
            )
   }

   onStart(e) {
      logError(`${this.LID} onStart ${e.type} ${this.touchIDs(e)}`)
      this.addActiveTouch(e)
      if (this.isActive)
         return
      const touch = this.getActiveTouch(e)
      this.#pos = this.extractPos(touch || e)
      this.#totalxy = { x:0, y:0 }
      !this.blockTarget(e) && e.preventDefault()
      this.activate( () => this.onActivate(e))
   }

   onMove(e) {
      if (this.blockTarget(e) || this.nrActiveTouches() > 1)
         return
      const touch = this.getActiveTouch(e)
      const pos = this.extractPos(touch || e)
      const delta = { dx:Math.trunc(pos.x-this.#pos.x), dy:Math.trunc(pos.y-this.#pos.y) }
      if (delta.dx || delta.dy) {
         this.#pos = { x:delta.dx+this.#pos.x, y:delta.dy+this.#pos.y }
         this.#totalxy = { x:delta.dx+this.#totalxy.x, y:delta.dy+this.#totalxy.y }
         this.callAction("move",e,{ dx:this.#negateX*delta.dx, dy:this.#negateY*delta.dy})
      }
   }

   onEnd(e) {
      logError(`${this.LID} onEnd ${e.type} ${this.touchIDs(e)}`)
      this.removeActiveTouch(e)
      if (this.nrActiveTouches()) {
         this.#pos = this.extractPos(this.getActiveTouch(e))
         return
      }
      this.deactivate()
   }
}

class TouchResizeController extends TouchController {
   #pos
   #totalxy

   constructor(htmlElem, resizeCallback, ...blockedControllers) {
      super(htmlElem, resizeCallback, {...ViewController.Options(blockedControllers[0]), nomouse:true}, ...blockedControllers)
   }

   blockTarget(e) {
      return ( e.target.nodeName === "INPUT"
               || e.target.nodeName === "SELECT"
               || e.target.matches(":focus")
            )
   }

   extractPos2(e1,e2) {
      return { x:Math.abs(e1.clientX - e2.clientX), y:Math.abs(e1.clientY - e2.clientY) }
   }

   onStart(e) {
      logError(`${this.LID} onStart ${e.type} ${this.touchIDs(e)}`)
      this.addActiveTouch(e)
      if (this.nrActiveTouches() === 2) {
         const touches = this.getActiveTouches(e)
         this.#pos = this.extractPos2(touches[0],touches[1])
         this.#totalxy = { x:0, y:0 }
      }
      this.activate( () => (this.onActivate(e), !this.blockTarget(e) && e.preventDefault()))
   }

   onMove(e) {
      // logError(`${this.LID} onMove ${e.type} ${this.touchIDs(e)} blockTarget:${this.blockTarget(e)},${this.blockTarget(touches[0])},${this.blockTarget(touches[1])}`)
      if (this.nrActiveTouches() !== 2)
         return
      e.preventDefault()
      const touches = this.getActiveTouches(e)
      const pos = this.extractPos2(touches[0],touches[1])
      const delta = { dx:Math.trunc(pos.x-this.#pos.x), dy:Math.trunc(pos.y-this.#pos.y) }
      if (delta.dx || delta.dy) {
         this.#pos = { x:delta.dx+this.#pos.x, y:delta.dy+this.#pos.y }
         this.#totalxy = { x:delta.dx+this.#totalxy.x, y:delta.dy+this.#totalxy.y }
         this.callAction("resize",e,delta)
      }
   }

   onEnd(e) {
      logError(`${this.LID} onEnd ${e.type} ${this.touchIDs(e)}`)
      this.removeActiveTouch(e)
      if (this.nrActiveTouches()) {
         if (this.nrActiveTouches() == 2) {
            const touches = this.getActiveTouches(e)
            this.#pos = this.extractPos2(touches[0],touches[1])
         }
         return
      }
      this.deactivate()
   }
}

class TransitionController extends ViewController {

   constructor(htmlElem, callback, ...blockedControllers) {
      super(htmlElem, callback, ...blockedControllers)
      this.addListener(this.htmlElem,"transitionrun",this.onStart.bind(this))
      this.onStart()
   }

   isInTransition() {
      return this.htmlElem && TransitionController.isInTransition(this.htmlElem)
   }

   ensureInTransitionElseEnd() {
      if (! this.isInTransition())
         this.onEnd()
   }

   onStart() {
      const transitions = TransitionController.getTransitions(this.htmlElem)
      const newTransitions = transitions.filter( transition => !this.hasActiveListenerForElement(transition))
      if (newTransitions.length) {
         const onEnd = this.onEnd.bind(this)
         this.activate()
         newTransitions.forEach( transition => {
            this.addActiveListener(transition, "cancel", onEnd)
            this.addActiveListener(transition, "finish", onEnd)
            this.addActiveListener(transition, "remove", onEnd)
         })
      }
   }

   onEnd(e) {
      if (!this.isInTransition()) {
         try { this.callActionOnce("transitionend",e,{isInTransition:false}) }
         finally { this.remove() }
      }
      else if (e) {
         this.removeActiveListenerForElement(e.target)
      }
   }

   static getTransitions(htmlElem) {
      return htmlElem.getAnimations(/*includes no childs*/).filter(animation => animation instanceof CSSTransition)
   }
   static isInTransition(htmlElem) {
      const animations = htmlElem.getAnimations(/*includes no childs*/)
      return animations.some(animation => animation instanceof CSSTransition)
   }
   static endTransition(htmlElem) {
      const transitions = this.getTransitions(htmlElem)
      transitions.forEach( transition => transition.finish())
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

   /** Stores last entered HTML element for drop operation.
    * Needed to support child elements within dropzone. */
   #startTarget = null

   constructor(htmlElem, dropCallback, ...blockedControllers) {
      super(htmlElem, dropCallback, ...blockedControllers)
      this.addListener(htmlElem,"dragenter",this.onStart.bind(this))
      // enable executing drop handler
      this.addListener(document,"dragover",DropController.preventDefault)
      // prevent opening of images if dropped not into target zone
      this.addListener(document,"drop",DropController.preventDefault)
   }

   castEvent(type, e) {
      return {type, target:e.target, currentTarget:e.currentTarget, items:e.dataTransfer.items }
   }

   onStart(e) {
      this.#startTarget = e.target // dragenter is sent before dragleave
      if (! this.isActive) {
         this.callAction("dropstart",e)
         this.activate( () => {
            this.addActiveListener(e.currentTarget,"drop",this.onDrop.bind(this)),
            this.addActiveListener(e.currentTarget,"dragleave",this.onEnd.bind(this))
         })
      }
   }

   onDrop(e) {
      e.preventDefault()
      try { this.callAction("drop",e) }
      finally { this.onEnd(e) }
   }

   onEnd(e) {
      if (this.#startTarget === e.target
         // ESCAPE key aborts operation and text child could be returned instead of startTarget
         || e.target.attributes === undefined) {
         try { this.callAction("dropend",e) }
         finally { this.deactivate() }
      }
   }
}

class ChangeController extends ViewController {

   constructor(htmlElem, dropCallback, ...blockedControllers) {
      const group = (Symbol.iterator in htmlElem) ? htmlElem : [ htmlElem ]
      super(group[0], dropCallback, ...blockedControllers)
      const onChange = this.onChange.bind(this)
      for (const elem of group)
         this.addListener(elem,"change",onChange)
   }

   castEvent(type, e) {
      const value = e.target.checked === undefined || e.target.checked
                     ? e.target.value
                     : ""
      return super.castEvent(type, e, { name:e.target.name, value, checked:e.target.checked})
   }

   onChange(e) {
      this.callAction("change",e)
   }
}

class FocusController extends ViewController {
   constructor(htmlElem, dropCallback, ...blockedControllers) {
      super(htmlElem, dropCallback, ...blockedControllers)
      const onFocus = this.onFocus.bind(this)
      const onBlur = this.onFocus.bind(this)
      // this.addListener(htmlElem,"focus",onFocus)
      // this.addActiveListener(e.target,"blur",onBlur)
      this.addListener(htmlElem,"focusin",onFocus)
      this.addListener(e.target,"focusout",onBlur)
   }
   onFocus(e) {
      this.callAction("focus",e)
   }
   onBlur(e) {
      this.callAction("blur",e)
   }
}

class KeyController extends ViewController {
   constructor(htmlElem, dropCallback, ...blockedControllers) {
      super(htmlElem, dropCallback, ...blockedControllers)
      this.addListener(htmlElem,"keydown",this.onKeyDown.bind(this))
      this.addListener(htmlElem,"keyup",this.onKeyUp.bind(this))
   }
   castEvent(type, e) {
      return super.castEvent(type, e, { key:e.key, altKey:e.altKey, ctrlKey:e.ctrlKey, metaKey:e.metaKey, repeat:e.repeat })
   }
   onKeyDown(e) {
      this.callAction("keydown",e)
   }
   onKeyUp(e) {
      this.callAction("keyup",e)
   }
}

class HTMLView {
   static error(msg) {
      throw Error(`View error: ${msg}`)
   }
   static needsBorderBox(htmlElem) {
      const needValue = "border-box"
      const styleProp = "box-sizing"
      const value = getComputedStyle(htmlElem)[styleProp]
      if (value != needValue) {
         console.warn(`${this.constructor.name} needs style['${styleProp}']=='${needValue}' instead of '${value}' to work properly on html element`, htmlElem)
         htmlElem.style.setProperty(styleProp,needValue)
      }
   }
   static isEmptyTextNode(htmlElem) {
      return htmlElem && htmlElem.nodeType === 3/*Node.TEXT_NODE*/ && htmlElem.textContent.trim() === ""
   }
   static isDocumentFragment(htmlElem) {
      return htmlElem && htmlElem.nodeType === 11/*Node.DOCUMENT_FRAGMENT_NODE*/
   }
   static getComputedStyles(htmlElem, styles) {
      const computedStyle = getComputedStyle(htmlElem)
      const styleValues = {}
      if (Array.isArray(styles)) {
         for (const key of styles)
            styleValues[key] = computedStyle[key]
      }
      else {
         for (const key in styles)
            styleValues[key] = computedStyle[key]
      }
      return styleValues
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
   static parseNr(str, defaultValue=0) {
      const nr = parseFloat(str)
      return isNaN(nr) ? defaultValue : nr
   }
   static getInitialPos(htmlElem) {
      const isNotConnected = !htmlElem.isConnected
      isNotConnected && document.body.appendChild(htmlElem)
      const cstyle = getComputedStyle(htmlElem)
      const position = cstyle.position
      const height = this.parseNr(cstyle.height)
      const width = this.parseNr(cstyle.width)
      const isStatic = position === "static"
      const top = isStatic ? 0 : this.parseNr(cstyle.top)
      const left = isStatic ? 0 : this.parseNr(cstyle.left)
      isNotConnected && htmlElem.remove()
      return { width:width||100, height:height||100, top, left, position }
   }
   /**
    * Selects an HTML element from the document and clones it if second argument is provided and true.
    * If the first argument is already an HTML element or undefined it is returned instead.
    * @param {HTMLElement|string|undefined} elemSel - A CSS selector or an HTML element.
    * @param {boolean=} isClone - Optional flag. Set value to true if a deep clone of the HTML element should be returned.
    **/
   static query(elemSel, isClone=false) {
      if (typeof elemSel === "string") {
         const htmlElem = document.querySelector(elemSel) ?? this.error(`Found no html node matching '${elemSel}'.`)
         if (htmlElem.localName === "template") {
            const child = htmlElem.content.children[0]?.cloneNode(true) ?? this.error(`Found no child node inside matching template '${elemSel}'.`)
            // connect at least once (touchstart wont work on tablet in case element created from template)
            return (document.body.append(child), child.remove(), child)
         }
         return isClone ? htmlElem.cloneNode(true) : htmlElem
      }
      return isClone && elemSel ? elemSel.cloneNode(true) : elemSel
   }
   static clone(elemSel) {
      return this.query(elemSel,true)
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
}

class View extends HTMLView {
   #htmlElem
   static htmlElem(viewElem) {
      if (viewElem instanceof View)
         return viewElem.#htmlElem
      return viewElem // instanceof HTMLElement
   }
   constructor(htmlElem) {
      super()
      this.#htmlElem = htmlElem
   }
   error(msg) {
      throw Error(`View error: ${msg}`)
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
      const elem = this.#htmlElem
      while (elem.childNodes.length)
         elem.childNodes[0].remove()
      elem.append(node)
   }
   getComputedStyle() {
      return getComputedStyle(this.#htmlElem)
   }
   getComputedStyles(styles) {
      return HTMLView.getComputedStyles(this.#htmlElem, styles)
   }
   getStyles(styles) {
      return HTMLView.getStyles(this.#htmlElem, styles)
   }
   setStyles(styles) {
      return HTMLView.setStyles(this.#htmlElem, styles)
   }
   addClass(className) {
      this.#htmlElem.classList.add(className)
   }
   removeClass(className) {
      this.#htmlElem.classList.remove(className)
   }
   setClass(switchOn, className) {
      if (switchOn)
         this.#htmlElem.classList.add(className)
      else
         this.#htmlElem.classList.remove(className)
   }
   get initialPos() {
      return HTMLView.getInitialPos(this.#htmlElem)
   }
   get connected() {
      return this.#htmlElem.isConnected
   }
   get hidden() {
      return this.getComputedStyle().display === "none"
   }
   get displayed() {
      return this.connected && !this.hidden
   }
   get focus() {
      return HTMLView.focus(this.#htmlElem)
   }
   get focusWithin() {
      return HTMLView.focusWithin(this.#htmlElem)
   }
}

class ViewConfig {
   #values
   constructor(viewElem) {
      this.#values = ViewConfig.parse(View.htmlElem(viewElem))
   }
   entries() {
      return Object.entries(this.#values)
   }
   hasValue(name) {
      return name in this.#values
   }
   getValue(name) {
      return this.#values[name]
   }
   getValueNames() {
      return Object.keys(this.#values)
   }
   static parseError(inputStr, msg) {
      throw Error(`ViewConfig error: parsing data-config=»${inputStr}«: ${msg}.`)
   }
   static parse(htmlElem) {
      const inputStr = typeof htmlElem==="string" ? htmlElem : htmlElem.dataset.config
      const nameRegex = /^[ \t]*([_a-zA-Z][-_a-zA-Z0-9]*)?[ \t]*/
      const valueRegex = /^[ \t]*('([^'\\]|\\.)*'|"([^\\"]|\\.)*"|[^'",:}{[\]]([^,:}{[\]]*[^ \t,:}{[\]])*|)[ \t]*/
      let unparsed = inputStr
      const skip = (nrChar) => unparsed = unparsed.substring(nrChar).trimLeft()
      const match = (regex) => {
         const parsed = regex.exec(unparsed); skip(parsed[0].length); return parsed
      }
      const expect = (char, name) => {
         (unparsed[0] != char && this.parseError(inputStr,`expect »${char}« instead of »${unparsed[0] ?? 'end of input'}«${name?' after name »'+name+'«':''}${!unparsed.length?'':' at '+(unparsed.length==1?'last position':'position '+(inputStr.length-unparsed.length))}`)) || skip(1)
      }
      const parseValue = (name) => {
         const matched = match(valueRegex)[1]
         if (matched==="") {
            ["'",'"'].includes(unparsed[0]) && this.parseError(inputStr,`missing closing »${unparsed[0]}«${name?' after name »'+name+'«':''} at ${unparsed.lnegth==1?'last position':'position '+(inputStr.length-unparsed.length)}`)
            return unparsed[0]==="{" ? parseNamedValues({}, "{", name) :
                  unparsed[0]==="[" ? parseValues([], name) : undefined
         }
         return !["'",'"'].includes(matched[0]) ? matched : matched.substring(1,matched.length-1).replaceAll("\\"+matched[0],matched[0])
      }
      const parseValues = (values, name) => {
         expect("[", name)
         while (unparsed[0] != ']') {
            values.push(parseValue(name) ?? (unparsed[0] != ']' && expect("value",name)) )
            unparsed[0] != ']' && expect(",",name) || (values.at(-1) ?? value.pop())
         }
         return (expect("]", name), values)
      }
      const parseNamedValues = (values, expectChar, name) => {
         expectChar && expect("{", name)
         while (unparsed && unparsed[0] != '}') {
            const matched = match(nameRegex)[1]
            if (!matched) { unparsed && unparsed[0] != '}' && expect("name",name); break; }
            expect(":",(name = matched))
            values[name] = parseValue(name) ?? ""
            unparsed && unparsed[0] != '}' && expect(",",name)
         }
         return (expectChar && expect("}", name), values)
      }
      const values = {}
      inputStr && parseNamedValues(values)
      unparsed && this.parseError(inputStr,`unexpected char '${unparsed[0]}' at ${unparsed.length==1?'last position':'position '+(inputStr.length-unparsed.length)}`)
      return values
   }
}

class ViewSlots {
   static SLOTATTR = "data-slot"
   #slots
   constructor(viewElem,...assertedSlotNames) {
      const htmlElem = View.htmlElem(viewElem)
      this.#slots = new Map()
      ViewSlots.getSlots(htmlElem).forEach( (namedNodes, name) => {
         this.#slots.set(name, namedNodes.map( htmlElem => new View(htmlElem)))
      })
      this.assertSlotNames(assertedSlotNames, htmlElem)
   }
   error(msg) {
      throw Error(`ViewSlots error: ${msg}`)
   }
   hasSlot(name) {
      return this.#slots.has(name)
   }
   getSlot(name) {
      this.assertSlotName(name)
      return this.#slots.get(name)[0]
   }
   getSlots(name) {
      this.assertSlotName(name)
      return this.#slots.get(name)
   }
   getSlotNames() {
      return new Set(this.#slots.keys())
   }
   assertSlotName(name, htmlElem) {
      if (!this.#slots.has(name)) {
         if (htmlElem)
            console.error("htmlElem",htmlElem,htmlElem.children)
         this.error(`Missing attribute ${ViewSlots.SLOTATTR}='${name}' in HTML.`)
      }
   }
   assertSlotNames(slotNamesArray, htmlElem) {
      for (const name of slotNamesArray)
         this.assertSlotName(name, htmlElem)
   }
   static getSlots(htmlElem) {
      const selector = "["+ViewSlots.SLOTATTR+"]"
      const slots = new Map()
      const nodes = [...htmlElem.querySelectorAll(selector)]
      htmlElem.matches(selector) && nodes.push(htmlElem)
      nodes.forEach(node => {
         const name = node.dataset.slot
         const namedNodes = slots.get(name) ?? []
         namedNodes.push(node)
         slots.set(name, namedNodes)
      })
      return slots
   }
}

/**
 * Encapsulates the HTML structure of a View which therefore needs
 * a different update strategy. The same ViewDecorator and/or ViewModel
 * is able to support multiple Views with a corresponding ViewUpdater.
 */
class ViewUpdater {

   buildView() {

   }

   updateView() {

   }

}

/**
 * Stores state of an applied decorator.
 * Useful for debugging and/or undecorating HTML elements.
 */
class ViewDecorator {
   viewModel = null
   controllers = {}
   /**
    * Constructor which decorates an HTML element (creates ViewController and binds their action callbacks).
    * @param {ViewModel} viewModel - The view-model which contains the root view.
    * @param {object} options - Additional options.
    */
   constructor(viewModel, options={}) {
      this.decorate(viewModel, options)
   }
   error(msg) {
      throw Error(`ViewDecorator error: ${msg}`)
   }
   decorate(viewModel, options={}) {
      if (! this.viewModel) {
         !(viewModel instanceof ViewModel) && this.error("Argument viewModel is not of type ViewModel.")
         this.viewModel = viewModel
         this.decorateOnce(viewModel, options)
      }
   }
   undecorate() {
      const viewModel = this.viewModel
      if (viewModel) {
         this.undecorateOnce(viewModel)
         for (const key in this.controllers) {
            if (this.controllers[key] instanceof ViewController)
               this.controllers[key].remove()
         }
         this.controllers = {}
         this.viewModel = null
      }
   }
   ///////////////////////////////////////
   // Should be overwritten in subclass //
   ///////////////////////////////////////
   decorateOnce(viewModel, options) {
      this.error("decorateOnce(viewModel,options) not implemented in subclass.")
   }
   undecorateOnce(viewModel) {
      this.error("undecorateOnce(viewModel) not implemented in subclass.")
   }
}

class ResizeDecorator extends ViewDecorator {

   decorateOnce(viewModel, { moveable }) {
      const htmlElem = viewModel.htmlElem
      const width = { outer:10, border:15, edge:20 }
      const size = { border: width.border+"px", edge:width.edge+"px", outer:"-"+width.outer+"px" }
      const borders = [
         moveable && View.newDiv({top:size.outer, left:0, right:0, height:size.border, cursor:"n-resize", position:"absolute"}),
         View.newDiv({right:size.outer, top:0, bottom:0, width:size.border, cursor:"e-resize", position:"absolute"}),
         View.newDiv({bottom:size.outer, left:0, right:0, height:size.border, cursor:"s-resize", position:"absolute"}),
         moveable && View.newDiv({left:size.outer, top:0, bottom:0, width:size.border, cursor:"w-resize", position:"absolute"}),
         moveable && View.newDiv({top:size.outer, left:size.outer, width:size.edge, height:size.edge, cursor:"nw-resize", position:"absolute"}),
         moveable && View.newDiv({top:size.outer, right:size.outer, width:size.edge, height:size.edge, cursor:"ne-resize", position:"absolute"}),
         View.newDiv({bottom:size.outer, right:size.outer, width:size.edge, height:size.edge, cursor:"se-resize", position:"absolute"}),
         moveable && View.newDiv({bottom:size.outer, left:size.outer, width:size.edge, height:size.edge, cursor:"sw-resize", position:"absolute"}),
      ]
      htmlElem.append(...borders.filter(border=>Boolean(border)))
      const moveHtmlElem = viewModel.hasSlot("move") ? viewModel.getSlot("move").htmlElem : htmlElem
      this.resizeState = { borders, moveHtmlElem, oldStyles:null, isResizeable:true, controllers:null }
      const move = moveable && new MoveController(moveHtmlElem,viewModel.on.Move,{parentLID:viewModel.LID})
      const resize = new TouchResizeController(htmlElem,viewModel.on[moveable ? "Resize" : "ResizeStatic"],{parentLID:viewModel.LID})
      const moveResizeGroup = [move,resize]
      const topResize = moveable && new MoveController(borders[0],viewModel.on.ResizeTop,{TARGET_ONLY:true,negateY:true,parentLID:viewModel.LID},moveResizeGroup)
      const rightResize = new MoveController(borders[1],viewModel.on.ResizeRight,{TARGET_ONLY:true,parentLID:viewModel.LID},moveResizeGroup)
      const bottomResize = new MoveController(borders[2],viewModel.on.ResizeBottom,{TARGET_ONLY:true,parentLID:viewModel.LID},moveResizeGroup)
      const leftResize = moveable && new MoveController(borders[3],viewModel.on.ResizeLeft,{TARGET_ONLY:true,negateX:true,parentLID:viewModel.LID},moveResizeGroup)
      const topLeftResize = moveable && new MoveController(borders[4],viewModel.on.ResizeTopLeft,{TARGET_ONLY:true,negateX:true,negateY:true,parentLID:viewModel.LID},moveResizeGroup,topResize,leftResize)
      const topRightResize = moveable && new MoveController(borders[5],viewModel.on.ResizeTopRight,{TARGET_ONLY:true,negateY:true,parentLID:viewModel.LID},moveResizeGroup,topLeftResize,topResize,rightResize)
      const bottomRightResize = new MoveController(borders[6],viewModel.on.ResizeBottomRight,{TARGET_ONLY:true,parentLID:viewModel.LID},moveResizeGroup,topRightResize,bottomResize,rightResize)
      const bottomLeftResize = moveable && new MoveController(borders[7],viewModel.on.ResizeBottomLeft,{TARGET_ONLY:true,negateX:true,parentLID:viewModel.LID},moveResizeGroup,topLeftResize,bottomRightResize,bottomResize,leftResize)
      this.resizeState.controllers = { move, resize, topResize, rightResize, bottomResize, leftResize, topLeftResize, topRightResize, bottomRightResize, bottomLeftResize }
      Object.assign(this.controllers, this.resizeState.controllers)
      this.switchStyles()
   }

   undecorateOnce(viewModel) {
      this.switchStyles()
      for (const border of this.resizeState.borders)
         (border) && border.remove()
      this.resizeState = null
   }

   /**
    * Changes cursor style of decorated element to "move" or restores old style (every call switches).
    */
   switchStyles() {
      if (this.resizeState?.oldStyles)
         (View.setStyles(this.resizeState.moveHtmlElem, this.resizeState.oldStyles), this.resizeState.oldStyles=null)
      else if (this.controllers.move)
         this.resizeState.oldStyles = View.setStyles(this.resizeState.moveHtmlElem, {cursor:"move"})
   }
   switchResizeable(isResizeable) {
      if (this.resizeState.isResizeable !== Boolean(isResizeable) && this.resizeState.controllers) {
         this.resizeState.isResizeable = Boolean(isResizeable)
         if (isResizeable) {
            for (const name in this.resizeState.controllers)
               this.resizeState.controllers[name].unblock?.()
            for (const border of this.resizeState.borders)
               border && View.setStyles(border,{visibility:""})
         }
         else {
            for (const name in this.resizeState.controllers)
               this.resizeState.controllers[name].block?.()
            for (const border of this.resizeState.borders)
               border && View.setStyles(border,{visibility:"hidden"})
         }
         this.switchStyles()
      }
   }
}

class WindowDecorator extends ResizeDecorator {

   onMaximized({value:vWindowState}) {
      this.switchResizeable(vWindowState != WindowVM.MAXIMIZED)
   }

   decorateOnce(viewModel, { decoratedHtmlElem, ...options }={}) {
      this.windowState = { parentElement:null, decoratedHtmlElem, oldStyles:null, boundOnMaximized:this.onMaximized.bind(this) }
      if (decoratedHtmlElem) {
         this.windowState.parentElement = decoratedHtmlElem.parentElement
         decoratedHtmlElem.parentElement?.replaceChild(viewModel.htmlElem, decoratedHtmlElem)
         this.windowState.oldStyles = View.setStyles(decoratedHtmlElem, { width:"100%", height:"100%", position:"static" })
         viewModel.vContent = decoratedHtmlElem
      }
      super.decorateOnce(viewModel, options)
      const move = this.controllers.move
      this.controllers.close = viewModel.hasSlot("close") && new ClickController(viewModel.getSlot("close").htmlElem, viewModel.on.Close, {parentLID:viewModel.LID}, move)
      this.controllers.min = viewModel.hasSlot("min") && new ClickController(viewModel.getSlot("min").htmlElem, viewModel.on.Minimize, {parentLID:viewModel.LID}, move)
      this.controllers.max = viewModel.hasSlot("max") && new ClickController(viewModel.getSlot("max").htmlElem, viewModel.on.Maximize, {parentLID:viewModel.LID}, move)
      viewModel.addPropertyListener("vWindowState",this.windowState.boundOnMaximized)
   }

   undecorateOnce(viewModel) {
      viewModel.removePropertyListener("vWindowState",this.windowState.boundOnMaximized)
      super.undecorateOnce(viewModel)
      if (this.windowState.decoratedHtmlElem) {
         const htmlElem = viewModel.htmlElem
         View.setStyles(this.windowState.decoratedHtmlElem, this.windowState.oldStyles)
         if (this.windowState.parentElement)
            this.windowState.parentElement.replaceChild(this.windowState.decoratedHtmlElem, htmlElem)
         else
            this.windowState.decoratedHtmlElem.remove()
      }
      this.windowState = null
   }

}

/**
 * Model which is bound to a single view and manages its state.
 */
class ViewModel {
   /** Default configurations (for all instances) which could be overwritten in subclass. */
   static Default = {
      /** Defines 'Set' of valid property names which could be observed for change. */
      listenableProperties: new Set(["hidden","connected"]),
      /** Class name which sets display to none if set on htmlElement. */
      hiddenClassName: "js-d-none",
      /** Constructor of decorator which adds additional elements and controllers to the view model. */
      decorator: null
   }
   static #viewUpdateListener = new ViewUpdateListener()
   #config
   #htmlElem
   #on = {}
   #eventHandlers = {}
   #view
   #decorator
   #propertyListeners = {}
   LID
   constructor(htmlElem, configAndOptions={}) {
      this.#config = Object.create(this.constructor.Default)
      this.#htmlElem = htmlElem
      this.#view = new View(htmlElem)
      this.LID = new LogID(this, null, null)
      VMX.binding.bind(htmlElem,this.LID)
      this.callInit(ViewModel, configAndOptions)
   }
   free() {
      const htmlElem = this.htmlElem
      this.#decorator && this.#decorator.undecorate?.()
      this.#config = null
      this.#htmlElem = null
      this.#on = {}
      this.#eventHandlers = {}
      this.#view = null
      this.#decorator = null
      this.#propertyListeners = {}
      VMX.binding.unbind(htmlElem,this.LID)
      this.LID.free()
      return htmlElem
   }
   callInit(_class, { initInSuperClass, ...configAndOptions }) {
      if (_class === this.constructor || (initInSuperClass && _class === Object.getPrototypeOf(Object.getPrototypeOf(this)).constructor)) {
         const decoOptions = this.init(configAndOptions)
         const decorator = this.config("decorator")
         decorator && (this.#decorator = new decorator(this,decoOptions))
      }
   }
   //
   // Overwritten in subclass
   //
   /**
    * Initializes view model after constructor has been run.
    * Sets config and option values.
    */
   init(configAndOptions) {
      for (const [ name, value ] of new ViewConfig(this.#htmlElem).entries()) {
         if (! (name in configAndOptions))
            this.setConfig(name, value)
      }
      for (const [ name, value ] of Object.entries(configAndOptions)) {
         if (name in this.#config)
            this.setConfig(name, value)
         else
            this.setOption(name, value)
      }
   }
   /**
    * Sets certain value of this ViewModel.
    * Overwrite setOption in subclass to support more options.
    * Supported names are from ["connected","hidden","name"].
    * The name is the name of the ViewModel used in debugging.
    * @param {string} name Name of option.
    * @param {any} value Value of option.
    */
   setOption(name, value) {
      switch (name, value) {
         case "connected": this.connected = value; break;
         case "hidden": this.hidden = value; break;
         case "name": this.name = String(value); break;
         default: this.logError(`Unsupported option »${String(name)}«.`); break; // in subclass: super.setOption(name, value)
      }
   }
   //
   // Error handling
   //
   error(msg) {
      throw Error(`${this.LID} error: ${msg}`)
   }
   logError(msg) {
      console.error(`${this.LID} error: ${msg}`)
   }
   //
   // Config & Options (could also change state)
   //
   config(name) {
      !(name in this.#config) && this.logError(`Unsupported config »${String(name)}«.`)
      return this.#config[name]
   }
   setConfig(name, value) {
      if (name in this.#config)
         this.#config[name] = value
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
      return this.LID.name
   }
   set name(name) {
      this.LID.name = name
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
   get connectedTo() {
      // !htmlElem.isConnected ==> parent == nextSibling == null
      const htmlElem = this.htmlElem
      return { parent: htmlElem.parentNode, nextSibling: htmlElem.nextSibling }
   }
   set connectedTo(connectTo) {
      const htmlElem = this.htmlElem
      const isConnected = htmlElem.isConnected
      if (!connectTo?.parent)
         htmlElem.remove()
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
         else
            htmlElem.remove()
         this.notifyPropertyListener("connected",isConnect)
      }
   }
   get hidden() {
      return this.view.getComputedStyle().display === "none"
   }
   set hidden(isHidden) {
      if (isHidden != this.hidden) {
         this.view.setClass(isHidden, this.#config.hiddenClassName)
         this.notifyPropertyListener("hidden",isHidden)
      }
   }
   get visibility() {
      return getComputedStyle(this.htmlElem).visibility === "visible"
   }
   set visibility(isVisible) {
      if (isVisible === true)
         this.htmlElem.style.visibility = "visible"
      else if (isVisible === false)
         this.htmlElem.style.visibility = "hidden"
   }
   onceOnViewUpdate(callback, delayCounter) {
      ViewModel.#viewUpdateListener.onceOnViewUpdate(callback, delayCounter)
   }
   //
   // Property Change Listener
   //
   assertListenableProperty(name) {
      if (!this.#config.listenableProperties.has(name))
         this.error(`Unsupported listenable '${name}'.`)
   }
   /**
    * Notifies all listeners of a property that its value was changed.
    * Every caller must ensure that cleaning up code is run in a finally clause
    * to ensure that an exception thrown by a listener does not mess up internal state.
    * @param {string} name - Name of listenable property
    * @param {any} newValue - New value of property after it has changed.
    */
   notifyPropertyListener(name, newValue) {
      const event = { vm:this, value:newValue, name }
      const callbacks = this.#propertyListeners[name]
      callbacks && callbacks.forEach( callback => callback(event))
   }
   addPropertyListener(name, callback) {
      this.assertListenableProperty(name)
      this.#propertyListeners[name] ??= []
      this.#propertyListeners[name].push(callback)
   }
   removePropertyListener(name, callback) {
      this.assertListenableProperty(name)
      if (this.#propertyListeners[name])
         this.#propertyListeners[name] = this.#propertyListeners[name].filter( cb => cb!==callback)
   }
   //
   // Event Handler
   //
   get on() {
      return this.#on
   }
   isEventHandler(name) {
      return (name in this.#on)
   }
   getEventHandler(name) {
      return this.#on[name]
   }
   dispatchEvent(handler, event) {
      handler.callback.call(this,event)
   }
   initEventHandler(name, callback) {
      const eventName = name.toLowerCase()
      const handler = {name, eventName, callback, dispatcher:null, defaultCallback:callback}
      const dispatcher = handler.dispatcher = this.dispatchEvent.bind(this,handler)
      this.#eventHandlers[eventName] = handler
      this.#on[eventName] = this.#on[name] = dispatcher
   }
   initEventHandlers(prototype) {
      const overwritten = []
      for (const key of Reflect.ownKeys(prototype))
         if (String(key).startsWith("on")) {
            const name = key.substring(2)
            if (this.isEventHandler(name.toLowerCase()))
               overwritten.push(key)
            this.initEventHandler(name,prototype[key])
         }
      return overwritten
   }
   /**
    * Overwrites the default event handler of this view-model.
    * @param {string} name The name of the event (is converted to lower case).
    * @param {(e:Event)=>void} callback Implements logic to handle the event sent from a ViewController.
    *                                   Supply null to restore the default behaviour.
    */
   setEventHandler(name, callback) {
      const handler = this.#eventHandlers[name.toLowerCase()]
      !handler && this.error(`Unsupported event handler '${name}'.`)
      handler.callback = callback ?? handler.defaultCallback
   }
}

class ResizeVM extends ViewModel {
   /** Overwrite default configurations. */
   static Default = Object.assign(Object.create(super.Default),{
      decorator: ResizeDecorator
   })

   constructor(htmlElem, configAndOptions={}) {
      super(htmlElem, configAndOptions)
      super.initEventHandlers(ResizeVM.prototype)
      this.callInit(ResizeVM, configAndOptions)
   }

   /**
    * Set moveable to false if moving window is to be disabled (resizing is supported).
    * Set moveable to true if moving window is also enabled for style="position:relative".
    */
   init({moveable, ...configAndOptions}) {
      const pos = this.view.initialPos
      super.init({ left:pos.left, top:pos.top, width:pos.width, height:pos.height, ...configAndOptions})
      const decoOptions = { moveable: moveable ?? this.moveable(pos.position) }
      return decoOptions
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

   setPosAttr(name, value) {
      if (isFinite(value))
         this.htmlElem.style[name] = this.roundPosValue(value)+"px"
   }
   getPosAttr(name) {
      return View.parseNr(this.htmlElem.style[name])
   }
   roundPosValue(value) {
      return Math.round(1000*Number(value)) / 1000
   }
   get pos() {
      return { width:this.getPosAttr("width"), height:this.getPosAttr("height"), top:this.getPosAttr("top"), left:this.getPosAttr("left") }
   }
   set pos(pos) {
      for (const name of ["width","height","top","left"]) {
         isFinite(pos[name]) && this.setPosAttr(name, pos[name])
      }
   }
   set vHeight(value/*pixels*/) {
      this.setPosAttr("height", (value < 0 ? 0 : value))
   }
   get vHeight() {
      return this.getPosAttr("height")
   }
   set vWidth(value/*pixels*/) {
      this.setPosAttr("width", (value < 0 ? 0 : value))
   }
   get vWidth() {
      return this.getPosAttr("width")
   }
   set vTop(value/*pixels*/) {
      this.setPosAttr("top", value)
   }
   get vTop() {
      return this.getPosAttr("top")
   }
   set vLeft(value/*pixels*/) {
      this.setPosAttr("left", value)
   }
   get vLeft() {
      return this.getPosAttr("left")
   }

   moveable(position) {
      return ["fixed","absolute"].includes(position ?? getComputedStyle(this.htmlElem)["position"])
   }

   move(deltax,deltay) { this.vLeft += deltax; this.vTop += deltay; }
   resizeTop(deltay) { this.vTop -= deltay; this.vHeight += deltay; }
   resizeRight(deltax) { this.vWidth += deltax; }
   resizeBottom(deltay) { this.vHeight += deltay; }
   resizeLeft(deltax) { this.vLeft -= deltax; this.vWidth += deltax; }
   resizeTopLeft(deltax,deltay) { this.resizeLeft(deltax); this.resizeTop(deltay); }
   resizeTopRight(deltax,deltay) { this.resizeRight(deltax); this.resizeTop(deltay); }
   resizeBottomRight(deltax,deltay) { this.resizeRight(deltax); this.resizeBottom(deltay); }
   resizeBottomLeft(deltax,deltay) { this.resizeLeft(deltax); this.resizeBottom(deltay); }
   resize(deltax,deltay) { this.resizeRight(deltax); this.resizeBottom(deltay); this.resizeLeft(deltax); this.resizeTop(deltay); }
   resizeStatic(deltax,deltay) { this.resizeRight(2*deltax); this.resizeBottom(2*deltay); }

   // event handlers are prefixed with "on"

   onMove(e) { this.move(e.dx,e.dy) }
   onResizeTop(e) { this.resizeTop(e.dy) }
   onResizeRight(e) { this.resizeRight(e.dx) }
   onResizeBottom(e) { this.resizeBottom(e.dy) }
   onResizeLeft(e) { this.resizeLeft(e.dx) }
   onResizeTopLeft(e) { this.resizeLeft(e.dx); this.resizeTop(e.dy); }
   onResizeTopRight(e) { this.resizeRight(e.dx); this.resizeTop(e.dy); }
   onResizeBottomRight(e) { this.resizeRight(e.dx); this.resizeBottom(e.dy); }
   onResizeBottomLeft(e) { this.resizeLeft(e.dx); this.resizeBottom(e.dy); }
   onResize(e) { this.resize(e.dx,e.dy) }
   onResizeStatic(e) { this.resizeStatic(e.dx,e.dy) }
}

class WindowVM extends ResizeVM {
   /** Overwrite default configurations. */
   static Default = Object.assign(Object.create(super.Default),{
      listenableProperties: new Set(["vWindowState", ...super.Default.listenableProperties]),
      decorator: WindowDecorator,
   })
   #windowState
   /**
    * Slot names are "content","title","close","min","max". They are all optional.
    */
   #slots
   #visiblePos={ width:0, height:0, top:0, left:0 }
   #transitionPos
   #oldTransitionStyle

   /// window states ///
   static HIDDEN = "hidden"; static VISIBLE = "visible"; static MINIMIZED = "minimized"; static MAXIMIZED = "maximized"; static CLOSED = "closed";
   /** Transition style for resize and change position animation. */
   static TRANSITION_STYLE = { "transition-delay":"0s", "transition-duration":"0.2s", "transition-property":"width,height,top,left", "transition-timing-function":"ease-out" }

   constructor(htmlElem, configAndOptions={}) {
      super(htmlElem, configAndOptions)
      super.initEventHandlers(WindowVM.prototype)
      this.#slots = new ViewSlots(this.view)
      this.callInit(WindowVM, configAndOptions)
   }

   init({ decoratedHtmlElem, moveable, ...configAndOptions }) {
      const view = decoratedHtmlElem ? new View(decoratedHtmlElem) : this.view
      const pos = view.initialPos
      this.#windowState = (view.displayed ? WindowVM.VISIBLE : WindowVM.HIDDEN)
      if (decoratedHtmlElem) {
         this.view.setStyles({ position: pos.position })
      }
      super.init({ left:pos.left, top:pos.top, width:pos.width, height:pos.height, ...configAndOptions})
      const decoOptions = { decoratedHtmlElem, moveable: moveable ?? this.moveable(pos.position) }
      return decoOptions
   }

   setOption(name, value) {
      switch (name) {
         case "content": this.vContent = value; break;
         case "title": this.vTitle = value; break;
         default: super.setOption(name, value); break;
      }
   }

   hasSlot(name) {
      return this.#slots.hasSlot(name)
   }
   getSlot(name) {
      return this.#slots.getSlot(name)
   }

   minimizedHeight() {
      const cstyle = this.view.getComputedStyle()
      const titleBarHeight = parseFloat(cstyle["border-top"]) + parseFloat(cstyle["border-bottom"])
               + parseFloat(this.getSlot("minimized-height").getComputedStyle().height)
      return isNaN(titleBarHeight) ? 20 : titleBarHeight
   }

   get vTitle() {
      return this.getSlot("title").html
   }
   set vTitle(html) {
      if (typeof html === "string")
         this.getSlot("title").html = html
   }

   get vContent() {
      return this.getSlot("content").node
   }
   set vContent(content) {
      if (content instanceof Node)
         this.getSlot("content").node = content
      else if (typeof content === "string")
         this.getSlot("content").node = View.newElement(content)
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
    * @param {{width:number,height:number,top:number,left:number,transition:boolean}} pos The new position in css pixel coordinates.
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
         this.#oldTransitionStyle ??= View.setStyles(htmlElem, WindowVM.TRANSITION_STYLE)
         View.setStyles(htmlElem, this.#transitionPos)
         new TransitionController(htmlElem, (e) => {
            if (! e.isInTransition) {
               this.disableTransition()
               onTransitionEnd?.()
            }
         }).ensureInTransitionElseEnd()
      }
      else {
         this.disableTransition()
         this.#transitionPos && View.setStyles(htmlElem, this.#transitionPos)
         onTransitionEnd?.()
      }
      this.#transitionPos = undefined
   }
   disableTransition() {
      if (this.#oldTransitionStyle) {
         View.setStyles(this.htmlElem, this.#oldTransitionStyle)
         this.#oldTransitionStyle = undefined
         TransitionController.endTransition(this.htmlElem)
      }
   }
   isInTransition() {
      return this.#transitionPos || this.#oldTransitionStyle || TransitionController.isInTransition(this.htmlElem)
   }
   onceOnTransitionEnd(onTransitionEnd) {
      if (this.isInTransition())
         new TransitionController(this.htmlElem, (e) => {
            !e.isInTransition && onTransitionEnd?.()
         })
      else
         onTransitionEnd?.()
   }
   async asyncOnTransitionEnd() {
      // TODO: move into onceOnTransitionEnd
      const promiseHolder = new PromiseHolder()
      this.onceOnTransitionEnd( promiseHolder.bindResolve(this.isInTransition()))
      return promiseHolder.promise
   }

   close() {
      this.vWindowState = WindowVM.CLOSED
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
         this.hidden = false
         this.#transitionPos = this.#visiblePos
         this.onceOnViewUpdate( () => {
            if (this.#windowState === WindowVM.VISIBLE) {
               this.#startTransition(true, () => {
                  if (this.#windowState === WindowVM.VISIBLE)
                     htmlElem.scrollIntoView?.({block:"nearest"})
               })
            }
         }, 1) // switching display from "none" (hidden true->false) needs one frame waiting to make transitions work
      }
      else if (state === WindowVM.HIDDEN) {
         this.hidden = true
         this.#transitionPos = undefined
      }
      else if (state === WindowVM.MINIMIZED) {
         this.hidden = false
         const cstyle = getComputedStyle(htmlElem)
         const minHeight = this.minimizedHeight()
         const pos = htmlElem.getBoundingClientRect()
         const width = (2*minHeight)+"px", height = minHeight+"px"
         const top = window.innerHeight-2*minHeight, left = 5
         this.#transitionPos = { width, height, top:(top-pos.top+parseFloat(cstyle.top))+"px", left:(left-pos.left+parseFloat(cstyle.left))+"px" }
         this.hidden = (oldState === WindowVM.HIDDEN)
         this.onceOnViewUpdate( () => {
            if (this.#windowState === WindowVM.MINIMIZED) {
               this.#startTransition(true, () => {
                  if (this.#windowState === WindowVM.MINIMIZED)
                     this.hidden = true
               })
            }
         }, 1)
      }
      else if (state === WindowVM.MAXIMIZED) {
         this.hidden = false
         const pos = htmlElem.getBoundingClientRect()
         const cstyle = getComputedStyle(htmlElem)
         this.#transitionPos = { width:"100vw", height:"100vh", top:(parseFloat(cstyle.top)-pos.top)+"px", left:(parseFloat(cstyle.left)-pos.left)+"px" }
         this.onceOnViewUpdate( () => {
            if (this.#windowState === WindowVM.MAXIMIZED) {
               this.#startTransition(true, () => {
                  if (this.#windowState === WindowVM.MAXIMIZED)
                     htmlElem.scrollIntoView?.({block:"start"})
               })
            }
         }, 1)
      }
      else if (state == WindowVM.CLOSED) {
         this.closedState = { connectedTo: this.connectedTo }
         this.connected = false
      }
      else
         this.error(`Unsupported window state: ${state}`)
      if (this.hasSlot("max"))
         this.getSlot("max").setClass(state == WindowVM.MAXIMIZED, "maximized")
      if (oldState === WindowVM.HIDDEN && this.#transitionPos)
         View.setStyles(htmlElem, this.#transitionPos)
      this.#windowState = state
      this.notifyPropertyListener("vWindowState",state)
   }

   // event handlers are prefixed with "on"

   onClose() {
      this.close()
   }
   onMinimize() {
      this.minimize()
   }
   onMaximize() {
      if (this.vWindowState === WindowVM.MAXIMIZED)
         this.show()
      else
         this.maximize()
   }
}

class WindowStackVM extends ViewModel {
   #template = View.query("#js-minimized-window-template")
   #windowVMs = new Map()

   constructor(htmlElem) {
      super(htmlElem)
      super.initEventHandlers(WindowStackVM.prototype)
   }

   setTemplate(elemSel) {
      this.#template = elemSel
   }

   addMinimized(windowVM) {
      const htmlElem = View.clone(this.#template)
      const slots = new ViewSlots(htmlElem,"title")
      slots.getSlot("title").html = windowVM.vTitle
      const controllers = [
         slots.hasSlot("close") && new ClickController(slots.getSlot("close").htmlElem, (e) => {
            windowVM.vWindowState = WindowVM.CLOSED
         }),
         new ClickController(slots.getSlot("title").htmlElem, (e) => {
            this.resetWindowState(windowVM)
         }),
      ]
      this.htmlElem.append(htmlElem)
      return { htmlElem, controllers }
   }
   removeMinimized(windowVM) {
      const state = this.#windowVMs.get(windowVM)
      const minimized = state?.minimized
      if (minimized) {
         state.minimized = null
         minimized.htmlElem.remove()
         minimized.controllers.forEach( cntrl => cntrl.remove())
      }
   }

   resetWindowState(windowVM) {
      const state = this.#windowVMs.get(windowVM)
      if (state)
         windowVM.vWindowState = state.vWindowState
   }

   unregister(windowVM) {
      this.removeMinimized(windowVM)
      this.#windowVMs.delete(windowVM)
      windowVM.removePropertyListener("vWindowState", this.on.Update)
   }
   register(windowVM) {
      if (this.#windowVMs.has(windowVM))
         return
      this.#windowVMs.set(windowVM, { windowVM, vWindowState:windowVM.vWindowState, minimized: null })
      windowVM.addPropertyListener("vWindowState", this.on.Update)
   }

   onUpdate({vm: windowVM, value: vWindowState}) {
      const state = this.#windowVMs.get(windowVM)
      if (!state)
         this.error("received update event for unregistered Window-View-Model.")
      if (vWindowState === WindowVM.MINIMIZED || vWindowState === WindowVM.HIDDEN) {
         if (!state.minimized)
            state.minimized = winstackVM.addMinimized(windowVM)
      }
      else if (vWindowState === WindowVM.CLOSED)
         this.unregister(windowVM)
      else {
         state.vWindowState = vWindowState
         this.removeMinimized(windowVM)
      }
   }

}

const VMX = new ViewModelContext()
