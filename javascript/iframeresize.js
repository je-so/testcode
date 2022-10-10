// (c) 2022 Jörg Seebohn
// Supports automatically resizing of height of embedded iframe
// Must be included in iframe container and iframe content html
(function() {
   const config={log:true,scrollyoffset:0}, configAttr=Symbol("iframe")
   function parseConfig(str) {
      const config={}
      str=typeof str === "string" ? str.replaceAll(" ","") + ";" : ""
      config.log=str.indexOf("log:") >= 0 && str.substr(str.indexOf("log:")+4).startsWith("true;")
      config.scrollyoffset=str.indexOf("scrollyoffset:") >= 0 && parseInt(str.substr(str.indexOf("scrollyoffset:")+14)) || 0
      return config
   }
   function log(config,...values) { if (!config || config.log) console.log("iframe",`(ms:${(""+(new Date().valueOf()%10000)).padStart(4,"0")})`,...values) }
   //////////////////////////
   // handling iframe content
   //////////////////////////
   if (window.parent !== window.self) {
      var contentHeightDiff=0,setHeight=0,setHeightDiff=0,updateInProgress=false,oldTimeout
      function getContentHeight() {
         const ymargin=((cstyle)=>parseInt(cstyle.marginTop) + parseInt(cstyle.marginBottom))(getComputedStyle(document.documentElement))
         return ymargin+document.documentElement.offsetHeight-document.body.offsetHeight+document.body.scrollHeight
      }
      function getTooSmall() { return document.documentElement.clientHeight !== document.documentElement.scrollHeight }
      function needUpdate(height) {
         const tooBig=document.documentElement.scrollHeight>height+2
         return tooBig || getTooSmall()
      }
      function onResizeContent() {
         const setHeightDiff2=Math.max(setHeight-document.documentElement.clientHeight+getTooSmall(),0)
         setHeightDiff=(setHeightDiff2<setHeightDiff-2 || setHeightDiff<setHeightDiff2 ? setHeightDiff2 : setHeightDiff)
         log(config,"onResizeContent:",{setHeight,clientHeight:document.documentElement.clientHeight,scrollHeight:document.documentElement.scrollHeight,setHeightDiff})
         // Even if startTimer clears any running timer and starts a new one
         // it is possible that the cleared timer had already fired and its event is waiting for processing in the global event queue
         // => state updateInProgress is used to prevent any new onTimer/send "iframe-height" cycle during an already running cycle
         updateInProgress=false; startTimer(0);
      }
      function onTimer() {
         if (updateInProgress) return
         const height=getContentHeight()+contentHeightDiff
         if (needUpdate(height)) {
            updateInProgress=true // wait for onResizeContent
            setHeight=height + setHeightDiff
            log(config,"updateHeight:",{setHeight,clientHeight:document.documentElement.clientHeight,scrollHeight:document.documentElement.scrollHeight,setHeightDiff})
            window.parent.postMessage({type:"iframe-height",value:setHeight},"*") // trigger onResizeContent
         }
         else
            startTimer(50)
      }
      function startTimer(timeout) {
         if (oldTimeout !== undefined) clearTimeout(oldTimeout)
         oldTimeout=setTimeout(onTimer,timeout)
      }
      function init() {
         // body style ensures that body.scrollHeight returns height also for position:absolute child elements of body
         document.body.style.position="relative"
         window.addEventListener("resize", onResizeContent)
         window.addEventListener("message", (event) => {
            if (window.parent === event.source && typeof event.data.type === "string" && event.data.type.startsWith("iframe-")) {
               log(config,"message:",event.data)
               switch (event.data.type) {
               case "iframe-resize": onResizeContent(); if(getTooSmall()) contentHeightDiff=Math.max(document.documentElement.scrollHeight-getContentHeight()+1,0); break
               case "iframe-set-config": startTimer(0); Object.assign(config,parseConfig(event.data.value)); break
               default: log(config,"error:","unknown message"); break
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
               case "iframe-height": // generates real or simulated resize event -> onResizeContent
                  if(getComputedStyle(iframe).height === event.data.value+"px")
                     event.source.postMessage({type: "iframe-resize"}, "*") // simulated event
                  else
                     iframe.style.height=event.data.value+"px" // height change -> generates "resize" event
                  break;
               case "iframe-get-config":
                  iframe[configAttr] = parseConfig(iframe.dataset["configResize"])
                  event.source.postMessage({type: "iframe-set-config", value:iframe.dataset["configResize"]}, "*")
                  break
               case "iframe-scrollto": // { x: 0 /* left */ | undefined /* unchanged */, y: 0 /* top */, smooth: true|false|undefined }
                  const cstyle=getComputedStyle(iframe)
                  if ("x" in event.data) event.data.x+=iframe.getBoundingClientRect().left + parseInt(cstyle.borderLeftWidth) + parseInt(cstyle.paddingLeft)
                  if ("y" in event.data) event.data.y+=iframe.getBoundingClientRect().top + iframe[configAttr].scrollyoffset + parseInt(cstyle.borderTopWidth) + parseInt(cstyle.paddingTop)
                  if (window.parent != window.self)
                     window.parent.postMessage(event.data, "*") // route to next parent
                  else
                     window.scrollTo({left:document.documentElement.scrollLeft + (event.data.x || 0), top: document.documentElement.scrollTop + (event.data.y || 0), behavior: event.data.smooth ? "smooth" : "auto" });
                  break
               default: log(iframe[configAttr],"error:","unknown message"); break
               }
            }
         })
      }
   })
})();
