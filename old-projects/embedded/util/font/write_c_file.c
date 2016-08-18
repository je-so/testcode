#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static size_t nrcolor(size_t size, unsigned char *data)
{
   size_t nr = 0;
   char inuse[256] = { 0 };

   for (size_t i = 0; i < size; ++i) {
      inuse[data[i]] = 1;
   }

   for (unsigned i = 0; i < 256; ++i) {
      if (inuse[i]) {
         ++ nr;
      }
   }

   return nr;
}

static void fonttable(size_t width, size_t height, size_t xoff, size_t yoff, size_t charwidth, size_t charheight, unsigned char *data)
{
   fprintf(stdout, "static const uint8_t s_font_width  = %zd;\n", charwidth);
   fprintf(stdout, "static const uint8_t s_font_height = %zd;\n", charheight);
   fprintf(stdout, "static const uint32_t s_font_glyph[/*32..126*/95*%zd] = {", charheight);
   for (unsigned chr = 0; chr < 95; ++chr) {
      size_t off = xoff + yoff * width + chr * charwidth;
      if (chr) fprintf(stdout, ",");
      fprintf(stdout, "\n   /* char %d '%c' */\n   ", chr+32, chr+32);
      for (size_t y = 0; y < charheight; ++y) {
         if (y) {
            if ((y % 8) == 0) {
               fprintf(stdout, ",\n   ");
            } else {
               fprintf(stdout, ", ");
            }
         }
         unsigned char *pixel = data + (off + y*width);
         size_t bits = 0;
         for (size_t x = 0; x < charwidth; ++x) {
            if (pixel[x]) {
               bits |= (1u << x);
            }
         }
         fprintf(stdout, "0x%08zx", bits);
      }
   }

   fprintf(stdout, "\n};\n");
}

static size_t getyoff(size_t width, size_t height, unsigned char *data)
{
   for (size_t yoff = 0; yoff < height; ++yoff) {
      for (size_t x = 0; x < width; ++x) {
         if (data[yoff * width + x] != 0xff) {
            return yoff ? yoff -1 : yoff;
         }
      }
   }

   return 0;
}

static size_t getcharheight(size_t width, size_t height, size_t yoff, unsigned char *data)
{
   for (size_t y = yoff+1; y < height; ++y) {
      int isempty = 1;
      for (size_t x = 0; x < width; ++x) {
         if (data[y * width + x] != 0xff) {
            isempty = 0;
            break;
         }
      }
      if (isempty) {
         return y - yoff;
      }
   }

   return height - yoff;
}

int main(const int argc, const char *argv[])
{
   FILE *file;
   unsigned char *data;

   if (argc < 8) {
      fprintf(stderr, "Usage: %s <file.gray> width height xoff yoff charwidth charheight\n", argv[0]);
      return 1;
   }
   long width = atol(argv[2]);
   long height = atol(argv[3]);
   long xoff = atol(argv[4]);
   long yoff = atol(argv[5]);
   long charwidth = atol(argv[6]);
   long charheight = atol(argv[7]);
   if (  width <= 0 || height <= 0 || xoff < 0
         || yoff < 0 || charwidth <= 0 || charheight <= 0) {
      fprintf(stderr, "%s: Error: Parameter <= 0\n", argv[0]);
      return 1;
   }

   if (charwidth > 32) {
      fprintf(stderr, "%s: Error: charwidth > 32\n", argv[0]);
      return 1;
   }

   file = fopen(argv[1], "r+");
   if (!file) goto ONERR;
   if (fseek(file, 0, SEEK_END) != 0) goto ONERR;
   size_t file_size = (size_t) ftell(file);
   if (file_size != width * height) {
      fprintf(stderr, "%s: Error: file_size != %ld\n", argv[0], width*height);
      return 1;
   }
   if (fseek(file, 0, SEEK_SET) != 0) goto ONERR;

   data = malloc((size_t)file_size);
   if (!data) {
      fprintf(stderr, "%s: Error: Out of memory\n", argv[0]);
      return 1;
   }
   if (fread(data, 1, file_size, file) != file_size) goto ONERR;

   if (nrcolor(file_size, data) != 2) {
      fprintf(stderr, "%s: Error: Picture not black and white\n", argv[0]);
      return 1;
   }

   size_t yoff2 = getyoff((size_t) width, (size_t) height, data);
   size_t charheight2 = getcharheight((size_t) width, (size_t) height, yoff2, data) + yoff2 - (size_t) yoff;
   if (charheight2 != (size_t) charheight) {
      fprintf(stderr, "%s: Error: charheight != %ld (yoff2:%ld)\n", argv[0], (long)charheight2, (long)yoff2);
      return 1;
   }

   fonttable((size_t)width, (size_t)height, (size_t)xoff, (size_t)yoff, (size_t)charwidth, (size_t)charheight, data);

   fclose(file);
   return 0;
ONERR:
   if (file) {
      fprintf(stderr, "Read error in file '%s'\n", argv[1]);
      fclose(file);
   } else {
      fprintf(stderr, "Cannot open file '%s'\n", argv[1]);
   }
   return 1;
}
