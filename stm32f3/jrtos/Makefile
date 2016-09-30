PROGRAM:=test
CFLAGS:=-DKONFIG_STACKSIZE=1024 -DKONFIG_USE_PSP -DKONFIG_UNITTEST -O2 -g
# used for startup.s
# CFLAGS+=-Wa,--defsym -Wa,KONFIG_STACKSIZE=1024 -Wa,--defsym -Wa,KONFIG_USE_PSP=1
SOURCE:=../startup.c test_main.c semaphore.c task.c scheduler.c
SOURCE+=../hw/cm4/atomic.c
SOURCE+=../hw/cm4/core.c
SOURCE+=../hw/cm4/cpustate.c
include ../Makefile
