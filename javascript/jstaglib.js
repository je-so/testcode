/////////////////////////////////////////
// Base Library for Defining View Tags //
/////////////////////////////////////////
const jstl=(() => {

   ///////////////
   // Constants //
   ///////////////
   const INIT_ATTR_PROPERTIES={ name:true, isEnum:false, isEmpty:false, isModelAccess:false, isRequired:false, pattern:false, reservedPattern:false }
   const TAGS_CONTAINER=document.createDocumentFragment()
   /** Maps url of loaded TAGS to parsed tags result. */
   const PARSED_TAGS={}
   /** Maps view names(all uppercase) to ParsedViewTag. */
   const PARSED_VIEWS={}
   const CONFIG={ tagLoaders: [defaultTagLoader] }
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
   const PREFIX_PATTERN=/^[A-Za-z][-_.:a-zA-Z0-9]*([-:]$|(?!$))|/
   const NONEMPTY_PREFIX_PATTERN=/^[A-Za-z][-_.:a-zA-Z0-9]*([-:]$|(?!$))/
   const TAGNAME_PATTERN=/^[a-zA-Z][_.a-zA-Z0-9]*$/
   const ONLY_WHITE_SPACE=/^[ \n\t]*$/
   const ATTRNAME_PATTERN=/^[_$a-z][_$a-z0-9]*/
   const VARNAME_PATTERN=/^[_$A-Za-z][_$A-Za-z0-9]*/
   const ACCESSPATH_PATTERN=/^[_$A-Za-z0-9]+([.][_$A-Za-z0-9]+)*$/
   /** Matches string "/^(val1|...|valN)$/" with matchresult[1] === (val1|...|valN).
    *  Used to verify that regex pattern matches an enum value. */
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

   const adaptException=(callback,errmsg) =>
   {
      try { return callback() } catch(e) { throw Error(typeof errmsg==="function" ? errmsg(e) : errmsg,{cause:e}) }
   }

   const strLocation=(node) => `Location: ${nodeHierarchy(node)}`
   const strSelector=(node) => `Selector: ${nodeSelector(node)}`
   const logErrorForNode=(node,attrName,message,{prevNode,cause}={}) =>
   {
      const previous=(prevNode ? `\nPrevious ${strLocation(prevNode)}\nPrevious ${strSelector(prevNode)}` : "")
      const options={logArgs: ["\nLocation:",...parentChain(node)]}
      return new ParseError(`<${nodeName(node)}${attrName?' '+attrName:''}>: ${message}\n${strLocation(node)}\n${strSelector(node)}${previous}`,cause?{...options,cause}:options)
   }
   const logWarningForNode=(node,message) =>
   {
      console.log(`<${nodeName(node)}>: ${message}\n${strLocation(node)}\n${strSelector(node)}`,"\nLocation:",...parentChain(node))
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
   const isNotEmptyNode=(node) => !(isEmptyTextNode(node) || isCommentNode(node))
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
      const attributes=([...(node.attributes??[])]).map(a => ` ${a.name}="${escapeString(a.value)}"`).join("")
      return parent+(parent ? " \u25b6 " : "")
                   +(isTextNode(node)
                        ? `${nodeName(node)} "${escapeString(node.nodeValue)}"`
                        : `<${nodeName(node)}${attributes}>`)
   }

   function parentChain(node)
   {
      return (node && node.parentNode) ? [...parentChain(node.parentNode),node] : []
   }

   function skipNodes(node,isSkipableNode=isEmptyNode,getSibling=(node)=>node.nextSibling)
   {
      while (node && isSkipableNode(node))
         node=getSibling(node)
      return node
   }

   function parseHTML(str)
   {
      const div=document.createElement("div")
      div.innerHTML=str
      return div.childNodes
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

   ////////////////////
   // config support //
   ////////////////////

   const addTagsLoader=(tagLoader) => CONFIG.tagLoaders.unshift(tagLoader)

   ///////////////////////
   // load & parse tags //
   ///////////////////////

   // !! returned values are not cloned deeply so do not mess with returned values !!
   const getViews=() => Object.values(PARSED_VIEWS).map( view => ({...view}))
   const getViewNames=() => Object.keys(PARSED_VIEWS).map( name => name.toLowerCase())
   const getParsedTags=() => Object.values(PARSED_TAGS).map( tag => ({...tag}))

   function removeViews(...viewNames)
   {
      viewNames.flat().map(name => PARSED_VIEWS[name.toUpperCase()]).forEach( view => {
         if (view) {
            -- view.tags.nrExportedViews
            delete PARSED_VIEWS[view.exportName]
         }
      })
   }

   function removeParsedTags(...tagURLs)
   {
      return tagURLs.flat().map(url => PARSED_TAGS[url]).filter(tags => tags!=undefined)
         .map( tags => {
            removeViews( tags.views.filter(view => tags===PARSED_VIEWS[view.exportName].tags).map(view => view.exportName) )
            if (tags.nrExportedViews !== 0) console.log(Error("INTERNAL ERROR"))
            delete PARSED_TAGS[tags.url]
            tags.tagsNode.remove()
            return tags
         })
   }

   async function defaultTagLoader(url,exportPrefix)
   {
      return fetch(url).then( response => {
         if (! response.ok)
            throw Error(`${response.status} ${response.statusText}.`)
         return { text:response.text(), url, exportPrefix}
      }).catch( e => {
         throw Error(`Loading tags from '${url}' failed with: ${e.message??e}`,{cause:e})
      })
   }

   async function loadTags(url,exportPrefix)
   {
      if (typeof url !== "string")
         throw Error(`Argument url should be of type string not ${typeof url}.`)
      if (typeof exportPrefix !== "string" && exportPrefix != null)
         throw Error(`Argument exportPrefix should be of type string|null not ${typeof exportPrefix}.`)
      const canonicalURL=adaptException(() => new URL(url,document.documentURI),()=>`Argument url '${url}' is invalid.`)
      for (const loader of CONFIG.tagLoaders) {
         const loadedTags=await loader(canonicalURL.href,exportPrefix)
         if (loadedTags)
            return parseTags(loadedTags)
      }
   }

   async function parseTags({text,url,exportPrefix})
   {
      if (typeof url !== "string")
         throw Error(`Argument url should be of type string not ${typeof url}.`)
      if (typeof text !== "string")
         throw Error(`Argument text should be of type string not ${typeof text}.`)
      if (typeof exportPrefix !== "string" && exportPrefix != null)
         throw Error(`Argument exportPrefix should be of type string|null not ${typeof exportPrefix}.`)
      const childNodes=parseHTML(text.trim())
      if (childNodes.length === 0)
         throw new ParseError(`Tags loaded from '${url}' are empty.`)
      if (childNodes.length !== 1)
         throw new ParseError(`Tags loaded from '${url}' has not a single root node but two: <${nodeName(childNodes[0])}> followed by <${nodeName(childNodes[1])}>.`)
      // already parsed ?
      if (PARSED_TAGS[url])
         return PARSED_TAGS[url] // ! do not generate an error even if value of exportPrefix is different
      const tags=childNodes[0]
      tags.setAttribute("url",url)
      if (exportPrefix != null)
         tags.setAttribute("export-prefix",exportPrefix)
      TAGS_CONTAINER.appendChild(tags)
      const parsedTags=new TagsVisitor().visitNode(DOMVisitingContext.newContext(tags)).result
      // Register-Tags //
      parsedTags.exportViews(PARSED_VIEWS)
      PARSED_TAGS[url]=parsedTags
      ///////////////////
      for (const importTag of parsedTags.imports) {
         await loadTags(importTag.url).catch( e => {throw logErrorForNode(importTag.importNode,"url",`Importing tags failed with:\n---\n${e.message}\n---`,{cause:e})})
      }
      return parsedTags
   }

   /** All node names are compared against all defined views.
    * A list of node names which are not matched against views is returned. */
   function resolveViews()
   {
      const unresolved=new Set()
      const prefixes=new Set(Object.values(PARSED_TAGS).map(tags => tags.exportPrefix.toUpperCase()).filter(p => p !== ""))
      const prefixEnd=/[-:](?![^-:]*[-:])/ // searches for the last ':' or '-' in a string

      const resolveView=(view) =>
      {
         const resolveNodes=(childNodes) =>
         {
            for (const child of childNodes) {
               if (child.nodeName[0] !== "#" && !child.nodeName.startsWith(view.taglibPrefix)) {
                  const view=PARSED_VIEWS[child.nodeName]
                  if (view) {
                     view.validateViewRef(child)
                     if (! view.hasBody()) {
                        // skip subChild cause it matched as view attribute
                        for (const subChild of child.childNodes)
                           resolveNodes(subChild.childNodes)
                        continue
                     }
                  }
                  else {
                     unresolved.add(child.nodeName)
                     const prefix=child.nodeName.substring(0,child.nodeName.search(prefixEnd)+1)
                     if (prefix.length > 0) {
                        const addition=prefixes.has(prefix) ? "" : " and prefix '"+prefix.toLowerCase()+"'"
                        throw logErrorForNode(child,undefined,`Unknown view${addition}.`)
                     }
                  }
               }
               resolveNodes(child.childNodes)
            }
         }
         for (const attr of view.attributes) {
            if (attr.value instanceof ChildNodes)
               resolveNodes(attr.value)
         }
         resolveNodes(view.childNodes)
      }

      for (const view of Object.values(PARSED_VIEWS))
         resolveView(view)

      return [...unresolved].map(nodeName => nodeName.toLowerCase()).sort()
   }

   /** Parses childnodes of all known views and generates controller nodes.
    * If a node matches the name of view it is considered a view reference.
    * If required attributes of a view are not set an error is generated. */
   function parseViewContent()
   {
      var i=0
      for (const view of Object.values(PARSED_VIEWS)) {
         console.log((++i) + ". view",view)
      }
      // validate view names and attributes
      console.log("unresolved tags",resolveViews())
      ///////////
      // TODO: build view controller
      ///////////
   }

   ////////////////////////////
   // Parsed Data Structures //
   ////////////////////////////

   class ParsedViewTag
   {
      constructor(init) { Object.assign(this,init) }
      hasBody()     { return this.attrFilter({type:"body"}).length>0 }
      hasNoChilds() { return this.attrFilter({type:/body|child/}).length===0 }
      attrFilter({type,required,nameSet})
      {
         type=(type && new RegExp(type))
         return this.attributes.filter( a =>
               (type===undefined     || a.type.match(type))
            && (required===undefined || a.required === required)
            && (nameSet===undefined  || nameSet.has(a.name)))
      }
      validateTypedAttributes(refNode,type,subNodes)
      {
         const [attrNames,isAttrNode]=[new Set(this.attrFilter({type}).map(a => a.name)), type === "string"]
         const uniqueNames=new Set()
         for (const node of subNodes) {
            if (uniqueNames.has(node.nodeName) || !uniqueNames.add(node.nodeName))
               throw logErrorForNode(isAttrNode ? refNode : node, isAttrNode && node.nodeName, `${isAttrNode?'Attribute':'Child node'} provided more than once to view${isAttrNode?'':` <${this.name}>`}.`,{prevNode: [...subNodes].find(p => p.nodeName === node.nodeName)})
            if (!attrNames.delete(node.nodeName.toLowerCase()))
               throw logErrorForNode(isAttrNode ? refNode : node, isAttrNode && node.nodeName, `${isAttrNode?'Attribute':'Child node'} unsupported by view${isAttrNode?'':` <${this.name}>`}.`)
         }
         const missing=this.attrFilter({required:true, nameSet:attrNames})[0]
         if (missing) throw logErrorForNode(refNode,undefined,`View requires ${isAttrNode ? `attribute '${missing.name}'` : `child node <${missing.name}>`}.`)
      }
      validateViewRef(refNode)
      {
         this.validateTypedAttributes(refNode,"string",refNode.attributes)
         if (! this.hasBody())
            this.validateTypedAttributes(refNode,"child",[...refNode.childNodes].filter(isNotEmptyNode))
      }
   }

   class ParsedTagsTag
   {
      constructor(init)
      {
         Object.assign(this,init)
         this.nrExportedViews=0
         for (const view of this.views) {
            view.tags=this
            view.exportName=(this.exportPrefix+view.name).toUpperCase()
         }
      }

      /* INFO: ======== TODO: remove
      get result() { return ({
         tagsNode: this.node,
         imports: this.#imports,
         views: this.#views,
         exportPrefix: this.attr("export-prefix"),
         taglibPrefix: this.attr("taglib-prefix"),
         url: this.attr("url")
      }) }
      ======== */

      exportViews(parsed_views)
      {
         const unexportedViews=[]
         for (const view of this.views) {
            if (parsed_views[view.exportName])
               unexportedViews.push(exportName)
            else {
               ++ this.nrExportedViews
               parsed_views[view.exportName]=view
            }
         }
         if (unexportedViews.length)
            // throw logErrorForNode(view.viewNode,"name",`View '${view.exportName.toLowerCase()}' not unique.`,{prevNode:PARSED_VIEWS[view.exportName].viewNode})
            logWarningForNode(tags,`Existing views are not overwritten, namely ${unexportedViews}.`)
      }
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

   // TODO: removes
   class Prefixer
   {
      static ALLOWEDCHARS=/^[-_.:a-zA-Z0-9]*/
      #knownPrefixes=new Set(); #matcher

      add(prefix)
      {
         if (typeof prefix !== "string")
            throw Error(`Prefix is not of type string but ${typeof prefix}.`)
         const allowedLen=prefix.match(Prefixer.ALLOWEDCHARS)[0].length
         if (allowedLen !== prefix.length)
            throw Error(`Prefix contains unsupported character '${prefix.substring(0,allowedLen-1)}»${prefix[allowedLen]}«'.`)
         this.#knownPrefixes.add(prefix.toUpperCase())
      }

      prepareMatch()
      {
         const matcher=[...this.#knownPrefixes].reduce( (matcher,key) => matcher+"|"+key.replace(".","\\."),"[*]")
         this.#matcher=new RegExp(`^(${matcher})`)
      }

      match(value)
      {
         const match=value.match(this.#matcher)
         return match
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
      }

      attr(name) { return this.validatedAttributes[name] }
      get node() { return this.nodeContext.node }
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
            else if (attr.isEmpty!==undefined && attr.isEmpty !== (value===""))
               visitingContext.throwError(`Attribute '${name}' must have a${attr.isEmpty?'n':' non'} empty value.`,name)
            else if (attr.reservedPattern && value.match(attr.reservedPattern))
               visitingContext.throwError(`Attribute '${name}' contains reserved value '${value}'.`,name)
            else if (attr.pattern) {
               const match=value.match(attr.pattern) ?? [""]
               if (match[0].length != value.length) {
                  const enumMatch=String(attr.pattern).match(VALIDENUM_PATTERN)
                  if (enumMatch)
                     visitingContext.throwError(`Attribute value should be set to one out of ${enumMatch[1]}. Value '${value}' is invalid.`,name)
                  else
                     visitingContext.throwError(`Attribute value contains invalid character '${value[match[0].length]}' at offset ${match[0].length}.`,name)
               }
            }
            // validate attribute value for expressions
            const fct=tryDataAccessFunction(value,visitingContext,name)
            if (attr.isModelAccess === true && !fct)
               visitingContext.throwError(`Attribute value must contain expression beginning with '&:'.`,name)
            else if (attr.isModelAccess === false && fct)
               visitingContext.throwError(`Attribute value does not support expressions like '${value.trim()}'.`,name)
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

      visitZeroOrMore(visitingContext,visitorClass,results,resultValidator)
      {
         while (visitorClass.isMatchingNodeName(visitingContext)) {
            const result=new visitorClass().visitNode(visitingContext).result
            if (resultValidator) resultValidator(result)
            if (results) results.push(result)
            visitingContext=visitingContext.newSiblingContext()
         }
         return visitingContext
      }

      visitOneOrMore(visitingContext,visitorClass,results,resultValidator)
      {
         const result=new visitorClass().visitNode(visitingContext).result
         if (resultValidator) resultValidator(result)
         if (results) results.push(result)
         return this.visitZeroOrMore(visitingContext.newSiblingContext(),visitorClass,results,resultValidator)
      }
   }

   ///////////////////////////////////////////////////////
   // Visitors parsing DOM nodes for defining View Tags //
   ///////////////////////////////////////////////////////
   // These visitors are the "model checkers" and
   // transform DOM tree to a javascript data structure.

   class xTextVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("#text")
      static Attributes=this.initAttributesFromArray([])

      #text;
      get result() { return this.#text }

      visitTextNode(visitingContext)
      {
         this.#text=visitingContext.node.nodeValue
      }
   }

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

   class xGenericVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("*")
      static Attributes=this.initAttributesFromArray(
         [{name:"vdata",isRequired:false,isEmpty:false,isModelAccess:true}
         ,{name:"show",isRequired:false,isEmpty:false,isModelAccess:true}
         ,{name:"*",isRequired:false},
         ])
      static TaglibVisitors={
         [xSetVisitor.NodeName]: xSetVisitor,
         [xForVisitor.NodeName]: xForVisitor,
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

   class xVDataVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("vdata")
      static Attributes=this.initAttributesFromArray([])

      #vdata
      get result() { return this.#vdata }

      visitChilds(visitingContext)
      {
         const text=new xTextVisitor().visitNode(visitingContext).result
         this.validateNoChild(visitingContext.newSiblingContext())
         const fct=buildDataAccessFunction(text,visitingContext)
         this.#vdata=visitingContext.execFunction(fct,text)
         if (typeof this.#vdata !== "object")
            visitingContext.throwError(`Expression '${expr}' should result in a value of type object not '${typeof this.#vdata}'.`)
      }
   }

   class xAttrVisitor extends DOMVisitor
   {
      static ATTRTYPE=/^(body|child|string)$/
      static NodeName=this.validateNodeName("attr")
      static Attributes=this.initAttributesFromArray(
                        [{name:"name",isRequired:true,isEmpty:false,pattern:ATTRNAME_PATTERN,reservedPattern:/^vdata$/}
                        ,{name:"required",isRequired:false,isEmpty:true}
                        ,{name:"type",isRequired:false,isEmpty:false,pattern:this.ATTRTYPE,isEnum:true}
                        ])

      static getResultValidator()
      {
         const validator=(function*() {
            let attr1,attr2;
            const attributesByName={}
            const unique=(attr) => {
               if (attr.name in attributesByName) logErrorForNode(attr.attrNode,"name",`Attribute value '${attr.name}' is not unique.`,{prevNode:attributesByName[attr.name].attrNode})
               attributesByName[attr.name]=attr
            }
            do { attr1=yield; unique(attr1) } while (attr1.type !== "body" && attr1.type !== "child")
            for (;;) {
               do { attr2=yield; unique(attr2) } while (attr2.type !== "body" && attr2.type !== "child")
               if (attr1.type !== attr2.type)
                  throw logErrorForNode(attr2.attrNode,"type",`Attribute value '${attr2.type}' excludes previous attribute with type '${attr1.type}'.`,{prevNode:attr1.attrNode})
               else if (attr2.type === "body")
                  throw logErrorForNode(attr2.attrNode,"type",`Attribute value 'body' is not allowed more than once.`,{prevNode:attr1.attrNode})
               // attr1.type==attr2.type=="child"
               attr1=attr2
            }
         })()
         validator.next() // execute validator until first yield (waiting for input)
         return validator.next.bind(validator)
      }

      #attrValue;
      get result() { return ({ attrNode: this.node, name: this.attr("name").toLowerCase(), required: (this.attr("required")===""), type: this.attr("type")??"string", value: this.#attrValue }) }
      static vdata()  { return ({ attrNode: null, name: "vdata", required: false, type: "string", value: "" }) }

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
         return new ParsedViewTag({ attributes: this.#attribs, childNodes: this.#childNodes,
            name: this.attr("name"), exportName: null, vdata: this.#vdata,
            viewNode: this.node, taglibPrefix: this.nodeContext.taglibPrefix(), tags: null})
      }

      visitChilds(visitingContext)
      {
         this.#vdata=new xVDataVisitor().visitNode(visitingContext).result
         const visitingContext2=this.visitZeroOrMore(visitingContext.newSiblingContext(),xAttrVisitor,this.#attribs,xAttrVisitor.getResultValidator())
         this.#attribs.push(xAttrVisitor.vdata())
         this.#childNodes=new xHtmlVisitor().visitNode(visitingContext2).result
         this.validateNoChild(visitingContext2.newSiblingContext())
      }
   }

   class xImportVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("import")
      static Attributes=this.initAttributesFromArray(
                        [{name:"import-prefix",isRequired:false,pattern:PREFIX_PATTERN}
                        ])

      #importPrefix; #url;
      get result() { return ({ importNode: this.node, importPrefix: this.#importPrefix, url: this.#url }) }

      visitChilds(visitingContext)
      {
         this.#url=new xTextVisitor().visitNode(visitingContext).result
         this.validateNoChild(visitingContext.newSiblingContext())
         this.#importPrefix=this.attr("import-prefix")??null/*use export-prefix*/
      }
   }


   class TagsVisitor extends DOMVisitor
   {
      static NodeName=this.validateNodeName("tags")
      static Attributes=this.initAttributesFromArray(
                        [{name:"url",isRequired:true,isEmpty:false}
                        ,{name:"export-prefix",isRequired:true,pattern:PREFIX_PATTERN}
                        ,{name:"taglib-prefix",isRequired:true,isEmpty:false,pattern:NONEMPTY_PREFIX_PATTERN}
                        ])

      #views=[]; #imports=[];
      get result()
      {
         return new ParsedTagsTag({ exportPrefix: this.attr("export-prefix"), imports: this.#imports,
            taglibPrefix: this.attr("taglib-prefix"), tagsNode: this.node, views: this.#views, url: this.attr("url")})
      }

      visitChilds(visitingContext)
      {
         visitingContext.setTaglibPrefix(this.attr("taglib-prefix"))
         const visitingContext2=this.visitZeroOrMore(visitingContext,xImportVisitor,this.#imports)
         const visitingContext3=this.visitOneOrMore(visitingContext2,xViewVisitor,this.#views)
         this.validateNoChild(visitingContext3)
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

      /////////////////////////////
      // Configuration Functions //
      /////////////////////////////
      addTagsLoader=addTagsLoader


      ////////////////////
      // Exported Types //
      ////////////////////
      ParseError=ParseError
      Prefixer=Prefixer
      ChildNodes=ChildNodes
      ControlNode=ControlNode
      DOMVisitingContext=DOMVisitingContext
      DOMVisitor=DOMVisitor

      ////////////////////////
      // Exported Constants //
      ////////////////////////
      VARNAME_PATTERN=VARNAME_PATTERN
      ACCESSPATH_PATTERN=ACCESSPATH_PATTERN

      /////////////////////////////
      // String Helper Functions //
      /////////////////////////////
      endWithDot=endWithDot
      escapeString=escapeString
      typeOf=typeOf

      //////////////////////////////////
      // General DOM Helper Functions //
      //////////////////////////////////
      isCommentNode=isCommentNode
      isTextNode=isTextNode
      isEmptyTextNode=isEmptyTextNode
      isEmptyNode=isEmptyNode
      nodeName=nodeName
      nodeHierarchy=nodeHierarchy
      parentChain=parentChain
      skipNodes=skipNodes

      //////////////////////////////////////////////////////
      // Functions Handling TAGS_CONTAINER / PARSED_VIEWS //
      //////////////////////////////////////////////////////
      nodeSelector=nodeSelector
      queryNode=queryNode
      queryNodes=queryNodes
      tryDataAccessFunction=tryDataAccessFunction
      getViews=getViews
      getViewNames=getViewNames
      getParsedTags=getParsedTags
      removeViews=removeViews
      removeParsedTags=removeParsedTags
      loadTags=loadTags
      parseTags=parseTags
      resolveViews=resolveViews
      parseViewContent=parseViewContent
   }

   return new JSTagLibrary()
})()
