<script>
   // dom library
   function dom(selector) {
      if (!(this instanceof dom)) {
         return new dom(selector)
      }
      this.elems =  ( typeof selector === "string" ? document.querySelectorAll(selector)
                   : selector.addEventListener || !selector.length ? [ selector ]
                   : selector )
   }
   Object.assign(dom, {
      // must be called every time an iframe is loaded
      adaptIFrameHeight(iframe) {
         const content = iframe.contentDocument.documentElement
         iframe.style.height = content.offsetHeight + "px" // reduce size to content size
         iframe.style.height = content.scrollHeight + "px" // add margins
         const scroll_bar_height = content.scrollHeight - content.clientHeight
         if (0 < scroll_bar_height && scroll_bar_height < 40) {
            iframe.style.height = (content.scrollHeight + scroll_bar_height) + "px"
         }
      },
      // must be called every time an iframe is loaded
      observeIFrameHeight(iframe) {
         const content = iframe.contentDocument.documentElement
         const keys = [ "offsetHeight", "offsetHeight", "clientHeight" ]
         let old = {}
         keys.forEach( key => old[key] = content[key])
         let timer = setInterval( () => { 
            if (timer && keys.some( key => content[key] != old[key])) {
               this.adaptIFrameHeight(iframe); 
               keys.forEach( key => old[key] = content[key])
            }
         }, 100)
         this.on(iframe.contentWindow, "unload", () => { 
            clearInterval(timer)
            timer = null
         })
      },
      on(el, event, eventHandler, optional) {
         el.addEventListener(event, eventHandler, optional)
      },
   })
   Object.assign(dom.prototype, {
      foreach(callback) {  for (const el of this.elems) { callback(el) } return this },
      get(i) { return this.elems[i] },
      on(event, eventHandler, optional) { return this.foreach( (el) => { el.addEventListener(event, eventHandler, optional) } ) },
      adaptIFrameHeight() { this.constructor.adaptIFrameHeight(this.get(0)) }
   })
</script>

<script>
   // example usage
   var iframe = document.getElementsByTagName("iframe")[0]
   dom.on(iframe, "load", function() {
      console.log("=== onload ====")
      console.log("body offsetHeight", iframe.contentDocument.body.offsetHeight)
      console.log("body scrollHeight", iframe.contentDocument.body.scrollHeight)
      console.log("body clientHeight", iframe.contentDocument.body.clientHeight)
      console.log("html offsetHeight", iframe.contentDocument.documentElement.offsetHeight)
      console.log("html scrollHeight", iframe.contentDocument.documentElement.scrollHeight)
      console.log("html clientHeight", iframe.contentDocument.documentElement.clientHeight)
      dom.adaptIFrameHeight(iframe)
      dom.observeIFrameHeight(iframe)
      setTimeout( function() {
         console.log("html offsetHeight", iframe.contentDocument.documentElement.offsetHeight)
         console.log("html scrollHeight", iframe.contentDocument.documentElement.scrollHeight)
         console.log("html clientHeight", iframe.contentDocument.documentElement.clientHeight)
         console.log("html offsetWidth", iframe.contentDocument.documentElement.offsetWidth)
         console.log("html scrollWidth", iframe.contentDocument.documentElement.scrollWidth)
         console.log("html clientWidth", iframe.contentDocument.documentElement.clientWidth)
      }, 100)
   })
</script>
