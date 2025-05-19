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
   /////////////
   // success //
   /////////////
   /** @type {200} */
   static OK = 200
   /** @type {204} */
   static NO_CONTENT = 204
   /////////////////
   // redirection //
   /////////////////
   /** redirect to URL in Location header @type {301} */
   static REDIRECT_PERMANENTLY = 301
   /** redirect to URL in Location header @type {302} */
   static REDIRECT_TEMPORARILY = 302
   /** get resource from URL in Location header which is rendered instead of body  @type {303} */
   static REDIRECT_OTHER = 303
   //////////////////
   // client error //
   //////////////////
   /** @type {400} */
   static BAD_REQUEST = 400
   /** @type {401} */
   static UNAUTHORIZED = 401
   /** @type {403} */
   static FORBIDDEN = 403
   /** @type {404} */
   static NOT_FOUND = 404
   /** @type {405} */
   static METHOD_NOT_ALLOWED = 405
   /** @type {409} */
   static CONFLICT = 409
   /** @type {422} */
   static UNPROCESSABLE_CONTENT = 422
   //////////////////
   // server error //
   //////////////////
   /** @type {500} */
   static INTERNAL_SERVER_ERROR = 500
   /** @type {501} */
   static NOT_IMPLEMENTED = 501
   /** @type {503} */
   static SERVICE_UNAVAILABLE = 503
}

class ServiceContext {
   /**
    * @typedef SessionProviderInterface
    * @property {(sreq:ServiceRequest, sessionType?:string)=>void} newSession
    */

   /**
    * @param {SessionProviderInterface} sessionProvider
    */
   constructor(sessionProvider) {
      this.sessionProvider = sessionProvider
   }
}

class ServiceRequest {
   /** @type {undefined|HttpSession} */
   session

   /**
    * @param {string} url
    * @return {URL}
    */
   static parseURL(url) { try { return new URL(url) } catch (error) { return new URL("invalid://") } }

   /**
    * @param {{address:string, family:"IPv4"|"IPv6", port:number}} clientIP
    * @param {Request} request
    * @param {ServiceContext} svc
    */
   constructor(clientIP, request, svc) {
      const hostport = request.headers.get("host")?.split(":")
      const url = ServiceRequest.parseURL(request.url)
      this.clientIP = clientIP
      this.hostname = hostport?.[0]
      this.port = hostport?.[1]
      this.headers = request.headers
      this.method = request.method
      this.pathname = decodeURIComponent(url.pathname)
      this.request = request
      this.response = new HttpResponse()
      this.url = url
      this.svc = svc
   }
   get isInvalidURL() { return this.url.href.startsWith("invalid:") }
   /////////
   // I/O //
   /////////
   /** @return {Promise<object>} */
   json() { return this.request.json() }
   /** @return {Promise<string>} */
   text() { return this.request.text() }
   /**
    * @param {BunFile} file
    * @returns {Promise<number>} Number of written bytes
    */
   receiveFile(file) { return Bun.write(file, this.request) }
}

class HttpResponse {
   static ContentType = {
      JSON: "application/json",
      HTML: "text/html",
      TEXT: "text/plain",
   }

   /** @type {Headers} */
   #headers
   /** @type {Map<string,HttpCookie>} */
   #cookies

   constructor() {
      this.#headers = new Headers()
      this.#cookies = new Map()
   }
   //////////////////////
   // Headers support //
   //////////////////////
   /**
    * Adds cookie to response. Overwrites previously set cookie value.
    * @param {HttpCookie} cookie
    */
   setCookie(cookie) { this.#cookies.set(cookie.getName(), cookie) }
   /** @param {string} value */
   setContentType(value) { this.setHeader("Content-Type", value) }
   /** @return {Headers} */
   getHeaders() { return this.#headers }
   /**
    * @param {string} name
    * @param {string} value
    */
   appendHeader(name, value) { this.#headers.append(name, value) }
   /** @param {string} name */
   deleteHeader(name) { this.#headers.delete(name) }
   /**
    * @param {string} name
    * @return {null|string}
    */
   getHeader(name) { return this.#headers.get(name) }
   /**
    * @param {string} name
    * @param {string} value
    */
   setHeader(name, value) { this.#headers.set(name, value) }
   ///////////////////
   // Send response //
   ///////////////////
   /** @returns {Response} */
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
         cookie.appendToResponse(this)
      }
      this.#cookies.clear()
      return new Response(body, { status, headers:this.#headers })
   }
   /**
    * @param {ServiceRequest|Request} sreq
    * @param {number} status
    * @param {null|string|ReadableStream|Blob} body
    * @returns {Response}
    */
   static sendError(sreq, status, body) {
      const response = new HttpResponse()
      const req = sreq instanceof ServiceRequest ? sreq.request : sreq
      response.setHeader("Access-Control-Allow-Methods", req.headers.get("Access-Control-Request-Method") ?? req.method)
      response.setHeader("Access-Control-Allow-Origin", req.headers.get("origin") ?? "")
      response.setHeader("Access-Control-Allow-Headers", req.headers.get("access-control-request-headers") ?? "")
      response.setHeader("Access-Control-Allow-Credentials", "true")
      return response.sendStatus(status, body)
   }
   /////////
   // I/O //
   /////////
   /**
    * @typedef {Blob&{exists():Promise<boolean>}} BunFile
    * @param {BunFile} file
    * @param {string} [contentType]
    * @return {Response}
    */
   sendFile(file, contentType) {
      this.setContentType(contentType ?? file.type)
      return this.sendOK(file)
   }
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
   // /** @type {null|string} */
   // domain=null

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
   appendToResponse(response) {
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
   maxage = 3600 // valid for 1 hour
   /** @type {Map<String,HttpSession>} */
   sessions = new Map()
   /** @type {HttpSession[]} */
   expiredSessions = []
   /** @type {HttpSession[]} */
   testSessionForExpiration = []
   /** @type {(session:HttpSession)=>void} */
   onCleanupSession

   /**
    * @param {string} id
    * @return {HttpCookie}
    */
   static newSessionCookie(id) { return new HttpCookie("HTTP_SESSION_ID",id) }
   /**
    * @param {Request} request
    * @return {null|string}
    */
   static sessionIDFromRequest(request) { return HttpCookie.fromRequest(request,"HTTP_SESSION_ID") }
   /** @return {string} */
   static generateSessionID() { return crypto.randomUUID().replaceAll("-","").toUpperCase() }

   /** @param {(session:HttpSession)=>void} onCleanupSession */
   constructor(onCleanupSession) {
      this.onCleanupSession = onCleanupSession
   }
   ////////////////////////
   // Session Management //
   ////////////////////////
   /**
    * @param {ServiceRequest} sreq
    */
   assignSessionFromRequest(sreq) {
      const id = HttpSessions.sessionIDFromRequest(sreq.request)
      if (id) {
         const session = this.sessions.get(id)
         if (session) {
            sreq.session = session
            if (session.isExpired) this.expireSession(sreq)
         }
      }
   }
   /**
    * @param {ServiceRequest} sreq
    * @param {string} [sessionType]
    */
   newSession(sreq, sessionType) {
      const sessionID = HttpSessions.generateSessionID()
      const sessionCookie = HttpSessions.newSessionCookie(sessionID)
      const session = new HttpSession(sessionID, this.maxage, sessionType)
      this.sessions.set(sessionID, session)
      session.addFreeListener(this.onCleanupSession)
      sessionCookie.setMaxAge(this.maxage)
      sreq.response.setCookie(sessionCookie)
      sreq.session = session
   }
   /**
    * @param {ServiceRequest} sreq
    */
   expireSession(sreq) {
      const session = sreq.session
      if (session) {
         this.#expireSession(session)
         sreq.session = undefined
      }
   }
   freeExpiredSessions() {
      for (let i=100; i > 0; --i)
         this.#testExpiration()
      for (let i=1000, session; i > 0 && (session=this.expiredSessions.shift()); --i)
         try { session.free() } catch (e) { /* ignore */ }
   }
   /** @param {HttpSession} session */
   #expireSession(session) {
      const sessionID = session.ID
      if (this.sessions.get(sessionID) === session) {
         this.sessions.delete(sessionID)
         this.expiredSessions.push(session)
         session.expire()
      }
   }
   /** @return {undefined} */
   #testExpiration() {
      if (this.testSessionForExpiration.length) {
         const session = this.testSessionForExpiration.pop()
         if (session?.isExpired) this.#expireSession(session)
      }
      else
         this.testSessionForExpiration = [...this.sessions.values()]
   }
}

const LoginSessionType = "login"

class HttpSession {
   /** @type {{[attr:string]:any}} */
   #attributes
   /** @type {number} */
   #expiration
   /** @type {string} */
   #sessionID
   /** @type {string} */
   #type
   /** @type {undefined|((session:HttpSession)=>void)[]} onFree */
   #onFree

   /**
    * @param {string} [sessionID]
    * @param {number} [maxage]
    * @param {string} [type]
    */
   constructor(sessionID, maxage, type) {
      this.#attributes = {}
      this.#expiration = sessionID && maxage ? maxage + this.epochSeconds() : 0
      this.#sessionID = sessionID || ""
      this.#type = type || ""
      this.#onFree = []
   }
   /** After return session is moved to expired queue and removed waits for freeing resources. */
   expire() {
      if (this.#expiration === 0) return
      this.#expiration = 0
   }
   /** After return {@link isValid} returns false and all registered onFree listeners are called. */
   free() {
      if (!this.isValid) return
      this.expire()
      const listeners = this.#onFree
      this.#onFree = undefined
      if (listeners) {
         listeners.forEach(onFree => { try { onFree(this) } catch(e) { /* ignore */ } })
         this.#sessionID = ""
      }
   }
   /** @param {(session:HttpSession)=>void} [onFree] */
   addFreeListener(onFree) { onFree && this.#onFree?.push(onFree) }
   /** @return {number} Since Epoch in seconds. */
   epochSeconds() { return Date.now()/1000 }
   ////////////////
   // Properties //
   ////////////////
   get attributes() { return Object.assign({}, this.#attributes) }
   get ID() { return this.#sessionID }
   /** @return {boolean} *true* if session has reached its maxage. */
   get isExpired() { return this.epochSeconds() > this.#expiration }
   /** @return {boolean} *true* if session ID is not invalid. */
   get isValid() { return this.#sessionID !== "" }
   /** @return {string} Returns type of session. Empty string is default type. Session dispatching is per type. */
   get type() { return this.#type }
   ////////////////
   // Attributes //
   ////////////////
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
}

class HttpServiceInterface
{
   /** @param {ServiceRequest} sreq */
   constructor(sreq) {
      this.request = sreq
      this.response = new HttpResponse()
   }
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async serve(sreq) { return HttpResponse.sendError(sreq,HTTP.NOT_IMPLEMENTED,"HttpServiceInterface not implemented") }
}

class ErrorHttpService {
   /**
    * @param {ServiceRequest} sreq
    * @param {boolean} [enableCORS]
    */
   constructor(sreq, enableCORS=true) {
      this.request = sreq
      this.response = new HttpResponse()
      /** @type {number} */
      this.status = HTTP.NOT_FOUND
      this.error = `Path ${sreq.pathname} and method ${sreq.method} could not be mapped to a service.`
      enableCORS && this.enableCrossOriginResourceSharing()
   }
   /**
    * @param {number} status
    * @param {string} error
    * @return {ErrorHttpService}
    */
   setStatus(status, error) {
      this.status = status
      this.error = error
      return this
   }
   enableCrossOriginResourceSharing()
   {
      const response = this.response, sreq = this.request
      response.setHeader("Access-Control-Allow-Methods", sreq.headers.get("Access-Control-Request-Method") ?? sreq.method)
      response.setHeader("Access-Control-Allow-Origin", sreq.headers.get("origin") ?? "")
      response.setHeader("Access-Control-Allow-Headers", sreq.headers.get("access-control-request-headers") ?? "")
      response.setHeader("Access-Control-Allow-Credentials", "true")
   }
   ////////////////////////////////////
   // Implement HttpServiceInterface //
   ////////////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async serve(sreq) {
      return this.response.sendStatus(this.status,this.error)
   }
   ////////////////////////////
   // Common Error Responses //
   ////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @param {number} status
    * @param {string} error
    * @return {Response}
    */
   static sendError(sreq, status, error) {
      const service = new ErrorHttpService(sreq)
      return service.response.sendStatus(status, error)
   }
   /**
    * @param {ServiceRequest} sreq
    * @return {Response}
    */
   static sendNotFound(sreq) {
      return this.sendError(sreq,HTTP.NOT_FOUND,`Path ${sreq.pathname} could not be resolved to a resource or service.`)
   }
   /**
    * @param {ServiceRequest} sreq
    * @return {Response}
    */
   static sendJSONSyntaxError(sreq) {
      return this.sendError(sreq,HTTP.UNPROCESSABLE_CONTENT,`Received JSON at path ${sreq.pathname} could not be parsed.`)
   }
   /**
    * @param {ServiceRequest} sreq
    * @returns {Response}
    */
   static sendMethodNotAllowed(sreq) {
      return this.sendError(sreq,HTTP.METHOD_NOT_ALLOWED,`Method ${sreq.method} not supported for path ${sreq.pathname}.`)
   }
   /**
    * @param {ServiceRequest} sreq
    * @returns {Response}
    */
   static sendInvalidURL(sreq) {
      return this.sendError(sreq,HTTP.BAD_REQUEST,`URL is invalid.`)
   }
}

class HttpService {
   /** @type {ServiceRequest} */
   #request
   /** @type {HttpResponse} */
   #response

   /**
    * @param {ServiceRequest} sreq
    */
   constructor(sreq) {
      this.#request = sreq
      this.#response = sreq.response
   }
   /**
    * @param {string} name
    * @returns {null|string}
    */
   cookie(name) { return HttpCookie.fromRequest(this.origrequest,name) }
   get directory() { return this.pathname.substring(0,this.pathname.lastIndexOf("/")+1) }
   get headers() { return this.#request.headers }
   get method() { return this.#request.method }
   get pathname() { return this.#request.pathname }
   get request() { return this.#request }
   get response() { return this.#response }
   get origrequest() { return this.#request.request }
   get session() { return this.#request.session }
   get url() { return this.#request.url }
   //////////////
   // Response //
   //////////////
   /** @param {string} value */
   setContentType(value) { this.#response.setContentType(value) }
   /** @param {string} name @param {null|string} value */
   setHeader(name, value) { value ? this.#response.setHeader(name, value) : this.#response.deleteHeader(name) }
   /** @param {string} name @param {string} value */
   appendHeader(name, value) { this.#response.appendHeader(name, value) }
   ////////////////////////////////////
   // Implement HttpServiceInterface //
   ////////////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async serve(sreq) {
      const method = sreq.method
      const pathname = sreq.pathname
      if (method !== "OPTIONS") {
         const site = this.headers.get("Sec-Fetch-Site")
         const crossOrigin = ("none" !== site && "same-origin" !== site)
         if (crossOrigin) {
            const options = this.accessControlOptions(sreq)
            const origin = this.allowOrigin(options)
            this.setHeader("Access-Control-Allow-Origin", origin)
            this.allowCredentials(options) && this.setHeader("Access-Control-Allow-Credentials", "true")
            if (origin !== options.origin) {
               return this.response.sendStatus(HTTP.FORBIDDEN, `Access forbidden.`)
            }
         }
      }
      switch(method) {
         case "GET": return this.doGet(sreq)
         case "HEAD": return this.doHead(sreq)
         case "OPTIONS": return this.doOptions(sreq)
         case "POST": return this.doPost(sreq)
         case "PUT": return this.doPut(sreq)
         default: return this.doHttpMethod(sreq)
      }
   }
   //////////////////
   // Cors Support //
   //////////////////
   /**
    * @typedef {{request:ServiceRequest,acrMethod:null|string,origin:null|string,acrHeaders:null|string}} AllowOptions Type of parameter used in all allow<Name> methods.
    * @param {ServiceRequest} sreq
    * @returns {AllowOptions} {@link AllowOptions}
    */
   accessControlOptions(sreq) { return { request:sreq, acrMethod:sreq.headers.get("access-control-request-method"), origin:sreq.headers.get("origin"), acrHeaders:sreq.headers.get("access-control-request-headers") } }
   /** @param {AllowOptions} options @returns {string} */
   allowMethods(options) { return "DELETE,GET,HEAD,OPTIONS,POST,PUT" /*"":no methods*/}
   /** @param {AllowOptions} options @returns {null|string} */
   allowOrigin(options) { return options.origin }
   /** @param {AllowOptions} options @returns {string} */
   allowHeaders(options) { return options.acrHeaders ?? ""/*no headers*/}
   /** @param {AllowOptions} options @returns {boolean} */
   allowCredentials(options) { return true/*with credentials*/}
   /////////////////////
   // Request Methods //
   /////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doOptions(sreq) {
      const options = this.accessControlOptions(sreq)
      if (options.acrMethod) {
         this.setHeader("Access-Control-Allow-Methods", this.allowMethods(options))
         this.setHeader("Access-Control-Allow-Origin", this.allowOrigin(options))
         this.setHeader("Access-Control-Allow-Headers", this.allowHeaders(options))
         this.setHeader("Access-Control-Allow-Credentials", this.allowCredentials(options)?"true":null)
      }
      else {
         this.setHeader("Allow", this.allowMethods(options))
      }
      return this.response.sendNoContent()
   }
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doGet(sreq) { return ErrorHttpService.sendMethodNotAllowed(sreq) }
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doHead(sreq) { return ErrorHttpService.sendMethodNotAllowed(sreq) }
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doPost(sreq) { return ErrorHttpService.sendMethodNotAllowed(sreq) }
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doPut(sreq) { return ErrorHttpService.sendMethodNotAllowed(sreq) }
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doHttpMethod(sreq) { return ErrorHttpService.sendMethodNotAllowed(sreq) }
}


class HttpServices
{
   /**
    * @typedef HttpServicesDispatchRequest
    * @property {ServiceRequest} sreq
    * @property {HttpSessions} sessions
    */
   /**
    * @param {ServiceRequest} sreq
    * @return {HttpServiceInterface}
    */
   dispatch(sreq) { throw Error("dispatch(sreq) not implemented.") }
}

class HttpServicesDispatchPath extends HttpServices {

   /** @type {Map<string,{service:null|(typeof HttpServiceInterface)|HttpServices}>} */
   pathMap
   /** @type {typeof HttpServiceInterface|HttpServices} */
   rootService

   /**
    * @param {typeof HttpServiceInterface|HttpServices} rootService
    * @param  {...[string,typeof HttpServiceInterface|HttpServices]} path2Service
    */
   constructor(rootService, ...path2Service) {
      super()
      this.pathMap = new Map()
      this.rootService = rootService
      for (const [ path, service ] of path2Service) {
         if (this.pathMap.get(path)?.service) throw Error(`HttpServiceDispatcher error: defined more than one service for path ${path}`)
         this.pathMap.set(path, {service})
      }
   }
   ////////////////////////////
   // Overwrite HttpServices //
   ////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @return {HttpServiceInterface}
    */
   dispatch(sreq) {
      let foundService = this.rootService
      for (let path=sreq.pathname; path.length; path=path.substring(0,path.lastIndexOf("/"))) {
         const entry = this.pathMap.get(path)
         if (entry?.service) {
            foundService = entry.service
            break
         }
      }
      if (!foundService)
         return new ErrorHttpService(sreq)
      if (foundService instanceof HttpServices)
         return foundService.dispatch(sreq)
      return new foundService(sreq)
   }
}

class HttpServicesDispatchSessionType extends HttpServices {

   /** @type {HttpServices} */
   dispatchNoSession
   /** @type {Map<string,HttpServices>} */
   dispatchSessionType

   /**
    * @param {HttpServices} dispatchNoSession
    * @param  {...[string,HttpServices]} sessiontypeDispatcher
    */
   constructor(dispatchNoSession, ...sessiontypeDispatcher) {
      super()
      this.dispatchNoSession = dispatchNoSession
      this.dispatchSessionType = new Map(sessiontypeDispatcher)
   }

   ////////////////////////////
   // Overwrite HttpServices //
   ////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @return {HttpServiceInterface}
    */
   dispatch(sreq) {
      const session = sreq.session
      const dispatcher = session && !session.isExpired && this.dispatchSessionType.get(session.type)
      return (dispatcher || this.dispatchNoSession).dispatch(sreq)
   }
}

class HttpServer {
   static INADDR_ANY = "0.0.0.0" /*unsafe*/ // safer variants "localhost", "127.0.0.1"
   static IN6ADDR_ANY = "::" /*unsafe*/ // safer variants "localhost", "::1"

   #hostname
   #port
   #serviceDispatcher
   #sessions
   #svc
   #expiredSessionTimeout

   /**
    * @param {HttpServices} serviceDispatcher
    * @param {string} hostname
    * @param {number} port
    */
   constructor(serviceDispatcher, hostname="localhost", port=8080) {
      this.#expiredSessionTimeout = false
      this.#hostname = hostname
      this.#port = port
      this.#serviceDispatcher = serviceDispatcher
      this.#sessions = new HttpSessions(this.cleanupSession.bind(this))
      this.#svc = new ServiceContext({
         newSession: (sreq,sessionType) => {
            this.#sessions.newSession(sreq,sessionType)
         }
      })
      this.freeExpiredSessions()
   }
   get port() { return this.#port }
   get hostname() { return this.#hostname }

   /** @param {HttpSession} session */
   cleanupSession(session) {
      console.log("==== TODO: SESSION CLEANUP ====")
   }

   freeExpiredSessions() {
      if (this.#expiredSessionTimeout) return
      this.#expiredSessionTimeout = true
      this.#sessions.freeExpiredSessions()
      setTimeout( () => { this.#expiredSessionTimeout = false; this.freeExpiredSessions() }, 5000)
   }

   run() {
      Bun.serve({ port:this.#port, hostname:this.#hostname, fetch:this.serve.bind(this) })
      return this
   }

   /**
    * @param {ServiceRequest} sreq
    * @param {any} e
    * @return {Response}
    */
   sendInternalServerError(sreq, e) {
      console.log("ServiceRequest: Internal Server Error")
      console.log("> method:", sreq.method)
      console.log("> pathname:", sreq.pathname)
      console.log("> url:", sreq.url.href)
      console.log("> headers:", sreq.headers)
      console.log("> exception:", e)
      return HttpResponse.sendError(sreq,HTTP.INTERNAL_SERVER_ERROR,"Service failed.")
   }

   /**
    * @param {Request} req
    * @param {any} server
    * @returns {Promise<Response>}
    */
   async serve(req, server) {
      const clientIP = server.requestIP(req)
      const sreq = new ServiceRequest(clientIP,req,this.#svc)
      if (sreq.isInvalidURL) {
         return ErrorHttpService.sendInvalidURL(sreq)
      }
      this.#sessions.assignSessionFromRequest(sreq)
      // console.log("headers",req.headers)
      // console.log("url", req.url)
      // console.log("req", req)
      // console.log("server", server)
      try {
         const service = this.#serviceDispatcher.dispatch(sreq)
         console.log(`client ${clientIP.address}:${clientIP.port},  origin ${req.headers.get("origin")}\n  ${req.method}${"       ".substring(0,7-req.method.length)}  ${sreq.url.pathname}`)
         return service.serve(sreq).catch(e => this.sendInternalServerError(sreq,e))
      }
      catch(e) {
         return this.sendInternalServerError(sreq,e)
      }
   }
}

class StaticFileWebService extends HttpService {
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doGet(sreq) {
      const pathname = sreq.pathname
      const filename = "."+pathname
      /** @type {BunFile} */
      const file = Bun.file(filename)

      if (await file.exists()) {
         return this.response.sendFile(file)
      }

      return this.response.sendStatus(HTTP.NOT_FOUND,`File not found: ${pathname}`)
   }
}

class UploadWebService extends HttpService {
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doPut(sreq) {
      const pathname = sreq.pathname
      const name = pathname.substring(9).replaceAll(/[^-_.;:?a-zA-Z0-9üöäÖÄÜß ]/g,"")
      const file = Bun.file(name)

      if (!name) {
         return this.response.sendStatus(HTTP.BAD_REQUEST, pathname.length <= 9 ? `Empty file name` : `Invalid file name`)
      }
      if (await file.exists()) {
         return this.response.sendStatus(HTTP.CONFLICT, `File exists »${name}«`)
      }

      console.log("upload »"+name+"«")
      return this.request.receiveFile(file)
      .then( (/*writtenBytes*/) => this.response.sendOK(`Saved file »${name}«`))
      .catch( (/*e*/) => file.exists().then(exists => exists && file.delete()).catch((/*e*/) => false))
      .then( response => response || this.response.sendStatus(HTTP.CONFLICT, `Writing file failed »${name}«`))
   }
}

class LoginWebService extends HttpService {
   /**
    * @param {ServiceRequest} sreq
    * @returns {Promise<Response>}
    */
   async doPost(sreq) {
      const pathname = sreq.pathname
      if (pathname !== "/login")
         return ErrorHttpService.sendNotFound(sreq)

      const credentials = await sreq.json().catch((/*e*/) => ErrorHttpService.sendJSONSyntaxError(sreq))
      if (credentials instanceof Response)
         return credentials
      if (credentials.name === "test" && credentials.password === "test") {
         sreq.svc.sessionProvider.newSession(sreq, LoginSessionType)
         console.log("new sessionid",sreq.session?.ID)
         return this.response.sendOK("Login succeeded.")
      }
      return this.response.sendStatus(HTTP.UNAUTHORIZED,"Login failure.")
   }
}

const authorizedServices = new HttpServicesDispatchPath(StaticFileWebService, ["/uploads",UploadWebService])
const unauthorizedServices = new HttpServicesDispatchPath(StaticFileWebService)
const sessionServices = new HttpServicesDispatchSessionType(unauthorizedServices, [LoginSessionType,authorizedServices])
const services = new HttpServicesDispatchPath(sessionServices, ["/login", LoginWebService])
const server = new HttpServer(services, "localhost" /*HttpServer.INADDR_ANY*/, 9090).run()
console.log("Started HttpServer listening on "+server.hostname+":"+server.port)
