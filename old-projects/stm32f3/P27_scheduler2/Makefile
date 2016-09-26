PROGRAM:=sched2
CFLAGS:=-DKONFIG_STACKSIZE=1024 -DKONFIG_USE_PSP -DKONFIG_ASSERT -O2 -g
# used for startup.s
# CFLAGS+=-Wa,--defsym -Wa,KONFIG_STACKSIZE=1024 -Wa,--defsym -Wa,KONFIG_USE_PSP=1
SOURCE:=../startup.c main.c ../µC/cpustate.c semaphore.c fifo.c task.c sched.c
include ../Makefile
