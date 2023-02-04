/////////////////////////////////////////
// Base Library for Defining View Tags //
/////////////////////////////////////////
const jstl=(() => {

   ///////////////
   // Constants //
   ///////////////
   const INIT_ATTR_PROPERTIES={ name:true, isEnum:false, isEmpty:false, isModelAccess:false, isRequired:false, pattern:false, reservedPattern:false }
   const TAGS_CONTAINER=document.createDocumentFragment()
   /** Maps view names(all uppercase) to view objects. */
   const KNOWN_VIEWS={}
   const ONLOAD_QUEUE=new Promise( (resolve) => {
      if ("complete" === document.readyState)
         resolve(new Event("load")) // event already occurred
      else
         window.addEventListener("load", resolve, {once:true}) // wait
   })
   /** Contains loaded extensions indexed by name. */
   const EXTENSIONS={}

   /////////////////////////
   // Regular Expressions //
   /////////////////////////
   const PREFIX_PATTERN=/^([A-Za-z][-_.:a-zA-Z0-9]*[-:]|)$/
   const NONEMPTY_PREFIX_PATTERN=/^[A-Za-z][-_.:a-zA-Z0-9]*[-:]$/
   const TAGNAME_PATTERN=/^[a-zA-Z][_.a-zA-Z0-9]*$/
   const ONLY_WHITE_SPACE=/^[ \n\t]*$/
   const VARNAME_PATTERN=/^[_$A-Za-z][_$A-Za-z0-9]*/
   const ACCESSPATH_PATTERN=/^[_$A-Za-z0-9]+([.][_$A-Za-z0-9]+)*$/
   /** Matches string representation of regex pattern "/^(val1|...|valN)$/"".
    * matchresult[1] === (val1|...|valN). Used to verify that pattern matches an enum value. */
   const VALIDENUM_PATTERN=/^[/][^]([(][-_$a-zA-Z0-9]*([|][-_$a-zA-Z0-9]*)*[)])[$][/]$/

   /////////////////////////
   // error & log support //
   /////////////////////////

   /** Error type which is thrown after it has been logged. */
   class ParseError extends Error
   {
      constructor(message,options) { super(message,options); console.log(this,...(options?.logArgs??[])); }
   }

   /**
    * Executes callback function within a try catch block ignoring exceptions from type eType.
    * @param {new () => Object} eType
    *   Constructor function of exception type.
    * @param {() => void} callback
    *   Function which is executed within a try catch block.
    */
   const ignoreException=(eType,callback) =>
   {
      try { callback() } catch(e) { if (!(e instanceof eType)) throw e; /* else ignore */ }
   }

   const ignoreParseError=(callback) =>
   {
      ignoreException(ParseError,callback)
   }

   const strLocation=(node) => `Location: ${nodeHierarchy(node)}`
   const strSelector=(node) => `Selector: ${nodeSelector(node)}`
   const logErrorForNode=(node,attrName,message,prevNode) =>
   {
      const previous=(prevNode ? `\nPrevious ${strLocation(prevNode)}\nPrevious ${strSelector(prevNode)}` : "")
      return new ParseError(`<${nodeName(node)}${attrName?' '+attrName:''}>: ${message}\n${strLocation(node)}\n${strSelector(node)}${previous}`,{logArgs: ["\nLocation:",...parentChain(node)]})
   }

   //////////////////////////
   // string & DOM support //
   //////////////////////////

   function endWithDot(msg)
   {
      return msg.endsWith('.') ? msg : msg+'.'
   }

   function escapeString(value)
   {
      return value.replaceAll("\\","\\\\").replaceAll("\"","\\\"").replaceAll("\t","\\t").replaceAll("\n","\\n")
   }

   function typeOf(value)
   {
      const type=typeof value;
      if (type !== "object") return type
      return (value===null
               ? String(value)
               : Array.isArray(value)
               ? "[object Array]"
               : `[object ${value.constructor?.name??"Object"}]`)
   }

   const isCommentNode=(node) => node.nodeType === 8 // node.COMMENT_NODE==8
   const isTextNode=(node) => node.nodeType === 3 // node.TEXT_NODE==3
   const isEmptyTextNode=(node) => (isTextNode(node) && ONLY_WHITE_SPACE.test(node.nodeValue))
   const isEmptyNode=(node) => (isEmptyTextNode(node) || isCommentNode(node))
   const nodeName=(node) => node ? node.nodeName.toLowerCase() : null

   function buildDataAccessFunction(expr,visitingContext,attrName)
   {
      try {
         if (expr.length == 0)
            throw Error(`Expression '' is empty.`)
         const isPath=(expr.match(ACCESSPATH_PATTERN) !== null)
         const functionBody=`"use strict"; return (${isPath?"d['":""}${expr}${isPath?"']":""})`
         try {
            return new Function("d","vd",functionBody)
         } catch(e) {
            throw Error(`${isPath?'':'Use chars only from [_$.A-Za-z0-9] to make expression an access path. '}Compilation of »${functionBody}« fails with: ${endWithDot(e.message)}`, {cause:e})
         }
      } catch(e) {
         if (attrName)
            visitingContext.throwError(`Attribute '${attrName}' contains invalid expression. ${e.message}`,attrName)
         else
            visitingContext.throwError(`Text node contains invalid expression. ${e.message}`)
      }
   }

   function tryDataAccessFunction(value,visitingContext,attrName)
   {
      if (value.indexOf("&:")<0) return null
      const expr=value.trim()
      try {
         if (!expr.startsWith("&:"))
            throw Error(`'${expr.substring(0,10)}${expr.length>10?"...":""}' does not start with '&:'.`)
         if (expr.length == 2)
            throw Error(`Expression '${expr}' is empty.`)
      } catch(e) {
         if (attrName)
            visitingContext.throwError(`Attribute '${attrName}' contains invalid expression. ${e.message}`,attrName)
         else
            visitingContext.throwError(`Text node contains invalid expression. ${e.message}`)
      }
      return buildDataAccessFunction(expr.substring(2),visitingContext,attrName)
   }

   function nodeHierarchy(node)
   {
      if (! node) return ""
      const parent=nodeHierarchy(node.parentElement)
      const attributes=([...(node.attributes??[])])
                        .map(attr => ` ${attr.name}="${escapeString(attr.value)}"`)
                        .join()
      return parent +(parent ? " \u25b6 " : "") +
                     (isTextNode(node)
                        ? `${nodeName(node)} "${escapeString(node.nodeValue)}"`
                        : `<${nodeName(node)}${attributes}>`)
   }

   function parentChain(node)
   {
      return (node && node.parentNode) ? [...parentChain(node.parentNode),node] : []
   }

   function skipNodes(node,isSkipableNode,getSibling=(node)=>node.nextSibling)
   {
      while (node && isSkipableNode(node))
         node=getSibling(node)
      return node
   }

   ///////////////////////
   // load & parse tags //
   ///////////////////////

   function nodeSelector(node,attrName)
   {
      const location=[]
      for (let child=node; child; child=child.parentElement) {
         const previous=isTextNode(child) ? (node) => node.previousSibling : (node) => node.previousElementSibling
         var nth=1
         for (let prev=previous(child); prev; prev=previous(prev))
            nth++
         location.unshift(nodeName(child) + `:nth-child(${nth})`)
      }
      return location.join(" > ") + (attrName ? `[${attrName}]` : "")
   }

   function queryNode(selector)
   {
      const selectText=selector.match(/[ ]*>[ ]*#text:nth-child\(([^)]+)\)$/)
      const node=TAGS_CONTAINER.querySelector(selectText ? selector.substring(0,selector.length-selectText[0].length) : selector)
      return selectText ? node.childNodes[selectText[1]-1] : node
   }

   function queryNodes(selector)
   {
      const selectText=selector.match(/[ ]*>[ ]*#text:nth-child\(([^)]+)\)$/)
      const nodes=TAGS_CONTAINER.querySelectorAll(selectText ? selector.substring(0,selector.length-selectText[0].length) : selector)
      return [...nodes].map(node => selectText ? node.childNodes[selectText[1]-1] : node)
   }

   function knownViewNames()
   {
      return Object.keys(KNOWN_VIEWS)
   }

   function removeView(name)
   {
      delete KNOWN_VIEWS[name]
   }

   async function loadTags({url,exportPrefix})
   {
      return fetch(url).then( response => {
         if (! response.ok)
            throw Error(`${response.status} ${response.statusText}.`)
         return response.text()
      }).catch( e => {
         throw Error(`Loading tags from '${url}' failed with: ${e.message??e}`,{cause:e})
      }).then( text => {
         return parseTags({text,url,exportPrefix})
      })
   }

   function parseTags({text,url,exportPrefix})
   {
      const container=document.createElement("div")
      container.innerHTML=text.trim()
      if (! container.childNodes.length)
         throw new ParseError(`Tags loaded from '${url}' are empty.`)
      const tags=container.childNodes[0]
      if (container.childNodes.length !== 1)
         throw new ParseError(`Tags loaded from '${url}' has not a single root node but two: <${nodeName(tags)}> followed by <${nodeName(container.childNodes[1])}>.`)
      tags.setAttribute("url",url)
      tags.setAttribute("export-prefix",exportPrefix)
      TAGS_CONTAINER.appendChild(tags)
      TagsVisitor.parseTags(tags)
      return tags
   }

   /** Parses childnodes of all known views and generates controller nodes.
    * If a node matches the name of view it is considered a view reference.
    * If required attributes of a view are not set an error is generated. */
   function parseViewContent()
   {
      var i=0
      for (const view of Object.values(KNOWN_VIEWS)) {
         console.log((++i) + ". view",view)
      }
      // TODO: 2.1 validate view names and attributes
      // TODO: 2.2 build view controller
   }

   ////////////////////
   // Helper Classes //
   ////////////////////
   class ChildNodes
   {
      #childNodes=[]
      appendChild(node)   { return this.#childNodes.push(node), this }
      [Symbol.iterator]() { return this.#childNodes[Symbol.iterator]() }
      toString()          { return this.#childNodes.map(node => node.outerHTML ?? node.nodeValue).join("") }

      static fromVisitingContext(visitingContext)
      {
         const childNodes=new ChildNodes()
         while (visitingContext.node) {
            childNodes.appendChild(visitingContext.node)
            visitingContext=visitingContext.newSiblingContext()
         }
         return childNodes
      }

      static fromParentNode(parentNode)
      {
         const childNodes=new ChildNodes()
         for (const node of parentNode.childNodes)
            childNodes.appendChild(node)
         return childNodes
      }
   }

   /** TODO: Either support import&prefixes or remove this class */
   class Prefixer
   {
      static ALLOWEDCHARS=/^[-_.:a-zA-Z0-9]*/

      #knownPrefixes={}
      #matcher=null

      set(prefix,value)
      {
         if (typeof prefix !== "string")
            throw Error(`Prefix is not of type string but ${typeof prefix}.`)
         if (! Prefixer.ALLOWEDCHARS.test(prefix)) {
            const pos=prefix.match(Prefixer.ALLOWEDCHARS)[0].length
            throw Error(`Prefix contains unsupported character '${prefix.substring(0,pos-1)}»${prefix[pos]}«'.`)
         }
         if (prefix in this.#knownPrefixes)
            throw Error(`Prefix »${String(prefix)}« is not unique.`)
         this.#knownPrefixes[prefix]=value
         this.buildMatcher()
      }

      prefixOf(value)
      {
         const match=value.match(this.#matcher)
         return match
      }

      buildMatcher()
      {
         const matcher=Object.keys(this.#knownPrefixes).reduce( (matcher,key) => matcher+"|"+key.replace(".","\\."))
         this.#matcher=new RegExp(`^(${matcher})`)
      }
   }

   class ControlNode
   {
      #attributes; #dynAttributes; #node; #modelAccessFct;
      constructor(node)
      {
         this.#attributes={}
         this.#dynAttributes={}
         this.#node=node
         this.#modelAccessFct=null
      }

      isConnected() { return this.#node.isConnected }
      node()     { return this.#node }
      nodeName() { return nodeName(this.#node) }
      isTextNode() { return isTextNode(this.#node) }
      attributes() { return this.#attributes }
      dynAttributes() { return this.#dynAttributes }
      /** isTextNode()==false ==> always returns null */
      modelAccessFct() { return this.#modelAccessFct }
   }

   //////////////////////////////
   // generic DOM node visitor //
   //////////////////////////////

   /**
    * Context referencing DOM node which is visited.
    * A root context is created with newContext and subsequent context
    * with newChildContext and newSiblingContext.
    */
   class DOMVisitingContext
   {
      /**
       * Creates a new visitor context which points to the HTML node which should be visited next.
       * @param {DOMVisitingContext|null} parent
       *  The parent context if this node is visited as child of another node else null.
       * @param {HTMLNode} node
       *  The node which is visited.
       */
      constructor(parent,node,config)
      {
         this.node=node
         this.parent=parent
         this.config=config
      }

      get nodeName() { return nodeName(this.node) }
      taglibPrefix() { return this.config.taglibPrefix }
      setTaglibPrefix(prefix) { this.config.taglibPrefix=prefix.toUpperCase() }
      taglibNodeName() { return this.node.nodeName.startsWith(this.config.taglibPrefix) ? this.node.nodeName.substring(this.config.taglibPrefix.length) : null }
      isTextNode()     { return this.node.nodeType === this.node.TEXT_NODE }

      static newContext(node)
      {
         return new DOMVisitingContext(null,node,{ taglibPrefix: "" })
      }

      newChildContext(isSkipableNode=isEmptyNode)
      {
         const childNode=skipNodes(this.node.firstChild,isSkipableNode)
         return new DOMVisitingContext(this,childNode,this.config)
      }

      newSiblingContext(isSkipableNode=isEmptyNode)
      {
         const siblingNode=skipNodes(this.node.nextSibling,isSkipableNode)
         return new DOMVisitingContext(this.parent,siblingNode,this.config)
      }

      newPreviousSiblingContext(isSkipableNode=isEmptyNode)
      {
         const siblingNode=skipNodes(this.node.previousSibling,isSkipableNode, (node) => node.previousSibling)
         return new DOMVisitingContext(this.parent,siblingNode,this.config)
      }

      logError(message,attrName)
      {
         const useParentNode=(this.node==null)
         const node= useParentNode ? this.parent.node : this.node
         return logErrorForNode(node,attrName,message)
      }

      execFunction(fct,expr,attrName)
      {
         try { return fct() } catch(e) { this.throwError(`Evaluating '${expr}' failed: ${endWithDot(e.message)}`,attrName) }
      }

      throwError(message,attrName)
      {
         throw this.logError(message,attrName)
      }
   }

   /**
    * Visits a single DOM node.
    * Subclasses overwrite methods visitAttributes, visitTextNode or visitChilds.
    * To start the visiting process call visitNode.
    * */
   class DOMVisitor
   {
      /** Called from subclass to initialize static Attributes property. */
      static initAttributesFromArray(Attributes)
      {
         const attributes={}
         for (let i=0; i<Attributes.length; ++i) {
            const attr=Attributes[i]
            if (typeof attr !== "object")
               throw Error(`Expect ${this.name}.Attributes[${i}] of type object.`)
            for (const key of Object.keys(attr))
               if (!(key in INIT_ATTR_PROPERTIES))
                  throw Error(`${this.name}.Attributes[${i}] contains unsupported property ${key}.`)
            if (Object.getOwnPropertySymbols(attr).length)
               throw Error(`${this.name}.Attributes[${i}] contains unsupported symbol properties.`)
            for (const key of Object.keys(INIT_ATTR_PROPERTIES))
               if (  INIT_ATTR_PROPERTIES[key] === true
                  && !(key in attr))
                  throw Error(`${this.name}.Attributes[${i}] is missing the required property ${key}.`)
            if (typeof attr.name !== "string")
               throw Error(`${this.name}.Attributes[${i}].name is not of type string.`)
            if (attr.isEnum && ! attr.pattern)
               throw Error(`${this.name}.Attributes[${i}] contains enum attribute (name=${attr.name}) with no pattern to match.`)
            if (attr.isEnum && !String(attr.pattern).match(VALIDENUM_PATTERN))
               throw Error(`${this.name}.Attributes[${i}] contains enum attribute (name=${attr.name}) with pattern not of the form /^(value1|...|valueN)$/. Wrong value is ${String(attr.pattern)}.`)
            attributes[attr.name]=attr
         }
         return attributes
      }

      /** Initializes shared values for properties 'nodeName' and 'attributes'
       * from static properties subClass.NodeName resp. subClass.Attributes.
       * This means all instances of subClass share values for these properties
       * by assigning them to subClass.prototype.
       */
      static validateNodeName(nodeName)
      {
         if (typeof nodeName !== "string"
             ||  (   !TAGNAME_PATTERN.test(nodeName)
                  && nodeName !== "*"
                  && nodeName !== "#comment"
                  && nodeName !== "#text"))
            throw Error(`${this.name}.NodeName='${nodeName}' is not in set of allowed values ['*','#comment','#text','TAGS'] and does not match ${TAGNAME_PATTERN}.`)
         return nodeName[0]==="#" ? nodeName : nodeName.toUpperCase()
      }

      static isMatchingNodeName(visitingContext)
      {
         if ( ! visitingContext.node) return false
         if ( "*" === this.NodeName) return true
         const nn=visitingContext.node.nodeName
         return ("#" === this.NodeName[0] && nn === this.NodeName)
                || ( nn.length === visitingContext.taglibPrefix().length+this.NodeName.length
                     && nn.startsWith(visitingContext.taglibPrefix())
                     && nn.endsWith(this.NodeName))
      }

      isMatchingNodeName(visitingContext) { return this.#subClass.isMatchingNodeName(visitingContext) }

      #subClass
      constructor()
      {
         this.#subClass=new.target
         this.nodeContext=null
         this.validatedAttributes={}
         this.dynamicAttributes={}
         this.textNodeFct=null // in case of text node containing expression: points to evaluating function
      }

      get nodeName() { return this.#subClass.NodeName.toLowerCase() }

      /////////////////////////////////////////////////////
      // Methods which should be overwritten in subclass //
      /////////////////////////////////////////////////////

      visitAttributes(visitingContext)
      {
         const node=visitingContext.node
         const attributes=this.#subClass.Attributes
         const anyAttr=attributes["*"]
         // check all attributes are allowed and valid
         for (const na of node.attributes) {
            const name=na.name, value=na.value, attr=attributes[name] ?? anyAttr
            if (! attr)
               visitingContext.throwError(`Attribute '${name}' not supported.`,name)
            else if (attr.isEmpty!==undefined && attr.isEmpty !== (value==""))
               visitingContext.throwError(`Attribute '${name}' must have a${attr.isEmpty?'n':' non'} empty value.`,name)
            else if (attr.reservedPattern && value.match(attr.reservedPattern))
               visitingContext.throwError(`Attribute '${name}' contains reserved value '${value}'.`,name)
            else if (attr.pattern) {
               const match=value.match(attr.pattern) ?? [""]
               if (match[0].length != value.length) {
                  const enumMatch=String(attr.pattern).match(VALIDENUM_PATTERN)
                  if (enumMatch)
                     visitingContext.throwError(`Attribute '${name}' should be set to a value out of ${enumMatch[1]}. Value '${value}' is invalid.`,name)
                  else
                     visitingContext.throwError(`Attribute '${name}' contains invalid character '${value[match[0].length]}' at offset ${match[0].length}.`,name)
               }
            }
            // validate attribute value for expressions
            const fct=tryDataAccessFunction(value,visitingContext,name)
            if (attr.isModelAccess === true && !fct)
               throw ParseError(`Attribute '${name}' must contain expression beginning with '&:'.`,name)
            else if (attr.isModelAccess === false && fct)
               throw ParseError(`Attribute '${name}' does not support expressions like '${value.trim()}'.`,name)
            if (fct)
               this.validatedAttributes[name]=this.dynamicAttributes[name]=fct
            else
               this.validatedAttributes[name]=value
         }
         // check for required attributes
         for (const attr of Object.values(attributes))
            if (attr.isRequired && !(attr.name in node.attributes))
               visitingContext.throwError(`Attribute '${attr.name}' required`)
      }

      visitTextNode(visitingContext)
      {
         tryDataAccessFunction(visitingContext.node.nodeValue,visitingContext)
      }

      visitChilds(visitingContext)
      {
         this.validateNoChild(visitingContext)
      }

      ///////////////////////////////////////
      // Methods to start visiting process //
      ///////////////////////////////////////

      visitNode(visitingContext)
      {
         if (!this.isMatchingNodeName(visitingContext))
            visitingContext.throwError(`Expect node <${this.nodeName}>${!visitingContext.parent?' as root':!visitingContext.node?` instead of </${visitingContext.parent.nodeName}>`:visitingContext.newPreviousSiblingContext().node?` after previous sibling <${visitingContext.newPreviousSiblingContext().nodeName}>`:` as first child of <${visitingContext.parent.nodeName}>`}.`)
         this.nodeContext=visitingContext
         if (visitingContext.isTextNode())
            this.visitTextNode(visitingContext)
         else {
            this.visitAttributes(visitingContext)
            this.visitChilds(visitingContext.newChildContext())
         }
         return this
      }

      ////////////////////////////////////////////////////
      // Helper methods used in overwritten visitChilds //
      ////////////////////////////////////////////////////

      validateNoChild(visitingContext)
      {
         if (visitingContext.node) {
            const prevName=visitingContext.newPreviousSiblingContext().nodeName
            if (prevName)
               visitingContext.throwError(`Expect no sibling node after <${prevName}>.`)
            else
               visitingContext.throwError(`Expect no child node within <${visitingContext.parent.nodeName}>.`)
         }
      }

      visitZeroOrMore(visitingContext,visitorClass,results)
      {
         while (visitorClass.isMatchingNodeName(visitingContext)) {
            const result=new visitorClass().visitNode(visitingContext).result
            visitingContext=visitingContext.newSiblingContext()
            if (results) results.push(result)
         }
         return visitingContext
      }

      visitOneOrMore(visitingContext,visitorClass,results)
      {
         const result=new visitorClass().visitNode(visitingContext).result
         if (results) results.push(result)
         return this.visitZeroOrMore(visitingContext.newSiblingContext(),visitorClass,results)
      }
   }

   ///////////////////////////////////////////////////////
   // Visitors parsing DOM nodes for defining View Tags //
   ///////////////////////////////////////////////////////
   // These visitors are the "model checkers" and
   // transform DOM tree to a javascript data structure.

   class xSetVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("set")
      static Attributes=this.initAttributesFromArray(
         [{name:"var",isRequired:true,pattern:VARNAME_PATTERN}
         ])

      visitChilds(visitingContext)
      {
         this.visitZeroOrMore(visitingContext,xGenericVisitor)
      }
   }

   class xForVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("for")
      static Attributes=this.initAttributesFromArray(
         [{name:"in",isRequired:true,isEmpty:false,isModelAccess:true}
         ,{name:"var",isRequired:true,pattern:VARNAME_PATTERN}
         ,{name:"index",isRequired:false,pattern:VARNAME_PATTERN}
         ,{name:"length",isRequired:false,pattern:VARNAME_PATTERN}
         ])

      visitChilds(visitingContext)
      {
         this.visitZeroOrMore(visitingContext,xGenericVisitor)
      }

   }

   class xAttrReferenceVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("*")
      static Attributes=this.initAttributesFromArray([])

      #attrName;

      visitAttributes(visitingContext)
      {
         super.visitAttributes(visitingContext)
         if (! xViewReferenceVisitor.isMatchingNodeName(visitingContext.parent))
            visitingContext.builder.throwErrorAtNode(visitingContext.node,`Node <${visitingContext.nodeName}> can only be used within nodes of type <x:v-viewname>.`)
         const prevContext=visitingContext.newPreviousSiblingContext()
         if (prevContext.node && ! this.isMatchingNodeName(prevContext))
            visitingContext.builder.throwErrorAtNode(visitingContext.node,`Node <${visitingContext.nodeName}> can not be used after <${prevContext.nodeName}> within node <${visitingContext.parent.nodeName}>.`)
         this.#attrName=visitingContext.nodeName.substring(4)
      }

      visitChilds(visitingContext)
      {
         this.visitZeroOrMore(visitingContext,xGenericVisitor)
         visitingContext.parent.builder.addReferencedAttribute(this.#attrName,ChildNodes.fromVisitingContext(visitingContext))
      }
   }

   class xViewReferenceVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("*")
      static Attributes=this.initAttributesFromArray(
         [{name:"vdata",isRequired:false,isEmpty:false,isModelAccess:true},
          {name:"*",isRequired:false},
         ])

      visitAttributes(visitingContext)
      {
         super.visitAttributes(visitingContext)
         const viewName=visitingContext.node.nodeName.substring(4)
         const builder=visitingContext.builder
         builder.addReferencedView(viewName)
         for (const [ name, value ] of Object.entries(this.validatedAttributes))
            builder.addReferencedAttribute(name,value)
      }

      visitChilds(visitingContext)
      {
         const visitingContext2=this.visitZeroOrMore(visitingContext,xAttrReferenceVisitor)
         if (visitingContext === visitingContext2)
            this.visitZeroOrMore(visitingContext,xGenericVisitor)
         else
            this.validateNoChild(visitingContext2)
      }
   }

   class xGenericVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("*")
      static Attributes=this.initAttributesFromArray([
         {name:"*",isRequired:false},
      ])
      static TaglibVisitors={
         [xSetVisitor.NodeName]: xSetVisitor,
         [xForVisitor.NodeName]: xForVisitor,
         /* TODO:
         [xAttrReferenceVisitor.NodeName]: xAttrReferenceVisitor,
         [xViewReferenceVisitor.NodeName]: xViewReferenceVisitor,
         */
      }

      visitChilds(visitingContext)
      {
         const taglibName=this.nodeContext.taglibNodeName()
         if (taglibName) { // call specific visitor for taglib nodes
            const taglibVisitor=xGenericVisitor.TaglibVisitors[taglibName]
            if (!taglibVisitor)
               this.nodeContext.throwError(`Unsupported taglib node.`)
            new taglibVisitor().visitNode(this.nodeContext)
         }
         else
            this.visitZeroOrMore(visitingContext,xGenericVisitor)
      }

   }

   class xHtmlVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("html")
      static Attributes=this.initAttributesFromArray([])

      #childNodes;
      get result() { return this.#childNodes }

      visitChilds(visitingContext)
      {
         this.visitZeroOrMore(visitingContext,xGenericVisitor)
         this.#childNodes=ChildNodes.fromVisitingContext(visitingContext)
      }
   }

   class xVDataTextVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("#text")
      static Attributes=this.initAttributesFromArray([])

      #vdata;
      get result() { return this.#vdata }

      visitTextNode(visitingContext)
      {
         const expr=visitingContext.node.nodeValue
         const fct=buildDataAccessFunction(expr,visitingContext)
         this.#vdata=visitingContext.execFunction(fct,expr)
         if (typeof this.#vdata !== "object")
            visitingContext.throwError(`Expression '${expr}' should result in a value of type object not '${typeof this.#vdata}'.`)
      }
   }

   class xVDataVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("vdata")
      static Attributes=this.initAttributesFromArray([])

      #vdata
      get result() { return this.#vdata }

      visitChilds(visitingContext)
      {
         this.#vdata=new xVDataTextVisitor().visitNode(visitingContext).result
         this.validateNoChild(visitingContext.newSiblingContext())
      }
   }

   class xAttrVisitor extends DOMVisitor
   {
      static ATTRTYPE=/^(body|nodes|string)$/
      static NodeName=this.validateNodeName("attr")
      static Attributes=this.initAttributesFromArray(
                        [{name:"name",isRequired:true,isEmpty:false,pattern:VARNAME_PATTERN}
                        ,{name:"required",isRequired:false,isEmpty:true}
                        ,{name:"type",isRequired:false,isEmpty:false,pattern:this.ATTRTYPE,isEnum:true}
                        ])

      #attrValue;
      get result() { return ({ name: this.validatedAttributes.name, value: this.#attrValue, required: ("required" in this.validatedAttributes) }) }

      visitChilds(visitingContext)
      {
         this.visitZeroOrMore(visitingContext,xGenericVisitor)
         this.#attrValue=ChildNodes.fromVisitingContext(visitingContext)
      }
   }

   class xViewVisitor extends DOMVisitor
   {
      static VIEWNAME=/^[_a-zA-Z][-_:.a-zA-Z0-9]*/
      static NodeName=this.validateNodeName("view")
      static Attributes=this.initAttributesFromArray(
                        [{name:"name",isRequired:true,pattern:this.VIEWNAME}
                        ,{name:"id",isRequired:false}
                        ])

      #attribs=[]; #childNodes; #vdata;
      get result()
      {
         return ({ attributes: this.#attribs, childNodes: this.#childNodes,
                  name: this.validatedAttributes.name, vdata: this.#vdata,
                  viewNode: this.nodeContext.node, taglibPrefix: this.nodeContext.taglibPrefix(), tags: null })
      }

      visitChilds(visitingContext)
      {
         this.#vdata=new xVDataVisitor().visitNode(visitingContext).result
         const visitingContext2=this.visitZeroOrMore(visitingContext.newSiblingContext(),xAttrVisitor,this.#attribs)
         this.#childNodes=new xHtmlVisitor().visitNode(visitingContext2).result
         this.validateNoChild(visitingContext2.newSiblingContext())
      }
   }

   class TagsVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("tags")
      static Attributes=this.initAttributesFromArray(
                        [{name:"url",isRequired:true,isEmpty:false}
                        ,{name:"export-prefix",isRequired:true,pattern:PREFIX_PATTERN}
                        ,{name:"taglib-prefix",isRequired:true,pattern:NONEMPTY_PREFIX_PATTERN}
                        ])

      #views=[];
      get result() { return ({ tagsNode: this.nodeContext.node, views: this.#views, exportPrefix: this.validatedAttributes["export-prefix"], taglibPrefix: this.validatedAttributes["taglib-prefix"] }) }

      visitChilds(visitingContext)
      {
         visitingContext.setTaglibPrefix(this.validatedAttributes["taglib-prefix"])
         const visitingContext2=this.visitOneOrMore(visitingContext,xViewVisitor,this.#views)
         this.validateNoChild(visitingContext2)
      }

      /** Parses view tags and makes them known (KNOWN_VIEWS).
       * No controlling function is generated.*/
      static parseTags(tags)
      {
         const result=new TagsVisitor().visitNode(DOMVisitingContext.newContext(tags)).result
         for (const view of result.views) {
            const prefixedName=result.exportPrefix+view.name
            view.tags=result
            if (KNOWN_VIEWS[prefixedName])
               throw logErrorForNode(view.viewNode,"name",`View '${prefixedName}' not unique.`,KNOWN_VIEWS[prefixedName].viewNode)
            KNOWN_VIEWS[prefixedName]=view
         }
         return result
      }


   }


   ////////////////////////////
   // Exported Module Object //
   ////////////////////////////

   class JSTagLibrary
   {
      ////////////////////
      // initialization //
      ////////////////////

      init()
      {
      }

      ///////////////////////
      // extension support //
      ///////////////////////

      registerExtension(name, getExtension)
      {
         if (typeof name !== "string" || name === "")
            throw new Error(`Expect 1st argument of type non empty string.`)
         else if (typeof getExtension !== "function")
            throw new Error(`Expect 2nd argument of type function returning the extension object.`)
         else if (EXTENSIONS[name])
            throw new Error(`Extension '${String(name)}' exists.`)
         EXTENSIONS[name]=getExtension()
      }

      getExtension(name)
      {
         const extension=EXTENSIONS[name]
         if (! extension)
            throw new Error(`Extension '${String(name)}' does not exist.`)
         return extension
      }

      getExtensions() { return { ...EXTENSIONS } }

      ///////////////////
      // event handler //
      ///////////////////

      /**
       * Registers callback on internal queue which executes all registered callbacks
       * once HTML document is fully loaded. Registering a callback after the document has been loaded
       * does call it next time the HTML event loop is processed.
       * @param {()=>void} callback
       *  Function which will be called once document with all resources (css,images,...) is loaded.
       */
      onload(callback) { ONLOAD_QUEUE.then(callback) }

      ////////////////////
      // Exported Types //
      ////////////////////

      ParseError=ParseError
      Prefixer=Prefixer // TODO: remove
      ChildNodes=ChildNodes
      ControlNode=ControlNode
      DOMVisitingContext=DOMVisitingContext
      DOMVisitor=DOMVisitor

      ////////////////////////
      // Exported Constants //
      ////////////////////////

      VARNAME_PATTERN=VARNAME_PATTERN
      ACCESSPATH_PATTERN=ACCESSPATH_PATTERN

      //////////////////////
      // Helper Functions //
      //////////////////////

      endWithDot=endWithDot            /////////////////////////
      escapeString=escapeString        // for string handling //
      typeOf=typeOf                    /////////////////////////

      isCommentNode=isCommentNode      ///////////////////////////////
      isTextNode=isTextNode            // general DOM node handling //
      isEmptyTextNode=isEmptyTextNode  ///////////////////////////////
      isEmptyNode=isEmptyNode
      nodeName=nodeName
      nodeHierarchy=nodeHierarchy
      parentChain=parentChain
      skipNodes=skipNodes

      nodeSelector=nodeSelector        /////////////////////////////////////////
      queryNode=queryNode              // handling of nodes in TAGS_CONTAINER //
      queryNodes=queryNodes            /////////////////////////////////////////
      tryDataAccessFunction=tryDataAccessFunction
      knownViewNames=knownViewNames
      removeView=removeView
      loadTags=loadTags
      parseTags=parseTags
      parseViewContent=parseViewContent
   }

   return new JSTagLibrary()
})()
