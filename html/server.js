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
 * Set hostname to a local or private ip address to disallow public access.
 */

Bun.serve({
   port: 9090,
   hostname: "0.0.0.0"/*unsafe*/, // "localhost", "127.0.0.1", "::1"
   async fetch(req, server) {
      const ip = server.requestIP(req)
      const corsHeaders = ({
            "Access-Control-Allow-Methods": "GET,HEAD,OPTIONS,POST,PUT",
            "Access-Control-Allow-Origin": req.headers.get("origin"),
            "Access-Control-Allow-Headers": req.headers.get("access-control-request-headers"),
         })
      const url = (() => { try { return new URL(req.url) }  catch { return null } })()
      const pathname = url ? decodeURI(url.pathname).replaceAll("/../","/").replaceAll(/\/\/+/g,"/") : ""
      if (!pathname.startsWith("/")) {
         return new Response(`URL path is wrong »${req.url}«`,{ status:400 })
      }

      console.log(ip.address,req.method,pathname)
      // console.log(req.headers)

      if (req.method === "OPTIONS") {
         return new Response(null, { status:204, headers:corsHeaders })
      }

      if (req.method === "PUT") {
         if (pathname.startsWith("/uploads/")) {
            const name = pathname.substring(9).replaceAll(/[^-_.;:?a-zA-Z0-9üöäÖÄÜß]/g,"")
            const file = Bun.file(name)

            if (!name) {
               console.log("status:400")
               if (pathname.length == 9)
                  return new Response(`Empty file name`,{ status:400, headers: corsHeaders })
               else
                  return new Response(`Invalid file name`,{ status:400, headers: corsHeaders })
            }

            if (await file.exists()) {
               console.log("status:409")
               return new Response(`File exists »${name}«`,{
                  status:409, headers: corsHeaders
               })
            }

            console.log("upload »"+name+"«")
            await Bun.write(file, req)
            return new Response(`Saved file »${name}«`,{
               status:200, headers: corsHeaders
            })

         }
      }

      if (req.method === "GET") {
         const file = Bun.file("."+pathname)

         if (await file.exists()) {
            return new Response(file, {
               status:200, headers: { ...corsHeaders, type: file.type }
            })
         }

         console.log("status:404")
         return new Response(null, {status:404})
      }

      console.log("status:501")
      return new Response(null, {status:501})
   }

});
