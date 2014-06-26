/* title: SyncFunc impl

   Implements <SyncFunc>.

   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/task/syncfunc.h
    Header file <SyncFunc>.

   file: C-kern/task/syncfunc.c
    Implementation file <SyncFunc impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncfunc.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: syncfunc_t

// group: lifetime


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   syncfunc_t func = syncfunc_FREE;

   // TEST syncfunc_FREE
   TEST(0 == func.mainfct);
   TEST(0 == func.state);
   TEST(0 == func.contlabel);

   return 0;
ONERR:
   return EINVAL;
}

// forward
static int test_execmd_sf(syncfunc_param_t * sfparam, uint32_t sfcmd);

static int test_getset(void)
{
   syncfunc_t       sfunc   = syncfunc_FREE;
   syncfunc_param_t sfparam = syncfunc_param_FREE;

   // TEST getstate_syncfunc
   TEST(0 == getstate_syncfunc(&sfparam));
   for (uintptr_t i = 1; i; i <<= 1) {
      sfparam.state = (void*) i;
      TEST((void*)i == getstate_syncfunc(&sfparam));
   }

   // TEST getsize_syncfunc: syncfunc_opt_NONE
   TEST(sizeof(syncfunc_f) == getsize_syncfunc(syncfunc_opt_NONE));

   // TEST getsize_syncfunc: syncfunc_opt_ALL
   TEST(sizeof(syncfunc_t) == getsize_syncfunc(syncfunc_opt_ALL));

   // TEST getsize_syncfunc: combination of flags
   for (syncfunc_opt_e opt1 = 0; opt1 <= syncfunc_opt_STATE; opt1 += syncfunc_opt_STATE) {
      const size_t s1 = getsize_syncfunc(opt1) - sizeof(syncfunc_f);
      TEST(s1 == (opt1 ? sizeof(void*) : 0));
      for (syncfunc_opt_e opt2 = 0; opt2 <= syncfunc_opt_CONTLABEL; opt2 += syncfunc_opt_CONTLABEL) {
         const size_t s2 = getsize_syncfunc(opt2) - sizeof(syncfunc_f);
         TEST(s2 == (opt2 ? sizeof(void*) : 0));
         TEST(sizeof(syncfunc_f) + s1 + s2 == getsize_syncfunc(opt1|opt2));
      }
   }

   // TODO: TEST optstate_syncfunc

   // TODO: TEST optcontlabel_syncfunc


   // TEST setstate_syncfunc
   for (uintptr_t i = 1; i; i <<= 1) {
      setstate_syncfunc(&sfparam, (void*) i);
      TEST((void*)i == getstate_syncfunc(&sfparam));
   }
   setstate_syncfunc(&sfparam, 0);
   TEST(0 == getstate_syncfunc(&sfparam));

   // TEST setall_syncfunc: syncfunc_opt_NONE
   setall_syncfunc(&sfunc, syncfunc_opt_NONE, &test_execmd_sf, (void*)2, (void*)3);
   TEST(sfunc.mainfct   == &test_execmd_sf);
   TEST(sfunc.state     == 0);
   TEST(sfunc.contlabel == 0);

   // TEST setall_syncfunc: syncfunc_opt_ALL
   memset(&sfunc, 0, sizeof(sfunc));
   setall_syncfunc(&sfunc, syncfunc_opt_ALL, &test_execmd_sf, (void*)2, (void*)3, ((syncwait_node_t) { (void*)4, (void*)5 }) );
   TEST(sfunc.mainfct   == &test_execmd_sf);
   TEST(sfunc.state     == (void*)2);
   TEST(sfunc.contlabel == (void*)3);
   TEST(sfunc.caller.next == (void*)4);
   TEST(sfunc.caller.prev == (void*)5);

   // TEST setall_syncfunc: combination of flags
   for (syncfunc_opt_e opt1 = 0; opt1 <= syncfunc_opt_STATE; opt1 += syncfunc_opt_STATE) {
      for (syncfunc_opt_e opt2 = 0; opt2 <= syncfunc_opt_CONTLABEL; opt2 += syncfunc_opt_CONTLABEL) {
         for (syncfunc_opt_e opt3 = 0; opt3 <= syncfunc_opt_CALLER; opt3 += syncfunc_opt_CALLER) {
            memset(&sfunc, 0, sizeof(sfunc));
            setall_syncfunc(&sfunc, opt1|opt2|opt3, &test_execmd_sf, (void*)2, (void*)3, ((syncwait_node_t) { (void*)4, (void*)5 }) );
            TEST(sfunc.mainfct == &test_execmd_sf);
            syncfunc_t * sf2 = &sfunc;
            if (opt1) {
               TEST(sfunc.state == (void*)2);
            } else {
               sf2 = (syncfunc_t *) ((uint8_t*)sf2 - sizeof(void*));
            }
            if (opt2) {
               TEST(sf2->contlabel == (void*)3);
            } else {
               sf2 = (syncfunc_t *) ((uint8_t*)sf2 - sizeof(void*));
            }
            if (opt3) {
               TEST(sf2->caller.next == (void*)4);
               TEST(sf2->caller.prev == (void*)5);
            } else {
               sf2 = (syncfunc_t *) ((uint8_t*)sf2 - sizeof(syncwait_node_t));
            }
            while (sf2 != &sfunc) {
               TEST(0 == * (void**) ((uint8_t*)sf2 + sizeof(syncfunc_t)));
               sf2 = (syncfunc_t *) ((uint8_t*)sf2 + sizeof(void*));
            }
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_execmd_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   execmd_syncfunc(sfparam, sfcmd, ONRUN, ONEXIT);

   // is executed in case of wrong sfcmd value
   sfparam->retcode = -1;
   return -1;

ONRUN:
   sfparam->contlabel = __extension__ &&ONCONTINUE;
   sfparam->retcode = 10;
   return syncfunc_cmd_RUN;

ONCONTINUE:
   static_assert(syncfunc_cmd_CONTINUE > syncfunc_cmd_RUN, "must run after continue");
   sfparam->retcode = 11;
   return syncfunc_cmd_CONTINUE;

ONEXIT:
   sfparam->retcode = 12;
   return syncfunc_cmd_EXIT;
}

static int test_exit_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   exit_syncfunc(sfparam, (int)sfcmd);
}

static int test_yield_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   execmd_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN:
   sfparam->retcode += 19;
   yield_syncfunc(sfparam);
   sfparam->retcode += 20;
   yield_syncfunc(sfparam);
   sfparam->retcode += 21;
   sfparam->contlabel = 0;
   return syncfunc_cmd_EXIT;

ONERR:
   return -1;
}

static int test_implsupport(void)
{
   syncfunc_param_t sfparam = syncfunc_param_FREE;

   // TEST execmd_syncfunc: valid sfcmd values
   static_assert(syncfunc_cmd_RUN == 0 && syncfunc_cmd_EXIT== 2, "3 different sfcmd");
   for (int cmd = syncfunc_cmd_RUN; cmd <= syncfunc_cmd_EXIT; ++cmd) {
      sfparam.retcode = 0;
      TEST(cmd == test_execmd_sf(&sfparam, (syncfunc_cmd_e)cmd));
      TEST(sfparam.syncrun     == 0);
      TEST(sfparam.state       == 0);
      TEST(sfparam.contlabel   != 0);
      TEST(sfparam.retcode -10 == cmd);
   }

   // TEST execmd_syncfunc: invalid sfcmd value
   sfparam.contlabel = 0;
   for (int cmd = syncfunc_cmd_EXIT+1; cmd <= syncfunc_cmd_EXIT+2; ++cmd) {
      sfparam.retcode = 0;
      TEST(-1 == test_execmd_sf(&sfparam, (syncfunc_cmd_e)cmd));
      TEST( 0 == sfparam.syncrun);
      TEST( 0 == sfparam.state);
      TEST( 0 == sfparam.contlabel);
      TEST(-1 == sfparam.retcode);
   }

   // TEST exit_syncfunc
   for (uint32_t cmd = 0; cmd <= 100000; cmd += 10000) {
      sfparam.retcode = -1;
      TEST(syncfunc_cmd_EXIT == test_exit_sf(&sfparam, cmd));
      TEST(sfparam.retcode     == (int)cmd);
      TEST(sfparam.syncrun     == 0);
      TEST(sfparam.state       == 0);
      TEST(sfparam.contlabel   == 0);
   }

   // TODO: TEST  wait_syncfunc

   // TODO: TEST  waitexit_syncfunc

   // TEST yield_syncfunc
   sfparam.contlabel = 0;
   for (int i = 19; i <= 21; ++i) {
      void * oldlabel = sfparam.contlabel;
      int    result   = i != 21 ? syncfunc_cmd_CONTINUE : syncfunc_cmd_EXIT;
      sfparam.retcode = 0;
      TEST(result   == test_yield_sf(&sfparam, i == 19 ? syncfunc_cmd_RUN : syncfunc_cmd_CONTINUE));
      TEST(oldlabel != sfparam.contlabel);
      TEST(i == sfparam.retcode);
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_syncfunc()
{
   if (test_initfree())       goto ONERR;
   if (test_getset())         goto ONERR;
   if (test_implsupport())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
