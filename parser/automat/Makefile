
run: reg
	./reg

reg: main.c automat.c automat_mman.c patriciatrie.c automat.h slist_node.h slist.h config.h test_errortimer.h foreach.h regexpr.h regexpr.c utf8.h utf8.c
	gcc -oreg -std=gnu99 main.c regexpr.c automat.c automat_mman.c patriciatrie.c utf8.c
