#include "config.h"
#include "regexpr.h"
#include <stdio.h>

int main()
{
   regexpr_t     regex;
  
   init_regexpr(&regex, 3, "abc", 0);
   printf("\nCompiled regex \"%s\" has nrstates: %zd\n", "abc", regex.matcher.nrstate);
   printf("Try match 'ab' matched len: %u\n", matchchar32_automat(&regex.matcher, 2, U"abc", true));
   printf("Try match 'abc' matched len: %u\n", matchchar32_automat(&regex.matcher, 3, U"abc", true));
   free_regexpr(&regex);
 
   init_regexpr(&regex, 14, "a+b+c+ &! abbc", 0);
   printf("\nCompiled regex \"%s\" has nrstates: %zd\n", "a+b+c+ &! abbc", regex.matcher.nrstate);
   // print_automat(&regex.matcher);
   printf("Try match 'abbc' matched len: %u\n", matchchar32_automat(&regex.matcher, 4, U"abbc", true));
   printf("Try match 'aabbbcc' matched len: %u\n", matchchar32_automat(&regex.matcher, 7, U"aabbbcc", true));
   free_regexpr(&regex);


   init_regexpr(&regex, 24, "[a-zA-Z0-9_]+ &! [0-9].*", 0);
   printf("\nCompiled regex \"%s\" has nrstates: %zd\n", "[a-zA-Z0-9_]+ &! [0-9].*", regex.matcher.nrstate);
   printf("Try match '1_' matched len: %u\n", matchchar32_automat(&regex.matcher, 2, U"1_", true));
   printf("Try match '_1Za' matched len: %u\n", matchchar32_automat(&regex.matcher, 4, U"_1Za", true));
   printf("Show deterministic finit autom. of regex\n");
   print_automat(&regex.matcher);
   free_regexpr(&regex);

   // build automaton without using regex
   printf("\nMake dfa from ndfa '(\\u0000|\\u0001|...|\\u0400)+' with 1024 ored states:\n");
   automat_t ndfa = automat_FREE;
   initmatch_automat(&ndfa, 0, 1, (char32_t[1]){0}, (char32_t[1]){0}); // match '\0' char
   for (char32_t c = 1; c <= 1024; ++c) {
      automat_t ndfa2 = automat_FREE;
      initmatch_automat(&ndfa2, &ndfa/*use same memory heap as ndfa*/, 1, (char32_t[1]){c}, (char32_t[1]){c}); // match c
      opor_automat(&ndfa, &ndfa2); // build ndfa = (ndfa) | (ndfa2)
   }
   oprepeat_automat(&ndfa, 1); // build ndfa = (ndfa)+
   makedfa_automat(&ndfa);
   print_automat(&ndfa);
   free_automat(&ndfa);

   return 0;
}
