#include "config.h"
#include "regexpr.h"
#include <stdio.h>

int main()
{
   regexpr_t     regex;
   // regexpr_err_t err = { 0 };
  
   init_regexpr(&regex, 3, "abc", 0);
   printf("Try regex \"%s\" (nrstates: %zd)\n", "abc", regex.matcher.nrstate);
   printf("Match ab len: %u\n", matchchar32_automat(&regex.matcher, 2, U"abc", true));
   printf("Match abc len: %u\n", matchchar32_automat(&regex.matcher, 3, U"abc", true));
   free_regexpr(&regex);
 
   init_regexpr(&regex, 14, "a+b+c+ &! abbc", 0);
   printf("Try regex \"%s\" (nrstates: %zd)\n", "a+b+c+ &! abbc", regex.matcher.nrstate);
   // print_automat(&regex.matcher);
   printf("Match abbc len: %u\n", matchchar32_automat(&regex.matcher, 4, U"abbc", true));
   printf("Match aabbbcc len: %u\n", matchchar32_automat(&regex.matcher, 7, U"aabbbcc", true));
   free_regexpr(&regex);


   init_regexpr(&regex, 24, "[a-zA-Z0-9_]+ &! [0-9].*", 0);
   printf("Try regex \"%s\" (nrstates: %zd)\n", "[a-zA-Z0-9_]+ &! [0-9].*", regex.matcher.nrstate);
   printf("Match 1_ len: %u\n", matchchar32_automat(&regex.matcher, 2, U"1_", true));
   printf("Match _1Za len: %u\n", matchchar32_automat(&regex.matcher, 4, U"_1Za", true));
   print_automat(&regex.matcher);
   free_regexpr(&regex);

   return 0;
}
