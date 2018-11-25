(function () { const VERSION="component-library 0.1";
const SCRIPT=document.currentScript;
/* Copyright 2018 Jörg Seebohn
---------------------------
- property : alias to api -
proto : Object.getPrototypeOf
c()   : createElement
q()   : querySelectorAll
qs()  : querySelector
qid() : getElementById
qtag(): getElementsByTagName
on()  : addEventListener
------------
- document -
- property : description -
componentlib() : returns document.currentScript which is a reference to the script tag which hosts this file
---------------------
- NodeListPrototype -
- HTMLCollectionPrototype -
- property : description -
forEach(callback) : Iterates every contained node and calls callback with node as parameter
find(callback)    : Iterates contained nodes until callback returns true and returns this element else returns undefined
------------------------ */
   Object.defineProperty(
      Object.getPrototypeOf({}), 'proto', { get: function() { return Object.getPrototypeOf(this) }, enumerable: true, configurable: false }
   );
   const HTMLDocumentPrototype = document.proto;
   const WindowPrototype = window.proto;
   const HTMLElementPrototype = document.createElement("iframe").proto.proto;
   (function (a) {
      for (_ of a) {
         if (_.createElement) _.c = _.createElement
         if (_.createTextNode) _.ct = _.createTextNode
         if (_.querySelectorAll) _.q = _.querySelectorAll
         if (_.querySelector) _.qs = _.querySelector
         if (_.getElementById) _.qid = _.getElementById
         if (_.getElementsByTagName) _.qtag = _.getElementsByTagName
         if (_.addEventListener) _.on = _.addEventListener
      }
   })([HTMLDocumentPrototype, WindowPrototype, HTMLElementPrototype]);
   HTMLDocumentPrototype.componentlib=function() {
      return SCRIPT
   };
   // Extend iframe.proto with functions (only supports iframes with same origin):
   // - installlib: adds this library to the internal iframe
   // - adaptHeight: changes height of iframe to size of contained document
   // - observeHeight: adds timer to window of iframe to adapt height on size change
   const HTMLIFrameElementPrototype = document.c("iframe").proto;
   (function (_) {
      // Must be called every time an iframe is loaded.
      _.installlib=function() {
         const s=document.c("script")
         const p=new Promise((res,rej) => s.on("load", () => res(s)))
         s.setAttribute("src", SCRIPT.src)
         this.contentDocument.documentElement.appendChild(s)
         return p
      };
      _.adaptHeight=function() {
         const content = this.contentDocument.documentElement
         this.style.height = content.offsetHeight + "px" // reduce size to content size
         this.style.height = content.scrollHeight + "px" // add margins
         const scroll_bar_height = content.scrollHeight - content.clientHeight
         if (0 < scroll_bar_height) {
            this.style.height = (content.scrollHeight + scroll_bar_height) + "px"
         }
      };
      _.observeHeight=function(milliseconds=100) {
         const content = this.contentDocument.documentElement
         const keys = [ "offsetHeight", "offsetHeight", "clientHeight" ]
         const old = {}
         keys.forEach( key => old[key] = content[key])
         const timer = setInterval( () => {
            if (keys.some( key => content[key] != old[key])) {
               this.adaptHeight();
               keys.forEach( key => old[key] = content[key])
            }
         }, milliseconds)
         this.contentWindow.addEventListener("unload", () => {
            clearInterval(timer)
         })
      };
   })(HTMLIFrameElementPrototype);
   const NodeListPrototype = document.c("iframe").q("iframe").proto;
   const HTMLCollectionPrototype = document.c("iframe").qtag("iframe").proto;
   (function (a) {
      for (_ of a) {
         _.forEach=function(fct) {
            for (let i=0; i<this.length; ++i) {
               fct(this[i])
            }
         };
         _.find=function(fct) {
            for (let i=0; i<this.length; ++i) {
               if(fct(this[i])) return this[i];
            }
         };
      }
   })([NodeListPrototype, HTMLCollectionPrototype]); 
})();
