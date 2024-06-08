/**
 * Class-Types for handling HTML-Views.
 *
 * === Wrapping of event listeners
 * * ViewListener
 * * ViewListeners
 * * ViewController
 * === Wrapping HTML
 * (not fully, htmlElem is accessd within VModel,VContrl.,VDecor.)
 * * HTMLView
 * * View
 * * ViewConfig
 * * ViewSlots
 * === Wrapping state and displayed data of a view
 * * ViewModel
 * === Binding of View, ViewModel, and Controllers
 * * ViewDecorator
 * * ViewUpdater
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
   /** All listeners are managed by this single instance. */
   static singleton = new ViewListeners()
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
   listenerExceptions = 0
   DEBUG = true
   constructor() {
      this.onCaptureBind = this.onCapture.bind(this)
      this.onBubbleBind = this.onBubble.bind(this)
   }
   error(msg) {
      throw Error(`ViewListeners error: ${msg}`)
   }
   logException(exc) {
      ++ this.listenerExceptions
      console.error("listener exception", exc)
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
               try { listener.eventHandler(e) }
               catch(exc) { this.logException(exc) }
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
      return this.singleton.getListenedElements(rootElem,eventType,eventHandler)
   }
}

/**
 * Supports event listeners and blocking of other controllers.
 */
class ViewController {
   static preventDefault = (e) => e.preventDefault()
   #isActive = false
   #listeners = []
   #activeListeners = []
   #blockedControllers = []
   #blockingCount = 0
   #othersBlocked = false
   #htmlElem
   #callback
   constructor(htmlElem, callback, ...blockedControllers) {
      this.#htmlElem = htmlElem
      this.#callback = callback
      this.#blockedControllers = blockedControllers.flat(Infinity).filter(cntrl => cntrl instanceof ViewController)
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
   callAction(e) {
      if (!this.isBlocked && this.#callback)
         this.#callback(e)
   }
   callActionOnce(e) {
      const callback = this.#callback
      if (!this.isBlocked && callback) {
         this.#callback = null
         callback(e)
      }
   }
   removeListeners(listenerArray) {
      listenerArray.forEach(listener => listener && ViewListeners.singleton.removeListener(listener))
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
   deactivate() {
      if (this.#isActive) {
         this.#isActive = false
         this.removeListeners(this.#activeListeners)
         this.blockOthers(false)
      }
   }
   activate(onActivateCallback) {
      if (!this.#isActive) {
         this.#isActive = true
         this.blockOthers(true)
         onActivateCallback?.()
      }
   }
   #addListener(htmlElem, eventType, eventHandler, options) {
      return ViewListeners.singleton.addListener(htmlElem, eventType, eventHandler,
         { ...options, owner: this, blockedOwners: this.#blockedControllers }
      )
   }
   addListener(htmlElem, eventType, eventHandler, options) {
      this.#listeners.push(this.#addListener(htmlElem,eventType,eventHandler,options))
      return this.#listeners.at(-1)
   }
   addActiveListener(htmlElem, eventType, eventHandler, options) {
      this.#activeListeners.push(this.#addListener(htmlElem,eventType,eventHandler,options))
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
   remove() {
      this.deactivate()
      this.removeListeners(this.#listeners)
      this.#htmlElem = null
      this.#callback = null
      this.#blockedControllers = []
      return this
   }
   onBlock() {
      // do nothing
   }
   onUnblock() {
      // do nothing
   }
   ////////////
   // Helper //
   ////////////
   getTouchById(touches, id) {
      for (const t of (touches||[]))
         if (t.identifier === id)
            return t
   }
}

class ClickController extends ViewController {

   constructor(htmlElem, clickCallback, ...blockedControllers) {
      super(htmlElem, clickCallback, ...blockedControllers)
      this.addListener(htmlElem,"click",this.onClick.bind(this))
      this.addListener(htmlElem,"touchstart",this.onStart.bind(this))
      this.addListener(htmlElem,"mousedown",this.onStart.bind(this))
   }

   onStart(e) {
      this.activate( () => e.touches
         ? [ this.addActiveListener(e.target,"touchend",this.onEnd.bind(this)),
             this.addActiveListener(e.target,"touchcancel",this.onEnd.bind(this)) ]
         : [ this.addActiveListener(document,"mouseup",this.onEnd.bind(this)) ]
      )
   }

   onClick(e) {
      this.callAction({type:"click", target:e.currentTarget})
   }

   onEnd(e) {
      if (e.targetTouches?.length)
         return
      this.deactivate()
   }
}

class MoveController extends ViewController {
   #pos
   #totalxy
   #truncxy
   DEBUG = ""

   constructor(htmlElem, moveCallback, ...blockedControllers) {
      super(htmlElem, moveCallback, ...blockedControllers)
      this.addListener(htmlElem,"mousedown",this.onStart.bind(this))
      this.addListener(htmlElem,"touchstart",this.onStart.bind(this))
   }

   extractXY(e) {
      return { x:e.pageX, y:e.pageY, id:e.identifier }
   }

   onStart(e) {
      /* TODO: remove
      if (this.DEBUG && e.touches)
         logError(`${this.DEBUG}: start ${e.targetTouches.length} active:${this.isActive} block:${this.isBlocked}`)
      */
      if (this.isActive || (e.touches && e.targetTouches.length !== 1)) {
         return
      }
      const touch = (e.targetTouches && e.targetTouches[0])
      this.#pos = this.extractXY(touch || e)
      this.#totalxy = { x:0, y:0 }
      this.#truncxy = { x:0, y:0 }
      e.preventDefault()
      this.activate( () => touch
            ?  [  this.addActiveListener(e.target,"touchmove",this.onMove.bind(this)),
                  this.addActiveListener(e.target,"touchend",this.onEnd.bind(this)),
                  this.addActiveListener(e.target,"touchcancel",this.onEnd.bind(this)) ]
            :  [  this.addActiveListener(document,"mousemove",this.onMove.bind(this)),
                  this.addActiveListener(document,"mouseup",this.onEnd.bind(this)) ])
   }

   onMove(e) {
      if (e.touches && e.targetTouches.length !== 1)
         return
      e.preventDefault()
      const touch = e.targetTouches && e.targetTouches[0]
      const pos = this.extractXY(touch || e)
      const totalxy = { x:pos.x-this.#pos.x+this.#totalxy.x, y:pos.y-this.#pos.y+this.#totalxy.y }
      const truncxy = { x:Math.trunc(totalxy.x), y:Math.trunc(totalxy.y) }
      const movexy = { type:"move", dx:truncxy.x-this.#truncxy.x, dy:truncxy.y-this.#truncxy.y }
      this.#pos = pos
      this.#totalxy = totalxy
      if (movexy.dx || movexy.dy) {
         this.#truncxy = truncxy
         this.callAction(movexy)
      }
   }

   onEnd(e) {
      if (e.targetTouches?.length) {
         this.#pos = this.extractXY(e.targetTouches[0])
         return
      }
      this.deactivate()
   }
}

class TouchResizeController extends ViewController {
   #pos
   #totalxy
   #truncxy

   constructor(htmlElem, resizeCallback, ...blockedControllers) {
      super(htmlElem, resizeCallback, ...blockedControllers)
      this.addListener(htmlElem,"touchstart",this.onStart.bind(this))
   }

   extractXY(e1,e2) {
      return { x:Math.abs(e1.pageX - e2.pageX), y:Math.abs(e1.pageY - e2.pageY) }
   }

   onStart(e) {
      if (this.isActive || e.targetTouches?.length !== 2) {
         return
      }
      this.#pos = this.extractXY(e.targetTouches[0],e.targetTouches[1])
      this.#totalxy = { x:0, y:0 }
      this.#truncxy = { x:0, y:0 }
      e.preventDefault()
      this.activate( () =>
         [  this.addActiveListener(e.target,"touchmove",this.onMove.bind(this)),
            this.addActiveListener(e.target,"touchend",this.onEnd.bind(this)),
            this.addActiveListener(e.target,"touchcancel",this.onEnd.bind(this)) ])
   }

   onMove(e) {
      if (e.targetTouches.length !== 2)
         return
      e.preventDefault()
      const pos = this.extractXY(e.targetTouches[0],e.targetTouches[1])
      const totalxy = { x:pos.x-this.#pos.x+this.#totalxy.x, y:pos.y-this.#pos.y+this.#totalxy.y }
      const truncxy = { x:Math.trunc(totalxy.x), y:Math.trunc(totalxy.y) }
      const resizexy = { type:"resize", dx:truncxy.x-this.#truncxy.x, dy:truncxy.y-this.#truncxy.y }
      this.#pos = pos
      this.#totalxy = totalxy
      if (resizexy.dx || resizexy.dy) {
         this.#truncxy = truncxy
         this.callAction(resizexy)
      }
   }

   onEnd(e) {
      if (e.targetTouches?.length >= 2) {
         this.#pos = this.extractXY(e.targetTouches[0],e.targetTouches[1])
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

   onStart(e) {
      if (this.htmlElem) {
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
   }

   onEnd(e) {
      if (this.htmlElem) {
         if (!TransitionController.isInTransition(this.htmlElem)) {
            try { this.callActionOnce({type:"transitionend", target:this.htmlElem, isInTransition:false }) }
            finally { this.remove() }
         }
         else if (e) {
            this.removeActiveListenerForElement(e.target)
         }
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

   onStart(e) {
      this.#startTarget = e.target
      if (! this.isActive) {
         const event = {type:"dropstart", target:e.currentTarget, items:e.dataTransfer.items }
         this.callAction(event)
         this.activate( () => [
            this.addActiveListener(e.currentTarget,"drop",this.onDrop.bind(this)),
            this.addActiveListener(e.currentTarget,"dragleave",this.onEnd.bind(this))
         ])
      }
   }

   onDrop(e) {
      e.preventDefault()
      this.callAction({type:"drop", target:e.currentTarget, items:e.dataTransfer.items})
      this.onEnd(e)
   }

   onEnd(e) {
      if (this.#startTarget === e.target
         // ESCAPE key aborts operation and text child could be returned instead of startTarget
         || e.target.attributes === undefined) {
         this.callAction({type:"dropend", target:e.currentTarget, items:e.dataTransfer.items})
         this.deactivate()
      }
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
         oldStyles[key] = styles.getPropertyValue(key)
         styles.setProperty(key, newStyles[key])
      }
      return oldStyles
   }
   static restoreStyles(htmlElem, oldStyles) {
      const styles = htmlElem.style
      for (const key in oldStyles)
         styles.setProperty(key, oldStyles[key])
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
      const isStatic = isNotConnected || position === "static"
      const top = isStatic ? 0 : this.parseNr(cstyle.top)
      const left = isStatic ? 0 : this.parseNr(cstyle.left)
      isNotConnected && htmlElem.remove()
      return { width, height, top, left, position }
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
   getInitialPos() {
      return HTMLView.getInitialPos(this.#htmlElem)
   }
}

class ViewConfig {
   #values
   constructor(htmlElem) {
      this.#values = ViewConfig.parse(htmlElem)
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
         this.error(`Missing attribute data-slot='${name}' in HTML.`)
      }
   }
   assertSlotNames(slotNamesArray, htmlElem) {
      for (const name of slotNamesArray)
         this.assertSlotName(name, htmlElem)
   }
   static getSlots(htmlElem) {
      const slots = new Map()
      const selector = "[data-slot]"
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

class ViewUpdateListener {
   #callbacks = []
   #frameID = null
   constructor(callback, delayCounter) {
      if (callback)
         this.onceOnViewUpdate(callback, delayCounter)
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
 * Model which is bound to a single view and manages its state.
 */
class ViewModel {
   /** Default configurations (for all instances) which could be overwritten in subclass. */
   static Default = {
      /** Defines 'Set' of valid property names which could be observed for change. */
      listenableProperties: new Set(["hidden","connected"]),
      /** Class name which sets display to none if set on htmlElement. */
      hiddenClassName: "js-d-none",
   }
   static #viewUpdateListener = new ViewUpdateListener()
   #config
   #htmlElem
   #name
   #on = {}
   #view
   #propertyListeners = {}
   constructor(htmlElem) {
      this.#config = Object.create(this.constructor.Default)
      this.#htmlElem = htmlElem
      this.#name = this.constructor.name
      this.#view = new View(htmlElem)
      const config = new ViewConfig(htmlElem)
      if (config.hasValue("hiddenClassName"))
         this.#config.hiddenClassName = config.getValue("hiddenClassName")
   }
   onceOnViewUpdate(callback, delayCounter) {
      ViewModel.#viewUpdateListener.onceOnViewUpdate(callback, delayCounter)
   }
   /** Overwrite setOption in subclass. */
   setOption(name, value) {
      switch (name, value) {
         case "connected": this.connected = value; break;
         case "hidden": this.hidden = value; break;
         case "name": this.#name = String(value); break;
         default: this.logError(`Unsupported option »${String(name)}«.`); break; // in subclass: super.setOption(name, value)
      }
   }

   error(msg) {
      throw Error(`${this.#name} error: ${msg}`)
   }
   logError(msg) {
      console.error(`${this.#name} error: ${msg}`)
   }
   setOptions(options={}) {
      for (const name in options)
         this.setOption(name, options[name])
      return this
   }
   get name() {
      return this.#name
   }
   get htmlElem() {
      return this.#htmlElem
   }
   get view() {
      return this.#view
   }
   isEventHandler(name) {
      return (name in this.#on)
   }
   get on() {
      return this.#on
   }
   getEventHandler(name) {
      return this.on[name]
   }
   bindEventHandler(name, eventHandler, thisValue=null) {
      this.#on[name] = eventHandler.bind(thisValue||this)
   }
   bindEventHandlers(prototype) {
      const overwritten = []
      for (const key of Reflect.ownKeys(prototype))
         if (String(key).startsWith("on")) {
            const name = key.substring(2)
            if (this.isEventHandler(name))
               overwritten.push(key)
            this.bindEventHandler(name,prototype[key])
         }
      return overwritten
   }
   detach() {
      const htmlElem = this.htmlElem
      this.#htmlElem = null
      this.#on = {}
      this.#view = null
      this.#propertyListeners = {}
      return htmlElem
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
   assertListenableProperty(name) {
      if (!this.#config.listenableProperties.has(name))
         this.error(`Listenable property '${name}' does not exist.`)
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
}

class ResizeVM extends ViewModel {

   constructor(htmlElem) {
      super(htmlElem)
      super.bindEventHandlers(ResizeVM.prototype)
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
         return this.htmlElem.style[name] = value+"px"
   }
   getPosAttr(name) {
      return View.parseNr(this.view.getComputedStyle()[name])
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
   onResizeTop(e) { this.resizeTop(-e.dy) }
   onResizeRight(e) { this.resizeRight(e.dx) }
   onResizeBottom(e) { this.resizeBottom(e.dy) }
   onResizeLeft(e) { this.resizeLeft(-e.dx) }
   onResizeTopLeft(e) { this.resizeLeft(-e.dx); this.resizeTop(-e.dy); }
   onResizeTopRight(e) { this.resizeRight(e.dx); this.resizeTop(-e.dy); }
   onResizeBottomRight(e) { this.resizeRight(e.dx); this.resizeBottom(e.dy); }
   onResizeBottomLeft(e) { this.resizeLeft(-e.dx); this.resizeBottom(e.dy); }
   onResize(e) { this.resize(e.dx,e.dy) }
   onResizeStatic(e) { this.resizeStatic(e.dx,e.dy) }
}

class WindowVM extends ResizeVM {
   static Default = Object.assign(Object.create(super.Default),{
      listenableProperties: new Set(["vWindowState", ...super.Default.listenableProperties])
   })
   #windowState
   #slots
   #visiblePos
   #transitionPos
   #oldTransitionStyle

   /// window states ///
   static HIDDEN = "hidden"; static VISIBLE = "visible"; static MINIMIZED = "minimized"; static MAXIMIZED = "maximized"; static CLOSED = "closed";
   /** Transition style for resize and change position animation. */
   static TRANSITION_STYLE = { "transition-delay":"0s", "transition-duration":"0.2s", "transition-property":"width,height,top,left", "transition-timing-function":"ease-out" }

   constructor(htmlElem) {
      super(htmlElem)
      super.bindEventHandlers(WindowVM.prototype)
      // slot names of optional window controls: "close", "min", "max"
      this.#slots = new ViewSlots(this.view, "content", "title")
      const pos = this.view.getInitialPos()
      this.#visiblePos = { width:pos.width+"px", height:pos.height+"px", top:pos.top+"px", left:pos.left+"px" }
      this.view.setStyles(this.#visiblePos)
      this.setOption("setInitialWindowState",true)
   }

   setOption(name, value) {
      switch (name) {
         case "content": this.vContent = value; break;
         case "setInitialWindowState": if (value) this.#windowState = (this.isDisplayed() ? WindowVM.VISIBLE : WindowVM.HIDDEN); break;
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

   isDisplayed() {
      return this.connected && !this.hidden
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
   roundPosValue(value) {
      return Math.round(1000*Number(value)) / 1000
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
      const promiseHolder = new PromiseHolder()
      this.onceOnTransitionEnd( promiseHolder.bindResolve(this.isInTransition()))
      return promiseHolder.promise
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
}

class WindowStackVM extends ViewModel {
   #template = View.query("#js-minimized-window-template")
   #windowVMs = new Map()

   constructor(htmlElem) {
      super(htmlElem)
      super.bindEventHandlers(WindowStackVM.prototype)
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
   /**
    * Constructor which decorates an HTML element (creates ViewController and binds their action callbacks).
    * @param {HTMLElement} htmlElem - The HTML element which serves as root view.
    * @param {object} options - Additional options. Property viewModel could be set to provide a ViewModel else a default one is created.
    */
   constructor(htmlElem, options={}) {
      this.viewModel = null
      this.controllers = {}
      this.decorate(htmlElem, options)
   }
   error(msg) {
      throw Error(`ViewDecorator error: ${msg}`)
   }
   decorate(htmlElem, {viewModel, ...options}={}) {
      if (! this.viewModel) {
         this.viewModel = viewModel ?? this.defaultModel(htmlElem,options)
         if (!(this.viewModel instanceof ViewModel))
            this.error("Argument viewModel is not of type ViewModel.")
         ViewDecorators.singleton.add(this.viewModel.htmlElem, this)
         this.decorateOnce(htmlElem, options)
      }
   }
   undecorate() {
      if (this.viewModel) {
         this.undecorateOnce()
         ViewDecorators.singleton.remove(this.viewModel.htmlElem, this)
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
   defaultModel(htmlElem, options) {
      this.error("defaultModel(htmlElem,options) not implemented in subclass.")
   }
   decorateOnce(htmlElem, options) {
      this.error("decorateOnce(htmlElem,options) not implemented in subclass.")
   }
   undecorateOnce(htmlElem, options) {
      this.error("undecorateOnce(htmlElem,options) not implemented in subclass.")
   }
}

class ResizeDecorator extends ViewDecorator {

   defaultModel(htmlElem, options) {
      return new ResizeVM(htmlElem)
   }

   decorateOnce(htmlElem, { moveable }) {
      const viewModel = this.viewModel
      const width = { outer:10, border:15, edge:20 }
      const size = { border: width.border+"px", edge:width.edge+"px", outer:"-"+width.outer+"px" }
      moveable ??= viewModel.moveable()
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
      this.resizeState = { borders, oldStylesHtmlElem:htmlElem, oldStyles:null, isResizeable:true, controllers:null }
      const moveWin = moveable && new MoveController(htmlElem,viewModel.on.Move)
      moveWin && (moveWin.DEBUG = "movewin")
      const resizeWin = new TouchResizeController(htmlElem,viewModel.on[moveable ? "Resize" : "ResizeStatic"])
      const moveResizeGroup = [moveWin,resizeWin]
      const topResize = moveable && new MoveController(borders[0],viewModel.on.ResizeTop,moveResizeGroup)
      const rightResize = new MoveController(borders[1],viewModel.on.ResizeRight,moveResizeGroup)
      const bottomResize = new MoveController(borders[2],viewModel.on.ResizeBottom,moveResizeGroup)
      const leftResize = moveable && new MoveController(borders[3],viewModel.on.ResizeLeft,moveResizeGroup)
      const topLeftResize = moveable && new MoveController(borders[4],viewModel.on.ResizeTopLeft,moveResizeGroup,topResize,leftResize)
      const topRightResize = moveable && new MoveController(borders[5],viewModel.on.ResizeTopRight,moveResizeGroup,topLeftResize,topResize,rightResize)
      const bottomRightResize = new MoveController(borders[6],viewModel.on.ResizeBottomRight,moveResizeGroup,topRightResize,bottomResize,rightResize)
      const bottomLeftResize = moveable && new MoveController(borders[7],viewModel.on.ResizeBottomLeft,moveResizeGroup,topLeftResize,bottomRightResize,bottomResize,leftResize)
      this.resizeState.controllers = { moveWin, resizeWin, topResize, rightResize, bottomResize, leftResize, topLeftResize, topRightResize, bottomRightResize, bottomLeftResize }
      Object.assign(this.controllers, this.resizeState.controllers)
      this.switchStyles()
   }

   /**
    * Changes cursor style of decorated element to "move" or restores old style (every call switches).
    */
   switchStyles() {
      if (this.resizeState?.oldStyles)
         (View.setStyles(this.resizeState.oldStylesHtmlElem, this.resizeState.oldStyles), this.resizeState.oldStyles=null)
      else if (this.controllers.moveWin)
         this.resizeState.oldStyles = View.setStyles(this.resizeState.oldStylesHtmlElem, {cursor:"move"})
   }
   switchResizeable(isResizeable) {
      if (this.resizeState.isResizeable != Boolean(isResizeable) && this.resizeState.controllers) {
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
   undecorateOnce() {
      this.switchStyles()
      for (const border of this.resizeState.borders)
         (border) && border.remove()
      this.resizeState = null
   }
}

class WindowDecorator extends ResizeDecorator {

   defaultModel(htmlElem, {template}) {
      const parentHtmlElem = View.query(template) ?? View.clone("#js-window-template")
      return new WindowVM(parentHtmlElem)
   }

   decorateOnce(htmlElem, {title, ...options}={}) {
      const viewModel = this.viewModel
      const pos = View.getInitialPos(htmlElem)
      viewModel.vTitle = title
      this.windowState = { replacedChild:false, oldChild:htmlElem, oldChildStyles:null, boundOnMaximzed:this.onMaximized.bind(this), boundOnClose:this.onClose.bind(this) }
      // make htmlElem child of viewModel.htmlElem if child instead of viewModel is decorated
      if (htmlElem !== viewModel.htmlElem) {
         this.windowState.replacedChild = Boolean(htmlElem.parentElement)
         htmlElem.parentElement?.replaceChild(viewModel.htmlElem, htmlElem)
         viewModel.vContent = htmlElem
         viewModel.setOptions({ left:pos.left, top:pos.top, width:pos.width||100, height:pos.height||100, setInitialWindowState:true })
         this.windowState.oldChildStyles = View.setStyles(htmlElem, { width:"100%", height:"100%", position:"static" })
         View.setStyles(viewModel.htmlElem, { position: pos.position })
      }
      super.decorateOnce(viewModel.htmlElem, { ...options, moveable: options.moveable ?? viewModel.moveable(pos.position) })
      const moveWin = this.controllers.moveWin
      this.controllers.close = viewModel.hasSlot("close") && new ClickController(viewModel.getSlot("close").htmlElem,
         this.windowState.boundOnClose,
      moveWin),
      this.controllers.min = viewModel.hasSlot("min") && new ClickController(viewModel.getSlot("min").htmlElem, () => {
         viewModel.vWindowState = WindowVM.MINIMIZED
      },moveWin),
      this.controllers.max = viewModel.hasSlot("max") && new ClickController(viewModel.getSlot("max").htmlElem, () => {
         viewModel.vWindowState = (viewModel.vWindowState === WindowVM.MAXIMIZED
                           ? WindowVM.VISIBLE
                           : WindowVM.MAXIMIZED)
      },moveWin)
      viewModel.addPropertyListener("vWindowState",this.windowState.boundOnMaximzed)
   }

   onMaximized({value:vWindowState}) {
      this.switchResizeable(vWindowState != WindowVM.MAXIMIZED)
   }
   onClose() {
      this.viewModel.vWindowState = WindowVM.CLOSED
   }
   setOnClose(onClose) {
      if (this.controllers.close)
         this.controllers.close.callback = onClose
   }
   setOnCloseDefault() {
      if (this.windowState)
         this.controllers.close.callback = this.windowState.boundOnClose
   }

   undecorateOnce() {
      super.undecorateOnce()
      const viewModel = this.viewModel
      const htmlElem = viewModel.htmlElem
      if (this.windowState.replacedChild) {
         htmlElem.parentElement.replaceChild(this.windowState.oldChild, htmlElem)
      }
      htmlElem.remove()
      if (this.windowState.oldChildStyles) {
         View.setStyles(this.windowState.oldChild, this.windowState.oldChildStyles)
      }
      viewModel.removePropertyListener("vWindowState",this.windowState.boundOnMaximzed)
      this.windowState = null
   }

}

/**
 * Maps HTML elements to applied decorators.
 */
 class ViewDecorators {
   static singleton = new ViewDecorators()
   #htmlElements = new WeakMap()
   constructor() {
   }
   error(msg) {
      throw Error(`ViewDecorators error: ${msg}`)
   }
   add(htmlElem, viewDecorator) {
      this.#htmlElements.set(htmlElem,
         (this.#htmlElements.get(htmlElem) || []).concat(viewDecorator)
      )
   }
   remove(htmlElem, viewDecorator) {
      const decorators = this.#htmlElements.get(htmlElem)
      if (decorators) {
         this.#htmlElements.set(htmlElem,
            decorators.filter(entry => entry !== viewDecorator)
         )
      }
   }
   /**
    * Returns information about decorators applied to an HTML element.
    * @param {HTMLElement} htmlElem Element whose decorators are to be returned.
    * @param {ViewDecorator=} decorator Optional decorator type (constructor). If undefined an array of all decorators applied to the element is returned.
    * @return {ViewDecorator[]} An array describing all applied decorators. The returned array may be empty.
    */
   get(htmlElem, decorator) {
      const decorators = this.#htmlElements.get(htmlElem) || []
      return decorator
         ? decorators.filter( entry => entry instanceof decorator )
         : decorators
   }
   /**
    * Checks if an element is decorated at all or decorated by a specific decorator.
    * @param {HTMLElement} htmlElem Element which checked for being decorated.
    * @param {Decorator=} decorator Optional decorator type (constructor) to check for. If undefined any dorator is considered.
    * @return {boolean} <tt>true</tt> if the element is decorated with provided decorator or decorated at all in case decorator is undefined.
    */
   isDecorated(htmlElem, decorator) {
      return this.get(htmlElem, decorator).length > 0
   }
   getDecoratedElements(rootElem, decorator) {
      const result = []
      if (this.isDecorated(rootElem, decorator))
         result.push(rootElem)
      for (const htmlElem of rootElem.querySelectorAll("*")) {
         if (this.isDecorated(htmlElem, decorator))
            result.push(htmlElem)
      }
      return result
   }
   static get(htmlElem, decorator) {
      return this.singleton.get(htmlElem, decorator)
   }
   static getDecoratedElements(rootElem, decorator) {
      return this.singleton.getDecoratedElements(rootElem,decorator)
   }
}
