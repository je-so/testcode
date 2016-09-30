PROGRAM:=sched2
CFLAGS:=-DKONFIG_STACKSIZE=1024 -DKONFIG_USE_PSP -O2 -g
# used for startup.s
# CFLAGS+=-Wa,--defsym -Wa,KONFIG_STACKSIZE=1024 -Wa,--defsym -Wa,KONFIG_USE_PSP=1
SOURCE:=../startup.c main.c ../hw/cm4/cpustate.c semaphore.c fifo.c task.c sched.c
include ../Makefile
