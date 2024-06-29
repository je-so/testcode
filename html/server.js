/**
 * HTML test server to support uploads.
 *
 * To do an upload use PUT /uploads/<filename>.
 * The server stores the file in its running directory.
 * Sub-directories are not supported.
 *
 * Use bun (download it from https://bun.sh/) to run the server.
 * > bun server.js
 *
 * The server listens on port 9090.
 * Set hostname to a local or private ip address to disable public access.
 */

const HTTP = {
   // 2XX - success
   OK: 200,
   NO_CONTENT: 204,
   // 3XX - redirect errors
   // 4XX - client errors
   BAD_REQUEST: 400,
   FORBIDDEN: 403,
   NOT_FOUND: 404,
   METHOD_NOT_ALLOWED: 405,
   CONFLICT: 409,
   UNPROCESSABLE_CONTENT: 422,
   // 5XX - server errors
   INTERNAL_SERVER_ERROR: 500,
   NOT_IMPLEMENTED: 501,
   SERVICE_UNAVAILABLE: 503,
}

const ContentType = {
   JSON: "application/json",
   HTML: "text/html",
   TEXT: "text/plain",
}

/**
 * Returns CORS headers used in an HTTP response.
 * @param {Request} req The HTTP request as received from the HTTP server.
 * @param {string} customHeaders A comma separated list of CORS-unsafe headers and custom headers, i.e. "User-Header, Content-Encoding, ...".
 * @returns {object} An object which contains headers for the HTTP response relevant to support CORS request.
 */
function buildCorsHeaders(req, customHeaders="") {
   const origin = req.headers.get("origin")
   return {
      "Access-Control-Allow-Origin": origin,
      "Access-Control-Expose-Headers": customHeaders,
      "Access-Control-Allow-Credentials": "true", // allow set-cookie
   }
}

/**
 * Create response with standard status codes.
 */
class Respond {
   // 2XX - success
   static ok(req, content, headers=null, customHeaders="") {
      return new Response(content, { status:HTTP.OK, headers:{ ...headers, ...buildCorsHeaders(req,customHeaders) }})
   }
   static noContent(req, headers=null) {
      return new Response(null, { status:HTTP.NO_CONTENT, headers:{ ...headers, ...buildCorsHeaders(req) }})
   }
   // 4XX - client errors
   static badRequest(req, err) {
      return new Response(String(err),{ status:HTTP.BAD_REQUEST, headers:buildCorsHeaders(req) })
   }
   static forbidden(req, err) {
      return new Response(String(err), { status:HTTP.FORBIDDEN, headers:buildCorsHeaders(req) })
   }
   static notFound(req) {
      return new Response(null, { status:HTTP.NOT_FOUND, headers:buildCorsHeaders(req) })
   }
   static methodNotAllowed(req, allowedMethods/*"GET,PUT,..."*/) {
      return new Response(null, { status:HTTP.METHOD_NOT_ALLOWED, headers:{ Allow:allowedMethods, ...buildCorsHeaders(req) }})
   }
   static conflict(req, err) {
      return new Response(String(err), { status:HTTP.CONFLICT, headers:buildCorsHeaders(req) })
   }
   static unprocessable(req, err) {
      return new Response(String(err), { status:HTTP.UNPROCESSABLE_CONTENT, headers:buildCorsHeaders(req) })
   }
   // 5XX - server errors
   static error(req, err) {
      return new Response(`Internal Error: ${String(err)}`, { status:HTTP.INTERNAL_SERVER_ERROR, headers:buildCorsHeaders(req) })
   }
   static notImplemented(req) {
      return new Response(null, { status:HTTP.NOT_IMPLEMENTED, headers:buildCorsHeaders(req) })
   }
}

//
// global vars
//

// initialized at bottom of file

let clientLogs
let clientSessions

//
// Http Server
//

Bun.serve({
   port: 9090,
   hostname: "0.0.0.0"/*unsafe*/, // "localhost", "127.0.0.1", "::1"
   async fetch(req, server) {
      const ip = server.requestIP(req)
      const corsHeaders = ({
            "Access-Control-Allow-Methods": "DELETE,GET,HEAD,OPTIONS,POST,PUT",
            "Access-Control-Allow-Origin": req.headers.get("origin"),
            "Access-Control-Allow-Headers": req.headers.get("access-control-request-headers"),
            "Access-Control-Allow-Credentials": "true",
         })
      const url = (() => { try { return new URL(req.url) } catch(e) { return null } })()
      const pathname = (() => { try { return url ? decodeURI(url.pathname).replaceAll("/../","/").replaceAll(/\/\/+/g,"/") : "" } catch(e) { return "" } })()
      if (!pathname.startsWith("/")) {
         return Respond.badRequest(req, `URL path is wrong »${req.url}«`)
      }

      const clientReq = new ClientRequest(req, url, pathname, ip)

      // console.log("clientReq",clientReq)
      // console.log("url",url)
      // console.log("ip",ip)
      // console.log("headers",req.headers)
      console.log(ip.address,req.method,pathname,"insession="+(clientReq.session!=null))

      if (req.method == "OPTIONS") {
         return Respond.noContent(req,corsHeaders)
      }

      if (pathname == "/register") {
         if (req.method != "POST" && req.method != "DELETE")
            return Respond.methodNotAllowed(req, "DELETE,POST")
         return req.text().then( clientid => {
            if (req.method == "POST")
               return clientSessions.registerClient(req, ip, clientid)
            else
               return clientSessions.unregisterClient(req, ip, clientid)
         }).catch( err => {
            return Respond.error(req, err)
         })
      }

      //
      // Test Server-Sent Events
      //
      if (pathname == "/sse") {
         const signal = req.signal
         const stream = new ReadableStream({
            type: "direct",
            async pull(controller) {
               console.log("======= start stream pull =======")
               try {
                  var count = 1
                  while (!signal.aborted) {
                     const bytes = await controller.write(`data: next event ${count++}\n\n`)
                     if (bytes <= 0) // connection closed from client ?
                        break
                     await Bun.sleep(2000)
                  }
               }
               catch(exc) {
                  // write called after controller.close() has been called
               }
               finally {
                  controller.close()
               }
               console.log("========== end stream pull ============")
            },
            cancel(controller) {
               controller.close()
            },
         })
         return Respond.ok(req, stream, { "Content-Type":"text/event-stream" })
      }

      if (pathname.startsWith("/model/log/")) {
         return clientLogs.doRequest(req, ip, pathname)
      }

      if (req.method === "PUT") {
         if (pathname.startsWith("/uploads/")) {
            const name = pathname.substring(9).replaceAll(/[^-_.;:?a-zA-Z0-9üöäÖÄÜß]/g,"")
            const file = Bun.file(name)

            if (!name) {
               console.log("status:400")
               if (pathname.length == 9)
                  return Respond.badRequest(req, `Empty file name`)
               else
                  return Respond.badRequest(req, `Invalid file name`)
            }

            if (await file.exists()) {
               console.log("status:409")
               return Respond.conflict(req, `File exists »${name}«`)
            }

            console.log("upload »"+name+"«")
            await Bun.write(file, req)
            return Respond.ok(req, `Saved file »${name}«`)
         }
      }

      if (req.method === "GET") {
         const file = Bun.file("."+pathname)

         if (await file.exists()) {
            return new Response(file, {
               status:200, headers: { ...corsHeaders, "Content-Type":file.type }
            })
         }

         console.log("status:404")
         return Respond.notFound(req)
      }

      console.log("status:501")
      return Respond.notImplemented(req)
   }

});

class ClientLog {
   len=0
   content=[]
   ip
   constructor(ip) {
      this.ip = ip
   }
   addText(text) {
      this.content.push(text)
      this.len += text.length
   }
}

class ClientLogs {
   logs = new Map(/*ip.address->ClientLog*/)
   info() {
      const result = []
      for (const cl of this.logs) {
         result.push( { ip: cl[0], loglen: cl[1].len })
      }
      return result
   }
   doRequest(req, ip, pathname) {
      const verb = pathname.substring("/model/log/".length)
      const ipaddr = ip.address
      let allowedMethods = "GET"
      if (verb == "info") {
         if (req.method == "GET")
            return Promise.resolve().then( () => {
               return Respond.ok(req, JSON.stringify(this.info()))
            }).catch( err => {
               return Respond.error(req, err)
            })
      }
      else if (verb == "" || verb == "content") {
         if (req.method == "POST") {
            // append content
            !this.logs.has(ipaddr) && this.logs.set(ipaddr, new ClientLog(ipaddr))
            const clientLog = this.logs.get(ipaddr)
            return req.text().then( text => {
               clientLog.addText(text)
               return Respond.noContent(req)
            }).catch( err => {
               return Respond.error(req, err)
            })
         }
         else if (req.method == "GET") {  // get content (range support)
            const range = req.headers.get("range")
            const clientLog = this.logs.get(ipaddr)
            if (!clientLog || !clientLog.len)
               return Respond.noContent(req)
            else
               return Respond.ok(req, clientLog.content.at(-1), { "Content-Type":ContentType.TEXT })
         }
         allowedMethods="GET,POST"
      }
      else {
         return Respond.notFound(req)
      }
      return Respond.methodNotAllowed(req, allowedMethods)
   }
}

clientLogs = new ClientLogs()

class ClientSession {
   ipaddr
   clientid
   sessionid
   constructor(ip, clientid) {
      this.ipaddr = ip.address
      this.clientid = clientid
      this.sessionid = crypto.randomUUID()
   }
}

class ClientSessions {
   sessions = new Map()

   isClientID(clientid) {
      return this.sessions.get(clientid) != null
   }
   newSession(ip, clientid) {
      if (this.sessions.get(clientid) == null) {
         const clientSession = new ClientSession(ip, clientid)
         this.sessions.set(clientSession.sessionid, clientSession)
         this.sessions.set(clientid, clientSession)
         return clientSession
      }
      return null
   }
   deleteSession(clientid, sessionid) {
      this.sessions.delete(clientid)
      this.sessions.delete(sessionid)
   }
   sessionID(req) {
      const cookies = req.headers.get("cookie")
      const start = cookies?.indexOf("sessionid=")
      if (start === undefined || start < 0)
         return ""
      const end = cookies.indexOf(";",start)
      return cookies.substring(cookies.indexOf("=",start)+1, Math.max(0,end) || undefined)
   }
   getSession(req, url) {
      const clientid = url.searchParams.get("clientid")
      const sessionid = this.sessionID(req)
      const session = this.sessions.get(sessionid)
      console.log("cid",clientid,"sid",sessionid)
      !clientid && (console.log("getSession no clientid"))
      !sessionid && (console.log("getSession no sessionid"))
      return session && session.clientid === clientid && session || undefined
   }
   registerClient(req, ip, clientid) {
      if (clientid.length < 30)
         return Respond.unprocessable(req, `Client id not allowed. id=${clientid}`)
      if (this.isClientID(clientid))
         return Respond.conflict(req, `Client already registered. id=${clientid}`)
      const session = this.newSession(ip, clientid)
      return new Response(`{ "ip":"${session.ipaddr}", "clientid:${clientid}", "sessionid":"${session.sessionid}" }`, { status:HTTP.OK, headers: { ...buildCorsHeaders(req), "set-cookie": "sessionid="+session.sessionid+"; Max-Age=3600; HttpOnly; SameSite=None; Path=/", "Content-Type":"application/json" } })
   }
   unregisterClient(req, ip, clientid) {
      const sessionid = this.sessionID(req)
      if (!this.sessions.has(clientid) || !this.sessions.has(sessionid))
         return Respond.forbidden(req, `Client not registered. id=${clientid}`)
      if (this.sessions.get(sessionid).ipaddr !== ip.address)
         return Respond.forbidden(req, `Client has wrong ip address. ipaddr=${ip.address}`)
      this.deleteSession(clientid, sessionid)
      return Respond.noContent(req)
   }
}

clientSessions = new ClientSessions()

class ClientRequest {
   req
   url
   pathname
   ip
   session
   constructor(req, url, pathname, ip) {
      this.req = req
      this.url = url
      this.pathname = pathname
      this.ip = ip
      this.session = clientSessions.getSession(req, url)
   }
}
