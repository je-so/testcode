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
    * @typedef DispatcherInterface
    * @property {(sreq:ServiceRequest)=>undefined|HttpServiceInterface} dispatch
    * @property {(sreq:ServiceRequest)=>Promise<Response>} forward
    */
   /**
    * @typedef SessionProviderInterface
    * @property {(service:HttpService, sessionType?:string)=>void} newSession
    */

   /**
    * @param {DispatcherInterface} dispatcher
    * @param {SessionProviderInterface} sessionProvider
    */
   constructor(dispatcher, sessionProvider) {
      this.dispatcher = dispatcher
      this.sessionProvider = sessionProvider
   }
}

class ServiceRequest {

   /**
    * @param {string} url
    * @return {URL}
    */
   static parseURL(url) { try { return new URL(url) } catch (error) { return new URL("invalid://") } }

   /**
    * @param {{address:string, family:"IPv4"|"IPv6", port:number}} clientIP
    * @param {ServiceContext} context
    * @param {Request} request
    * @param {HttpSession} [session]
    * @param {Request} [bodyRequest]
    */
   constructor(clientIP, context, request, session, bodyRequest) {
      const host = request.headers.get("host")?.split(":")
      const url = ServiceRequest.parseURL(request.url)
      this.bodyRequest = bodyRequest ?? request
      this.clientIP = clientIP
      this.context = context
      this.dispatchedPath = ""
      this.hostname = host?.[0]
      this.hostport = host?.[1]
      this.method = request.method
      this.pathname = decodeURIComponent(url.pathname)
      this.reqHeaders = request.headers
      this.request = request
      this.session = session
      this.url = url
   }
   get bodyUsed() { return this.bodyRequest.bodyUsed }
   get invalidURL() { return this.url.href.startsWith("invalid:") }
   /** @param {string} name @return {null|string} */
   reqHeader(name) { return this.reqHeaders.get(name) }
   /** @param {Request} req */
   reqInitOptions(req) { return { headers:req.headers, method:req.method, } }
   ////////////
   // Update //
   ////////////
   /** @param {string} dispatchedPath */
   setDispatchedPath(dispatchedPath) { this.dispatchedPath = dispatchedPath }
   //////////////////
   // Construction //
   //////////////////
   /** @param {Request} [bodyRequest] @return {ServiceRequest} */
   newBody(bodyRequest) { return new ServiceRequest(this.clientIP,this.context,this.request,this.session,bodyRequest) }
   /** @param {string} pathname @return {ServiceRequest} */
   newPath(pathname) { const url = this.url; url.pathname = pathname; return this.newRequest(new Request(url,this.reqInitOptions(this.request))) }
   /** @param {Request} request @return {ServiceRequest} */
   newRequest(request) { return new ServiceRequest(this.clientIP,this.context,request,this.session,this.bodyRequest) }
   /** @param {HttpSession} session @return {ServiceRequest} */
   newSession(session) { return new ServiceRequest(this.clientIP,this.context,this.request,session,this.bodyRequest) }
   /////////
   // I/O //
   /////////
   /** @return {Promise<object>} */
   json() { return this.bodyRequest.json() }
   /** @return {Promise<string>} */
   text() { return this.bodyRequest.text() }
   /**
    * @param {BunFile} file
    * @returns {Promise<number>} Number of written bytes
    */
   receiveFile(file) { return Bun.write(file,"").then(_ => Bun.write(file, this.bodyRequest)) } // Bun.write(file,"") needed to support receiving empty files (Bun-Bug)
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
   /**
    * @param {string} filename
    * @param {BunFile} file
    * @param {string} [contentType]
    * @return {Response}
    */
   sendFileForDownload(filename, file, contentType) {
      this.setContentType(contentType ?? file.type)
      this.setHeader("content-disposition","attachment; filename*=UTF-8''"+encodeURIComponent(filename))
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
    * @param {Headers} headers
    * @param {string} name
    * @returns {null|string} Value of cookie.
    */
   static fromHeaders(headers, name) {
      const reqCookie = headers.get("cookie")
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
    * @param {number} maxage
    * @return {HttpCookie}
    */
   static newSessionCookie(id, maxage) {
      const sessionCookie = new HttpCookie("HTTP_SESSION_ID",id)
      sessionCookie.setMaxAge(maxage)
      return sessionCookie
   }
   /**
    * @param {Headers} headers
    * @return {null|string}
    */
   static sessionIDFromRequest(headers) { return HttpCookie.fromHeaders(headers,"HTTP_SESSION_ID") }
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
    * @param {Headers} headers
    * @return {undefined|HttpSession}
    */
   sessionFromHeaders(headers) {
      const id = HttpSessions.sessionIDFromRequest(headers)
      if (id) {
         const session = this.sessions.get(id)
         if (session) {
            if (session.isExpired)
               this.#expireSession(session)
            else
               return session
         }
      }
   }
   /**
    * @param {HttpService} service
    * @param {string} [sessionType]
    */
   newSession(service, sessionType) {
      const sessionID = HttpSessions.generateSessionID()
      const sessionCookie = HttpSessions.newSessionCookie(sessionID, this.maxage)
      const session = new HttpSession(sessionID, this.maxage, sessionType)
      this.sessions.set(sessionID, session)
      session.addFreeListener(this.onCleanupSession)
      service.setSession(session,sessionCookie)
   }
   /**
    * @param {HttpService} service
    */
   expireSession(service) {
      const session = service.session
      if (session) {
         const sessionCookie = HttpSessions.newSessionCookie("", 0)
         this.#expireSession(session)
         service.clearSession(sessionCookie)
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

class HttpServiceInterface {
   /**
    * @param {ServiceRequest} sreq
    */
   constructor(sreq) {
      this.context = sreq.context
      this.request = sreq
      this.response = new HttpResponse()
   }
   /**
    * @returns {Promise<Response>}
    */
   async serve() { return ErrorHttpService.Error(this.request,HTTP.NOT_IMPLEMENTED,"HttpServiceInterface not implemented").serve() }
}

class ErrorHttpService {
   /**
    * @param {ServiceRequest} sreq
    * @param {boolean} [enableCORS]
    */
   constructor(sreq, enableCORS=true) {
      this.context = sreq.context
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
   enableCrossOriginResourceSharing() {
      const response = this.response, sreq = this.request
      response.setHeader("Access-Control-Allow-Methods", sreq.reqHeader("Access-Control-Request-Method") ?? sreq.method)
      response.setHeader("Access-Control-Allow-Origin", sreq.reqHeader("origin") ?? "")
      response.setHeader("Access-Control-Allow-Headers", sreq.reqHeader("access-control-request-headers") ?? "")
      response.setHeader("Access-Control-Allow-Credentials", "true")
   }
   /**
    * @returns {Response}
    */
   send() { return this.response.sendStatus(this.status,this.error) }
   ////////////////////////////////////
   // Implement HttpServiceInterface //
   ////////////////////////////////////
   /**
    * @returns {Promise<Response>}
    */
   async serve() { return this.response.sendStatus(this.status,this.error) }
   ////////////////////////////
   // Common Error Responses //
   ////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @param {number} status
    * @param {string} error
    * @return {ErrorHttpService}
    */
   static Error(sreq, status, error) { return new ErrorHttpService(sreq).setStatus(status,error) }
   /**
    * @param {ServiceRequest} sreq
    * @return {ErrorHttpService}
    */
   static NotFound(sreq) { return this.Error(sreq,HTTP.NOT_FOUND,`Path ${sreq.pathname} could not be resolved to a resource or service.`) }
   /**
    * @param {ServiceRequest} sreq
    * @return {ErrorHttpService}
    */
   static JSONSyntaxError(sreq) { return this.Error(sreq,HTTP.UNPROCESSABLE_CONTENT,`Received JSON at path ${sreq.pathname} could not be parsed.`) }
   /**
    * @param {ServiceRequest} sreq
    * @return {ErrorHttpService}
    */
   static MethodNotAllowed(sreq) { return this.Error(sreq,HTTP.METHOD_NOT_ALLOWED,`Method ${sreq.method} not supported for path ${sreq.pathname}.`) }
   /**
    * @param {ServiceRequest} sreq
    * @return {ErrorHttpService}
    */
   static InvalidURL(sreq) { return this.Error(sreq,HTTP.BAD_REQUEST,`URL is invalid.`) }
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
      this.#response = new HttpResponse()
   }
   /**
    * @param {string} name
    * @returns {null|string}
    */
   cookie(name) { return HttpCookie.fromHeaders(this.reqHeaders,name) }
   get context() { return this.#request.context }
   get dispatchedPath() { return this.#request.dispatchedPath }
   get directory() { return this.pathname.substring(0,this.pathname.lastIndexOf("/")+1) }
   get method() { return this.#request.method }
   get pathname() { return this.#request.pathname }
   /** @param {string} name @return {null|string} */
   reqHeader(name) { return this.#request.reqHeaders.get(name) }
   get reqHeaders() { return this.#request.reqHeaders }
   get request() { return this.#request }
   get response() { return this.#response }
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
   /** @param {HttpCookie} sessionCookie */
   clearSession(sessionCookie) {
      this.#request.session = undefined
      this.response.setCookie(sessionCookie)
   }
   /** @param {HttpSession} session @param {HttpCookie} sessionCookie */
   setSession(session, sessionCookie) {
      this.#request.session = session
      this.response.setCookie(sessionCookie)
   }
   ////////////////////////////////////
   // Implement HttpServiceInterface //
   ////////////////////////////////////
   /**
    * @returns {Promise<Response>}
    */
   async serve() {
      const method = this.method
      const pathname = this.pathname
      if (method !== "OPTIONS") {
         const site = this.reqHeaders.get("Sec-Fetch-Site")
         const crossOrigin = ("none" !== site && "same-origin" !== site)
         if (crossOrigin) {
            const options = this.accessControlOptions()
            const origin = this.allowOrigin(options)
            this.setHeader("Access-Control-Allow-Origin", origin)
            this.allowCredentials(options) && this.setHeader("Access-Control-Allow-Credentials", "true")
            if (origin !== options.origin) {
               return this.response.sendStatus(HTTP.FORBIDDEN, `Access forbidden.`)
            }
         }
      }
      switch(method) {
         case "GET": return this.doGet()
         case "HEAD": return this.doHead()
         case "OPTIONS": return this.doOptions()
         case "POST": return this.doPost()
         case "PUT": return this.doPut()
         default: return this.doHttpMethod()
      }
   }
   //////////////////
   // Cors Support //
   //////////////////
   /**
    * @typedef {{acrMethod:null|string,origin:null|string,acrHeaders:null|string}} AllowOptions Type of parameter used in all allow<Name> methods.
    * @returns {AllowOptions} {@link AllowOptions}
    */
   accessControlOptions() { return { acrMethod:this.reqHeader("access-control-request-method"), origin:this.reqHeader("origin"), acrHeaders:this.reqHeader("access-control-request-headers") } }
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
    * @returns {Promise<Response>}
    */
   async doOptions() {
      const options = this.accessControlOptions()
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
    * @returns {Promise<Response>}
    */
   async doGet() { return ErrorHttpService.MethodNotAllowed(this.request).serve() }
   /**
    * @returns {Promise<Response>}
    */
   async doHead() { return ErrorHttpService.MethodNotAllowed(this.request).serve() }
   /**
    * @returns {Promise<Response>}
    */
   async doPost() { return ErrorHttpService.MethodNotAllowed(this.request).serve() }
   /**
    * @returns {Promise<Response>}
    */
   async doPut() { return ErrorHttpService.MethodNotAllowed(this.request).serve() }
   /**
    * @returns {Promise<Response>}
    */
   async doHttpMethod() { return ErrorHttpService.MethodNotAllowed(this.request).serve() }
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
    * @return {undefined|HttpServiceInterface}
    */
   dispatch(sreq) { throw Error("dispatch(sreq) not implemented.") }
}

class HttpServicesDispatchPath extends HttpServices {

   /** @type {Map<string,(typeof HttpServiceInterface)|HttpServices>} */
   dirMap
   /** @type {Map<string,(typeof HttpServiceInterface)|HttpServices>} */
   fileMap

   /**
    * @param  {...[string,typeof HttpServiceInterface|HttpServices]} path2Service
    */
   constructor(...path2Service) {
      super()
      this.dirMap = new Map()
      this.fileMap = new Map()
      for (const [ path, service ] of path2Service) {
         if (!path.startsWith("/")) throw Error(`Path »${path.substring(0,10)}${path.length>10?"...":""}« needs to start with '/'.`)
         if (path.indexOf("//") !== -1) throw Error(`Path »${path.substring(0,10)}${path.length>10?"...":""}« contains '//'.`)
         const isDir = path.endsWith("/");
         const pathname = isDir ? path.substring(0,path.length-1) : path;
         if (  this.fileMap.get(pathname) !== undefined
            || this.dirMap.get(pathname) !== undefined)
            throw Error(`Path »${path.substring(0,10)}${path.length>10?"...":""}« is not unique.`)
            ;
         isDir ? this.dirMap.set(pathname,service) : this.fileMap.set(pathname,service)
      }
   }
   ////////////////////////////
   // Overwrite HttpServices //
   ////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @return {undefined|HttpServiceInterface}
    */
   dispatch(sreq) {
      const pathname = sreq.pathname
      let subpath = pathname.substring(sreq.dispatchedPath.length)
      let foundService = this.fileMap.get(subpath)

      if (foundService === undefined) {
         while (subpath.length) {
            subpath = subpath.substring(0,subpath.lastIndexOf("/"))
            foundService = this.dirMap.get(subpath)
            if (foundService) break
            if (this.fileMap.has(subpath))
               return ErrorHttpService.NotFound(sreq)
         }
      }
      console.log("dispatch:",Boolean(foundService),sreq.dispatchedPath+subpath)
      if (!foundService)
         return foundService
      sreq.setDispatchedPath(sreq.dispatchedPath+subpath)
      if (foundService instanceof HttpServices)
         return foundService.dispatch(sreq)
      return new foundService(sreq)
   }
}

class HttpServicesDispatchSessionType extends HttpServices {

   /** @type {Map<string,HttpServices>} */
   dispatchSessionType

   /**
    * @param  {...[string,HttpServices]} sessiontypeDispatcher
    */
   constructor(...sessiontypeDispatcher) {
      super()
      this.dispatchSessionType = new Map(sessiontypeDispatcher)
   }

   ////////////////////////////
   // Overwrite HttpServices //
   ////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @return {undefined|HttpServiceInterface}
    */
   dispatch(sreq) {
      const session = sreq.session
      const dispatcher = session && !session.isExpired && this.dispatchSessionType.get(session.type)
      return dispatcher ? dispatcher.dispatch(sreq) : undefined
   }
}

class HttpServicesDispatchSequence extends HttpServices {
   /** @type {HttpServices[]} */
   sequence

   /**
    * @param {...HttpServices} sequence
    */
   constructor(...sequence) {
      super()
      this.sequence = sequence
   }

   ////////////////////////////
   // Overwrite HttpServices //
   ////////////////////////////
   /**
    * @param {ServiceRequest} sreq
    * @return {undefined|HttpServiceInterface}
    */
   dispatch(sreq) {
      for (const services of this.sequence) {
         const service = services.dispatch(sreq)
         if (service) return service
      }
   }
}

class HttpServer {
   static INADDR_ANY = "0.0.0.0" /*unsafe*/ // safer variants "localhost", "127.0.0.1"
   static IN6ADDR_ANY = "::" /*unsafe*/ // safer variants "localhost", "::1"

   #hostname
   #port
   #serviceDispatcher
   #sessions
   #context
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
      this.#context = new ServiceContext({
         dispatch: (sreq) => { return this.#serviceDispatcher.dispatch(sreq) },
         forward: async (sreq) => { return (this.#serviceDispatcher.dispatch(sreq) ?? ErrorHttpService.NotFound(sreq)).serve().catch(e => this.sendInternalServerError(sreq,e)) }
      }, {
         newSession: (service,sessionType) => {
            this.#sessions.newSession(service,sessionType)
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
      console.log("> headers:", sreq.reqHeaders)
      console.log("> exception:", e)
      return ErrorHttpService.Error(sreq,HTTP.INTERNAL_SERVER_ERROR,"Service failed.").send()
   }

   /**
    * @param {Request} req
    * @param {any} server
    * @returns {Promise<Response>}
    */
   async serve(req, server) {
      const clientIP = server.requestIP(req)
      const session = this.#sessions.sessionFromHeaders(req.headers)
      const sreq = new ServiceRequest(clientIP,this.#context,req,session)
      if (sreq.invalidURL) {
         return ErrorHttpService.InvalidURL(sreq).serve()
      }
      // console.log("headers",req.headers)
      // console.log("url", req.url)
      // console.log("req", req)
      // console.log("server", server)
      try {
         console.log(`client ${clientIP.address}:${clientIP.port},  origin ${req.headers.get("origin")},\n  ${req.method}${"       ".substring(0,7-req.method.length)}  ${sreq.url.pathname}`)
         const service = this.#serviceDispatcher.dispatch(sreq)
         return (service ?? ErrorHttpService.NotFound(sreq)).serve().catch(e => this.sendInternalServerError(sreq,e))
      }
      catch(e) {
         return this.sendInternalServerError(sreq,e)
      }
   }
}

class StaticFileWebService extends HttpService {
   /**
    * @returns {Promise<Response>}
    */
   async doGet() {
      const pathname = this.pathname
      const filename = "."+pathname
      /** @type {BunFile} */
      const file = Bun.file(filename)

      if (await file.exists()) {
         return this.response.sendFile(file)
      }

      return this.response.sendStatus(HTTP.NOT_FOUND,`File not found: ${pathname}`)
   }
}

class ForwardUploadWebService extends HttpService {
   /**
    * @returns {Promise<Response>}
    */
   async doPut() {
      const pathname = this.pathname

      if (pathname.startsWith("/forwarduploads")) {
         const sreq = this.request.newPath(pathname.replace("/forwarduploads","/uploads"))
         return this.context.dispatcher.forward(sreq)
      }

      return super.doPut()
   }
}

class UploadWebService extends HttpService {
   /**
    * @returns {Promise<Response>}
    */
   async doPut() {
      const pathname = this.pathname
      const nameoffset = this.dispatchedPath.length+1
      const name = pathname.substring(nameoffset).replaceAll(/[^-_.;:?a-zA-Z0-9üöäÖÄÜß ]/g,"")
      const file = Bun.file(name)

      if (!name) {
         return this.response.sendStatus(HTTP.BAD_REQUEST, pathname.length <= nameoffset ? `Empty file name` : `Invalid file name`)
      }
      if (await file.exists()) {
         return this.response.sendStatus(HTTP.CONFLICT, `File exists »${name}«`)
      }

      console.log("upload »"+name+"«")
      return this.request.receiveFile(file)
      .then( (/*writtenBytes*/) => this.response.sendOK(`Saved file »${name}«`))
      .catch( (e) => { console.log("receiveFile failed",e); return file.exists().then(exists => exists && file.delete()).catch((/*e*/) => false) })
      .then( response => response || this.response.sendStatus(HTTP.CONFLICT, `Writing file failed »${name}«`))
   }
}

class LoginWebService extends HttpService {
   /**
    * @returns {Promise<Response>}
    */
   async doPost() {
      const pathname = this.pathname
      const sreq = this.request

      const credentials = await sreq.json().catch((/*e*/) => ErrorHttpService.JSONSyntaxError(sreq).send())
      if (credentials instanceof Response)
         return credentials
      if (credentials.name === "test" && credentials.password === "test") {
         this.context.sessionProvider.newSession(this, LoginSessionType)
         return this.response.sendOK("Login succeeded.")
      }
      return this.response.sendStatus(HTTP.UNAUTHORIZED,"Login failure.")
   }
}

class EchoWebService extends HttpService {
   /**
    * @returns {Promise<Response>}
    */
   async doGet() {
      const pathname = this.pathname
      const reply = pathname.substring(this.dispatchedPath.length+(this.dispatchedPath?1:0))
      return this.response.sendOK(`${this.dispatchedPath} echos »${reply}«`)
   }
}

const echoServices = new HttpServicesDispatchPath(["/service1/",EchoWebService], ["/service2/",EchoWebService])
const commonServices = new HttpServicesDispatchPath(["/",StaticFileWebService], ["/echo/",echoServices])
const authorizedServices = new HttpServicesDispatchPath(["/forwarduploads/",ForwardUploadWebService], ["/uploads/",UploadWebService])
const sessionServices = new HttpServicesDispatchSessionType([LoginSessionType,authorizedServices])
const authorizationSwitch = new HttpServicesDispatchSequence(sessionServices,commonServices)
const services = new HttpServicesDispatchPath(["/",authorizationSwitch], ["/login", LoginWebService])
const server = new HttpServer(services, "localhost" /*HttpServer.INADDR_ANY*/, 9090).run()
console.log("Started HttpServer listening on "+server.hostname+":"+server.port)
