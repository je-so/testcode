/* title: SyncFunc impl

   Implementiere <SyncFunc>.

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

// group: update

void relink_syncfunc(syncfunc_t * sfunc, const size_t structsize)
{
   const int iswaitfor = (sfunc->optfields & syncfunc_opt_WAITFOR);
   if (iswaitfor && !(sfunc->optfields & syncfunc_opt_WAITRESULT)) {
      synclink_t * waitfor = addrwaitfor_syncfunc(sfunc);
      if (isvalid_synclink(waitfor)) {
         relink_synclink(waitfor);
      }
   }

   if (0 != (sfunc->optfields & syncfunc_opt_WAITLIST)) {
      synclinkd_t * waitlist = addrwaitlist_syncfunc(sfunc, iswaitfor);
      if (isvalid_synclinkd(waitlist)) {
         relink_synclinkd(waitlist);
      }
   }

   if (0 != (sfunc->optfields & syncfunc_opt_CALLER)) {
      const int isstate = (sfunc->optfields & syncfunc_opt_STATE);
      synclink_t * caller = addrcaller_syncfunc(sfunc, structsize, isstate);
      if (isvalid_synclink(caller)) {
         relink_synclink(caller);
      }
   }
}

void unlink_syncfunc(syncfunc_t * sfunc, const size_t structsize)
{
   const int iswaitfor = (sfunc->optfields & syncfunc_opt_WAITFOR);
   if (iswaitfor && !(sfunc->optfields & syncfunc_opt_WAITRESULT)) {
      synclink_t * waitfor = addrwaitfor_syncfunc(sfunc);
      if (isvalid_synclink(waitfor)) {
         unlink_synclink(waitfor);
      }
   }

   if (0 != (sfunc->optfields & syncfunc_opt_WAITLIST)) {
      synclinkd_t * waitlist = addrwaitlist_syncfunc(sfunc, iswaitfor);
      if (isvalid_synclinkd(waitlist)) {
         unlink_synclinkd(waitlist);
      }
   }

   if (0 != (sfunc->optfields & syncfunc_opt_CALLER)) {
      const int isstate = (sfunc->optfields & syncfunc_opt_STATE);
      synclink_t * caller = addrcaller_syncfunc(sfunc, structsize, isstate);
      if (isvalid_synclink(caller)) {
         unlink_synclink(caller);
      }
   }
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_sfparam(void)
{
   struct syncrunner_t *  R = (struct syncrunner_t*)1;
   syncfunc_param_t sfparam = syncfunc_param_FREE;

   // TEST syncfunc_param_FREE
   TEST(0 == sfparam.syncrun);
   TEST(0 == sfparam.contoffset);
   TEST(0 == sfparam.state);
   TEST(0 == sfparam.condition);
   TEST(0 == sfparam.waiterr);
   TEST(0 == sfparam.retcode);

   // TEST syncfunc_param_INIT
   sfparam = (syncfunc_param_t) syncfunc_param_INIT(R);
   TEST(R == sfparam.syncrun);
   TEST(0 == sfparam.contoffset);
   TEST(0 == sfparam.state);
   TEST(0 == sfparam.condition);
   TEST(0 == sfparam.waiterr);
   TEST(0 == sfparam.retcode);

   return 0;
ONERR:
   return EINVAL;

}

// forward
static int test_start_sf(syncfunc_param_t * sfparam, uint32_t sfcmd);

static int test_initfree(void)
{
   syncfunc_t sfunc = syncfunc_FREE;

   // TEST syncfunc_FREE
   TEST(0 == sfunc.mainfct);
   TEST(0 == sfunc.contoffset);
   TEST(0 == sfunc.optfields);
   TEST(0 == sfunc.waitfor.link);
   TEST(0 == sfunc.waitlist.prev);
   TEST(0 == sfunc.waitlist.next);
   TEST(0 == sfunc.caller.link);
   TEST(0 == sfunc.state);

   // TEST init_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      memset(&sfunc, 255, sizeof(sfunc));
      init_syncfunc(&sfunc, &test_start_sf, opt);
      TEST(sfunc.mainfct    == &test_start_sf);
      TEST(sfunc.contoffset == 0);
      TEST(sfunc.optfields  == opt);
      // not initialised
      TEST(sfunc.waitfor.link  != 0);
      TEST(sfunc.waitlist.prev != 0);
      TEST(sfunc.waitlist.next != 0);
      TEST(sfunc.caller.link   != 0);
      TEST(sfunc.state         != 0);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_getset(void)
{
   syncfunc_t sfunc = syncfunc_FREE;
   syncfunc_t sfunc2 = syncfunc_FREE;
   size_t     size;

   // TEST getsize_syncfunc: syncfunc_opt_NONE
   TEST(offsetof(syncfunc_t, waitfor) == getsize_syncfunc(syncfunc_opt_NONE));

   // TEST getsize_syncfunc: syncfunc_opt_ALL
   TEST(sizeof(syncfunc_t) == getsize_syncfunc(syncfunc_opt_ALL));

   // TEST getsize_syncfunc: combination of flags
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      size = offsetof(syncfunc_t, waitfor);
      if (opt & syncfunc_opt_WAITFOR)  size += sizeof(sfunc.waitfor);
      if (opt & syncfunc_opt_WAITLIST) size += sizeof(sfunc.waitlist);
      if (opt & syncfunc_opt_CALLER)   size += sizeof(sfunc.caller);
      if (opt & syncfunc_opt_STATE)    size += sizeof(sfunc.state);
      TEST(size == getsize_syncfunc(opt));
   }

   // TEST offwaitfor_syncfunc
   TEST(offsetof(syncfunc_t, waitfor) == offwaitfor_syncfunc());

   // TEST offwaitlist_syncfunc
   TEST(offwaitlist_syncfunc(false) == offwaitfor_syncfunc());
   TEST(offwaitlist_syncfunc(true)  == offwaitfor_syncfunc() + sizeof(sfunc.waitfor));

   // TEST offcaller_syncfunc
   for (size = getsize_syncfunc(syncfunc_opt_ALL); size >= offwaitfor_syncfunc(); --size) {
      for (int isstate = 0; isstate <= 1; ++isstate) {
         for (int iscaller = 0; iscaller <= 1; ++iscaller) {
            const size_t expect = size
                                - (isstate ? sizeof(sfunc.state) : 0)
                                - (iscaller ? sizeof(sfunc.caller) : 0);
            TEST(expect == offcaller_syncfunc(size, isstate, iscaller));
         }
      }
   }

   // TEST offstate_syncfunc
   for (size = getsize_syncfunc(syncfunc_opt_ALL); size >= offwaitfor_syncfunc(); --size) {
      for (int isstate = 0; isstate <= 1; ++isstate) {
         const size_t expect = size
                             - (isstate ? sizeof(sfunc.state) : 0);
         TEST(expect == offstate_syncfunc(size, isstate));
      }
   }

   // TEST addrwaitresult_syncfunc
   TEST(&sfunc.waitresult == addrwaitresult_syncfunc(&sfunc));

   // TEST addrwaitfor_syncfunc
   TEST(&sfunc.waitfor == addrwaitfor_syncfunc(&sfunc));

   // TEST addrwaitlist_syncfunc
   TEST(addrwaitlist_syncfunc(&sfunc, true)  == &sfunc.waitlist);
   TEST(addrwaitlist_syncfunc(&sfunc, false) == (synclinkd_t*)&sfunc.waitfor);

   // TEST addrcaller_syncfunc
   size = getsize_syncfunc(syncfunc_opt_ALL);
   TEST(addrcaller_syncfunc(&sfunc, size, true) == &sfunc.caller);
   for (size = getsize_syncfunc(syncfunc_opt_ALL); size >= offwaitfor_syncfunc(); --size) {
      for (int isstate = 0; isstate <= 1; ++isstate) {
         void * expect = (uint8_t*) &sfunc
                       + size
                       - (isstate ? sizeof(sfunc.state) : 0)
                       - sizeof(sfunc.caller);
         TEST(addrcaller_syncfunc(&sfunc, size, isstate) == (synclink_t*)expect);
      }
   }

   // TEST addrstate_syncfunc
   size = getsize_syncfunc(syncfunc_opt_ALL);
   TEST(addrstate_syncfunc(&sfunc, size) == &sfunc.state);
   for (size = getsize_syncfunc(syncfunc_opt_ALL); size >= offwaitfor_syncfunc(); --size) {
      void * expect = (uint8_t*) &sfunc
                    + size
                    - sizeof(sfunc.state);
      TEST(addrstate_syncfunc(&sfunc, size) == (void**)expect);
   }

   // TEST waitforcast_syncfunc: 0 value return invalid address
   TEST(0 != waitforcast_syncfunc(0));

   // TEST waitlistcast_syncfunc: 0 value return invalid address
   TEST(0 != waitlistcast_syncfunc(0, true));
   TEST(0 != waitlistcast_syncfunc(0, false));

   // TEST waitforcast_syncfunc: value returned from addrwaitfor_syncfunc(&sfunc)
   TEST(&sfunc == waitforcast_syncfunc(addrwaitfor_syncfunc(&sfunc)));

   // TEST waitlistcast_syncfunc: value returned from addrwaitlist_syncfunc(&sfunc, ?)
   TEST(&sfunc == waitlistcast_syncfunc(addrwaitlist_syncfunc(&sfunc, true), true));
   TEST(&sfunc == waitlistcast_syncfunc(addrwaitlist_syncfunc(&sfunc, false), false));

   // TEST waitforcast_syncfunc: connected link
   init_synclink(&sfunc.waitfor, &sfunc2.caller);
   TEST(&sfunc == waitforcast_syncfunc(sfunc2.caller.link));

   // TEST waitlistcast_syncfunc: connected link
   init_synclinkd(addrwaitlist_syncfunc(&sfunc, true), &sfunc2.waitlist);
   TEST(&sfunc == waitlistcast_syncfunc(sfunc2.waitlist.next, true));
   TEST(&sfunc == waitlistcast_syncfunc(sfunc2.waitlist.prev, true));
   init_synclinkd(addrwaitlist_syncfunc(&sfunc, false), &sfunc2.waitlist);
   TEST(&sfunc == waitlistcast_syncfunc(sfunc2.waitlist.next, false));
   TEST(&sfunc == waitlistcast_syncfunc(sfunc2.waitlist.prev, false));

   // TEST setresult_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      for (int result = -10; result <= 10; ++result) {
         sfunc = (syncfunc_t) syncfunc_FREE;
         sfunc.optfields = opt;
         setresult_syncfunc(&sfunc, result);
         TEST(sfunc.optfields  == (opt | syncfunc_opt_WAITRESULT));
         TEST(sfunc.waitresult == result);
      }
   }

   // TEST relink_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      const int  iswaitfor = (opt & syncfunc_opt_WAITFOR);
      const int  isresult  = (opt & syncfunc_opt_WAITRESULT);
      const int  iswaitlist = (opt & syncfunc_opt_WAITLIST);
      const int  iscaller = (opt & syncfunc_opt_CALLER);
      const int  isstate  = (opt & syncfunc_opt_STATE);

      size = getsize_syncfunc(opt);
      sfunc  = (syncfunc_t) syncfunc_FREE;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      sfunc.optfields = opt;

      // test relink_syncfunc: invalid links
      relink_syncfunc(&sfunc, size);
      if (iswaitfor) {
         addrwaitfor_syncfunc(&sfunc)->link = &sfunc2.waitfor;
      }
      if (iswaitlist) {
         addrwaitlist_syncfunc(&sfunc, iswaitfor)->prev = &sfunc2.waitlist;
         addrwaitlist_syncfunc(&sfunc, iswaitfor)->next = &sfunc2.waitlist;
      }
      if (iscaller) {
         addrcaller_syncfunc(&sfunc, size, isstate)->link = &sfunc2.caller;
      }

      // test relink_syncfunc: valid links
      relink_syncfunc(&sfunc, size);
      if (iswaitfor) {
         TEST(addrwaitfor_syncfunc(&sfunc)->link == &sfunc2.waitfor);
         if (!isresult) {
            TEST(addrwaitfor_syncfunc(&sfunc) == sfunc2.waitfor.link);
         } else {
            TEST(0 == isvalid_synclink(&sfunc2.waitfor));
         }
      } else {
         TEST(0 == isvalid_synclink(&sfunc2.waitfor));
      }
      if (iswaitlist) {
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor)->prev == &sfunc2.waitlist);
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor)->next == &sfunc2.waitlist);
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor) == sfunc2.waitlist.prev);
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor) == sfunc2.waitlist.next);
      } else {
         TEST(0 == isvalid_synclinkd(&sfunc2.waitlist));
      }
      if (iscaller) {
         TEST(addrcaller_syncfunc(&sfunc, size, isstate)->link == &sfunc2.caller);
         TEST(addrcaller_syncfunc(&sfunc, size, isstate) == sfunc2.caller.link);
      } else {
         TEST(0 == isvalid_synclink(&sfunc2.caller));
      }
   }

   // TEST unlink_syncfunc
   for (syncfunc_opt_e opt = 0; opt <= syncfunc_opt_ALL; ++opt) {
      const int  iswaitfor = (opt & syncfunc_opt_WAITFOR);
      const int  isresult  = (opt & syncfunc_opt_WAITRESULT);
      const int  iswaitlist = (opt & syncfunc_opt_WAITLIST);
      const int  iscaller = (opt & syncfunc_opt_CALLER);
      const int  isstate  = (opt & syncfunc_opt_STATE);

      size = getsize_syncfunc(opt);
      sfunc  = (syncfunc_t) syncfunc_FREE;
      sfunc2 = (syncfunc_t) syncfunc_FREE;
      sfunc.optfields = opt;

      // test unlink_syncfunc: invalid links (nothing is done)
      unlink_syncfunc(&sfunc, size);
      if (iswaitfor) {
         init_synclink(addrwaitfor_syncfunc(&sfunc), &sfunc2.waitfor);
      }
      if (iswaitlist) {
         init_synclinkd(addrwaitlist_syncfunc(&sfunc, iswaitfor), &sfunc2.waitlist);
      }
      if (iscaller) {
         init_synclink(addrcaller_syncfunc(&sfunc, size, isstate), &sfunc2.caller);
      }

      // test unlink_syncfunc: valid links
      unlink_syncfunc(&sfunc, size);
      // link target sfunc2 is made invalid
      if (iswaitfor && isresult) {
         TEST(addrwaitfor_syncfunc(&sfunc) == sfunc2.waitfor.link);
      } else {
         TEST(0 == isvalid_synclink(&sfunc2.waitfor));
      }
      TEST(0 == isvalid_synclinkd(&sfunc2.waitlist));
      TEST(0 == isvalid_synclink(&sfunc2.caller));
      // sfunc is not changed
      if (iswaitfor) {
         TEST(addrwaitfor_syncfunc(&sfunc)->link == &sfunc2.waitfor);
      }
      if (iswaitlist) {
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor)->prev == &sfunc2.waitlist);
         TEST(addrwaitlist_syncfunc(&sfunc, iswaitfor)->next == &sfunc2.waitlist);
      }
      if (iscaller) {
         TEST(addrcaller_syncfunc(&sfunc, size, isstate)->link == &sfunc2.caller);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_start_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONEXIT);

   // is executed in case of wrong sfcmd value
   sfparam->retcode = -1;
   return -1;

ONRUN:
   sfparam->contoffset = (uint16_t) __extension__ ((uintptr_t) &&ONCONTINUE - (uintptr_t) &&syncfunc_START);
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

static int test_wait_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN: ;
   int err = wait_syncfunc(sfparam, (void*)1);
   if (err) goto ONERR;
   err = wait_syncfunc(sfparam, (void*)2);
   if (err) goto ONERR;
   err = wait_syncfunc(sfparam, (void*)3);
   if (err) goto ONERR;
   return syncfunc_cmd_EXIT;

ONERR:
   return -1;
}

static int test_waiterr_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN: ;
   intptr_t err = sfparam->waiterr;
   err = wait_syncfunc(sfparam, (void*)err);
   err = wait_syncfunc(sfparam, (void*)err);
   err = wait_syncfunc(sfparam, (void*)err);
   return syncfunc_cmd_EXIT;

ONERR:
   return -1;
}

static int test_waitexit_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN: ;
   int retcode = sfparam->retcode;
   int err;
   sfparam->state = (void*)(intptr_t)retcode;
   err = waitexit_syncfunc(sfparam, &retcode);
   if (err) goto ONERR;
   sfparam->state = (void*)(intptr_t)retcode;
   err = waitexit_syncfunc(sfparam, &retcode);
   if (err) goto ONERR;
   sfparam->state = (void*)(intptr_t)retcode;
   err = waitexit_syncfunc(sfparam, &retcode);
   if (err) goto ONERR;
   sfparam->state = (void*)(intptr_t)retcode;
   return syncfunc_cmd_EXIT;

ONERR:
   return -1;
}

static int test_waitexiterr_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN: ;
   int retcode = sfparam->retcode;
   intptr_t err = sfparam->waiterr;
   sfparam->state = (void*)(intptr_t)retcode;
   sfparam->syncrun = (void*)err;
   err = waitexit_syncfunc(sfparam, &retcode);
   sfparam->state = (void*)(intptr_t)retcode;
   sfparam->syncrun = (void*)err;
   err = waitexit_syncfunc(sfparam, &retcode);
   sfparam->state = (void*)(intptr_t)retcode;
   sfparam->syncrun = (void*)err;
   err = waitexit_syncfunc(sfparam, &retcode);
   sfparam->state = (void*)(intptr_t)retcode;
   sfparam->syncrun = (void*)err;
   return syncfunc_cmd_EXIT;

ONERR:
   return -1;
}

static int test_yield_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   start_syncfunc(sfparam, sfcmd, ONRUN, ONERR);

   goto ONERR;

ONRUN:
   sfparam->retcode += 19;
   yield_syncfunc(sfparam);
   sfparam->retcode += 20;
   yield_syncfunc(sfparam);
   sfparam->retcode += 21;
   sfparam->contoffset = 0;
   return syncfunc_cmd_EXIT;

ONERR:
   return -1;
}

static int test_implsupport(void)
{
   syncfunc_param_t sfparam = syncfunc_param_FREE;

   // TEST getstate_syncfunc
   TEST(0 == getstate_syncfunc(&sfparam));
   for (uintptr_t i = 1; i; i <<= 1) {
      sfparam.state = (void*) i;
      TEST((void*)i == getstate_syncfunc(&sfparam));
   }

   // TEST setstate_syncfunc
   memset(&sfparam, 0, sizeof(sfparam));
   for (uintptr_t i = 1; i; i <<= 1) {
      setstate_syncfunc(&sfparam, (void*) i);
      TEST((void*)i == getstate_syncfunc(&sfparam));
   }
   setstate_syncfunc(&sfparam, 0);
   TEST(0 == getstate_syncfunc(&sfparam));

   // TEST start_syncfunc: valid sfcmd values
   static_assert(syncfunc_cmd_RUN == 0 && syncfunc_cmd_EXIT== 2, "3 different sfcmd");
   for (int cmd = syncfunc_cmd_RUN; cmd <= syncfunc_cmd_EXIT; ++cmd) {
      sfparam.retcode = 0;
      TEST(cmd == test_start_sf(&sfparam, (syncfunc_cmd_e)cmd));
      TEST(sfparam.syncrun     == 0);
      TEST(sfparam.contoffset  != 0);
      TEST(sfparam.state       == 0);
      TEST(sfparam.condition   == 0);
      TEST(sfparam.waiterr     == 0);
      TEST(sfparam.retcode -10 == cmd);
   }

   // TEST start_syncfunc: invalid sfcmd value
   sfparam.contoffset = 0;
   for (int cmd = syncfunc_cmd_WAIT; cmd <= syncfunc_cmd_WAIT+16; ++cmd) {
      sfparam.retcode = 0;
      TEST(-1 == test_start_sf(&sfparam, (syncfunc_cmd_e)cmd));
      TEST( 0 == sfparam.syncrun);
      TEST( 0 == sfparam.contoffset);
      TEST( 0 == sfparam.state);
      TEST( 0 == sfparam.condition);
      TEST( 0 == sfparam.waiterr);
      TEST(-1 == sfparam.retcode);
   }

   // TEST exit_syncfunc
   for (uint32_t cmd = 0; cmd <= 100000; cmd += 10000) {
      sfparam.retcode = -1;
      TEST(syncfunc_cmd_EXIT == test_exit_sf(&sfparam, cmd));
      TEST(sfparam.syncrun   == 0);
      TEST(sfparam.contoffset == 0);
      TEST(sfparam.state     == 0);
      TEST(sfparam.condition == 0);
      TEST(sfparam.waiterr   == 0);
      TEST(sfparam.retcode   == (int)cmd);
   }

   // TEST wait_syncfunc: waiterr == 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? syncfunc_cmd_WAIT : syncfunc_cmd_EXIT;
      sfparam.condition = 0;
      TEST(result == test_wait_sf(&sfparam, i == 1 ? syncfunc_cmd_RUN : syncfunc_cmd_CONTINUE));
      TEST(0 == sfparam.syncrun);
      TEST(0 == sfparam.state);
      if (i != 4) {
         TEST(oldoff   != sfparam.contoffset);
         TEST((void*)i == sfparam.condition);
      } else {
         TEST(oldoff == sfparam.contoffset);
         TEST(0      == sfparam.condition);
      }
      TEST(0 == sfparam.waiterr);
      TEST(0 == sfparam.retcode);
   }

   // TEST wait_syncfunc: waiterr != 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? syncfunc_cmd_WAIT : syncfunc_cmd_EXIT;
      sfparam.condition = 0;
      sfparam.waiterr   = i;
      TEST(result == test_waiterr_sf(&sfparam, i == 1 ? syncfunc_cmd_RUN : syncfunc_cmd_CONTINUE));
      TEST(0 == sfparam.syncrun);
      TEST(0 == sfparam.state);
      if (i != 4) {
         TEST(oldoff   != sfparam.contoffset);
         TEST((void*)i == sfparam.condition);
      } else {
         TEST(oldoff == sfparam.contoffset);
         TEST(0      == sfparam.condition);
      }
      TEST(i == sfparam.waiterr);
      TEST(0 == sfparam.retcode);
   }

   // TEST waitexit_syncfunc: waiterr == 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? syncfunc_cmd_WAIT : syncfunc_cmd_EXIT;
      sfparam.state     = 0 ;
      sfparam.condition = (void*)1;
      sfparam.retcode   = (int) i;
      TEST(result == test_waitexit_sf(&sfparam, i == 1 ? syncfunc_cmd_RUN : syncfunc_cmd_CONTINUE));
      TEST(0        == sfparam.syncrun);
      TEST((void*)i == sfparam.state);
      if (i != 4) {
         TEST(oldoff != sfparam.contoffset);
         TEST(0      == sfparam.condition); // clears condition
      } else {
         TEST(oldoff   == sfparam.contoffset);
         TEST((void*)1 == sfparam.condition);
      }
      TEST(0 == sfparam.waiterr);
      TEST(i == sfparam.retcode);
   }

   // TEST waitexit_syncfunc: waiterr != 0
   memset(&sfparam, 0, sizeof(sfparam));
   for (intptr_t i = 1; i <= 4; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 4 ? syncfunc_cmd_WAIT : syncfunc_cmd_EXIT;
      sfparam.syncrun   = 0;
      sfparam.state     = 0;
      sfparam.condition = (void*)1;
      sfparam.waiterr   = i;
      sfparam.retcode   = (int) -i;
      TEST(result == test_waitexiterr_sf(&sfparam, i == 1 ? syncfunc_cmd_RUN : syncfunc_cmd_CONTINUE));
      TEST((void*) i == sfparam.syncrun);
      TEST((void*)-i == sfparam.state);
      if (i != 4) {
         TEST(oldoff != sfparam.contoffset);
         TEST(0      == sfparam.condition); // clears condition
      } else {
         TEST(oldoff   == sfparam.contoffset);
         TEST((void*)1 == sfparam.condition);
      }
      TEST(i  == sfparam.waiterr);
      TEST(-i == sfparam.retcode);
   }

   // TEST yield_syncfunc
   memset(&sfparam, 0, sizeof(sfparam));
   for (int i = 19; i <= 21; ++i) {
      uint16_t oldoff = sfparam.contoffset;
      int      result = i != 21 ? syncfunc_cmd_CONTINUE : syncfunc_cmd_EXIT;
      sfparam.retcode = 0;
      TEST(result == test_yield_sf(&sfparam, i == 19 ? syncfunc_cmd_RUN : syncfunc_cmd_CONTINUE));
      TEST(0 == sfparam.syncrun);
      TEST(0 == sfparam.state);
      TEST(oldoff != sfparam.contoffset);
      TEST(0 == sfparam.condition);
      TEST(0 == sfparam.waiterr);
      TEST(i == sfparam.retcode);
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_syncfunc()
{
   if (test_sfparam())        goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_getset())         goto ONERR;
   if (test_implsupport())    goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
