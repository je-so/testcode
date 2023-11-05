/**
 * The namespace of the imported API from pdfkit library.
 * @namespace pdfkit
 */
import * as _pdfkit from "pdfkit"
/**
 * The namespace of the imported API from pdflib library.
 * @namespace pdflib
 */
import * as pdflib from "pdflib"
/**
 * The namespace of the imported API from blob-stream library.
 * @namespace blobstream
 */
import * as _blobstream from "blob-stream"
/**
 * The namespace of the imported API from fontkit library.
 * @namespace fontkit
 */
import fontkit from "fontkit"

const pdfkit={}
pdfkit.PDFDocument = _pdfkit.PDFDocument ?? window.PDFDocument
pdfkit.PageSizes = pdfkit.PDFDocument.SIZES
const blobstream={}
blobstream.blobStream = _blobstream.blobStream ?? window.blobStream

const module = {
   name: "jsm-pdf",
   exports: {},
}

const errmsg=(msg) => `${module.name}: ${msg}`
const logConsole=(e) => { console.log(e); return e }
const newError=(msg,cause) => logConsole(new Error(errmsg(msg),cause ? {cause} : undefined))
const throwError=(msg,cause) => { throw newError(msg,cause) }
/**
 * Checks if value is a Promise a Promise is returned which calls thenCallbach after Promise has been resolved.
 * If value is not a Promise it is returned unchanged.
 * @param {any|Promise<any>} value    The value which is checked for being a Promise.
 * @param {(any)=>void} thenCallback  The function which is called if the Promise is resolved. The thenCallback should assign the given argument to the same value as ifPromise's return value is asigned to.
 * @returns {any|Promise<any>} Either value or a Promise which calls thenCallback if value is resolved.
 */
const ifPromise=(value, thenCallback) =>
                     value instanceof Promise
                     ? value.then( v => { thenCallback(v); return v })
                     : value
/**
 * If one the asyncOps is a Promise (==async op in progresss) a Promise is returned which resolves
 * if all asyncOps are fulfilled and then calls createValueCallback.
 * The return value of createValueCallback is used to resolve the returned Promise.
 * If none of the provided asyncOps is a Promise createValueCallback is called without delay and
 * its return value is returned from this function.
 *
 * @param {(any|Promise<any>)[]} asyncOps  Array of values or running async operations (Promise).
 * @param {()=>any} createValueCallback    The callback which computes the result and returns it.
 * @param {undefined|(any)=>void} thenCallback The function which is called if onReady returns a Promise and this Promise is resolved. The thenCallback should assign the given argument to the same value as onReady's return value is asigned to. If thenCallback is undefined it is ignored.
 * @returns {any|Promise<any>} If asyncOps contains a Promise a Promise is returned else the return value of createValueCallback.
 */
const onReady=(asyncOps, createValueCallback, thenCallback) =>
         asyncOps.some( op => op instanceof Promise)
         ? Promise.all(asyncOps).then(_ => { const v = createValueCallback(); if (thenCallback) thenCallback(v); return v })
         : createValueCallback()
/**
 * Replacement for typeof. For a null value "null" is returned instead of "object".
 * For an object value of a sub-type of Object the name of its constructor function is returned.
 *
 * A value out of ["bigint","boolean","null","number","NaN","Infinity","object","string","symbol","undefined"] or v's constructor name (<tt>v.constructor.name</tt>) is returned.
 * @param {any} v    The value whose type is to be determined.
 * @returns {string} The type-name of the given value.
 */
const typename=(v) => typeof v === "number" ? (isFinite(v) ? "number" : isNaN(v) ? "NaN" : "Infinity")
         : typeof v !== "object" ? typeof v
         : v === null ? "null"
         : v.constructor === Object ? "object"
         : v.constructor?.name ?? "object"

const assertFunction=(value,argName) => { if (typeof value !== "function") throwError(`expect argument »${argName}« of type function not ${typename(value)}`) ; return value }
const assertNumber=(value,argName) => { if (typeof value !== "number" || !isFinite(value)) throwError(`expect argument »${argName}« of type number not ${typename(value)}`) ; return value }
const assertString=(value,argName) => { if (typeof value !== "string") throwError(`expect argument »${argName}« of type string not ${typename(value)}`) ; return value }
const assertType=(Type,value,argName) => { if (!(value instanceof Type)) throwError(`expect argument »${argName}« of type ${Type.name} not ${typename(value)}`) ; return value }

if (!pdfkit.PDFDocument) throwError("could not import PDFDocument from pdfkit")
if (!pdfkit.PageSizes) throwError("could not import PageSizes from pdfkit")
if (!blobstream.blobStream) throwError("could not import blobStream from blobstream")


/////////////////////
// Unit Conversion //
/////////////////////

// All coordinate and size values are measured in points.
// If you want to use other units you need to convert them into points
// and returned values (size, position, ...) back into other units.

/**
 * Supports conversion of values from millitmeter, points, percent into values measured in points.
 */
class LengthUnit {
   static assertBaseValue(baseValue,argName) { return baseValue !== undefined ? assertNumber(baseValue,"baseValue") : throwError(`expect argument »${argName}« of absolute length unit not '%'-unit`) }

   static mm = new LengthUnit("mm", mmValue => mmValue * 72 / 25.4, ptValue => ptValue * 25.4 / 72)
   static pt = new LengthUnit("pt", ptValue => ptValue, ptValue => ptValue)
   static percent = new LengthUnit("%", (percentValue, argName, baseValue) => percentValue * this.assertBaseValue(baseValue,argName) / 100,
                           (ptValue, argName, baseValue) => 100 * this.assertBaseValue(baseValue,argName) / ptValue)

   /**
    * Constructs immutable object which manages conversion of values into values in points.
    * @param {string} name  Abbreviated name of the physical unit of a value.
    * @param {(number,argName,number?) => number} toPt    Conversion function to convert a value from given unit into a value in points.
    * @param {(number,argName,number?) => number} fromPt  Conversion function to convert a value from points into a value in given unit.
    */
   constructor(name, toPt, fromPt) {
      this.name = name
      this.convertToPt = toPt
      this.convertFromPt = fromPt
      Object.freeze(this)
   }

}

/**
 * A value with a physical or relative unit. A percent value is relative to a base value in points.
 */
class uValue
{
   static #valuePattern=/^([-+]?[0-9]+([.][0-9]+)?)\s?(pt|mm|%)?$/
   static #unitsMap=new Map([ ["mm", LengthUnit.mm], ["%", LengthUnit.percent], ["pt", LengthUnit.pt]])

   static parse(strValue) {
      const match=String(strValue).match(this.#valuePattern)
      if (!match)
         throwError(`expect value of format "[+-]<digits>[.<digits>][ ][pt|mm|%]" not "${String(strValue)}"`)
      return new uValue(assertNumber(parseFloat(match[1]),"value"), match[3])
   }

   static get pt() { return LengthUnit.pt }
   static get mm() { return LengthUnit.mm }
   static get percent() { return LengthUnit.percent }
   static get ['%']()   { return LengthUnit.percent }

   static convertToPt(number,argName,baseValue) {
      if (number instanceof uValue)
         number = number.convertToPt(argName,baseValue)
      return assertNumber(number,argName)
   }

   #unit
   #value

   /**
    * Constructs a value measured in some unit. Percent values are relative to a base value.
    * @param {number} value            The value which is measured in some unit.
    * @param {LengthUnit|string} unit  The length unit the value is measured in.
    */
   constructor(value, unit="pt") {
      this.#unit = ( unit instanceof LengthUnit ? unit
                    : typeof unit === "string" ? (uValue.#unitsMap.get(unit) ?? throwError(`Expect unit one of ['mm','pt','%'] not '${unit}'`))
                    : throwError(`Expect unit of type LengthUnit|string not ${typename(unit)}`))
      this.#value = assertNumber(value, "value")
   }

   convertToPt(argName,baseValue) { return this.#unit.convertToPt(this.#value,argName,baseValue) }

   getUnit()   { return this.#unit }
   getValue()  { return this.#value }
}


///////////////////////////
// Event Listener Helper //
///////////////////////////

class EventListeners {

   #eventListeners={}

   constructor(...events) {
      for (const event of events) {
         this.#eventListeners[event] = [/*no listeners*/]
      }
   }

   /**
    * Returns true if event is supported.
    * @param {string} event  The name of the event.
    * @returns {boolean}     <tt>true</tt> if event is known.
    */
   isSupported(event) { return Array.isArray(this.#eventListeners[event]) }

   #assertListeners(listeners,event) { return Array.isArray(listeners) ? listeners : throwError(`unknown event »${String(event)}«`) }

   #getListeners(event) { return this.#assertListeners(this.#eventListeners[event],event) }

   addListener(event, callback) {
      const listeners = this.#getListeners(event)
      if (-1 === listeners.findIndex(assertFunction(callback,"callback")))
         listeners.push(callback)
   }

   removeListener(event, callback) {
      const listeners = this.#getListeners(event)
      const i = listeners.findIndex(assertFunction(callback,"callback"))
      if (-1 !== i)
         listeners.splice(i,1)
   }

   callListener(event, _this, ...args) {
      const listeners = this.#getListeners(event)
      for (const listener of listeners)
         listener.call(_this,args)
   }
}


////////////////////////////
// Binary PDF Data Helper //
////////////////////////////

/**
 * Holds result after PDF document has been closed (see {@link Document.close}).
 *
 * Also supports merging of PDF documents only available as binary data.
 */
class PDFData
{
   #promise
   #resolve
   #reject

   constructor()
   {
      this.#promise = new Promise( (resolve,reject) => {
         this.#resolve = resolve
         this.#reject  = reject
      })
   }

   /**
    * Sets PDF document content or Error.
    * The set data could be read with {@link toBlob} or any other to<Type> function.
    * @param {Blob|ArrayBuffer|Uint8Array|Error} blob Contains PDF content as binary data or Error object.
    */
   set(blob) {
      if (PDFData.checkBinaryType(blob)) {
         this.#resolve(PDFData.toBlob(blob))
      }
      else {
         this.#reject( blob instanceof Error ? blob : newError(`Unsupported data type ${typename(blob)}`) )
      }
   }

   //
   // binary data is readable in different formats
   //

   /** @returns {Promise<Blob>} PDF document content as Blob. */
   async toBlob() { return this.#promise }
   /** @returns {Promise<ArrayBuffer>} PDF document content as ArrayBuffer. */
   async toArrayBuffer() { return this.#promise.then( blob => blob.arrayBuffer() ) }
   /** @returns {Promise<string>} PDF document content as object URL. */
   async toObjectURL() { return this.#promise.then( blob => URL.createObjectURL(blob)) }
   /** @returns {Promise<string>} PDF document content as data URL. */
   async toDataURL() {
      return this.#promise.then( blob => new Promise((resolve,reject) => {
         const reader = new FileReader()
         reader.onload = (event => resolve(reader.result))
         reader.onerror = (event => reject(newError(`error converting blob to data url`,reader.error)))
         reader.readAsDataURL(blob)
      }))
   }

   //
   // conversion of binary data into Blob|ArrayBuffer
   //

   /** Returns union type of all types accepted as PDF data, which are convertible to Blob. */
   static get binaryType() { return "PDFData|Blob|ArrayBuffer|Uint8Array|string" }
   /**
    * Checks if data type is supported.
    * @param {any} bytes Content of PDF document as binary data.
    * @returns {boolean} <tt>true</tt> in case parameter bytes is convertible into type Blob.
    */
   static checkBinaryType(bytes) { return (bytes instanceof Blob) || (bytes instanceof PDFData) || (bytes instanceof Uint8Array) || (bytes instanceof ArrayBuffer) || typeof bytes === "string" }
   /**
    * Converts supported binary data type into type Blob.
    * @param {any} bytes  Content of PDF document as binary data.
    * @returns {Promise<ArrayBuffer>} Binary data converted into type Blob.
    */
   static async toBlob(bytes) {
      if (bytes instanceof Blob)
         return bytes
      if (bytes instanceof PDFData)
         return bytes.toBlob()
      if (bytes instanceof Uint8Array || bytes instanceof ArrayBuffer)
         return new Blob([bytes], { type: 'application/pdf' })
      if (typeof bytes === "string" &&
          (bytes.startsWith("data:") || bytes.startsWith("blob:"))) {
         return fetch(bytes)
            .then(resp => resp.blob())
            .catch(e => throwError("PDF data contains invalid data or object URL",e))
      }
      throwError("PDF data contains no data or object URL")
   }
   /**
    * Converts supported binary data type into type ArrayBuffer.
    * @param {any} bytes  Content of PDF document as binary data.
    * @returns {Promise<ArrayBuffer>} Binary data converted into type ArrayBuffer.
    */
   static async toArrayBuffer(bytes) {
      if (bytes instanceof Blob)
         return bytes.arrayBuffer()
      if (bytes instanceof PDFData)
         return bytes.toArrayBuffer()
      if (bytes instanceof Uint8Array)
         return bytes.buffer
      if (bytes instanceof ArrayBuffer)
         return bytes
      if (typeof bytes === "string" &&
          (bytes.startsWith("data:") || bytes.startsWith("blob:"))) {
         return fetch(bytes)
            .then(resp => resp.arrayBuffer())
            .catch(e => throwError("PDF data contains invalid data or object URL",e))
      }
      throwError("PDF data contains no data or object URL")
   }

   //
   // handling of raw PDF documents (binary data)
   //

   /**
    * The function merges one or more PDF documents into one and returns the merged PDF data.
    * A single PDF document is provided either by its data (Blob or ArrayBuffer)
    * or a wrapper object with properties bytes and pages.
    * Pages is an array holding all page numbers (starting from 1) to be copied into the resulting document.
    *
    * The pages are copied in the same order as stored in pages array.
    * You could supply a page number more than once in which case this page is copied more than once.
    * In case pages is empty or undefined all pages are copied.
    * @param {(ArrayBuffer|{bytes:ArrayBuffer,pages?:number[]})[]} pdfData  Data content of one or more PDF documents. If they are wrapped in an object set property blob to content of PDF and supply an pages array to select the pages for copy and also their order.
    * @returns {PDFData} A {@link PDFData wrapper object} for the merged data. Use {@link toBlob} or {@link toDataURL} or another to<Type> function to access the merged data.
    */
   static merge(...pdfData) {
      if (!pdfData.length)
         return null

      async function wrapData()
      {
         const wrappedPDfData = pdfData.map( data => {
            const isWrapper = typeof data === "object" && ("bytes" in data)
            const bytes = isWrapper ? data.bytes : data
            const pages = isWrapper ? data.pages : undefined
            if (!PDFData.checkBinaryType(bytes))
               throwError(`expect ${isWrapper ? 'bytes property':'PDF data'} of type (${PDFData.binaryType}${isWrapper?'':'|{bytes:PDFData|...}'}) not ${typename(bytes)}`)
            if (pages !== undefined && !Array.isArray(pages))
               throwError(`expect pages property of type (Array|undefined) not ${typename(pages)}`)
            return { bytes, pages }
         })

         for (const wrapper of wrappedPDfData) {
            wrapper.bytes = await PDFData.toArrayBuffer(wrapper.bytes)
         }

         return wrappedPDfData
      }

      const data=new PDFData()

      wrapData()
      .then( wrappedPDfData => pdflibMerge(wrappedPDfData))
      .then( mergedData => data.set(mergedData))
      .catch( e => data.set(e))

      return data
   }

   /**
    * Returns an array of all pages defined in the document.
    * @param {ArrayBuffer} pdfData The bytes of the PDF document.
    * @returns {number[]} Array which lists all contained page numbers.
    */
   static async pageNumbers(pdfData) {
      if (!PDFData.checkBinaryType(pdfData))
         throwError(`expect PDF data of type (${PDFData.binaryType}) not ${typename(pdfData)}`)
      return pdflibPageNumbers(await PDFData.toArrayBuffer(pdfData))
   }
}


/////////////////////////
// Font Files Handling //
/////////////////////////

/**
 * TODO:
 */
class FontData
{
   static loaded=new Map()

   constructor(fontID, url, data) {
      this.data = ifPromise(data, data => this.data = data)
      this.fileName = url.substring(url.indexOf("/")+1)
      this.id = fontID
      this.url = url

      FontData.loaded.set(fontID, this)
   }

   get asyncOperation() { return this.data }
   get error() { return this.data instanceof Error ? this.data : undefined }
   get isError() { return this.error !== undefined }

   static load(fontID, url)
   {
      return FontData.loaded.get(fontID) ?? new FontData(fontID, url, fetch(url).then(
         response => {
            if (!response.ok)
               throwError(`can not load font from url »${url}«`,`${response.status}: ${response.statusText}`)
            return response.arrayBuffer()
      }).catch( e => this.data = e ))
   }

   static get(fontID) {
      const font = FontData.loaded.get(fontID)
      if (!font)
         throwError(`fontID '${fontID}' is undefined`)
      return font
   }

   static async asyncOperation() {
      return Promise.all([...FontData.loaded.values()].map( fd => fd.data ))
   }

}

/**
 * TODO:
 */
class FontkitFont
{
   #fontData
   #fontkitFont

   constructor(fontDataOrID) {
      const fontData = fontDataOrID instanceof FontData ? fontDataOrID : FontData.get(fontDataOrID)
      this.#fontData = fontData
      this.#fontkitFont = null
   }

   /**
    * Called once from subclass to set font object which parses {@link FontData}
    * and provides query functions for font metrics. The font provider implementation used is fontkit.
    * Supports either a Promise which resolves to font provider or the provider itself.
    * The subclass must call this function in its constructor.
    * If the return value of {@link asyncOperation} is not a Promise then this object
    * could be queried for font metrics.
    */
   set fontkitFont(fontkitFont) {
      if (this.#fontkitFont === null)
         this.#fontkitFont = ifPromise(fontkitFont, f => this.#fontkitFont = f)
   }

   /**
    * Returns either a Promise or a font object.
    * In case of a Promise you have to wait until returned Promise is resolved
    * before querying this object for font metrics.
    */
   get asyncOperation() { return this.#fontkitFont }
   get error() { return this.#fontkitFont instanceof Error ? this.#fontkitFont : undefined }
   get isError() { return this.error !== undefined }
   get id() { return this.#fontData.id }
   get fontData() { return this.#fontData }
   get fontkitFont() { return this.#fontkitFont }
   get name() { return this.#fontkitFont.familyName }
   get postscriptName() { return this.#fontkitFont.postscriptName }

   get ascent() { return this.#fontkitFont.ascent }
   get descent() { return this.#fontkitFont.descent }
   get bbox() { return this.#fontkitFont.bbox }
   get capHeight() { return this.#fontkitFont.capHeight }
   get xHeight() { return this.#fontkitFont.xHeight }
   get unitsPerEm() { return this.#fontkitFont.unitsPerEm }
   get maxAscent() { return Math.max(this.#fontkitFont.bbox.maxY, this.#fontkitFont.ascent) }
   get minDescent() { return Math.min(this.#fontkitFont.bbox.minY, this.#fontkitFont.descent) }
   get textHeight() { return this.maxAscent - this.minDescent }

   textWidth(text) {
      const font = this.#fontkitFont
      const fontFeatures = undefined
      const glyphs = font.layout(text, fontFeatures).glyphs
      // const width = glyphs.reduce((sum,glyph) => sum + glyph.advanceWidth, 0/*init of sum*/)
      const positions = font.layout(text, fontFeatures).positions
      const width = positions.reduce((sum,pos) => sum + pos.xAdvance, 0/*init of sum*/)
      return width
   }

   scaledXHeight(textSize) {
      return this.xHeight * textSize / this.unitsPerEm
   }

   scaledCapHeight(textSize) {
      return this.capHeight * textSize / this.unitsPerEm
   }

   scaledMaxAscent(textSize) {
      return this.maxAscent * textSize / this.unitsPerEm
   }

   scaledMinDescent(textSize) {
      return this.minDescent * textSize / this.unitsPerEm
   }

   scaledTextHeight(textSize) {
      return this.textHeight * textSize / this.unitsPerEm
   }

   scaledTextWidth(text, textSize) {
      return this.textWidth(text) * textSize / this.unitsPerEm
   }

}

/**
 * A font data wrapper used to query font metrics without adding it to a {@link Document} first.
 *
 * The fontkit library is used as font engine.
 */
class FFont extends FontkitFont
{
   constructor(fontDataOrID) {
      super(fontDataOrID)
      this.fontkitFont = onReady([this.fontData.asyncOperation],
                           () => fontkit.create(this.fontData.data))
   }
}

/**
 * A font wrapper for a font added to {@link Document}.
 * A value of this type is returned by {@link Document.addFont addFont}.
 *
 * This object can be used to query font metrics.
 */
class EmbeddedFont extends FontkitFont
{
   #doc
   #font

   /**
    * Embeds a to a PDF document and wraps the underlying implementation.
    * The constructor operates asynchronously and the caller has to
    * wait until the async operation has completed (see {@link asyncOperation}).
    *
    * @param {string|FontData} fontDataOrID
    *   The binaray data of the loaded font.
    * @param {Document} doc
    *   The pdfkit document which implements the pdf rendering.
    * @returns
    *   An object of type {@link EmbeddedFont}.
    */
   constructor(fontDataOrID, doc) {
      super(fontDataOrID)
      this.#doc = doc
      this.#font = onReady([this.fontData.asyncOperation,doc.asyncOperation], () => {
         try {
            const doc=this.#doc.pdfkitDoc
            doc.registerFont(this.fontData.id, this.fontData.data)
            doc.font(this.fontData.id)
            return doc._fontFamilies[this.fontData.id]
         } catch(e) {
            return newError(`can not embed font with id »${this.fontData.id}«`,e)
         }
      }, f => this.#font = f)
      this.fontkitFont = onReady([this.#font], () => this.#font instanceof Error ? this.#font : this.#font.font)
   }

   get pdfkitFont() { return this.#font }

}


//////////////////
// PDF Document //
//////////////////

/**
 * Wraps pdfkit.PDFDocument API.
 * This object has a getter {@link pdfkitDoc}
 * to access the implementing pdfkit.PDFDocument object.
 */
class Document
{
   /**
    * Internal PDF document implementation managed by pdfkit library.
    * As long as initialization is not done a Promise is stored.
    * @type {pdfkit.PDFDocument|Promise<pdfkit.PDFDocument>}
    */
   #pdf
   /**
    * Maps fontID to {@link EmbeddedFont}. Use {@link addFont} to add new entries and {@link getFont} to query this map.
    * @type Map<string,EmbeddedFont>
    */
   #embeddedFonts=new Map()
   /**
    * The default page size and orientation of newly added pages. Defaults are set in constructor.
    * @property {'p'|'l'}                orientation  - Page orientation 'portrait' or 'landscape'.
    * @property {string|[number,number]} pageSize     - Size of page, either predefined name or arry of width and height in points.
    */
   #defaults={}
   /**
    * Stores listeners for supported events. Supported events are "addPage".
    */
   #eventListeners=new EventListeners("addPage")

   /**
    * Returns an object whose keys are the names of predefined page sizes.
    * The values are tuples defining width and height in points of the page.
    */
   static get predefinedSizes()
   {
      const pageSizes = {}
      for (const name of Object.keys(pdfkit.PageSizes))
         pageSizes[name.toLowerCase()] = pdfkit.PageSizes[name]
      return pageSizes
   }

   /**
    * PDF library options which controls the beaviour of the underlying library implementing the encoding.
    * @typedef {Object} PDFLibraryOptions
    * @property {boolean} autoFirstPage - Default is false. Set it to true if you want to the first page to be added automatically in the constructor.
    * @property {boolean} compress      - Default is true in which case the content is compressed before stored in the PDF document.
    * @property {Margin} margin         - Default margin of a new page.
    * @property {'l'|'landscape'|'p'|'portrait'} orientation  The orientation of the page where "p" stands for portrait and "l" for landscape mode. In case of "l" the given page size is rotated (height is used as width and vice versa).
    * @property {string} pdfVersion     - Default is "1.7" which is written as version of the generated PDF.
    */

   /**
    * PDF metadata describing language, subject, creation date and other.
    * @typedef {Object} PDFMetadata
    * @property {string} author    The author of the document.
    * @property {string} creator   The name of the application which created the document content.
    * @property {string} keywords  One or more keywords (separated by space) to tag the content for search engines.
    * @property {string} language  The language of the document, i.e. "de-DE" or "en-US".
    * @property {string} producer  The name of the library or driver which encodes PDF output. In most cases this equals creator.
    * @property {string} subject   The topic of the document.
    * @property {string} title     The document title.
    * @property {date} creationDate      The date and time when the document was created.
    * @property {date} modificationDate  The date and time when the document was changed last.
    */

   /**
    * Creates a new PDF document.
    *
    * @param {string | [number,number]} size  Either the name of a predefined page size or an array with width and height in points.
    * @param {PDFMetadata & PDFLibraryOptions} options  Options containg {@link PDFMetadata} and also {@link PDFLibraryOptions} to control behaviour of library.
    */
   constructor(size="a4", options={}) {
      const orientation = options.orientation ?? "p"
      if (assertString(orientation,"orientation")[0] != "p" && orientation[0] != "l")
         throwError(`expect orientation to be one of ['p','l','portrait','landscape'] and not ${orientation}`)
      if (! (typeof size === "string" && this.predefinedSizes[size.toLowerCase()])
         && (!Array.isArray(size) || size.length != 2
            || size[0] <= 0 || size[1] <= 0))
            throwError(`unsupported page size ${size}`)
      // set default values for all pages
      this.#defaults.orientation = orientation[0] === 'p' ? 'portrait' : 'landscape'
      this.#defaults.size        = size
      this.#defaults.margin      = options.margin
      const pdfkitOptions = {
         autoFirstPage: options.autoFirstPage ?? false,
         bufferPages: true, // pages are not written to the output stream on the fly but stored in memory
         compress: options.compress ?? true,
         layout: this.#defaults.orientation,
         info: {}, // metadata
         margins: this.#defaults.margin,
         pdfVersion: options.pdfVersion ?? "1.7",
         size: this.#defaults.size
      }
      this.#copyMetadata(pdfkitOptions.info, options)
      this.#pdf = new pdfkit.PDFDocument(pdfkitOptions)
   }

   /**
    * After all content has been created call this function to get the content of the PDF document.
    * @returns {PDFData}  The content of the PDF document encoded as binary data.
    */
   close() {
      const doc = this.#pdf
      const stream = doc.pipe(blobstream.blobStream())
      const data = new PDFData()
      doc.end()
      stream.on('finish', () => data.set(stream.toBlob('application/pdf')))
      return data
   }

   get asyncOperation() { return this.#pdf }
   get pdfkitDoc() { return this.#pdf }
   get predefinedSizes() { return Document.predefinedSizes }
   get defaultOrientation() { return this.#defaults.orientation }
   get defaultSize() { return this.#defaults.size }

   /**
    * Copies metadata from src into internal representation stored in dest.
    * @param {Object} dest     The metadata used internally which is set from content in src.
    * @param {PDFMetadata} src The metadata used in the API.
    */
   #copyMetadata(dest, src) {
      const metaData = [
         ["author", "Author"], ["creator", "Creator"], ["keywords", "Keywords"],
         ["language", "Language"], ["producer", "Producer"], ["subject", "Subject"], ["title", "Title"],
         ["creationDate", "CreationDate"], ["modificationDate","ModDate"]
      ]
      metaData.forEach( ([from, to]) => { if (src[from]) dest[to] = src[from] })
   }

   /**
    * Sets meta information describing language, author, creation date, modification date and other.
    * All properties of parameter info are optional.
    *
    * @param {PDFMetadata} info  The object which contains all metadata. See {@link PDFMetadata}.
    */
   setMetadata(info={}) { this.#copyMetadata(this.#pdf.info,info) }

   /**
    * Adds an already loaded font to this document.
    * You can not use the returnd object but have to wait finishing
    * its initialization process by waiting on the Promise
    * returned from {@link EmbeddedFont.asyncOperation asyncOperation}.
    * If this method is called more than once for the same {@link FontData}
    * the previously created {@link EmbeddedFont} object is returned.
    * @param {string|FontData} fontDataOrID  Either font-ID of a loaded font or the loaded {@link FontData} object.
    * @returns {EmbeddedFont}  Either a new or an existing embedded font of type {@link EmbeddedFont}.
    */
   addFont(fontDataOrID) {
      const fontData = fontDataOrID instanceof FontData ? fontDataOrID : FontData.get(fontDataOrID)
      const fontID = fontData.id
      if (!this.#embeddedFonts.has(fontID))
         this.#embeddedFonts.set(fontID, new EmbeddedFont(fontData, this))
      return this.#embeddedFonts.get(fontID)
   }

   getFont(fontDataOrID) {
      const fontID = fontDataOrID instanceof FontData ? fontDataOrID.id : fontDataOrID
      const font = this.#embeddedFonts.get(fontID)
      if (!font)
         throwError(`fontID '${String(fontID)}' is undefined`)
      return font
   }

   addPage(options={}) {
      const doc = this.#pdf
      options.size   = options.size ?? this.#defaults.size
      options.layout = (options.orientation ?? this.#defaults.orientation)[0]  === 'p' ? 'portrait' : 'landscape'
      const m = options.margin ?? this.#defaults.margin
      options.margins = m instanceof Margin ? m.toObject() : m
      doc.addPage(options)
      const currentPage = this.nrPages
      this.#eventListeners.callListener("addPage",this,currentPage)
   }

   nrPages() {
      const doc = this.#pdf
      return doc.bufferedPageRange().count
   }

   /**
    * Contains description of a single document page.
    * @typedef {Object} PageInfo
    * @property {number} nr       The page number is described (and is the current one).
    * @property {number} nrPages  The number of all pages in the document.
    * @property {object} margins  A value object with top,right,bottom,and left values in points.
    * @property {number} width    The width of page in points.
    * @property {number} height   The height of the page in points.
    */

   /**
    * Calls for all pages in ascending order (1..nrPages) pageCallback.
    * @param {(doc:Document,pageInfo:PageInfo) => void} pageCallback  Callback to draw something on the current page.
    */
   forPages(pageCallback) {
      const doc = this.#pdf
      const range = doc.bufferedPageRange()
      const nrPages = range.start + range.count
      assertFunction(pageCallback,"pageCallback")
      for (let i=range.start; i<nrPages; ++i) {
         doc.switchToPage(i)
         pageCallback(this, { nr:i+1, nrPages, margins:doc.page.margins, width:doc.page.width, height:doc.page.height })
      }
   }

   addListenerAddPage(callback) { this.#eventListeners.addListener("addPage",callback) }
   removeListenerAddPage(callback) { this.#eventListeners.removeListener("addPage",callback) }

   /////////////////////////
   // query drawing state //
   /////////////////////////

   currentPageNr() {
      throwError("not implemented")
   }

   pageHeight(pageNr) {
      throwError("not implemented")
      /*
      pageNr ??= this.currentPageNr()
      if (! (pageNr > 0))
         throwError(`first page number starts with 1 and not with '${pageNr}'`)
      const doc = this.#pdf
      return doc.getPageHeight(pageNr)
      */
   }

   pageWidth(pageNr) {
      throwError("not implemented")
      /*
      pageNr ??= this.currentPageNr()
      if (! (pageNr > 0))
         throwError(`first page number starts with 1 and not with '${pageNr}'`)
      const doc = this.#pdf
      return doc.getPageWidth(pageNr)
      */
   }

   /**
    *
    * @param {number} pageNr The page number for which the size is returned (starts with 1)
    * @returns {Rectangle}   The size of the page as {@link Rectangle}.
    */
   pageSize(pageNr) {
      return new Rectangle(0, 0, this.pageWidth(pageNr), this.pageHeight(pageNr))
   }

   textArea(pageNr) {
      return new TextArea(this, pageSize(pageNr))
   }

   //////////////////////////////////////
   // change state for drawing content //
   //////////////////////////////////////

}


///////////////////////////////
// Draw PDF Document Content //
///////////////////////////////

class FontStyle
{
   #fontID;       // font ID: "<name>-<style>"
   #fontSize;     // font size in points
   #charSpace;    // spacing after a character in points (letter-spacing in CSS)
   #lineHeight;   // line height in percent of the maximum text height, a value of 1 means 100%

   FontStyle(fontID, fontSize, charSpace=0, lineHeight=1.1) {
      this.#fontID   = fontID
      this.#fontSize = fontSize
      this.#charSpace = charSpace
      this.#lineHeight = lineHeight
   }

   get charSpace()  { return this.#charSpace }
   get fontSize()   { return this.#fontSize }
   get fontID()     { return this.#fontID }
   get lineHeight() { return this.#lineHeight }

   setCharSpace(charSpace) {
      return new FontStyle(this.#fontID, this.#fontSize, charSpace, this.#lineHeight)
   }
   setFontSize(fontSize) {
      return new FontStyle(this.#fontID, fontSize, this.#charSpace, this.#lineHeight)
   }
   setLineHeight(lineHeight) {
      return new FontStyle(this.#fontID, this.#fontSize, this.#charSpace, lineHeight)
   }
}

class Margin
{
   #top; #right; #bottom; #left;

   constructor(top, right, bottom, left) {
      this.#top = assertNumber(top,"top")
      this.#right = (right === undefined ? top : assertNumber(right,"right"))
      this.#bottom = (bottom === undefined ? top : assertNumber(bottom,"bottom"))
      this.#left = (left === undefined ? this.#right : assertNumber(left,"left"))
   }

   get top() { return this.#top }
   get right() { return this.#right }
   get bottom() { return this.#bottom }
   get left() { return this.#left }
   get width() { return this.#left + this.#right }
   get height() { return this.#top + this.#bottom }

   setTop(top) { return top === this.#top ? this : new Margin(top,this.#right,this.#bottom,this.#left) }
   setRight(right) { return right === this.#right ? this : new Margin(this.#top,right,this.#bottom,this.#left) }
   setBottom(bottom) { return bottom === this.#bottom ? this : new Margin(this.#top,this.#right,bottom,this.#left) }
   setLeft(left) { return left === this.#left ? this : new Margin(this.#top,this.#right,this.#bottom,left) }
   toObject() { return { top:this.#top, right:this.#right, bottom:this.#bottom, left:this.#left }}

}

class Rectangle
{
   #left; #top; #width; #height;

   constructor(left, top, width, height) {
      this.#left = left       // x-offset (0 == left)
      this.#top = top         // y-offset (0 == top)
      this.#width = width     // left + width == right
      this.#height = height   // top + height == bottom
   }

   get left() { return this.#left }
   get right() { return this.#left + width }
   get top() { return this.#top }
   get bottom() { return this.#top + height }
   get width() { return this.#width }
   get height() { return this.#height }

   subtractMargin(margin) {
      assertType(Margin,margin,"margin")
      return new Rectangle(this.#left + margin.left, this.#top + margin.top, this.#width - margin.width, this.#height - margin.height)
   }

}

class Pos
{
   #x; #y;

   constructor(x, y) {
      this.#x = x
      this.#y = y
   }

}

class TextArea
{
   #rectArea
   #xoff
   #yoff

   constructor(doc, rectArea) {
      this.#rectArea = rectArea
      this.#xoff = 0
      this.#yoff = 0
   }
}

class PageLayout
{
   // TODO: header-, footer-, main-, sidebar- Region
   // TODO: subtract header, footer, sidebar from page-size to get main-Region
}


///////////////////////////////////////////////////////////////
// Handling of binary PDF data with help of PDF-LIB library. //
///////////////////////////////////////////////////////////////

async function pdflibMerge(pdfData)
{
   const mergedPdf = await pdflib.PDFDocument.create()
   let copyMetadata = true

   async function copyPages(toPDF, fromData, pages) {
      if (!(fromData instanceof ArrayBuffer))
         throwError(`expect PDF data of type (ArrayBuffer) not ${typename(fromData)}`)
      const fromPDF = await pdflib.PDFDocument.load(fromData)
      const allPages = fromPDF.getPageIndices()
      const fromPages = (pages && pages.length
                        ? pages.map(pnr => pnr-1/*page index starting from 0*/).filter(pi => allPages.includes(pi))
                        : allPages)
      const copiedPages = await toPDF.copyPages(fromPDF, fromPages)
      copiedPages.forEach( page => toPDF.addPage(page))
      if (copyMetadata) {
         copyMetadata = false
         if (fromPDF.getCreator()) toPDF.setCreator(fromPDF.getCreator())
         if (fromPDF.getTitle()) toPDF.setTitle(fromPDF.getTitle())
         if (fromPDF.getSubject()) toPDF.setSubject(fromPDF.getSubject())
         if (fromPDF.getKeywords()) toPDF.setKeywords([fromPDF.getKeywords()])
         if (fromPDF.getLanguage()) toPDF.setLanguage(fromPDF.getLanguage())
      }
   }

   for (const fromPdf of pdfData) {
      await copyPages(mergedPdf, fromPdf.bytes, fromPdf.pages)
   }

   return mergedPdf.save()
}

async function pdflibPageNumbers(pdfData)
{
   if (!(pdfData instanceof ArrayBuffer))
      throwError(`expect PDF data of type (ArrayBuffer) not ${typename(pdfData)}`)
   const doc = await pdflib.PDFDocument.load(pdfData)
   return doc.getPageIndices().map(pi => pi+1/*page numbers start from 1*/)
}


////////////
// Export //
////////////

Object.assign(module.exports, {
   module,
   assertType,
   uValue,
   FontData,
   FFont,
   PDFData,
   Document,
   PageLayout,
   Margin,
})

export default module.exports