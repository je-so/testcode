CFLAGS := -O3 -std=c99 -pedantic -Wall -Wextra -Wconversion -Wshadow
CFLAGS += -Wcast-qual -Wwrite-strings -Wstrict-prototypes
CFLAGS += -Wformat-nonliteral -Wformat-y2k

header = iperf.h
src = iperf.c

iperf: $(src) $(header)
	gcc $(CFLAGS) -oiperf $(src) -lpthread

test: iperf
	./iperf 4 thread

clean:
	rm -f ./iperf
