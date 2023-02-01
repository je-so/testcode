/////////////////////////////////////////
// Base Library for Defining View Tags //
/////////////////////////////////////////
const jstl=(() => {

   ///////////////
   // Constants //
   ///////////////
   const INIT_ATTR_PROPERTIES={ name:true, isEnum:false, isEmpty:false, isModelAccess:false, isRequired:false, pattern:false, reservedPattern:false }
   const TAGS_CONTAINER=document.createDocumentFragment()
   const ONLOAD_QUEUE=new Promise( (resolve) => {
      if ("complete" === document.readyState)
         resolve(new Event("load")) // event already occurred
      else
         window.addEventListener("load", resolve, {once:true}) // wait
   })
   /** Contains all additional loaded extensions indexed by their name. */
   const EXTENSIONS={}

   /////////////////////////
   // Regular Expressions //
   /////////////////////////
   const PREFIX_PATTERN=/^([A-Za-z][-_.:a-zA-Z0-9]*[-:]|)$/
   const NONEMPTY_PREFIX_PATTERN=/^[A-Za-z][-_.:a-zA-Z0-9]*[-:]$/
   const NEWLINE=/\n/g
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

   ///////////////////////////
   // Some Helper Functions //
   ///////////////////////////

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

   function buildDataAccessFunction(expr)
   {
      if (expr.length == 0)
         throw Error(`Expression '' is empty.`)
      const isPath=(expr.match(ACCESSPATH_PATTERN) !== null)
      const functionBody=`"use strict"; return (${isPath?"d['":""}${expr}${isPath?"']":""})`
      try {
         return new Function("d","vd",functionBody)
      } catch(e) {
         throw Error(`${isPath?'':'Use only characters from [_$.A-Za-z0-9] to mark expression as access path. '}Compilation of »${functionBody}« fails with: ${endWithDot(e.message)}`, {cause:e})
      }
   }

   function tryDataAccessFunction(value)
   {
      if (value.indexOf("&:")<0)
         return null
      const expr=value.trim()
      if (!expr.startsWith("&:"))
         throw Error(`'${expr.substring(0,10)}${expr.length>10?"...":""}' does not start with '&:'.`)
      return buildDataAccessFunction(expr.substring(2))
   }

   function nodeHierarchy(node)
   {
      if (! node) return ""
      const parent=node.parentNode?.parentNode ? nodeHierarchy(node.parentNode) + " \u25b8 ": ""
      const attributes=([...(node.attributes??[])])
                        .map(attr => ` ${attr.name}="${escapeString(attr.value)}"`)
                        .join()
      return parent + (isTextNode(node)
                        ? `${nodeName(node)} "${escapeString(node.nodeValue)}"`
                        : `<${nodeName(node)}${attributes}>`)
   }

   function parentChain(node)
   {
      if (! node) return []
      if (node.parentNode?.parentNode)
         return [...parentChain(node.parentNode),node]
      else
         return [node]
   }

   function skipNodes(node,isSkipableNode,getSibling=(node)=>node.nextSibling)
   {
      while (node && isSkipableNode(node))
         node=getSibling(node)
      return node
   }

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

   function loadTags({url,exportPrefix/*'e:' or 'form-'*/})
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

   ////////////////////
   // Helper Classes //
   ////////////////////
   class Prefixer
   {
      static ALLOWEDCHARS=/^[-_.:a-zA-Z0-9]*/

      #knownPrefixes={}
      #matcher=null

      set(prefix,value)
      {
         if (typeof prefix !== "string")
            throw Error(`Expected string not ${typeof prefix}.`)
         if (! Prefixer.ALLOWEDCHARS.test(prefix)) {
            const pos=prefix.match(Prefixer.ALLOWEDCHARS)[0].length
            throw Error(`contains unsupported character '${prefix.substring(0,pos-1)}»${prefix[pos]}«'.`)
         }
         if (prefix in this.#knownPrefixes)
            throw Error(`»${String(prefix)}« is not unique.`)
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
      constructor(parent,node)
      {
         this.errorCount=0
         this.node=node
         this.parent=parent
         this.config=parent?.config ?? ({ taglibPrefix:"", localPrefix:"" })
         this.useParentNode=false
      }

      get nodeName() { return nodeName(this.node) }
      localPrefix()  { return this.config.localPrefix }
      taglibPrefix() { return this.config.taglibPrefix }

      static newContext(node)
      {
         return new DOMVisitingContext(null,node)
      }

      newChildContext(isSkipableNode=isEmptyNode)
      {
         const childNode=skipNodes(this.node.firstChild,isSkipableNode)
         return new DOMVisitingContext(this,childNode)
      }

      newSiblingContext(isSkipableNode=isEmptyNode)
      {
         const siblingNode=skipNodes(this.node.nextSibling,isSkipableNode)
         return new DOMVisitingContext(this.parent,siblingNode)
      }

      newPreviousSiblingContext(isSkipableNode=isEmptyNode)
      {
         const siblingNode=skipNodes(this.node.previousSibling,isSkipableNode, (node) => node.previousSibling)
         return new DOMVisitingContext(this.parent,siblingNode)
      }

      setTaglibPrefix(prefix) { this.config.taglibPrefix=prefix.toUpperCase() }
      setLocalPrefix(prefix)  { this.config.localPrefix=prefix.toUpperCase() }

      switchToParentNode()
      {
         this.useParentNode=(this.parent!=null)
         return this
      }

      logErrorForNode(node,attrName,message,locationNode)
      {
         locationNode??=node
         const hierarchy=nodeHierarchy(locationNode)
         const selector=nodeSelector(locationNode)
         return new ParseError(`<${nodeName(node)}${attrName?' '+attrName:''}>: ${message}\nLocation: ${hierarchy}\nSelector: ${selector}`,{logArgs: ["\nLocation:",...parentChain(locationNode)]})
      }

      logError(message,attrName)
      {
         const noNode=this.node==null
         const node=this.useParentNode || noNode ? (this.parent.errorCount++, this.parent.node) : this.node
         this.useParentNode=false // TODO: remove useParentNode + switchToParentNode
         this.errorCount++
         return this.logErrorForNode(node,attrName,message,this.node??node)
      }

      buildFunction(attr)
      {
         try {
            return buildDataAccessFunction(attr?.value ?? this.node.nodeValue)
         } catch(e) {
            this.throwError(e.message,attr?.name)
         }
      }

      execFunction(fct,attr)
      {
         try { return fct() } catch(e) { this.throwError(`Evaluating '${attr?.value??this.node.nodeValue}' failed: ${endWithDot(e.message)}`,attr?.name) }
      }

      throwError(message,attrName)
      {
         throw this.logError(message,attrName)
      }

      onErrorThrow()
      {
         if (this.errorCount) throw new ParseError()
      }

      isTextNode()
      {
         return this.node.nodeType === this.node.TEXT_NODE
      }

   }

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
         this.attributes=new.target.Attributes
         // TODO: remove ??
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
         const anyAttr=this.attributes["*"]
         // check all attributes are allowed and valid
         const dynamicAttributes={}
         for (const na of node.attributes) {
            const name=na.name, value=na.value, attr=this.attributes[name] ?? anyAttr
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
                     visitingContext.throwError(`Attribute '${name}' should be set to a value out of ${enumMatch[1]}. Value '${value}' is invalid.`)
                  else
                     visitingContext.throwError(`Attribute '${name}' contains invalid character '${value[match[0].length]}' at offset ${match[0].length}`)
               }
            }
            // validate attribute value for expressions
            try {
               const fct=tryDataAccessFunction(value)
               if (attr.isModelAccess === true && !fct)
                  throw Error(`must contain expression beginning with '&:' or '&)'.`)
               else if (attr.isModelAccess === false && fct)
                  throw Error(`does not support expressions like '${value.trim()}'.`)
               if (fct)
                  this.validatedAttributes[name]=this.dynamicAttributes[name]=fct
               else
                  this.validatedAttributes[name]=value
            }
            catch(e) {
               visitingContext.throwError(`Attribute '${name}' ${e.message}`)
            }
         }
         // check for required attributes
         for (const name in this.attributes)
            if (this.attributes[name].isRequired && !(name in node.attributes))
               visitingContext.throwError(`Attribute '${name}' required`)
      }

      visitTextNode(visitingContext)
      {
         try {
            const node=visitingContext.node
            const text=node.nodeValue
            tryDataAccessFunction(text)
         }
         catch(e) {
            visitingContext.throwError(`Text node contains invalid expression. ${e.message}`)
         }
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
   }



   ////////////////////////////////////////////////////////
   // Visitors parsing HTML nodes for defining an x:view //
   ////////////////////////////////////////////////////////
   // These visitors are the "model checkers" and transform
   // to a resulting data structure.

   class xSetVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("set")
      static Attributes=this.initAttributesFromArray(
         [{name:"var",isRequired:true,pattern:VARNAME_PATTERN}
         ])

      visitChilds(visitingContext)
      {
         this.visitZeroOrMore(visitingContext,xHtmlChildVisitor)
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
         this.visitZeroOrMore(visitingContext,xHtmlChildVisitor)
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
         this.visitZeroOrMore(visitingContext,xHtmlChildVisitor)
         visitingContext.parent.builder.addReferencedAttribute(this.#attrName,ChildNodes.fromVisitingContext(visitingContext))
      }

   }

   class xViewReferenceVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("*")
      static Attributes=this.initAttributesFromArray(
         [{name:"viewdata",isRequired:false,isEmpty:false,isModelAccess:true},
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
            this.visitZeroOrMore(visitingContext,xHtmlChildVisitor)
         else
            this.validateNoChild(visitingContext2)
      }

   }

   class xHtmlChildVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("*")
      static Attributes=this.initAttributesFromArray([
         {name:"*",isRequired:false},
      ])
      static SupportedVisitors={
         [xSetVisitor.NodeName]: xSetVisitor,
         [xForVisitor.NodeName]: xForVisitor,
         [xAttrReferenceVisitor.NodeName]: xAttrReferenceVisitor,
         [xViewReferenceVisitor.NodeName]: xViewReferenceVisitor,
      }
      #matchingVisitor;

      visitAttributes(visitingContext)
      {
         // call specific visitor for nodes of type <x:...>
         if (visitingContext.nodeName.startsWith("x:")) {
            const isPrefix=(visitingContext.nodeName[3] === "-")
            const nodeName=(isPrefix ? visitingContext.nodeName.substring(0,4) : visitingContext.nodeName)
            const visitorConstructor=xHtmlChildVisitor.SupportedVisitors[nodeName]
            if (!visitorConstructor)
               visitingContext.builder.throwErrorAtNode(visitingContext.node,`Unsupported node <${visitingContext.nodeName}>.`)
            this.#matchingVisitor=new visitorConstructor()
            this.#matchingVisitor.visitNode(visitingContext)
         }
         else
            super.visitAttributes(visitingContext)
      }

      visitChilds(visitingContext)
      {
         if (! this.#matchingVisitor)
            this.visitZeroOrMore(visitingContext,xHtmlChildVisitor)
      }
   }

   class xGenericVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("*")
      static Attributes=this.initAttributesFromArray([
         {name:"*",isRequired:false},
      ])

      visitChilds(visitingContext)
      {
         this.visitZeroOrMore(visitingContext,xGenericVisitor)
      }

   }

   class xHtmlVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("html")
      static Attributes=this.initAttributesFromArray([])

      visitChilds(visitingContext)
      {
         this.visitZeroOrMore(visitingContext,xGenericVisitor)
         // TODO:
         // visitingContext.builder.setxHtmlContext(visitingContext.parent)
         // this.visitZeroOrMore(visitingContext,xHtmlChildVisitor)
         // visitingContext.parent.builder.setHTML(ChildNodes.fromVisitingContext(visitingContext))
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
         const fct=visitingContext.buildFunction()
         this.#vdata=visitingContext.execFunction(fct)
         if (typeof this.#vdata !== "object")
            visitingContext.throwError(`Expression '${visitingContext.node.nodeValue}' should result in a value of type object not '${typeof this.#vdata}'.`)
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

      #attrValue

      get result() { return ({ name: this.validatedAttributes.name, childNodes: this.#attrValue, required: ("required" in this.validatedAttributes) }) }

      visitChilds(visitingContext)
      {
         // generic checks of default value
         this.visitZeroOrMore(visitingContext,xGenericVisitor)
         // TODO: use ChildNodes class !!!
         this.#attrValue=visitingContext.parent.node.childNodes
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

      #attribs; #vdata;

      get result() { return ({ vdata: this.#vdata, attribs: this.#attribs }) }

      visitChilds(visitingContext)
      {
         this.#vdata=new xVDataVisitor().visitNode(visitingContext).result
         this.#attribs=[]
         const visitingContext2=this.visitZeroOrMore(visitingContext.newSiblingContext(),xAttrVisitor,this.#attribs)
         new xHtmlVisitor().visitNode(visitingContext2)
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
                        ,{name:"local-prefix",isRequired:true,pattern:PREFIX_PATTERN}
                        ])

      #views=[];

      visitChilds(visitingContext)
      {
         visitingContext.setTaglibPrefix(this.validatedAttributes["taglib-prefix"])
         const visitingContext2=this.visitZeroOrMore(visitingContext,xViewVisitor,this.#views)
         this.validateNoChild(visitingContext2)
         console.log("views",this.#views)
      }

      static parseTags(tags)
      {
         new TagsVisitor().visitNode(DOMVisitingContext.newContext(tags))
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
      Prefixer=Prefixer
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
      loadTags=loadTags
      parseTags=parseTags
   }

   return new JSTagLibrary()
})()
