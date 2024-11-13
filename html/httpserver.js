
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
   static Status = {
      /////////////////
      // 2XX success //
      /////////////////
      OK: 200,
      NO_CONTENT: 204,
      /////////////////////
      // 3XX redirection //
      /////////////////////

      //////////////////////
      // 4XX client error //
      //////////////////////
      BAD_REQUEST: 400,
      FORBIDDEN: 403,
      NOT_FOUND: 404,
      METHOD_NOT_ALLOWED: 405,
      CONFLICT: 409,
      UNPROCESSABLE_CONTENT: 422,
      //////////////////////
      // 5XX server error //
      //////////////////////
      INTERNAL_SERVER_ERROR: 500,
      NOT_IMPLEMENTED: 501,
      SERVICE_UNAVAILABLE: 503,
   }
   static ContentType = {
      JSON: "application/json",
      HTML: "text/html",
      TEXT: "text/plain",
   }
}

class HttpResponse {
   static Status = HTTP.Status
   static ContentType = HTTP.ContentType

   static sendOK(msg=null) { return new Response(msg, { status:200 }) }
   static sendError(status, msg=null) { return new Response(msg, { status }) }
   static sendMethodNotAllowedError(method, pathname) { return this.sendError(HTTP.Status.METHOD_NOT_ALLOWED,`Method ${method} not supported for path ${pathname}.`) }
   static sendInternalServerError(e) {
      if (e) {
         console.log("===== start exception =====")
         console.log(e)
         console.log("===== end exception =====")
      }
      return this.sendError(HTTP.Status.INTERNAL_SERVER_ERROR,"Service failed.")
   }
   static sendInvalidURLError(url) { return this.sendError(HTTP.Status.BAD_REQUEST,`URL ${url} is not valid.`) }

   // TODO: add CORS support

   /**
    * Returns CORS headers used in an HTTP response.
    * @param {Request} req The HTTP request as received from the HTTP server.
    * @param {string} customHeaders A comma separated list of CORS-unsafe headers and custom headers, i.e. "User-Header, Content-Encoding, ...".
    * @returns {object} An object which contains headers for the HTTP response relevant to support CORS request.
    */
   static buildCorsHeaders(req, customHeaders="") {
      const origin = req.headers.get("origin")
      return {
         "Access-Control-Allow-Origin": origin,
         "Access-Control-Expose-Headers": customHeaders,
         "Access-Control-Allow-Credentials": "true", // allow set-cookie
      }
   }
}

class HttpCookie {
   constructor(name, value) {
      this.name=String(name)
      this.value=String(value).replaceAll(";","")
      this.maxage=-1
      this.path="/"
      this.httpOnly=false
      this.secure=false
      this.domain=null
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
    * @param {Headers} headers The headers of the
    */
   append(headers) {
      headers.append("Set-Cookie",this.name + "=" + this.value
         +("; SameSite=Strict")
         +(this.path ? "; Path="+this.path : "")
         +(this.httpOnly ? "; HttpOnly" : "")
         +(this.maxage>=0 ? "; Max-Age="+this.maxage : "")
         +(this.secure ? "; Secure" : "")
      )
   }
}

class HttpSession {
   /**
    * @type {Map<string,HttpSession}
    */
   static sessions=new Map()
   static testSessionForExpiration=[]
   static generateSessionID() { return crypto.randomUUID().replaceAll("-","").toUpperCase() }
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
      return session
   }
   static newSession(service) {
      const session = new HttpSession()
      session.append(service)
      return session
   }
   static testExpiration() {
      if (this.testSessionForExpiration.length) {
         const session = this.testSessionForExpiration.pop()
         session.isExpired() && session.invalidate(null)
      }
      else
         this.testSessionForExpiration = [...this.sessions.values()]
   }
   #sessionid
   #maxage
   #expiration
   #attributes
   constructor(sessionid) {
      this.#sessionid = sessionid ? sessionid : HttpSession.generateSessionID()
      this.#maxage = 3600 // valid for 1 hour
      this.#expiration = this.#maxage + Date.now()/1000
      this.#attributes = {}
   }
   getID() { return this.#sessionid }
   isValid() { return this.#sessionid !== "" }
   isExpired() { return Date.now()/1000 > this.#expiration }
   attributes() { return Object.assign({}, this.#attributes) }
   getAttribute(name) { return this.#attributes[String(name)] }
   setAttribute(name, value) { this.#attributes[String(name)] = value }
   append(service) {
      if (!this.#sessionid) return
      const sessionCookie = new HttpCookie("JS_SESSIONID", this.#sessionid)
      sessionCookie.setMaxAge(this.#maxage)
      sessionCookie.append(service.getResponseHeaders())
      HttpSession.sessions.set(this.#sessionid, this)
   }
   invalidate(service) {
      if (!this.#sessionid) return
      const sessionCookie = new HttpCookie("JS_SESSIONID", this.#sessionid)
      sessionCookie.setMaxAge(0)
      service && sessionCookie.append(service.getResponseHeaders())
      HttpSession.sessions.delete(this.#sessionid)
      this.#sessionid = ""
   }
}

class HttpSessionWrapper {
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
    * @type {HttpHeaders}
    */
   #responseHeaders
   /**
    * @type {null|HttpSession}
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
      this.#url = url
      this.#pathname = pathname
      this.#clientIP = clientIP
      this.#responseHeaders = new Headers()
      const session = HttpSession.getSession(this)
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
   getSession(createIfNotExist=true) {
      if (!this.#session && createIfNotExist) {
         this.#session = new HttpSessionWrapper(this, HttpSession.newSession(this))
      }
      return this.#session
   }
   getRequest() { return this.#request }
   getResponseHeaders() { return this.#responseHeaders }
   getURL() { return this.#url }
   setContentType(value) { this.#responseHeaders.set("Content-Type", value) }
   async serve() {
      const req = this.#request
      switch(req.method) {
         case "GET": return this.doGet(req, this.#pathname)
         case "POST": return this.doPost(req, this.#pathname)
         case "PUT": return this.doPut(req, this.#pathname)
         default: return this.doUnknown(req, this.#pathname)
      }
   }
   async doGet(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req.method, pathname) }
   async doPost(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req.method, pathname) }
   async doPut(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req.method, pathname) }
   async doUnknown(req, pathname) { return HttpResponse.sendMethodNotAllowedError(req.method, pathname) }
}

class HttpServiceDispatcher {
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
            console.log(`${clientIP.address}  ${req.method}  ${pathname}`)
            if (service)
               return new service(req, url, pathname, clientIP).serve().catch(e => HttpResponse.sendInternalServerError(e))
            else
               return HttpResponse.sendError(HTTP.Status.NOT_FOUND,`Path ${pathname} could not be mapped to a service.`)
         }
         catch(e) {
            return HttpResponse.sendInternalServerError(e)
         }
      }
      catch(e) {
         return HttpResponse.sendInvalidURLError(req.url)
      }
   }
}

class StaticFileWebService extends HttpService {
   async doGet(req, pathname) {
      const file = Bun.file("."+pathname)

      console.log("id=",this.getSession().getID())

      if (await file.exists()) {
         const headers = this.getResponseHeaders()
         this.setContentType(file.type)
         console.log("response headers",headers)
         return new Response(file, { status:200, headers })
      }

      return HttpResponse.sendError(HTTP.Status.NOT_FOUND,"file not found: "+pathname)
   }
}

class UploadWebService extends HttpService {

   async doPut(req, pathname) {
      const name = pathname.substring(9).replaceAll(/[^-_.;:?a-zA-Z0-9üöäÖÄÜß]/g,"")
      const file = Bun.file(name)

      if (!name) {
         return HttpResponse.sendError(HTTP.BAD_REQUEST, pathname.length == 9 ? `Empty file name` : `Invalid file name`)
      }
      if (await file.exists()) {
         return HttpResponse.sendError(HTTP.CONFLICT, `File exists »${name}«`)
      }

      console.log("upload »"+name+"«")
      await Bun.write(file, req)
      return HttpResponse.sendOK(`Saved file »${name}«`)
   }
}

const services = new HttpServiceDispatcher(StaticFileWebService, ["/uploads",UploadWebService])
const hs = new HttpServer(services, "localhost" /*HttpServer.INADDR_ANY*/, 9090).run()
console.log("Started HttpServer listening on "+hs.hostname+":"+hs.port)
