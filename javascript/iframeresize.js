// (c) 2022 Jörg Seebohn
// Supports automatically resizing of height of embedded iframe
// Must be included in parent html and iframe html
(function() {
   const config={log:true,scrollyoffset:0}, configAttr=Symbol("iframe")
   function parseConfig(str) {
      const config={}
      str=typeof str === "string" ? str.replaceAll(" ","") : ""
      config.log=str.indexOf("log:") >= 0 && str.substr(str.indexOf("log:")+4).startsWith("true;")
      config.scrollyoffset=str.indexOf("scrollyoffset:") >= 0 && parseInt(str.substr(str.indexOf("scrollyoffset:")+14))
      config.scrollyoffset=((nr)=>Number.isInteger(nr) && nr || 0)(config.scrollyoffset)
      return config
   }
   function log(config,...values) { if (!config || config.log) console.log("iframe",...values) }
   //////////////////////////
   // handling iframe content
   //////////////////////////
   if (window.parent !== window.self) {
      var setHeight=0,sbarHeight=0,oldTimeout;
      function startTimer(timeout) {
         if (oldTimeout !== undefined) clearTimeout(oldTimeout)
         oldTimeout=setTimeout(updateHeight,timeout)
      }
      function getContentHeight() {
         const ymargin=((cstyle)=>parseInt(cstyle.marginTop) + parseInt(cstyle.marginBottom))(getComputedStyle(document.documentElement))
         return ymargin+document.documentElement.offsetHeight-document.body.offsetHeight+document.body.scrollHeight
      }
      function onResizeContent() {
         // the difference between the set height and the client height which does not contain height of scrollbar results in height of scrollbar
         const sbarHeight2=Math.max(setHeight-document.documentElement.clientHeight+1,0)
         sbarHeight=(sbarHeight2<sbarHeight-2 || sbarHeight<sbarHeight2 ? sbarHeight2 : sbarHeight)
         log(config,"onResizeContent:",{setHeight,clientHeight:document.documentElement.clientHeight,sbarHeight})
         startTimer(0)
      }
      function updateHeight() {
         const scrollX=(document.documentElement.clientWidth !== document.documentElement.scrollWidth)
         const height=getContentHeight()+(scrollX?sbarHeight:0)
         const tooSmall=(document.documentElement.clientHeight !== document.documentElement.scrollHeight)
         const tooBig=document.documentElement.scrollHeight>height+2
         if (tooSmall || tooBig) {
            setHeight=height
            log(config,"updateHeight:",{setHeight,clientHeight:document.documentElement.clientHeight,scrollHeight:document.documentElement.scrollHeight,sbarHeight,scrollX})
            window.parent.postMessage({type:"iframe-height",value:setHeight},"*")
         } else
            startTimer(50)
      }
      function init() {
         // body style ensures that body.scrollHeight returns height also of absolute positioned elements within body
         document.body.style.overflow="visible";document.body.style.height="20px";document.body.style.position="relative"
         window.addEventListener("resize", onResizeContent)
         window.addEventListener("message", (event) => {
            if (typeof event.data.type === "string" && event.data.type.startsWith("iframe-")) {
               log(config,"message:",event.data)
               switch (event.data.type) {
               case "iframe-resize": onResizeContent(); break
               case "iframe-set-config": startTimer(0); Object.assign(config,parseConfig(event.data.value)); break
               default: log("error:","unknown message"); break
               }
            }
         })
         window.parent.postMessage({type:"iframe-get-config"},"*")
      }
      ("complete" === document.readyState ? init : window.addEventListener)("load",init)
   }
   ////////////////////////////
   // handling parent of iframe
   ////////////////////////////
   window.addEventListener("message", (event) => {
      if (typeof event.data.type === "string" && event.data.type.startsWith("iframe-")) {
         [...document.querySelectorAll("iframe")].forEach( iframe => {
            if (iframe.contentWindow === event.source) {
               log(iframe[configAttr],"message:",event.data)
               switch (event.data.type) {
               case "iframe-height": // simulates or generates resize event -> onResizeContent
                  if(getComputedStyle(iframe).height === event.data.value+"px")
                     event.source.postMessage({type: "iframe-resize"}, "*") // simulates event
                  else
                     iframe.style.height=event.data.value+"px" // height change -> event generation
                  break;
               case "iframe-get-config": iframe[configAttr]=parseConfig(iframe.dataset["configResize"]); event.source.postMessage({type: "iframe-set-config", value:iframe.dataset["configResize"]}, "*"); break
               case "iframe-scroll-y": window.scrollTo(document.documentElement.scrollLeft,iframe.getBoundingClientRect().top+event.data.value+document.documentElement.scrollTop+iframe[configAttr].scrollyoffset); break
               default: log("error:","unknown message"); break
               }
            }
         })
      }
   })
})();
