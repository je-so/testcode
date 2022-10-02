// (c) 2022 Jörg Seebohn
// Supports automatically resizing of height of embedded iframe
// Must be included in parent html and iframe html
(function() {
   const config={log:true,scrollyoffset:0}, configSymbol=Symbol("iframe")
   function getConfig(str) {
      const config={}
      str=typeof str === "string" ? str.replaceAll(" ","") : ""
      config.log=str.indexOf("log:") >= 0 && str.substr(str.indexOf("log:")+4).startsWith("true;")
      config.scrollyoffset=str.indexOf("scrollyoffset:") >= 0 && parseInt(str.substr(str.indexOf("scrollyoffset:")+14))
      if (! Number.isInteger(config.scrollyoffset)) config.scrollyoffset=0
      return config
   }
   function log(config,...values) { if (!config || config.log) console.log("iframe",...values) }
   //////////////////////////
   // handling iframe content
   //////////////////////////
   if (window.parent !== window.self) {
      var setHeight=0,ydiff=0,oldTimeout;
      function getContentHeight() {
         const ymargin=((cstyle)=>parseInt(cstyle.marginTop) + parseInt(cstyle.marginBottom))(getComputedStyle(document.documentElement))
         return ymargin+document.documentElement.offsetHeight-document.body.offsetHeight+document.body.scrollHeight
      }
      function startTimer(timeout) {
         if (typeof oldTimeout !== "undefined") clearTimeout(oldTimeout)
         oldTimeout=setTimeout(updateHeight,timeout)
      }
      function onResizeContent() {
         // the difference between the set height and the client height which does not contain height of scrollbar results in height of scrollbar
         const ydiff2=Math.max(setHeight-document.documentElement.clientHeight+1,0)
         ydiff=(ydiff2<ydiff-2 || ydiff<ydiff2 ? ydiff2 : ydiff)
         log(config,"onResizeContent:",{setHeight,clientHeight:document.documentElement.clientHeight,ydiff})
         startTimer(0)
      }
      function updateHeight() {
         const scrollX=(document.documentElement.clientWidth !== document.documentElement.scrollWidth)
         const height=getContentHeight()+(scrollX?ydiff:0)
         const tooSmall=(document.documentElement.clientHeight !== document.documentElement.scrollHeight)
         const tooBig=document.documentElement.scrollHeight>height+2
         if (tooSmall || tooBig) {
            setHeight=height
            log(config,"updateHeight:",{setHeight,clientHeight:document.documentElement.clientHeight,scrollHeight:document.documentElement.scrollHeight,ydiff,scrollX})
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
               case "iframe-set-config": startTimer(0); Object.assign(config,getConfig(event.data.value)); break
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
               log(iframe[configSymbol],"message:",event.data)
               switch (event.data.type) {
               case "iframe-height":
                  if(getComputedStyle(iframe).height === event.data.value+"px")
                     event.source.postMessage({type: "iframe-resize"}, "*")
                  else
                     iframe.style.height=event.data.value+"px"
                  break;
               case "iframe-get-config": iframe[configSymbol]=getConfig(iframe.dataset["configResize"]); event.source.postMessage({type: "iframe-set-config", value:iframe.dataset["configResize"]}, "*"); break
               case "iframe-scroll-y": window.scrollTo(document.documentElement.scrollLeft,iframe.getBoundingClientRect().top+event.data.value+document.documentElement.scrollTop+iframe[configSymbol].scrollyoffset); break
               default: log("error:","unknown message"); break
               }
            }
         })
      }
   })
})();
