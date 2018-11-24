<script>
// js-component-library
// getter proto === Object.getPrototypeOf
// function c === createElement
// function q === querySelectorAll
// function qs === querySelector
// function qid === getElementById
// function qtag === getElementsByTagName
// function on === addEventListener
// focument.jscomponentlib() === returns getElementsByTagName("script")[i] with value i so that its innerText contains "// js-component-library"
(function () {
   Object.defineProperty(
      Object.getPrototypeOf({}), 'proto', { get: function() { return Object.getPrototypeOf(this) }, enumerable: true, configurable: false }
   );
   (function(a) {
      for (_ of a) {
         if (_.createElement) _.c = _.createElement
         if (_.createTextNode) _.ct = document.createTextNode
         if (_.querySelectorAll) _.q = _.querySelectorAll
         if (_.querySelector) _.qs = _.querySelector
         if (_.getElementById) _.qid = _.getElementById
         if (_.getElementsByTagName) _.qtag = _.getElementsByTagName
         if (_.addEventListener) _.on = _.addEventListener
      }
   })([document.proto,window.proto,document.createElement("iframe").proto.proto]);
   document.proto.jscomponentlib=function() {
      return this.qtag("script").find( s => {
         return s.innerText.substring(0,30).search("// js-component-library")>=0;
      })
   };
   // Extend iframe.proto with functions:
   // - adaptHeight: changes height of iframe to size of contained document
   // - observeHeight: adds timer to window of iframe to adapt height on size change
   (function (_) {
   // Must be called every time an iframe is loaded.
   // Only supports iframes with same origin!
      _.installlib=function() {
         const s=this.contentDocument.createElement("script")
         s.appendChild(this.contentDocument.createTextNode(document.jscomponentlib().innerText))
         this.contentDocument.documentElement.appendChild(s)
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
   })(Object.getPrototypeOf(document.c("iframe")));
   // Extends NodeListPrototype and HTMLCollectionPrototype with functions:
   // - forEach(callback): Iterates every contained node and calls callback with node as parameter
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
   })([document.c("iframe").q("iframe").proto,document.c("iframe").qtag("iframe").proto]);
})();
</script>

<script>
   // example usage
   var iframe = document.qtag("iframe")[0]
   var div = document.qtag("div")[0]

   window.on("message", receiveMessage);

   function receiveMessage(event)
   {
      console.log("event.origin", event.origin, event)
   }

   div.on("scroll", function(e) {
      console.log("div", "" + this, this.scrollLeft, e)
      console.log("div offsetWidth", this.offsetWidth)
      console.log("div scrollWidth", this.scrollWidth)
      console.log("div clientWidth", this.clientWidth)
      console.log("div scrollHeight", this.scrollHeight)
      console.log("div clientHeight", this.clientHeight)
      console.log("div offsetHeight", this.offsetHeight)
   })

   iframe.contentWindow.addEventListener("resize", function() {
      console.log("=== on resize ===")
   })

   iframe.on("load", function() {
      console.log("=== onload ====")
      console.log("body offsetHeight", iframe.contentDocument.body.offsetHeight)
      console.log("body scrollHeight", iframe.contentDocument.body.scrollHeight)
      console.log("body clientHeight", iframe.contentDocument.body.clientHeight)
      console.log("html offsetHeight", iframe.contentDocument.documentElement.offsetHeight)
      console.log("html scrollHeight", iframe.contentDocument.documentElement.scrollHeight)
      console.log("html clientHeight", iframe.contentDocument.documentElement.clientHeight)
      iframe.adaptHeight()
      iframe.observeHeight()
      setTimeout( function() {
         console.log("html offsetHeight", iframe.contentDocument.documentElement.offsetHeight)
         console.log("html scrollHeight", iframe.contentDocument.documentElement.scrollHeight)
         console.log("html clientHeight", iframe.contentDocument.documentElement.clientHeight)
         console.log("html offsetWidth", iframe.contentDocument.documentElement.offsetWidth)
         console.log("html scrollWidth", iframe.contentDocument.documentElement.scrollWidth)
         console.log("html clientWidth", iframe.contentDocument.documentElement.clientWidth)
      }, 100)
   })

   iframe.installlib()
</script>
