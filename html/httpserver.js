/**
 * HTTP Server which supports uploading into working directory.
 *
 * The server is implemented in HttpServer.
 * Serving static files is implemented in StaticFileWebService.
 * Uploading is implemented in UploadWebService.
 *
 * To do an upload use PUT /uploads/<filename>.
 * The server stores the file in its starting directory.
 * Sub-directories are not supported.
 *
 * Use bun (download it from https://bun.sh/) to run the server.
 * > bun httpserver.js
 *
 * The server listens on port 9090 but only for local connections from the same host.
 * Set hostname to HttpServer.INADDR_ANY for supporting access from LAN.
 */

class HTTP {
   /////////////////
   // 2XX success //
   /////////////////
   static OK = 200
   static NO_CONTENT = 204
   /////////////////////
   // 3XX redirection //
   /////////////////////
   //////////////////////
   // 4XX client error //
   //////////////////////
   static BAD_REQUEST = 400
   static FORBIDDEN = 403
   static NOT_FOUND = 404
   static METHOD_NOT_ALLOWED = 405
   static CONFLICT = 409
   static UNPROCESSABLE_CONTENT = 422
   //////////////////////
   // 5XX server error //
   //////////////////////
   static INTERNAL_SERVER_ERROR = 500
   static NOT_IMPLEMENTED = 501
   static SERVICE_UNAVAILABLE = 503
}

class HttpResponse {
   static ContentType = {
      JSON: "application/json",
      HTML: "text/html",
      TEXT: "text/plain",
   }
   /**
    * @type {Headers}
    */
   #headers
   constructor() {
      this.#headers = new Headers()
   }
   // Headers support //
   getHeaders() { return this.#headers }
   appendHeader(name, value) { this.#headers.append(name, value) }
   setHeader(name, value) { this.#headers.set(name, value) }
   // Send response //
   sendNoContent() { return new Response(null, { status:HTTP.NO_CONTENT, headers:this.#headers }) }
   /**
    * @param {null|string|ReadableStream|Blob} body
    * @returns {Response}
    */
   sendOK(body=null) { return new Response(body, { status:HTTP.OK, headers:this.#headers  }) }
   /**
    * @param {number} status
    * @param {null|string|ReadableStream|Blob} body
    * @returns {Response}
    */
   sendError(status, body=null) { return new Response(body, { status, headers:this.#headers  }) }
   /**
    * @param {Request} req
    * @param {number} status
    * @param {null|string|ReadableStream|Blob} body
    * @returns {Response}
    */
   static sendError(req, status, body) {
      const response = new HttpResponse()
      response.setHeader("Access-Control-Allow-Methods", req.headers.get("Access-Control-Request-Method") ?? req.method)
      response.setHeader("Access-Control-Allow-Origin", req.headers.get("origin") ?? "")
      response.setHeader("Access-Control-Allow-Headers", req.headers.get("access-control-request-headers") ?? "")
      response.setHeader("Access-Control-Allow-Credentials", "true")
      return response.sendError(status, body)
   }
   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Response}
    */
   static sendMethodNotAllowedError(req, pathname) { return this.sendError(req,HTTP.METHOD_NOT_ALLOWED,`Method ${req.method} not supported for path ${pathname}.`) }
   /**
    * @param {Request} req
    * @returns {Response}
    */
   static sendInvalidURLError(req) { return this.sendError(req,HTTP.BAD_REQUEST,`URL ${req.url} is not valid.`) }
}

class HttpCookie {
   /** @type {string} */
   name
   /** @type {string} */
   value
   /** @type {number} */
   maxage=-1
   /** @type {string} */
   path="/"
   /** @type {boolean} */
   httpOnly=false
   /** @type {boolean} */
   secure=false
   /** @type {null|string} */
   domain=null
   /**
    * @param {string} name
    * @param {string} value
    */
   constructor(name, value) {
      this.name=name
      this.value=value.replaceAll(";","")
   }
   /**
    * @returns {number} Maximum age in seconds (default value is -1). If negative, the cookie is deleted after browser exits.
    */
   getMaxAge() { return this.maxage }
   /**
    * @returns {string} Returns path on the server to which the browser sends the cookie including all subpaths. Default value is "/".
    */
   getPath() { return this.path }
   /**
    * @returns {boolean} Returns true, if cookie is not exposed to client-side scripting code. Default value is false.
    */
   isHttpOnly() { return this.httpOnly }
   /**
    * @returns {boolean} Returns true, if cookie is sent from browser only over "https://"". Default value is false.
    */
   isSecure() { return this.secure }
   /**
    * @param {boolean} httpOnly If true, value is not exposed to client-side scripting code (to mitigate cross-site scripting attacks).
    */
   setHttpOnly(httpOnly) { this.httpOnly=Boolean(httpOnly) }
   /**
    * @param {number} seconds Integer specifying the maximum age of the cookie in seconds; if negative (default), the cookie is deleted after browser exits; if zero, deletes the cookie
    */
   setMaxAge(seconds) { this.maxage=Math.trunc(seconds) }
   /**
    * @param {string} path Sets path on the server to which the browser sends the cookie including all subpaths.
    */
   setPath(path) {
      if (typeof path === "string" && (path.startsWith("/") || path === ""))
         this.path = path.replaceAll(";","")
   }
   /**
    * @param {boolean} secure if true, cookie is sent from browser only over "https://""; if false, sent on any protocol
    */
   setSecure(secure) { this.secure=Boolean(secure) }
   /**
    * @param {string} value The new value of the cookie.
    */
   setValue(value) { this.value=String(value).replaceAll(";","") }
   /**
    * @param {HttpResponse} response Response which receives "set-cookie" header.
    */
   append(response) {
      response.appendHeader("Set-Cookie",this.name + "=" + this.value
         +("; SameSite=Strict")
         +(this.path ? "; Path="+this.path : "")
         +(this.httpOnly ? "; HttpOnly" : "")
         +(this.maxage>=0 ? "; Max-Age="+this.maxage : "")
         +(this.secure ? "; Secure" : "")
      )
   }
}

class HttpSessions {
   /**
    * @type {Map<string,HttpSession>}
    */
   static sessions=new Map()
   /**
    * @type {HttpSession[]}
    */
   static testSessionForExpiration=[]
   /**
    * @returns {string}
    */
   static generateSessionID() { return crypto.randomUUID().replaceAll("-","").toUpperCase() }
   /**
    * @param {HttpService} service
    * @returns {null|HttpSession}
    */
   static getSession(service) {
      this.testExpiration()
      const sessionid = service.getCookie("JS_SESSIONID")
      const session = sessionid ? this.sessions.get(sessionid) : null
      if (session && session.isExpired()) {
         session.invalidate(service)
         return null
      }
      else if (session)
         session.append(service) // renew max-age
      else if (sessionid)
         new HttpSession(sessionid).invalidate(service)
      return session ?? null
   }
   /**
    * @param {HttpService} service
    * @returns {HttpSession}
    */
   static newSession(service) {
      const session = new HttpSession(null)
      session.append(service)
      return session
   }
   /**
    * @returns {undefined}
    */
   static testExpiration() {
      if (HttpSessions.testSessionForExpiration.length) {
         const session = HttpSessions.testSessionForExpiration.pop()
         if (session && session.isExpired()) session.invalidate(null)
      }
      else
         HttpSessions.testSessionForExpiration = [...HttpSessions.sessions.values()]
   }
}

class HttpSession {
   /** @type {string} */
   #sessionid
   /** @type {number} */
   #maxage
   /** @type {number} */
   #expiration
   /** @type {{[attr:string]:any}} */
   #attributes
   /**
    * @param {string?} sessionid
    */
   constructor(sessionid) {
      this.#sessionid = sessionid ? sessionid : HttpSessions.generateSessionID()
      this.#maxage = 3600 // valid for 1 hour
      this.#expiration = this.#maxage + this.timeSeconds()
      this.#attributes = {}
   }
   timeSeconds() { return Date.now()/1000 }
   getID() { return this.#sessionid }
   isValid() { return this.#sessionid !== "" }
   /**
    * @returns {boolean} *true* if session has reached its maxage.
    */
   isExpired() { return this.timeSeconds() > this.#expiration }
   attributes() { return Object.assign({}, this.#attributes) }
   getAttribute(name) { return this.#attributes[String(name)] }
   setAttribute(name, value) { this.#attributes[String(name)] = value }
   append(service) {
      if (!this.#sessionid) return
      const sessionCookie = new HttpCookie("JS_SESSIONID", this.#sessionid)
      sessionCookie.setMaxAge(this.#maxage)
      sessionCookie.append(service.getResponse())
      HttpSessions.sessions.set(this.#sessionid, this)
   }
   /**
    * @param {null|HttpService} service
    */
   invalidate(service) {
      if (!this.#sessionid) return
      const sessionCookie = new HttpCookie("JS_SESSIONID", this.#sessionid)
      sessionCookie.setMaxAge(0)
      service && sessionCookie.append(service.getResponse())
      HttpSessions.sessions.delete(this.#sessionid)
      this.#sessionid = ""
   }
}

class HttpSessionWrapper {
   /** @type {HttpService} */
   service
   /** @type {HttpSession} */
   session
   /**
    * @param {HttpService} service
    * @param {HttpSession} session
    */
   constructor(service, session) {
      this.service = service
      this.session = session
   }
   invalidate() { const result = this.session.invalidate(this.service); this.service.invalidateSession(); return result }
   getID() { return this.session.getID() }
   isValid() { return this.session.isValid() }
   isExpired() { return this.session.isExpired() }
   attributes() { return this.session.attributes() }
   getAttribute(name) { return this.session.getAttribute(name) }
   setAttribute(name, value) { return this.session.setAttribute(name, value) }
}

class HttpService {
   /**
    * The HTTP request for which this service instance should generate an answer
    * @type {Request}
    */
   #request
   /**
    * @type {HttpResponse}
    */
   #response
   /**
    * The URL of the request.
    * @type {URL}
    */
   #url
   /**
    * decoded pathname
    * @type {string}
    */
   #pathname
   /**
    * The client IP address.
    * @type {{address: string, family:"IPv4"|"IPv6", port:number}}
    */
   #clientIP
   /**
    * @type {null|HttpSessionWrapper}
    */
   #session
   /**
    * @param {Request} req
    * @param {URL} url
    * @param {string} pathname
    * @param {{address: string, family:"IPv4"|"IPv6", port:number}} clientIP
    */
   constructor(req, url, pathname, clientIP) {
      this.#request = req
      this.#response = new HttpResponse()
      this.#url = url
      this.#pathname = pathname
      this.#clientIP = clientIP
      const session = HttpSessions.getSession(this)
      this.#session = session ? new HttpSessionWrapper(this, session) : null
   }
   getCookie(name) {
      const reqCookie = this.#request.headers.get("cookie")
      if (reqCookie) {
         const nameEq = name+"="
         const start = reqCookie.startsWith(nameEq) ? 0 : reqCookie.indexOf("; "+nameEq)+2
         if (start !== 1) {
            const vstart = start + nameEq.length
            const end = reqCookie.indexOf(';', vstart)
            return reqCookie.substring(vstart,end > 0 ? end : undefined)
         }
      }
      return null
   }
   getPathname() { return this.#pathname }
   getDirectory() { return this.#pathname.substring(0,this.#pathname.lastIndexOf("/")+1) }
   invalidateSession() {
      const session = this.#session
      if (session) {
         this.#session = null
         session.invalidate()
      }
   }
   createSession() {
      this.#session ??= new HttpSessionWrapper(this, HttpSessions.newSession(this))
      return this.#session
   }
   getSession() { return this.#session }
   getRequest() { return this.#request }
   getResponse() { return this.#response }
   getURL() { return this.#url }
   setContentType(value) { this.#response.setHeader("Content-Type", value) }
   setResponseHeader(name, value) { this.#response.setHeader(name, value) }
   appendResponseHeader(name, value) { this.#response.appendHeader(name, value) }
   getAllowOptions(req, pathname) {
      // method, origin, headers could null
      return { pathname, method:req.headers.get("Access-Control-Request-Method"), origin:req.headers.get("origin"), headers:req.headers.get("access-control-request-headers") }
   }
   /////////////////////////////
   // Generic Service Methods //
   /////////////////////////////
   async serve() {
      const req = this.#request
      if (req.method !== "OPTIONS") {
         const site = req.headers.get("Sec-Fetch-Site")
         const sameOrigin = ("none" === site || "same-origin" === site)
         if (!sameOrigin) {
            const options = this.getAllowOptions(req, this.#pathname)
            const origin = this.allowOrigin(options)
            this.setResponseHeader("Access-Control-Allow-Origin", origin)
            this.allowCredentials(options) && this.setResponseHeader("Access-Control-Allow-Credentials", "true")
            if (origin !== options.origin) {
               return this.getResponse().sendError(HTTP.FORBIDDEN, `Access forbidden.`)
            }
         }
      }
      switch(req.method) {
         case "GET": return this.doGet(req, this.#pathname)
         case "HEAD": return this.doHead(req, this.#pathname)
         case "OPTIONS": return this.doOptions(req, this.#pathname)
         case "POST": return this.doPost(req, this.#pathname)
         case "PUT": return this.doPut(req, this.#pathname)
         default: return this.doUnknown(req, this.#pathname)
      }
   }
   async doOptions(req, pathname) {
      const options = this.getAllowOptions(req, pathname)
      if (options.method) {
         this.setResponseHeader("Access-Control-Allow-Methods", this.allowMethods(options))
         this.setResponseHeader("Access-Control-Allow-Origin", this.allowOrigin(options))
         this.setResponseHeader("Access-Control-Allow-Headers", this.allowHeaders(options))
         this.allowCredentials(options) && this.setResponseHeader("Access-Control-Allow-Credentials", "true")
      }
      else {
         this.setResponseHeader("Allow", this.allowMethods(options))
      }
      return this.getResponse().sendNoContent()
   }
   //////////////////////////////////////////
   // Cors Support / overwrite in sub type //
   //////////////////////////////////////////
   allowMethods(options) { return "DELETE,GET,HEAD,OPTIONS,POST,PUT" /*"" no methods*/}
   allowOrigin(options) { return options.origin /*"" no origin*/}
   allowHeaders(options) { return options.headers /*"" no headers*/}
   allowCredentials(options) { return true /*false no credentials*/}
   /////////////////////////////////////////////
   // Service Methods / overwrite in sub type //
   /////////////////////////////////////////////
   async doGet(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
   async doHead(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
   async doPost(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
   async doPut(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
   async doUnknown(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
}

class HttpServiceDispatcher {
   /**
    * @type {Map<string,{service:null|(typeof HttpService)}>}
    */
   pathMap
   /**
    * @type {typeof HttpService}
    */
   rootService
   /**
    * @param {typeof HttpService} rootService
    * @param  {...[string,typeof HttpService]} path2Service
    */
   constructor(rootService, ...path2Service) {
      this.pathMap = new Map()
      this.rootService = rootService
      for (const entry of path2Service) {
         const path = entry[0]
         const service = entry[1]
         let pathPrefix = ""
         for (const dir of path.split("/")) {
            if (dir === "") continue
            pathPrefix += "/"+dir
            if (!this.pathMap.get(pathPrefix))
               this.pathMap.set(pathPrefix, {service:null})
         }
         if (this.pathMap.get(pathPrefix)?.service) throw Error(`HttpServiceDispatcher error: defined more than one service for path ${pathPrefix}`)
         this.pathMap.set(pathPrefix, {service})
      }
   }
   /**
    * @param {string} path A directory must always end in "/". Accessing "/dir/" or "/dir/filename" works but "/dir" returns service for root.
    * @returns {typeof HttpService} rootService
    */
   getService(path) {
      let foundService = this.rootService
      for (let pos=path.indexOf("/",1); pos > 0; pos=path.indexOf("/",pos+1)) {
         const subpath = this.pathMap.get(path.substring(0,pos))
         if (!subpath) break
         if (subpath.service) foundService=subpath.service
      }
      return foundService
   }
}

class HttpServer {
   static INADDR_ANY = "0.0.0.0" /*unsafe*/ // safer variants "localhost", "127.0.0.1"
   static IN6ADDR_ANY = "::" /*unsafe*/ // safer variants "localhost", "::1"
   #hostname
   #port
   #serviceDispatcher
   constructor(serviceDispatcher, hostname="localhost", port=8080) {
      this.#hostname = hostname
      this.#port = port
      this.#serviceDispatcher = serviceDispatcher
   }
   get port() { return this.#port }
   get hostname() { return this.#hostname }

   run() {
      Bun.serve({ port:this.#port, hostname:this.#hostname, fetch:this.serve.bind(this) })
      return this
   }

   sendInternalServerError(req, e) {
      console.log("===== start exception =====")
      console.log(e)
      console.log("===== end exception =====")
      return HttpResponse.sendError(req,HTTP.INTERNAL_SERVER_ERROR,"Service failed.")
   }

   async serve(req, server) {
      try {
         const clientIP = server.requestIP(req)
         const url = new URL(req.url)
         const pathname = decodeURIComponent(url.pathname)
         // console.log("headers",req.headers)
         // console.log("req", req)
         // console.log("server", server)
         try {
            const service = this.#serviceDispatcher.getService(pathname)
            console.log(`${clientIP.address}:${clientIP.port}  ${req.method}  ${pathname}`)
            if (service)
               return new service(req, url, pathname, clientIP).serve().catch(e => this.sendInternalServerError(req,e))
            else
               return HttpResponse.sendError(req,HTTP.NOT_FOUND,`Path ${pathname} could not be mapped to a service.`)
         }
         catch(e) {
            return this.sendInternalServerError(req,e)
         }
      }
      catch(e) {
         return HttpResponse.sendInvalidURLError(req)
      }
   }
}

class StaticFileWebService extends HttpService {
   async doGet(req, pathname) {
      const file = Bun.file("."+pathname)

      console.log("id=",this.createSession().getID())

      if (await file.exists()) {
         const headers = this.getResponse().getHeaders()
         this.setContentType(file.type)
         console.log("response headers",headers)
         return this.getResponse().sendOK(file)
      }

      return this.getResponse().sendError(HTTP.NOT_FOUND,"file not found: "+pathname)
   }
}

class UploadWebService extends HttpService {

   async doPut(req, pathname) {
      const name = pathname.substring(9).replaceAll(/[^-_.;:?a-zA-Z0-9üöäÖÄÜß]/g,"")
      const file = Bun.file(name)

      if (!name) {
         return this.getResponse().sendError(HTTP.BAD_REQUEST, pathname.length <= 9 ? `Empty file name` : `Invalid file name`)
      }
      if (await file.exists()) {
         return this.getResponse().sendError(HTTP.CONFLICT, `File exists »${name}«`)
      }

      console.log("upload »"+name+"«")
      return Bun.write(file, req)
      .then( (/*writtenBytes*/) => this.getResponse().sendOK(`Saved file »${name}«`))
      .catch( (/*e*/) => this.getResponse().sendError(HTTP.CONFLICT, `Error writing file »${name}«`))
   }
}

const services = new HttpServiceDispatcher(StaticFileWebService, ["/uploads",UploadWebService])
const hs = new HttpServer(services, "localhost" /*HttpServer.INADDR_ANY*/, 9090).run()
console.log("Started HttpServer listening on "+hs.hostname+":"+hs.port)
