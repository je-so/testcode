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
// import { stat } from "node:fs/promises";

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
   /**
    * @type {Map<string,HttpCookie>}
    */
   #cookies
   constructor() {
      this.#headers = new Headers()
      this.#cookies = new Map()
   }
   /**
    * Adds cookie to response. Overwrites previously set cookie value.
    * @param {HttpCookie} cookie
    */
   setCookie(cookie) { this.#cookies.set(cookie.getName(), cookie) }
   // Headers support //
   /**
    * @return {Headers}
    */
   getHeaders() { return this.#headers }
   /**
    * @param {string} name
    * @param {string} value
    */
   appendHeader(name, value) { this.#headers.append(name, value) }
   /**
    * @param {string} name
    */
   deleteHeader(name) { this.#headers.delete(name) }
   /**
    * @param {string} name
    * @param {string} value
    */
   setHeader(name, value) { this.#headers.set(name, value) }
   // Send response //
   /**
    * @returns {Response}
    */
   sendNoContent() { return this.sendStatus(HTTP.NO_CONTENT, null) }
   /**
    * @param {null|string|ReadableStream|Blob} body
    * @returns {Response}
    */
   sendOK(body=null) { return this.sendStatus(HTTP.OK, body) }
   /**
    * @param {number} status
    * @param {null|string|ReadableStream|Blob} body
    * @returns {Response}
    */
   sendStatus(status, body=null) {
      for (const cookie of this.#cookies.values()) {
         cookie.append(this)
      }
      this.#cookies.clear()
      return new Response(body, { status, headers:this.#headers })
   }
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
      return response.sendStatus(status, body)
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
    * @param {Request} request
    * @param {string} name
    * @returns {null|string} Value of cookie.
    */
   static fromRequest(request, name) {
      const reqCookie = request.headers.get("cookie")
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

   /**
    * @param {string} name
    * @param {string} value
    */
   constructor(name, value) {
      this.name=name
      this.value=value.replaceAll(";","")
   }
   /**
    * @returns {string} Name of cookie.
    */
   getName() { return this.name }
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
    * @type {HttpSession[]}
    */
   testSessionForExpiration=[]
   /**
    * @returns {string}
    */
   static generateSessionID() { return crypto.randomUUID().replaceAll("-","").toUpperCase() }
   /**
    * @param {string} id
    * @returns {HttpCookie}
    */
   static newSessionCookie(id) { return new HttpCookie("JS_SESSIONID",id) }
   /**
    * @param {Request} request
    * @returns {null|string}
    */
   sessionIDFromRequest(request) {
      this.testExpiration()
      const sessionid = HttpCookie.fromRequest(request,"JS_SESSIONID")
      return sessionid
   }
   /**
    * @returns {undefined}
    */
   testExpiration() {
      if (this.testSessionForExpiration.length) {
         const session = this.testSessionForExpiration.pop()
         if (session && session.isExpired()) session.invalidate()
      }
      else
         this.testSessionForExpiration = [...(this.getSessions()?.values() ?? [])]
   }
   ////////////////////////////
   // Overwritten in Subtype //
   ////////////////////////////
   /**
    * @returns {Map<string,HttpSession>}
    */
   getSessions() { throw Error(`getSessions not implemented.`) }
}

class HttpServiceSessions extends HttpSessions {
   /**
    * @type {Map<string,HttpServiceSession>}
    */
   sessions=new Map()
   /**
    * @returns {Map<string,HttpServiceSession>}
    */
   getSessions() { return this.sessions }
   /**
    * @param {Request} request
    * @returns {null|HttpServiceSession}
    */
   fromRequest(request) {
      const sessionid = this.sessionIDFromRequest(request)
      const session = sessionid ? this.sessions.get(sessionid) : null
      return session ?? null
   }
   /**
    * @param {HttpServiceSession} session
    */
   register(session) { this.sessions.set(session.ID(), session) }
   /**
    * @param {HttpServiceSession} session
    */
   unregister(session) { this.sessions.delete(session.ID()) }
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
    * @param {string} [sessionid]
    */
   constructor(sessionid) {
      this.#sessionid = sessionid ? sessionid : HttpSessions.generateSessionID()
      this.#maxage = 3600 // valid for 1 hour
      this.#expiration = this.#maxage + this.timeSeconds()
      this.#attributes = {}
   }
   timeSeconds() { return Date.now()/1000 }
   ID() { return this.#sessionid }
   isValid() { return this.#sessionid !== "" }
   /**
    * @returns {boolean} *true* if session has reached its maxage.
    */
   isExpired() { return this.timeSeconds() > this.#expiration }
   attributes() { return Object.assign({}, this.#attributes) }
   /**
    * @param {string} name
    * @returns {any} Either value else undefined.
    */
   attribute(name) { return this.#attributes[name] }
   /**
    * @param {string} name
    * @param {any} value
    */
   setAttribute(name, value) { this.#attributes[name] = value }
   /**
    * @returns {void}
    */
   invalidate() {
      if (!this.#sessionid) return
      this.#sessionid = ""
   }
   /**
    * @param {HttpResponse} response
    */
   attachTo(response) {
      if (!this.#sessionid) return
      const sessionCookie = HttpSessions.newSessionCookie(this.#sessionid)
      sessionCookie.setMaxAge(this.#maxage)
      response.setCookie(sessionCookie)
   }
   /**
    * @param {HttpResponse} response
    */
   static removeFrom(response) {
      const sessionCookie = HttpSessions.newSessionCookie("")
      sessionCookie.setMaxAge(0)
      response.setCookie(sessionCookie)
   }
}

class HttpServiceSession extends HttpSession {
   static sessions = new HttpServiceSessions()
   /**
    * @param {Request} request
    * @returns {null|HttpServiceSession}
    */
   static fromRequest(request) { return this.sessions.fromRequest(request) }

   /** @type {HttpService} */
   service
   /**
    * @param {HttpService} service
    * @param {string} [sessionid]
    */
   constructor(service, sessionid) {
      super(sessionid)
      this.service = service
      HttpServiceSession.sessions.register(this)
   }
   /**
    * @returns {void}
    */
   invalidate() {
      if (!this.isValid()) return
      HttpServiceSession.sessions.unregister(this)
      super.invalidate()
      this.service.invalidateSession()
   }
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
    * @type {null|HttpServiceSession}
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
      const session = this.#session = HttpServiceSession.fromRequest(req)
      if (!session || session.isExpired())
         this.#invalidateSession()
   }
   /**
    * @param {string} name
    * @returns {null|string}
    */
   getCookie(name) { return HttpCookie.fromRequest(this.#request,name) }
   getPathname() { return this.#pathname }
   getDirectory() { return this.#pathname.substring(0,this.#pathname.lastIndexOf("/")+1) }
   getRequest() { return this.#request }
   getResponse() { return this.#response }
   getURL() { return this.#url }
   //// Session
   getSession() { return this.#session }
   #invalidateSession() {
      const session = this.#session
      this.#session = null
      session?.invalidate()
      HttpServiceSession.removeFrom(this.#response)
   }
   invalidateSession() {
      if (!this.#session) return
      this.#invalidateSession()
   }
   /**
    * Returns a newly created session. An already existing session is invalidated before a new one is created.
    * @returns {HttpServiceSession} The newly created session.
    */
   newSession() {
      this.invalidateSession()
      return this.ensureSession()
   }
   /**
    * Returns a session, either an existing or a newly created one if no one exists.
    * @returns {HttpServiceSession} The existing or newly created session.
    */
   ensureSession() {
      this.#session ??= new HttpServiceSession(this)
      this.#session.attachTo(this.getResponse()) // renew max-age
      return this.#session
   }
   //// Header
   /** @param {string} value */
   setContentType(value) { this.#response.setHeader("Content-Type", value) }
   /** @param {string} name @param {null|string} value */
   setResponseHeader(name, value) { value ? this.#response.setHeader(name, value) : this.#response.deleteHeader(name) }
   /** @param {string} name @param {string} value */
   appendResponseHeader(name, value) { this.#response.appendHeader(name, value) }
   /**
    * @typedef {{req:Request,pathname:string,acrMethod:null|string,origin:null|string,acrHeaders:null|string}} AllowOptions Type of parameter used in all allow<Name> methods.
    * @param {Request} req
    * @param {string} pathname
    * @returns {AllowOptions} {@link AllowOptions}
    */
   getAllowOptions(req, pathname) {
      return { req, pathname, acrMethod:req.headers.get("access-control-request-method"), origin:req.headers.get("origin"), acrHeaders:req.headers.get("access-control-request-headers") }
   }
   /////////
   // I/O //
   /////////
   /**
   * @typedef {any} BunFile
   */
   /**
    * @param {BunFile} file
    * @returns {Promise<number>} Number of written bytes
    */
   receiveFile(file) { return Bun.write(file, this.getRequest()) }
   /**
    * @param {BunFile} file
    * @returns {Response}
    */
   sendFile(file) { return this.getResponse().sendOK(file) }
   /////////////////////////////
   // Generic Service Methods //
   /////////////////////////////
   /**
    * @returns {Promise<Response>}
    */
   async serve() {
      const req = this.#request
      if (req.method !== "OPTIONS") {
         const site = req.headers.get("Sec-Fetch-Site")
         const crossOrigin = ("none" !== site && "same-origin" !== site)
         if (crossOrigin) {
            const options = this.getAllowOptions(req, this.#pathname)
            const origin = this.allowOrigin(options)
            this.setResponseHeader("Access-Control-Allow-Origin", origin)
            this.allowCredentials(options) && this.setResponseHeader("Access-Control-Allow-Credentials", "true")
            if (origin !== options.origin) {
               return this.getResponse().sendStatus(HTTP.FORBIDDEN, `Access forbidden.`)
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
   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Promise<Response>}
    */
   async doOptions(req, pathname) {
      const options = this.getAllowOptions(req, pathname)
      if (options.acrMethod) {
         this.setResponseHeader("Access-Control-Allow-Methods", this.allowMethods(options))
         this.setResponseHeader("Access-Control-Allow-Origin", this.allowOrigin(options))
         this.setResponseHeader("Access-Control-Allow-Headers", this.allowHeaders(options))
         this.setResponseHeader("Access-Control-Allow-Credentials", this.allowCredentials(options)?"true":null)
      }
      else {
         this.setResponseHeader("Allow", this.allowMethods(options))
      }
      return this.getResponse().sendNoContent()
   }
   //////////////////////////////////////////
   // Cors Support / overwrite in sub type //
   //////////////////////////////////////////
   /** @param {AllowOptions} options @returns {string} */
   allowMethods(options) { return "DELETE,GET,HEAD,OPTIONS,POST,PUT" /*"":no methods*/}
   /** @param {AllowOptions} options @returns {null|string} */
   allowOrigin(options) { return options.origin }
   /** @param {AllowOptions} options @returns {string} */
   allowHeaders(options) { return options.acrHeaders ?? ""/*no headers*/}
   /** @param {AllowOptions} options @returns {boolean} */
   allowCredentials(options) { return true/*with credentials*/}
   /////////////////////////////////////////////
   // Service Methods / overwrite in sub type //
   /////////////////////////////////////////////
   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Promise<Response>}
    */
   async doGet(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Promise<Response>}
    */
   async doHead(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Promise<Response>}
    */
   async doPost(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Promise<Response>}
    */
   async doPut(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req, pathname) }
   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Promise<Response>}
    */
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

   /**
    * @param {Request} req
    * @param {any} server
    * @returns {Promise<Response>}
    */
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
            console.log(`${clientIP.address}:${clientIP.port}  ${req.method}  ${pathname}  origin:${req.headers.get("origin")}`)
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
   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Promise<Response>}
    */
   async doGet(req, pathname) {
      const filename = "."+pathname
      const file = Bun.file(filename)

      console.log("id=",this.ensureSession().ID())


      if (await file.exists()) {
         const headers = this.getResponse().sendOK().headers // this.getResponse().getHeaders()
         // const info = await stat(filename).catch(()=>null)
         // if (info) this.setResponseHeader("Last-Modified",new Date(info.mtimeMs).toUTCString())
         this.setContentType(file.type)
         console.log("response headers",headers)
         return this.sendFile(file)
      }

      return this.getResponse().sendStatus(HTTP.NOT_FOUND,`File not found: ${pathname}`)
   }
}

class UploadWebService extends HttpService {

   /**
    * @param {Request} req
    * @param {string} pathname
    * @returns {Promise<Response>}
    */
   async doPut(req, pathname) {
      const name = pathname.substring(9).replaceAll(/[^-_.;:?a-zA-Z0-9üöäÖÄÜß]/g,"")
      const file = Bun.file(name)

      if (!name) {
         return this.getResponse().sendStatus(HTTP.BAD_REQUEST, pathname.length <= 9 ? `Empty file name` : `Invalid file name`)
      }
      if (await file.exists()) {
         return this.getResponse().sendStatus(HTTP.CONFLICT, `File exists »${name}«`)
      }

      console.log("upload »"+name+"«")
      return this.receiveFile(file)
      .then( (/*writtenBytes*/) => this.getResponse().sendOK(`Saved file »${name}«`))
      .catch( (/*e*/) => this.getResponse().sendStatus(HTTP.CONFLICT, `Error writing file »${name}«`))
   }
}

const services = new HttpServiceDispatcher(StaticFileWebService, ["/uploads",UploadWebService])
const hs = new HttpServer(services, "localhost" /*HttpServer.INADDR_ANY*/, 9090).run()
console.log("Started HttpServer listening on "+hs.hostname+":"+hs.port)
