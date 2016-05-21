/* title: UTF-8
   8-bit U(niversal Character Set) T(ransformation) F(ormat)

   This encoding of the unicode character set is backward-compatible
   with ASCII and avoids problems with endianess.

   Encoding:
   Unicode character       - UTF-8
   (codepoint)             - (encoding)
   0x00 .. 0x7F            - 0b0xxxxxxx
   0x80 .. 0x7FF           - 0b110xxxxx 0b10xxxxxx
   0x800 .. 0xFFFF         - 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
   0x10000 .. 0x1FFFFF     - 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   0x200000 .. 0x3FFFFFF   - 0b111110xx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
   0x4000000 .. 0x7FFFFFFF - 0b1111110x 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx

   If you want UTF-8 encoding to be compatible with UTF-16 (0 .. 0x10FFFF)
   restrict yourself to max. 4 bytes per character.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/string/utf8.h
    Header file <UTF-8>.

   file: C-kern/string/utf8.c
    Implementation file <UTF-8 impl>.
*/
#ifndef CKERN_STRING_UTF8_HEADER
#define CKERN_STRING_UTF8_HEADER

// === exported types
struct utf8validator_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_string_utf8
 * Test UTF-8 functions. */
int unittest_string_utf8(void);
#endif



// struct: utf8

// group: global-variables

/* variable: g_utf8_bytesperchar
 * Stores the length in bytes of an encoded utf8 character indexed by the first encoded byte. */
extern uint8_t g_utf8_bytesperchar[64];

// group: query

/* function: maxchar_utf8
 * Returns the maximum character value (unicode code point) which can be encoded into utf-8.
 * The minumum unicode code point is 0. The returned value is 0x10FFFF. */
char32_t maxchar_utf8(void);

/* function: maxsize_utf8
 * Returns the maximum size in bytes of an utf-8 encoded multibyte sequence. */
uint8_t maxsize_utf8(void);

/* function: isfirstbyte_utf8
 * Returns true if this byte is a possible first (start) byte of an utf-8 encoded multibyte sequence.
 * This function assumes correct encoding therefore it is possible that <isfirstbyte_utf8>
 * returns true and <decodechar_utf8> returns 0. */
bool isfirstbyte_utf8(const uint8_t firstbyte);

/* function: issinglebyte_utf8
 * Returns true if this byte is a single byte encoded value. Which is equal to ASCII encoding. */
int issinglebyte_utf8(const uint8_t firstbyte);

/* function: sizePfirst_utf8
 * Returns the size in bytes of a correct encoded mb-sequence by means of the value of its first byte.
 * The number of bytes is calculated from firstbyte - the first byte of the encoded byte sequence.
 * The returned values are in the range 1..4 (1..<maxsize_utf8>).
 * The first byte must not be necessarily a valid byte ! */
uint8_t sizePfirst_utf8(const uint8_t firstbyte);

/* function: sizechar_utf8
 * Returns the size in bytes of uchar as encoded mb-sequence.
 * The returned values are in the range 1..<maxchar_utf8>.
 * If uchar is bigger than <maxchar_utf8> no error is reported and the function returns <maxsize_utf8>. */
uint8_t sizechar_utf8(char32_t uchar);

/* function: length_utf8
 * Returns number of UTF-8 characters encoded in string buffer.
 * The first byte of a multibyte sequence determines its size.
 * This function assumes that utf8 encodings are correct and does not check
 * the encoding of bytes following the first.
 * Illegal encoded start bytes are not counted but skipped.
 * The last multibyte sequence is counted as one character even if
 * one or more bytes are missing.
 *
 * Parameter:
 * strstart - Start of string buffer (lowest address)
 * strend   - Highest memory address of byte after last byte in the string buffer.
 *            If strend <= strstart then the string is considered the empty string.
 *            Set this value to strstart + length_of_string_in_bytes. */
size_t length_utf8(const uint8_t * strstart, const uint8_t * strend);

/* function: find_utf8
 * Searches for unicode character in utf8 encoded string str of size bytes.
 * The returned value points to the start addr of the multibyte sequence.
 * A return value of 0 inidcates that str[size] does not contain the multibyte sequence
 * or that uchar is bigger than <maxchar_utf8> and therefore invalid. */
const uint8_t * find_utf8(size_t size, const uint8_t str[size], char32_t uchar);

// group: encode-decode

/* function: decodechar_utf8
 * Decodes utf-8 encoded bytes beginning from strstart and returns character in uchar.
 * The string must be big enough but needs never larger as <maxsize_utf8>.
 * Use <sizePfirst_utf8> to determine the size if strstart contains less then <maxsize_utf8> bytes.
 *
 * The number of decoded bytes is returned.
 *
 * A return value of 0 indicates an invalid first byte of the multibyte sequence (EILSEQ).
 * The function assumes that all other bytes except the first are encoded correctly.
 * Use <utf8validator_t> to make sure that a string contains only a valid encoded utf8 characters.
 *
 * Example:
 *
 * > uint8_t * str    = &strbuffer[0];
 * > uint8_t * strend = strbuffer + sizeof(strbuffer);
 * > while (str < strend) {
 * >    if (strend-str < maxsize_utf8()) {
 * >       if (sizePfirst_utf8(str[0]) > (strend-str)) {
 * >          ...not enough data for last character...
 * >          break;
 * >       }
 * >    }
 * >    char32_t uchar;
 * >    uint8_t  len = decodechar_utf8(str, &uchar);
 * >    str += len;
 * >    ... do something with uchar ...
 * > } */
uint8_t decodechar_utf8(const uint8_t strstart[/*maxsize_utf8() or big enough*/], /*out*/char32_t * uchar);

/* function: encodechar_utf8
 * Encodes uchar into UTF-8 enocoded string of size strsize starting at strstart.
 * The number of written bytes are returned. The maximum return value is <maxsize_utf8>.
 * A return value of 0 indicates an error. Either uchar is greater then <maxchar_utf8>
 * or strsize is not big enough. */
uint8_t encodechar_utf8(char32_t uchar, size_t strsize, /*out*/uint8_t strstart[strsize]);

/* function: skipchar_utf8
 * Skips the next utf-8 encoded character.
 * The encoded byte sequence is *not* checked for correctness.
 * The number of skipped bytes is returned. The maximum return value is <maxsize_utf8>.
 * The values returned are in the range [1..6]. */
uint8_t skipchar_utf8(const uint8_t strstart[/*maxsize_utf8() or big enough*/]);


/* struct: utf8validator_t
 * Allows to validate multiple memory blocks of bytes.
 * If a multibyte sequence crosses a block boundary the first part
 * of it is stored internally as prefix data which is used as
 * prefix during decoding of the next data block. */
typedef struct utf8validator_t {
   uint8_t  sizeprefix;
   uint8_t  prefix[6/*maxsize_utf8()*/];
} utf8validator_t;

// group: lifetime

/* define: utf8validator_INIT
 * Static initializer. */
#define utf8validator_INIT \
         { 0, { 0 } }

/* function: init_utf8validator
 * Same as assigning <utf8validator_INIT>. */
void init_utf8validator(/*out*/utf8validator_t *utf8validator);

/* function: free_utf8validator
 * Clear data members and checks that there is no internal prefix stored.
 *
 * Returns:
 * 0      - All multi-byte character sequences fit into last buffer.
 * EILSEQ - Last multi-byte character sequence was incomplete. Need more data. */
static inline int free_utf8validator(utf8validator_t *utf8validator);

// group: query

/* function: sizeprefix_utf8validator
 * Returns a value != 0 if the last multibyte sequence was not fully contained in the last validated buffer. */
uint8_t sizeprefix_utf8validator(const utf8validator_t *utf8validator);

// group: validate

/* function: validate_utf8validator
 * Validates a data block of length size in bytes.
 * If the last multibyte sequence is not fully contained in the data block but a valid prefix it is stored internally as prefix.
 * If this function is called another time the internal prefix is prepended to the data block.
 * If an error occurs EILSEQ is returned the parameter offset is set to the offset of the byte which is not encoded correctly. */
int validate_utf8validator(utf8validator_t *utf8validator, size_t size, const uint8_t data[size], /*err*/size_t *erroffset);



// section: inline implementation

// group: utf8_t

/* function: maxchar_utf8
 * Implements <utf8.maxchar_utf8>. */
#define maxchar_utf8() \
         ((char32_t)0x7fffffff)

/* function: isfirstbyte_utf8
 * Implements <utf8.isfirstbyte_utf8>. */
#define isfirstbyte_utf8(firstbyte) \
         (0x80 != ((firstbyte)&0xc0))

/* function: issinglebyte_utf8
 * Implements <utf8.issinglebyte_utf8>. */
#define issinglebyte_utf8(firstbyte) \
         (0 == ((firstbyte) & 0x80))

/* function: maxsize_utf8
 * Implements <utf8.maxsize_utf8>. */
#define maxsize_utf8() \
         ((uint8_t)6)

/* function: sizePfirst_utf8
 * Implements <utf8.sizePfirst_utf8>. */
#define sizePfirst_utf8(firstbyte) \
         (g_utf8_bytesperchar[((uint8_t)(firstbyte)) >> 2])

/* function: sizechar_utf8
 * Implements <utf8.sizechar_utf8>. */
#define sizechar_utf8(uchar) \
         ( __extension__ ({            \
            char32_t _ch = (uchar);    \
            (1 + (_ch > 0x7f)          \
               + (_ch > 0x7ff)         \
               + (_ch > 0xffff)        \
               + (_ch > 0x1fffff)      \
               + (_ch > 0x3FFFFFF));   \
         }))

/* function: skipchar_utf8
 * Implements <utf8.skipchar_utf8>. */
#define skipchar_utf8(strstart) \
         (sizePfirst_utf8(*(strstart)))

// group: utf8validator_t

/* define: init_utf8validator
 * Implements <utf8validator_t.init_utf8validator>. */
#define init_utf8validator(utf8validator) \
         ((void)(*(utf8validator) = (utf8validator_t) utf8validator_INIT))

/* define: free_utf8validator
 * Implements <utf8validator_t.free_utf8validator>. */
static inline int free_utf8validator(utf8validator_t* utf8validator)
{
         int err = utf8validator->sizeprefix ? EILSEQ : 0;
         utf8validator->sizeprefix = 0;
         return err;
}

/* define: sizeprefix_utf8validator
 * Implements <utf8validator_t.sizeprefix_utf8validator>. */
#define sizeprefix_utf8validator(utf8validator) \
         ((utf8validator)->sizeprefix)


#endif
