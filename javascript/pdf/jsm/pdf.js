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
const ifPromise=(value, thenCallback) =>
                     value instanceof Promise
                     ? value.then( thenCallback )
                     : value
const onReady=(asyncOps, createValueCallback) =>
         asyncOps.some( op => op instanceof Promise)
         ? Promise.all(asyncOps).then(_ => createValueCallback())
         : createValueCallback()
const _typeof=(v) => typeof v !== "object" ? typeof v
         : v === null ? "null"
         : v.constructor === Object || typeof v.constructor !== "function" ? "object"
         : v.constructor.name

const assertFunction=(value,argName) => { if (typeof value !== "function") throw newError(`expect argument »${argName}« of type function`) ; return value }
const assertNumber=(value,argName) => { if ((typeof value !== "number" && !(value instanceof Number)) || isNaN(value)) throw newError(`expect argument »${argName}« of type number`) ; return value }
const assertType=(Type,value,argName) => { if (!(value instanceof Type)) throw newError(`expect argument »${argName}« of type ${Type.name}`) ; return value }

if (!pdfkit.PDFDocument) throw newError("could not import PDFDocument from pdfkit")
if (!pdfkit.PageSizes) throw newError("could not import PageSizes from pdfkit")
if (!blobstream.blobStream) throw newError("could not import blobStream from blobstream")


///////////////////////////
// Value Unit Conversion //
///////////////////////////

// All coordinate and size values are measured in points.
// If you want to use other units you need to convert them into points
// and returned values (size, position, ...) back into other units.

/**
 * Converts millimeter (mm) into points (pt).
 * @param {Number} mm   A value in millimeter.
 * @returns             The value in millimeter converted into a value in points.
 */
const mm2pt = (mm) => mm * 72 / 25.4
/**
 * Converts points (pt) into millimeter (mm).
 * @param {Number} mm   A value in millimeter.
 * @returns             The value in points converted into a value in millimeter.
 */
const pt2mm = (pt) => pt * 25.4 / 72


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

   #getListeners(event) {
      const listeners = this.#eventListeners[event]
      if (listeners === undefined)
         throw newError(`unknown event »${String(event)}«`)
      return listeners
   }

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
// Data Conversion Helper //
////////////////////////////

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

   set(blob) {
      if (blob instanceof Blob) {
         this.#resolve(blob)
      }
      else if (blob instanceof ArrayBuffer || blob instanceof Uint8Array) {
         this.#resolve(new Blob([blob], { type: 'application/pdf' }))
      }
      else if (blob instanceof Error) {
         this.#reject(blob)
      }
      else {
         this.#reject(newError(`Unsupported data type »${_typeof(blob)}«`))
      }
   }

   async toBlob() { return this.#promise }
   async toArrayBuffer() { return this.#promise.then( blob => blob.arrayBuffer()) }
   async toObjectURL() { return this.#promise.then( blob => URL.createObjectURL(blob)) }
   async toDataURL() {
      return this.#promise.then( blob => new Promise((resolve,reject) => {
         const reader = new FileReader()
         reader.onload = (event => resolve(reader.result))
         reader.onerror = (event => reject(newError(`error converting blob to data url`,reader.error)))
         reader.readAsDataURL(blob)
      }))
   }

}

////////////////////////
// User Font Handling //
////////////////////////

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
               throw newError(`can not load font from url »${url}«`,`${response.status}: ${response.statusText}`)
            return response.arrayBuffer()
      }).catch( e => this.data = e ))
   }

   static get(fontID) {
      const font = FontData.loaded.get(fontID)
      if (!font)
         throw new Error(errmsg(`fontID '${fontID}' is undefined`))
      return font
   }

   static async asyncOperation() {
      return Promise.all([...FontData.loaded.values()].map( fd => fd.data ))
   }

}

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

class BaseFont
{
   #fontData
   #fontkitFont

   constructor(fontDataOrID) {
      const fontData = fontDataOrID instanceof FontData ? fontDataOrID : FontData.get(fontDataOrID)
      this.#fontData = fontData
      this.#fontkitFont = null
   }

   /**
    * Called once from subclass to set font object which wraps the {@link FontData}
    * and provides query functions for font metrics. The font provider is fontkit.
    * Supports either a Promise which resolves to font object or the font object itself.
    * If the return value of {@link asyncOperation} is not a Promise then this BaseFont
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

class FFont extends BaseFont
{
   constructor(fontDataOrID) {
      super(fontDataOrID)
      this.fontkitFont = onReady([this.fontData.asyncOperation],
                           () => fontkit.create(this.fontData.data))
   }
}


/**
 * A font wrapper for a font added to Document.
 * A value of this type is returned by {@link Document.addFont}
 */
class EmbeddedFont extends BaseFont
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
            return this.#font = doc._fontFamilies[this.fontData.id]
         } catch(e) {
            this.#font = newError(`can not embed font with id »${this.fontData.id}«`,e)
         }
      })
      this.fontkitFont = onReady([this.#font], () => this.#font instanceof Error ? this.#font : this.#font.font)
   }

   get pdfkitFont() { return this.#font }

}

//////////////
// Document //
//////////////

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
    * @property {string} pdfVersion     - Default is "1.7" which is written as version of the generated PDF.
    */

   /**
    * PDF metadata describing language, subject, creation date and other.
    * @typedef {Object} PDFMetadata
    * @property {string} author   - The author of the document.
    * @property {string} creator  - The name of the application which created the document content.
    * @property {string} keywords - One or more keywords (separated by space) to tag the content for search engines.
    * @property {string} language - The language of the document, i.e. "de-DE" or "en-US".
    * @property {string} producer - The name of the library or driver which encoded PDF output.
    *                               In most cases this equals creator.
    * @property {string} subject  - The topic of the document.
    * @property {string} title    - The document title.
    * @property {date} creationDate     - The date and time when the document was created.
    * @property {date} modificationDate - The date and time when the document was changed last.
    */

   /**
    * Creates a new PDF document.
    *
    * @param {string | [number,number]} size - Either the name of a predefined page size or an array with width and height in points.
    * @param {'l'|'landscape'|'p'|'portrait'} orientation - The orientation of the page where "p" stands for portrait and "l" for landscape mode. In case of "l" the given page size is rotated (height is used as width and vice versa).
    * @param {PDFMetadata & PDFLibraryOptions} options - Options containg {@link PDFMetadata} and also {@link PDFLibraryOptions} to control behaviour of library.
    */
   constructor(size="a4", orientation="p", options={}) {
      if (orientation[0] != "p" && orientation[0] != "l")
         throw newError(`support only 'p' (portrait) or 'l' (landscape) and not ${orientation}`)
      if (! (typeof size === "string" && this.predefinedSizes[size.toLowerCase()])
         && (!Array.isArray(size) || size.length != 2
            || size[0] <= 0 || size[1] <= 0))
            throw newError(`unsupported page size ${size}`)
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
    * TODO
    * @returns
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
    * @param {PDFMetadata} info - The object which contains all metadata. See {@link PDFMetadata}.
    */
   setMetadata(info={}) { this.#copyMetadata(this.#pdf.info,info) }

   /**
    * Adds an already loaded font to this document.
    * You can not use the returnd object but have to wait finishing
    * its initialization process by waiting on the Promise
    * returned from {@link EmbeddedFont.asyncOperation asyncOperation}.
    * If this method is called more than once for the same {@link FontData}
    * the previously created {@link EmbeddedFont} object is returned.
    * @param {string|FontData} fontDataOrID Either font-ID of a loaded font or the loaded {@link FontData} object.
    * @returns {EmbeddedFont} Either a new or an existing embedded font of type {@link EmbeddedFont}.
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
         throw newError(`fontID '${fontID}' is undefined`)
      return font
   }

   addPage(options={}) {
      const doc = this.#pdf
      options.size   = options.size ?? this.#defaults.size
      options.layout = (options.orientation ?? this.#defaults.orientation)[0]  === 'p' ? 'portrait' : 'landscape'
      options.margins = this.#defaults.margin
      doc.addPage(options)
      const currentPage = this.nrPages
      this.#eventListeners.callListener("addPage",this,currentPage)
   }

   nrPages() {
      const doc = this.#pdf
      return doc.bufferedPageRange().count
   }

   /**
    * @param {(doc,currentPage,nrPages,currentMargin) => void} pageCallback
    *   Callback to draw something on the current page.
    */
   forPages(pageCallback) {
      const doc = this.#pdf
      const range = doc.bufferedPageRange()
      const nrPages = range.start + range.count
      assertFunction(pageCallback,"pageCallback")
      for (let i=range.start; i<nrPages; ++i) {
         const currentPage = i + 1
         doc.switchToPage(i)
         pageCallback(this,currentPage,nrPages,doc.page.margins)
      }
   }

   addListenerAddPage(callback) { this.#eventListeners.addListener("addPage",callback) }
   removeListenerAddPage(callback) { this.#eventListeners.removeListener("addPage",callback) }

   /////////////////////////
   // query drawing state //
   /////////////////////////

   currentPageNr() {
      throw newError("not implemented")
   }

   pageHeight(pageNr) {
      throw newError("not implemented")
      /*
      pageNr ??= this.currentPageNr()
      if (! (pageNr > 0))
         throw new Error(errmsg(`first page number starts with 1 and not with '${pageNr}'`))
      const doc = this.#pdf
      return doc.getPageHeight(pageNr)
      */
   }

   pageWidth(pageNr) {
      throw newError("not implemented")
      /*
      pageNr ??= this.currentPageNr()
      if (! (pageNr > 0))
         throw new Error(errmsg(`first page number starts with 1 and not with '${pageNr}'`))
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

async function pdflibMerge(fromPdfData) {
   const mergedPdf = await pdflib.PDFDocument.create()
   let copyMetadata = true

   async function copyPages(toPDF, fromData, pages) {
      if (fromData instanceof Blob)
         fromData = await fromData.arrayBuffer()
      else if (!(fromData instanceof ArrayBuffer))
         throw newError(`expect PDF data of type (Blob|ArrayBuffer) not ${_typeof(fromData)}`)
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

   for (const fromPdf of fromPdfData) {
      const isWrapper = typeof fromPdf === "object" && ("blob" in fromPdf)
      const pdfData = isWrapper ? fromPdf.blob : fromPdf
      const pages = isWrapper ? fromPdf.pages : undefined
      if (!(pdfData instanceof Blob) && !(pdfData instanceof ArrayBuffer))
         throw newError(`expect PDF data of type (Blob|ArrayBuffer|{blob:Blob|ArrayBuffer,pages:undefined|number[]}) not ${isWrapper ? '{blob:'+_typeof(pdfData)+'}':_typeof(pdfData)}`)
      if (pages !== undefined && !Array.isArray(pages))
         throw newError(`expect pages property of type (Array|undefined) not ${_typeof(pages)}`)
      await copyPages(mergedPdf, pdfData, pages)
   }

   return mergedPdf
}

/**
 * The function copies pages from one or more PDF documents into a single PDF
 * document and returns the merged PDF data.
 * In case a Blob is provided or pages property in the wrapper is either undefined
 * or an empty array all pages are copied.
 * The pages array contains page numbers starting from 1.
 * The pages are copied in order. You could supply page numbers more than once in which case
 * the page is copied more than once.
 * @param {(Blob|{blob:Blob,pages:undefined|number[]})[]} fromPdfData - Data content of one or more PDF documents. If they are wrapped in an object set property blob to content of PDF and supply an pages array to select the pages for copy and also their order.
 * @returns {PDFData} See {@link PDFData} how to access to returned data.
 */
function merge(...fromPdfData) {
   if (!fromPdfData.length)
      return null

   const data=new PDFData()

   pdflibMerge(fromPdfData)
   .then( mergedPdf => mergedPdf.save())
   .then( mergedData => data.set(mergedData))
   .catch( e => data.set(e))

   return data
}

async function pageNumbers(fromPdfData) {
   if (fromPdfData instanceof Blob)
      fromPdfData = await fromPdfData.arrayBuffer()
   else if (!(fromPdfData instanceof ArrayBuffer))
      throw newError(`expect PDF data of type (Blob|ArrayBuffer) not ${_typeof(fromPdfData)}`)
   const doc = await pdflib.PDFDocument.load(fromPdfData)
   const pageIndices = doc.getPageIndices()
   return pageIndices.map(pi => pi+1/*page numbers start from 1*/)
}

Object.assign(module.exports, {
   module,
   assertType,
   mm2pt,
   pt2mm,
   merge,
   pageNumbers,
   FontData,
   Document,
   PDFData,
   FFont,
   Margin,
})

export default module.exports