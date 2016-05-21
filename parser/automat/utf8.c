/* title: UTF-8 impl
   Implements <UTF-8>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2013 Jörg Seebohn

   file: C-kern/api/string/utf8.h
    Header file <UTF-8>.

   file: C-kern/string/utf8.c
    Implementation file <UTF-8 impl>.
*/

#include "config.h"
#include "utf8.h"
#include "memstream.h"
#include "test_errortimer.h"

// section: utf8

uint8_t g_utf8_bytesperchar[64] = {
   /* [0 .. 127]/4 */
   /* 0*/ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   // [128 .. 191]/4 no valid first byte but also encoded as 1
   /*32*/ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   // [192 .. 223]/4 2 byte sequences (but values [192..193] are invalid)
   /*48*/ 2, 2, 2, 2, 2, 2, 2, 2,
   // [224 .. 239]/4 3 byte sequences
   /*56*/ 3, 3, 3, 3,
   // [240 .. 247]/4 4 byte sequences
   /*60*/ 4, 4,
   // [248 .. 251]/4 5 byte sequences
   /*62*/ 5,
   // [252 .. 253]/4 6 byte sequences
   /*63*/ 6
   // [254 .. 255]/4 are illegal but also mapped to g_utf8_bytesperchar[63] (6 Bytes)
};


uint8_t decodechar_utf8(const uint8_t strstart[], /*out*/char32_t * uchar)
{
   char32_t uchr;
   uint8_t  firstbyte;

   firstbyte = strstart[0];

   switch (firstbyte) {
   case   0: case   1: case   2: case   3: case   4: case   5: case   6: case   7: case   8: case   9:
   case  10: case  11: case  12: case  13: case  14: case  15: case  16: case  17: case  18: case  19:
   case  20: case  21: case  22: case  23: case  24: case  25: case  26: case  27: case  28: case  29:
   case  30: case  31: case  32: case  33: case  34: case  35: case  36: case  37: case  38: case  39:
   case  40: case  41: case  42: case  43: case  44: case  45: case  46: case  47: case  48: case  49:
   case  50: case  51: case  52: case  53: case  54: case  55: case  56: case  57: case  58: case  59:
   case  60: case  61: case  62: case  63: case  64: case  65: case  66: case  67: case  68: case  69:
   case  70: case  71: case  72: case  73: case  74: case  75: case  76: case  77: case  78: case  79:
   case  80: case  81: case  82: case  83: case  84: case  85: case  86: case  87: case  88: case  89:
   case  90: case  91: case  92: case  93: case  94: case  95: case  96: case  97: case  98: case  99:
   case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: case 108: case 109:
   case 110: case 111: case 112: case 113: case 114: case 115: case 116: case 117: case 118: case 119:
   case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127:
      // 1 byte sequence
      *uchar = firstbyte;
      return 1;

   case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137:
   case 138: case 139: case 140: case 141: case 142: case 143: case 144: case 145: case 146: case 147:
   case 148: case 149: case 150: case 151: case 152: case 153: case 154: case 155: case 156: case 157:
   case 158: case 159: case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167:
   case 168: case 169: case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177:
   case 178: case 179: case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187:
   case 188: case 189: case 190: case 191:
      // used for encoding follow bytes
      goto ONERR;

   // 0b110xxxxx
   case 192: case 193:
      // not used in two byte sequence (value < 0x80)
      goto ONERR;
   case 194: case 195: case 196: case 197: case 198: case 199: case 200: case 201: case 202: case 203:
   case 204: case 205: case 206: case 207: case 208: case 209: case 210: case 211: case 212: case 213:
   case 214: case 215: case 216: case 217: case 218: case 219: case 220: case 221: case 222: case 223:
      // 2 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x1F) << 6);
      uchr  |= (strstart[1] ^ 0x80);
      *uchar = uchr;
      return 2;

   // 0b1110xxxx
   case 224: case 225: case 226: case 227: case 228: case 229: case 230: case 231:
   case 232: case 233: case 234: case 235: case 236: case 237: case 238: case 239:
      // 3 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0xF) << 12);
      uchr  ^= (char32_t)strstart[1] << 6;
      uchr  ^= strstart[2];
      *uchar = uchr ^ (((char32_t)0x80 << 6) | 0x80);
      return 3;

   // 0b11110xxx
   case 240: case 241: case 242: case 243: case 244: case 245: case 246: case 247:
      // 4 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x7) << 18);
      uchr  ^= (char32_t)strstart[1] << 12;
      uchr  ^= (char32_t)strstart[2] << 6;
      uchr  ^= strstart[3];
      *uchar = uchr ^ (((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80);
      return 4;

   // 0b111110xx
   case 248: case 249: case 250: case 251:
      // 5 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x3) << 24);
      uchr  ^= (char32_t)strstart[1] << 18;
      uchr  ^= (char32_t)strstart[2] << 12;
      uchr  ^= (char32_t)strstart[3] << 6;
      uchr  ^= strstart[4];
      *uchar = uchr ^ (((((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80) << 6) | 0x80);
      return 5;

   // 0b1111110x
   case 252: case 253:
      // 6 byte sequence
      uchr   = ((uint32_t)(firstbyte & 0x1) << 30);
      uchr  ^= (char32_t)strstart[1] << 24;
      uchr  ^= (char32_t)strstart[2] << 18;
      uchr  ^= (char32_t)strstart[3] << 12;
      uchr  ^= (char32_t)strstart[4] << 6;
      uchr  ^= strstart[5];
      *uchar = uchr ^ (((((((((char32_t)0x80 << 6) | 0x80) << 6) | 0x80) << 6) | 0x80) << 6) | 0x80);
      return 6;

   case 254: case 255:
      // not used for encoding
      goto ONERR;
   }

ONERR:
   return 0;
}

uint8_t encodechar_utf8(char32_t uchar, size_t strsize, /*out*/uint8_t strstart[strsize])
{
   if (uchar <= 0xffff) {
      if (uchar <= 0x7f) {
         if (strsize < 1) return 0;
         strstart[0] = (uint8_t) uchar;
         return 1;
      } else if (uchar <= 0x7ff) {
         if (strsize < 2) return 0;
         strstart[0] = (uint8_t) (0xC0 | (uchar >> 6));
         strstart[1] = (uint8_t) (0x80 | (uchar & 0x3f));
         return 2;
      } else {
         if (strsize < 3) return 0;
         strstart[0] = (uint8_t) (0xE0 | (uchar >> 12));
         strstart[1] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f));
         strstart[2] = (uint8_t) (0x80 | (uchar & 0x3f));
         return 3;
      }
   } else if (uchar <= 0x1fffff) {  // 0b11110xxx
      if (strsize < 4) return 0;
      strstart[0] = (uint8_t) (0xF0 | (uchar >> 18));
      strstart[1] = (uint8_t) (0x80 | ((uchar >> 12) & 0x3f));
      strstart[2] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f));
      strstart[3] = (uint8_t) (0x80 | (uchar & 0x3f));
      return 4;
   } else if (uchar <= 0x3FFFFFF) { // 0b111110xx
      if (strsize < 5) return 0;
      strstart[0] = (uint8_t) (0xF8 | (uchar >> 24));
      strstart[1] = (uint8_t) (0x80 | ((uchar >> 18) & 0x3f));
      strstart[2] = (uint8_t) (0x80 | ((uchar >> 12) & 0x3f));
      strstart[3] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f));
      strstart[4] = (uint8_t) (0x80 | (uchar & 0x3f));
      return 5;
   } else if (uchar <= 0x7FFFFFFF) { // 0b1111110x
      if (strsize < 6) return 0;
      strstart[0] = (uint8_t) (0xFC | (uchar >> 30));
      strstart[1] = (uint8_t) (0x80 | ((uchar >> 24) & 0x3f));
      strstart[2] = (uint8_t) (0x80 | ((uchar >> 18) & 0x3f));
      strstart[3] = (uint8_t) (0x80 | ((uchar >> 12) & 0x3f));
      strstart[4] = (uint8_t) (0x80 | ((uchar >> 6) & 0x3f));
      strstart[5] = (uint8_t) (0x80 | (uchar & 0x3f));
      return 6;
   }

   return 0;
}

size_t length_utf8(const uint8_t *strstart, const uint8_t *strend)
{
   size_t len = 0;

   if (strstart < strend) {
      const uint8_t* next = strstart;

      if ((size_t)(strend - strstart) >= maxsize_utf8()) {
         const uint8_t* end = strend - maxsize_utf8();
         do {
            const unsigned sizechr = sizePfirst_utf8(*next);
            next += sizechr;
            len  += 1;
         } while (next <= end);
      }

      while (next < strend) {
         const unsigned sizechr = sizePfirst_utf8(*next);
         if ((size_t)(strend-next) < sizechr) {
            next = strend;
         } else {
            next += sizechr;
         }
         ++ len;
      }
   }

   return len;
}

const uint8_t * find_utf8(size_t size, const uint8_t str[size], char32_t uchar)
{
   const uint8_t *found;
   uint8_t        utf8[maxsize_utf8()];
   uint8_t        len = encodechar_utf8(uchar, sizeof(utf8), utf8);

   if (! len) return 0;

   for (found = memchr(str, utf8[0], size); found; found += len, found = memchr(found, utf8[0], (size_t) (str + size - found))) {
      for (unsigned i = 1; i < len; ++i) {
         if (utf8[i] != found[i]) goto CONTINUE;
      }
      return found;
      CONTINUE: ;
   }

   return 0;
}


// section: utf8validator_t

int validate_utf8validator(utf8validator_t * utf8validator, size_t size, const uint8_t data[size], /*err*/size_t *erroffset)
{
   static_assert(sizeof(utf8validator->prefix) == maxsize_utf8(), "prefix contains at least 1 byte less");

   const uint8_t *next    = data;
   const uint8_t *endnext = data + (size >= maxsize_utf8() ? size - maxsize_utf8() : 0);
   size_t         size_missing = 0;
   size_t         erroff  = 0;

   if (size == 0) return 0;  // nothing to do

   if (utf8validator->sizeprefix) {
      // complete prefix and check it
      size_t size_needed  = sizePfirst_utf8(utf8validator->prefix[0]);
      size_missing = size_needed - utf8validator->sizeprefix;
      size_missing = size < size_missing ? size : size_missing;
      for (size_t i = 0; i < size_missing; ) {
         utf8validator->prefix[utf8validator->sizeprefix++] = data[i++];
      }
      // if size_next is too low then VALIDATE generates an error (utf8validator->prefix is filled with 0)
      next = utf8validator->prefix;
      endnext = utf8validator->prefix;
      goto VALIDATE;
   }

   for (;;) {

      if (next >= endnext) {
         if (utf8validator->sizeprefix) {
            utf8validator->sizeprefix = 0;
            next = data + size_missing;
            endnext = data + size;
            size_missing = 0;
            if ((size_t)(endnext - next) >= maxsize_utf8()) {
               endnext -= maxsize_utf8();
               goto VALIDATE;
            }
         }
         size_t nrbytes = (size_t) (data + size - next);
         if (! nrbytes) break; // whole buffer done
         if (sizePfirst_utf8(next[0]) > nrbytes) {
            utf8validator->sizeprefix = (uint8_t) nrbytes;
            for (size_t i = 0; i < nrbytes; ++i) {
               utf8validator->prefix[i] = next[i];
            }
            for (size_t i = nrbytes; i < sizeof(utf8validator->prefix); ++i) {
               utf8validator->prefix[i] = 0; // ensures error during validate (utf8validator->sizeprefix < sizePfirst_utf8(next[0])
            }
            // size_complement == 0
            next = utf8validator->prefix;
         }
         endnext = next;
      }

      // validate sequence

VALIDATE:

      switch (next[0]) {
      case   0: case   1: case   2: case   3: case   4: case   5: case   6: case   7: case   8: case   9:
      case  10: case  11: case  12: case  13: case  14: case  15: case  16: case  17: case  18: case  19:
      case  20: case  21: case  22: case  23: case  24: case  25: case  26: case  27: case  28: case  29:
      case  30: case  31: case  32: case  33: case  34: case  35: case  36: case  37: case  38: case  39:
      case  40: case  41: case  42: case  43: case  44: case  45: case  46: case  47: case  48: case  49:
      case  50: case  51: case  52: case  53: case  54: case  55: case  56: case  57: case  58: case  59:
      case  60: case  61: case  62: case  63: case  64: case  65: case  66: case  67: case  68: case  69:
      case  70: case  71: case  72: case  73: case  74: case  75: case  76: case  77: case  78: case  79:
      case  80: case  81: case  82: case  83: case  84: case  85: case  86: case  87: case  88: case  89:
      case  90: case  91: case  92: case  93: case  94: case  95: case  96: case  97: case  98: case  99:
      case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: case 108: case 109:
      case 110: case 111: case 112: case 113: case 114: case 115: case 116: case 117: case 118: case 119:
      case 120: case 121: case 122: case 123: case 124: case 125: case 126: case 127:
         // 1 byte sequence
         next += 1;
         continue;

      case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135: case 136: case 137:
      case 138: case 139: case 140: case 141: case 142: case 143: case 144: case 145: case 146: case 147:
      case 148: case 149: case 150: case 151: case 152: case 153: case 154: case 155: case 156: case 157:
      case 158: case 159: case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167:
      case 168: case 169: case 170: case 171: case 172: case 173: case 174: case 175: case 176: case 177:
      case 178: case 179: case 180: case 181: case 182: case 183: case 184: case 185: case 186: case 187:
      case 188: case 189: case 190: case 191:
         // used for encoding following bytes
         goto ONERR;

      case 192: case 193:
         // not used in two byte sequence (value < 0x80)
         goto ONERR;

      case 194: case 195: case 196: case 197: case 198: case 199: case 200: case 201:
      case 202: case 203: case 204: case 205: case 206: case 207: case 208: case 209: case 210: case 211:
      case 212: case 213: case 214: case 215: case 216: case 217: case 218: case 219: case 220: case 221:
      case 222: case 223:
         // 2 byte sequence
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
         next += 2;
         continue;

      case 224:
         // 3 byte sequence
         if ((next[1] & 0xC0) != 0x80)  { erroff = 1; goto ONERR; }
         if (next[1] < 0xA0/*too low*/) { erroff = 1; goto ONERR; }
         goto CHECK3_COMMON;

      case 225: case 226: case 227: case 228: case 229: case 230: case 231: case 232:
      case 233: case 234: case 235: case 236: case 237: case 238: case 239:
         // 3 byte sequence
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
      CHECK3_COMMON:
         if ((next[2] & 0xC0) != 0x80) { erroff = 2; goto ONERR; }
         next += 3;
         continue;

      case 240:
         // 4 byte sequence
         if ((next[1] & 0xC0) != 0x80)  { erroff = 1; goto ONERR; }
         if (next[1] < 0x90/*too low*/) { erroff = 1; goto ONERR; }
         goto CHECK4_COMMON;

      case 241: case 242: case 243: case 244: case 245: case 246: case 247:
         // 4 byte sequence
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
      CHECK4_COMMON:
         if ((next[2] & 0xC0) != 0x80) { erroff = 2; goto ONERR; }
         if ((next[3] & 0xC0) != 0x80) { erroff = 3; goto ONERR; }
         next += 4;
         continue;

      case 248:
         // 5 byte sequence
         if ((next[1] & 0xC0) != 0x80)  { erroff = 1; goto ONERR; }
         if (next[1] < 0x88/*too low*/) { erroff = 1; goto ONERR; }
         goto CHECK5_COMMON;

      case 249: case 250: case 251:
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
      CHECK5_COMMON:
         if ((next[2] & 0xC0) != 0x80) { erroff = 2; goto ONERR; }
         if ((next[3] & 0xC0) != 0x80) { erroff = 3; goto ONERR; }
         if ((next[4] & 0xC0) != 0x80) { erroff = 4; goto ONERR; }
         next += 5;
         continue;

      case 252:
         // 6 byte sequence
         if ((next[1] & 0xC0) != 0x80)  { erroff = 1; goto ONERR; }
         if (next[1] < 0x84/*too low*/) { erroff = 1; goto ONERR; }
         goto CHECK6_COMMON;

      case 253:
         // 6 byte sequence
         if ((next[1] & 0xC0) != 0x80) { erroff = 1; goto ONERR; }
      CHECK6_COMMON:
         if ((next[2] & 0xC0) != 0x80) { erroff = 2; goto ONERR; }
         if ((next[3] & 0xC0) != 0x80) { erroff = 3; goto ONERR; }
         if ((next[4] & 0xC0) != 0x80) { erroff = 4; goto ONERR; }
         if ((next[5] & 0xC0) != 0x80) { erroff = 5; goto ONERR; }
         next += 6;
         continue;

      case 254: case 255:
         goto ONERR;

      }// switch (firstbyte)

   }// for(;;)

   return 0;
ONERR:
   // check that not the missing part of prefix generated the error (filled with 0) !
   if (utf8validator->sizeprefix) {
      // is prefix valid ?
      if (utf8validator->sizeprefix <= erroff) return 0; // it is
      if (size_missing) {
         // completion of prefix is wrong
         next = data;
         erroff -= (utf8validator->sizeprefix - size_missing)/*compensate for already stored prefix*/;
      } else {
         // last sequence is a bad prefix
         next = data + size - utf8validator->sizeprefix;
      }
      utf8validator->sizeprefix = 0;
   }
   // set err parameter
   if (erroffset) *erroffset = erroff + (size_t) (next - data);
   return EILSEQ;
}
