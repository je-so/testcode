#define _GNU_SOURCE
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

/* function: rsearchstr
 * Suche Teilstring substr im Gesamtstring data von hinten nach vorn.
 * Search substring substr in whole string data in *reverse* order.
 * If substr is not found 0 is returned. If substr is contained more than
 * once the occurrence with the highest address is reported (reverse order).
 *
 * Used algorithmus:
 * Knuth-Morris-Pratt!
 *
 * Worst case:
 * O(size)
 *
 * Unchecked Precondition:
 * - substrsize >= 1
 *
 */
static const uint8_t* rsearchstr(size_t size, const uint8_t data[size], uint8_t substrsize, const uint8_t substr[substrsize])
{
   /* For all (0 <= i < substrsize): sidx[i] gives the index into substr
    * where you have to continue to compare if comparison failed for index i.
    * sidx[substrsize-1] is set to substrsize which marks the end of chain.
    */
   uint8_t sidx[substrsize];
   sidx[substrsize-1] = substrsize;    // 0 length match
   for (int i = substrsize-1, i2 = substrsize; i; ) {
      while (i2 < substrsize && substr[i] != substr[i2]) {
         i2 = sidx[i2];
      }
      sidx[--i] = (uint8_t) (--i2);
   }

   size_t dpos = size;
   int    spos = substrsize;
   while (dpos > 0) {
      --dpos;
      --spos;
      if (substr[spos] != data[dpos]) {
         spos = sidx[spos];
         while (spos < substrsize && substr[spos] != data[dpos]) {
            spos = sidx[spos];
         }
      }
      if (! spos) return data + dpos;
   }

   return 0;
}

/* function: searchstr
 * Suche Teilstring substr im Gesamtstring data von vorn nach hinten.
 * Search substring substr in whole string data.
 * If substr is not found 0 is returned. If substr is contained more than
 * once the occurrence with the lowest address is reported.
 *
 * Used algorithmus:
 * Simplified Boyer-Moore without Bad-Character-Heuristic.
 * Like KMP but comparing starts from end of substr (substr[substrsize-1]).
 *
 * Worst case:
 * O(size)
 *
 * Special case (data == 0 || substrsize == 0):
 * The value 0 is returned if length of substr or data is 0.
 *
 */
const uint8_t* searchstr(size_t size, const uint8_t data[size], uint8_t substrsize, const uint8_t substr[substrsize])
{
   // nr of positions substr is to shift right
   // array is indexed by nr of matched positions starting from end
   uint8_t shift[substrsize];
   const uint8_t* pos;

   if (substrsize < 2) {
      return substrsize ? memchr(data, substr[0], size) : 0;
   }

   if (substrsize > size) {
      return 0;
   }

   {
      pos = memrchr(substr, substr[substrsize-1], substrsize-1);
      int lastoff = (pos ? 1 + (int) (pos - substr) : 0);

      shift[0] = 1;
      shift[1] = (uint8_t) (substrsize - lastoff);

      // copied from rsearchstr (KMP index helper array)
      uint8_t sidx[substrsize];
      sidx[substrsize-1] = substrsize;    // 0 length match
      for (int i = substrsize-1, i2 = substrsize; i; ) {
         while (i2 < substrsize && substr[i] != substr[i2]) {
            i2 = sidx[i2];
         }
         sidx[--i] = (uint8_t) (--i2);
      }

      for (int nrmatched = 2; nrmatched < substrsize; ++nrmatched) {
         if (lastoff >= nrmatched) {
            int dpos = lastoff-nrmatched+1;
            int spos = substrsize-nrmatched+1;
            while (dpos > 0) {
               --dpos;
               --spos;
               if (substr[spos] != substr[dpos]) {
                  spos = sidx[spos];
                  while (spos < substrsize && substr[spos] != substr[dpos]) {
                     spos = sidx[spos];
                  }
               }
               if (nrmatched == substrsize-spos) break;
            }
            lastoff = (spos < substrsize ? dpos+(substrsize-spos) : 0);
         }
         shift[nrmatched] = (uint8_t) (substrsize - lastoff);
      }
   }

   const int substrsize1 = substrsize-1;

   int eoff = 0;
   int skip = 0;

   const uint8_t* const
   endpos = data + size - substrsize1;
   pos = data;

   do {
      int off = substrsize1;
      while (substr[off] == pos[off]) {
         if (off == eoff) {
            if (off <= skip) {
               return pos;
            }
            off -= skip; // make worst case O(size) instead O(size*substrlen)
            eoff = 0;
         }
         --off;
      }
      int nrmatched = substrsize1 - off;
      int pincr = shift[nrmatched];
      skip = nrmatched;
      pos += pincr;
      eoff = substrsize - pincr;
   } while (pos < endpos);

   return 0;
}

int main()
{
   char * s = "ababbbcabaabbbabababbbababbba";
   char * p = "ababbba";

   printf("rsrch %s\n", rsearchstr(strlen(s), s, strlen(p), p));
   printf("rsrch %s\n", rsearchstr(strlen(s)-1, s, strlen(p), p));

   const uint8_t* pos1 = searchstr(strlen(s), s, strlen(p), p);
   printf("srch %s\n", pos1);
   printf("srch %s\n", searchstr(strlen(pos1+1), pos1+1, strlen(p), p));

   char data[10000];
   memset(data, 'a', sizeof(data));
   for (int i = 0; i < 10000; i += 10) {
      data[i] = 'b';
   }
   data[10000-10] = 'a';
   data[10000-11] = 'b';
   p = "aaaaaaaaaa";
   printf("srch %s\n", searchstr(sizeof(data), data, strlen(p), p));

   return 0;
}

