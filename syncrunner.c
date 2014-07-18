/* title: SyncRunner impl

   Implements <SyncRunner>.

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

   file: C-kern/api/task/syncrunner.h
    Header file <SyncRunner>.

   file: C-kern/task/syncrunner.c
    Implementation file <SyncRunner impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/syncrunner.h"
#include "C-kern/api/task/synccond.h"
#include "C-kern/api/task/syncfunc.h"
#include "C-kern/api/task/synclink.h"
#include "C-kern/api/task/syncqueue.h"
#include "C-kern/api/err.h"
#include "C-kern/api/ds/foreach.h"
#include "C-kern/api/ds/inmem/queue.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#include "C-kern/api/test/errortimer.h"
#endif


// section: syncrunner_t

// group: static variables

#ifdef KONFIG_UNITTEST
/* variable: s_syncrunner_errtimer
 * Simulate errors in <free_syncrunner>. */
static test_errortimer_t s_syncrunner_errtimer = test_errortimer_FREE;
#endif

/* variable: s_syncrunner_rwqsize
 * Die Größe der Elemente in der run und der wait queue. */
static uint8_t s_syncrunner_rwqsize[6] = {
   // run queues
   getsize_syncfunc(syncfunc_opt_NONE),
   getsize_syncfunc(syncfunc_opt_CALLER),
   getsize_syncfunc(syncfunc_opt_CALLER|syncfunc_opt_STATE),
   // wait queues
   getsize_syncfunc(syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST),
   getsize_syncfunc(syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST|syncfunc_opt_CALLER),
   getsize_syncfunc(syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST|syncfunc_opt_CALLER|syncfunc_opt_STATE)
};

// group: constants

/* define: WAITQUEUE_OFFSET
 * Runqueues belegen Indizes [0 .. WAITQUEUE_OFFSET-1].
 * Waitqueues belegen Indizes [OFFSET_WAITQUEUE .. lengthof(s_syncrunner_rwqsize)-1]. */
#define WAITQUEUE_OFFSET   3

// group: memory

/* define: MOVE
 * Verschiebe Speicher von src nach dest in Einheiten von long.
 *
 * Unchecked Precondition:
 * o (0 == structsize % sizeof(long)) */
#define MOVE(dest, src, structsize) \
         do { \
            unsigned _nr = (structsize) / sizeof(long); \
            unsigned _n  = 0; \
            long * _s = (long*) (src); \
            long * _d = (long*) (dest); \
            do { \
               _d[_n] = _s[_n]; \
            } while (++_n < _nr); \
         } while (0)

/* function: move_syncfunc
 * Verschiebt <syncfunc_t> der Größe size von src nach dest.
 * In src enthaltene Links werden dabei angepasst, so daß
 * Linkziele nach dem Verschieben korrekt zurückzeigen. */
static void move_syncfunc(syncfunc_t * dest, syncfunc_t * src, uint16_t structsize)
{
   if (src != dest) {
      // move in memory

      MOVE(dest, src, structsize);

      // adapt links to new location

      relink_syncfunc(dest, structsize);
   }
}

// group: lifetime

int init_syncrunner(/*out*/syncrunner_t * srun)
{
   int err;
   unsigned qidx;

   static_assert( lengthof(s_syncrunner_rwqsize) == lengthof(srun->rwqueue),
                  "every queue has its own size");

   for (qidx = 0; qidx < lengthof(srun->rwqueue); ++qidx) {
      err = init_syncqueue(&srun->rwqueue[qidx], s_syncrunner_rwqsize[qidx], (uint8_t) qidx);
      if (err) goto ONERR;
   }

   srun->caller = 0;
   initself_synclinkd(&srun->wakeup);

   return 0;
ONERR:
   while (qidx > 0) {
      (void) free_syncqueue(&srun->rwqueue[--qidx]);
   }
   return err;
}

int free_syncrunner(syncrunner_t * srun)
{
   int err = 0;
   int err2;

   for (unsigned i = 0; i < lengthof(srun->rwqueue); ++i) {
      err2 = free_syncqueue(&srun->rwqueue[i]);
      SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err2);
      if (err2) err = err2;
   }

   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

// group: queue-helper

/* function: find_run_queue
 * Liefere aus optfields berechneten index für <s_syncrunner_rwqsize> und <rwqueue>.
 *
 * Precondition:
 * - (0 == (optfields & syncfunc_opt_WAITFOR)) && (0 == (optfields & syncfunc_opt_WAITLIST)) */
static inline unsigned find_run_queue(const syncfunc_opt_e optfields)
{
   static_assert( getsize_syncfunc(syncfunc_opt_STATE) == getsize_syncfunc(syncfunc_opt_CALLER),
                  "do not differentiate between syncfunc_opt_STATE or syncfunc_opt_CALLER");

   int run_queue_index = ((optfields & syncfunc_opt_CALLER) != 0)
                       + ((optfields & syncfunc_opt_STATE) != 0);

   static_assert( lengthof(((syncrunner_t*)0)->rwqueue) >= WAITQUEUE_OFFSET
                  && WAITQUEUE_OFFSET == 3, "returned value is in range");

   return (unsigned) run_queue_index/*0 .. 2*/;
}

/* function: find_wait_queue
 * Liefere aus optfields berechneten index für <s_syncrunner_rwqsize> und <rwqueue>.
 *
 * Precondition:
 * - (0 != (optfields & syncfunc_opt_WAITFOR)) && (0 != (optfields & syncfunc_opt_WAITLIST)) */
static inline unsigned find_wait_queue(const syncfunc_opt_e optfields)
{
   static_assert( getsize_syncfunc(syncfunc_opt_STATE) == getsize_syncfunc(syncfunc_opt_CALLER),
                  "do not differentiate between syncfunc_opt_STATE or syncfunc_opt_CALLER");

   int wait_queue_index = ((optfields & syncfunc_opt_CALLER) != 0)
                        + ((optfields & syncfunc_opt_STATE) != 0);

   static_assert( WAITQUEUE_OFFSET == 3
                  && lengthof(((syncrunner_t*)0)->rwqueue) == 6, "returned value is in range");

   return WAITQUEUE_OFFSET + (unsigned) wait_queue_index/*0 .. 2*/;
}

/* function: remove_syncqueue
 * Lösche sfunc aus squeue.
 * Entweder wird das letzte Element in squeue nach sfunc kopiert
 * oder, wenn das letzte Element das preallokierte Element ist,
 * einfach sfunc als neues freies Element markiert.
 * Das letzte Element wird dann aus der Queue gelöscht.
 *
 * Unchecked Precondition:
 * o Alle Links in sfunc sind ungültig;
 *   (sfunc->waitfor.link == 0 && sfunc->caller.link == 0 && sfunc->waitlist.prev == 0 && sfunc->waitlist.next == 0).
 * o sfunc ist in queue abgelegt / sfunc ∈ queue / (squeue == queuefromaddr_syncqueue(sfunc))
 * o nextfree_syncqueue(squeue) == 0 || (nextfree_syncqueue(squeue) != 0 && "content is free and not on use")
 * */
static int remove_syncqueue(syncqueue_t * squeue, syncfunc_t * sfunc)
{
   int err;
   queue_t * queue = genericcast_queue(squeue);
   void    * last  = last_queue(queue, elemsize_syncqueue(squeue));

   if (!last) {
      // should never happen cause at least sfunc is stored in squeue
      err = ENODATA;
      goto ONERR;
   }

   if (last == nextfree_syncqueue(squeue)) {
      setnextfree_syncqueue(squeue, sfunc);
   } else {
      move_syncfunc(sfunc, last, elemsize_syncqueue(squeue));
   }

   err = removelast_syncqueue(squeue);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

/* function: wait_queue
 * Gib <syncqueue_t> zurück, in der sfunc abgelegt ist.
 * Der Wert 0 wird zurückgegeben, wenn sfunc nicht zu srun gehört. */
static inline syncqueue_t * wait_queue(syncrunner_t * srun, syncfunc_t * sfunc)
{
   syncqueue_t * squeue = queuefromaddr_syncqueue(sfunc);
   return squeue && squeue == &srun->rwqueue[find_wait_queue(sfunc->optfields)] ? squeue : 0;
}

// group: query

size_t size_syncrunner(const syncrunner_t * srun)
{
   size_t size = size_syncqueue(&srun->rwqueue[0]);
   size -= (0 != nextfree_syncqueue(&srun->rwqueue[0]));

   for (unsigned i = 1; i < lengthof(srun->rwqueue); ++i) {
      size += size_syncqueue(&srun->rwqueue[i]);
      size -= (0 != nextfree_syncqueue(&srun->rwqueue[0]));
   }

   return size;
}

// group: update

int addasync_syncrunner(syncrunner_t * srun, syncfunc_f mainfct, void * state)
{
   int err;
   syncfunc_t * sf;

   if (state) {
      const unsigned qidx = find_run_queue(syncfunc_opt_STATE);

      sf  = nextfree_syncqueue(&srun->rwqueue[qidx]);
      ONERROR_testerrortimer(&s_syncrunner_errtimer, &err, ONERR);
      err = preallocate_syncqueue(&srun->rwqueue[qidx]);
      if (err) goto ONERR;

      init_syncfunc(sf, mainfct, syncfunc_opt_STATE);
      *addrstate_syncfunc(sf, elemsize_syncqueue(&srun->rwqueue[qidx])) = state;

   } else {
      const unsigned qidx = find_run_queue(syncfunc_opt_NONE);

      sf  = nextfree_syncqueue(&srun->rwqueue[qidx]);
      ONERROR_testerrortimer(&s_syncrunner_errtimer, &err, ONERR);
      err = preallocate_syncqueue(&srun->rwqueue[qidx]);
      if (err) goto ONERR;

      init_syncfunc(sf, mainfct, syncfunc_opt_NONE);
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int addcall_syncrunner(syncrunner_t * srun, syncfunc_f mainfct, void * state)
{
   int err;
   syncfunc_t * sf;

   if (state) {
      const unsigned qidx = find_run_queue(syncfunc_opt_CALLER|syncfunc_opt_STATE);
      const uint16_t size = elemsize_syncqueue(&srun->rwqueue[qidx]);

      sf  = nextfree_syncqueue(&srun->rwqueue[qidx]);
      ONERROR_testerrortimer(&s_syncrunner_errtimer, &err, ONERR);
      err = preallocate_syncqueue(&srun->rwqueue[qidx]);
      if (err) goto ONERR;

      init_syncfunc(sf, mainfct, syncfunc_opt_CALLER|syncfunc_opt_STATE);
      *addrcaller_syncfunc(sf, size, true) = (synclink_t) synclink_FREE;
      *addrstate_syncfunc(sf, size) = state;

      srun->caller = addrcaller_syncfunc(sf, size, true);

   } else {
      const unsigned qidx = find_run_queue(syncfunc_opt_CALLER);
      const uint16_t size = elemsize_syncqueue(&srun->rwqueue[qidx]);

      sf  = nextfree_syncqueue(&srun->rwqueue[qidx]);
      ONERROR_testerrortimer(&s_syncrunner_errtimer, &err, ONERR);
      err = preallocate_syncqueue(&srun->rwqueue[qidx]);
      if (err) goto ONERR;

      init_syncfunc(sf, mainfct, syncfunc_opt_CALLER);
      *addrcaller_syncfunc(sf, size, false) = (synclink_t) synclink_FREE;

      srun->caller = addrcaller_syncfunc(sf, size, false);
   }

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

/* function: link_to_wakeup
 * Füge einzelnes waitlist von <syncfunc_t> zu <syncrunner_t.wakeup> hinzu. */
static inline void link_to_wakeup(syncrunner_t * srun, synclinkd_t * waitlist)
{
   initprev_synclinkd(waitlist, &srun->wakeup);
}

/* function: linkall_to_wakeup
 * Füge waitlist von <syncfunc_t> und alle, auf die es zeigt, zu <syncrunner_t.wakeup> hinzu. */
static inline void linkall_to_wakeup(syncrunner_t * srun, synclinkd_t * waitlist)
{
   spliceprev_synclinkd(waitlist, &srun->wakeup);
}

/* function: wakeup_caller
 * Fügt <syncfunc_t.caller> (falls vorhanden) am Ende von <syncrunner_t.wakeup> hinzu.
 *
 * Unchecked Precondition:
 * o size == getsize_syncfunc(sfunc->optfields)
 * o (isstate != 0) == (0 != (sfunc->optfields & syncfunc_opt_STATE))
 * o retcode == syncfunc_param_t.retcode
 * o sfunc returned synccmd_EXIT
 * */
static inline void wakeup_caller(syncrunner_t * srun, syncfunc_t * sfunc, uint16_t size, int isstate, int retcode)
{
   if (0 != (sfunc->optfields & syncfunc_opt_CALLER)) {
      synclink_t * caller = addrcaller_syncfunc(sfunc, size, isstate);

      if (isvalid_synclink(caller)) {
         syncfunc_t * wakeup = waitforcast_syncfunc(caller->link);
         *caller = (synclink_t) synclink_FREE;
         setresult_syncfunc(wakeup, retcode);
         link_to_wakeup(srun, addrwaitlist_syncfunc(wakeup, true));
      }
   }
}

/* function: wakeup2_syncrunner
 * Implementiert <wakeup_syncrunner> und <wakeupall_syncrunner>.
 * Parameter isall == true bestimmt, daß alle wartenden Funktionen aufgeweckt werden sollen. */
static inline int wakeup2_syncrunner(syncrunner_t * srun, synccond_t * scond, const bool isall)
{
   if (! iswaiting_synccond(scond)) return 0;

   syncfunc_t  * wakeupfunc = waitfunc_synccond(scond);
   synclinkd_t * waitlist = addrwaitlist_syncfunc(wakeupfunc, true);
   syncqueue_t * squeue   = wait_queue(srun, wakeupfunc);

   if (!squeue) return EINVAL;

   unlink_synccond(scond);

   if (! isvalid_synclinkd(waitlist)) {
      // last waiting function
      link_to_wakeup(srun, waitlist);

   } else if (isall) {
      linkall_to_wakeup(srun, waitlist);

   } else {
      syncfunc_t * sfunc = waitlistcast_syncfunc(waitlist->next, true);
      link_synccond(scond, sfunc);
      unlink_synclinkd(waitlist);
      link_to_wakeup(srun, waitlist);
   }

   return 0;
}

int wakeup_syncrunner(syncrunner_t * srun, struct synccond_t * scond)
{
   int err;

   err = wakeup2_syncrunner(srun, scond, false);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int wakeupall_syncrunner(syncrunner_t * srun, struct synccond_t * scond)
{
   int err;

   err = wakeup2_syncrunner(srun, scond, true);
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: execute

/* define: CALL_SYNCFUNC
 * Bereite srun und param vor und rufe sfunc->mainfct auf.
 * Parameter werden mehrfach ausgewertet!
 * Setzt <syncrunner_t.caller> auf 0.
 * Kopiert <syncfunc_t.contoffset> nach <syncfunc_param_t.contoffset>.
 * Kopiert <syncfunc_t.state> nach <syncfunc_param_t.state>, falls isstate != 0,
 * ansonsten wird <syncfunc_param_t.state> auf 0 gesetzt.
 * <syncfunc_t.mainfct> wird mit Kommando <synccmd_RUN> bzw. <synccmd_CONTINUE> aufgerufen,
 * je nachdem, ob <syncfunc_t.contoffset> 0 ist oder nicht.
 *
 * Parameter:
 * srun  - Pointer auf <syncrunner_t>.
 * sfunc - Pointer auf <syncfunc_t>.
 * size  - (size == getsize_syncfunc(sfunc->optfields))
 * isstate - (isstate != 0) == (0 != (sfunc->optfields & syncfunc_opt_STATE))
 * param - Name von lokalem <syncfunc_param_t>
 *
 * Unchecked Precondition:
 * o (0 != isstate) == (0 != (sfunc->optfields & syncfunc_opt_STATE))
 * o size == getsize_syncfunc(sfunc->optfields) */
#define CALL_SYNCFUNC(srun, sfunc, size, isstate, param) \
         ( __extension__ ({                                                                     \
            (srun)->caller = 0; /* prepare: ensure this func has no other function called */    \
            param.contoffset = (sfunc)->contoffset;                                             \
            param.state      = isstate ? *addrstate_syncfunc(sfunc, size) : 0;                  \
            (sfunc)->mainfct(&param, param.contoffset == 0 ? synccmd_RUN : synccmd_CONTINUE);   \
         }))

/* function: link_waitfields
 * Initialisiere <syncfunc_t.waitfor> und <syncfunc_t.waitlist>.
 * Ist param->condition == 0, dann wird <syncfunc_t.waitfor> mit srun->caller verbunden
 * und <syncfunc_t.waitlist> auf 0 gesetzt. Ist param->condition != 0, wird entweder
 * <syncfunc_t.waitfor> mit param->condition verbunden (<link_synccond>) und <syncfunc_t.waitlist>
 * genullt oder – falls schon eine andere <syncfunc_t> auf param->condition wartet –
 * <syncfunc_t.waitfor> genullt und mittels <syncfunc_t.waitlist> eine Warteliste aufgebaut.
 *
 * Unchecked Precondition:
 * o 0 != (sfunc->optfields & syncfunc_opt_WAITFOR)
 * o 0 != (sfunc->optfields & syncfunc_opt_WAITLIST)
 */
static inline void link_waitfields(syncrunner_t * srun, syncfunc_t * sfunc, syncfunc_param_t * param)
{
   if (param->condition) {
      if (! iswaiting_synccond(param->condition)) {
         link_synccond(param->condition, sfunc);
         *addrwaitlist_syncfunc(sfunc, true) = (synclinkd_t) synclinkd_FREE;

      } else {
         if (!wait_queue(srun, waitfunc_synccond(param->condition))/*other srun*/) goto ONERR;

         *addrwaitfor_syncfunc(sfunc) = (synclink_t) synclink_FREE;
         synclinkd_t * waitlist = addrwaitlist_syncfunc(waitfunc_synccond(param->condition), true);
         if (isvalid_synclinkd(waitlist)) {
            initprev_synclinkd(addrwaitlist_syncfunc(sfunc, true), waitlist);
         } else {
            init_synclinkd(addrwaitlist_syncfunc(sfunc, true), waitlist);
         }
      }

   } else {
      if (!srun->caller/*no function called*/) goto ONERR;
      init_synclink(addrwaitfor_syncfunc(sfunc), srun->caller);
      *addrwaitlist_syncfunc(sfunc, true) = (synclinkd_t) synclinkd_FREE;
   }

   return;
ONERR:
   // use syncfunc_param_t.waiterr to transport error
   clearopt_syncfunc(sfunc, syncfunc_opt_WAITFOR_CALLED);
   setopt_syncfunc(sfunc, syncfunc_opt_WAITFOR_CONDITION);
   setresult_syncfunc(sfunc, EINVAL);
   link_to_wakeup(srun, addrwaitlist_syncfunc(sfunc, true));
}

/* function: process_wakeup_list
 * Starte alle in <syncrunner_t.wakeup> gelistete <syncfunc_t>.
 * Der Rückgabewert vom Typ <synccmd_e> entscheidet, ob die Funktion weiterhin
 * in der Wartequeue verbleibt oder in die Runqueue verschoben wird.
 *
 * Falls in <syncfunc_t.optfields> die Bits <syncfunc_opt_WAITRESULT> und <syncfunc_opt_WAITFOR_CALLED>
 * gesetzt sind, wird <syncfunc_param_t.retcode> auf den Wert von <syncfunc_t.waitresult> gesetzt
 * und <syncfunc_param_t.waiterr> auf 0. Falls in <syncfunc_t.optfields> nur Bit <syncfunc_opt_WAITRESULT>
 * gesetzt ist (das Bit <syncfunc_opt_WAITFOR_CONDITION> wird als gesetzt angenommen,  da
 * <syncfunc_opt_WAITRESULT> entweder <syncfunc_opt_WAITFOR_CONDITION> oder <syncfunc_opt_WAITFOR_CALLED> erfordert),
 * ist der Wert in <syncfunc_param_t.retcode> undefiniert und <syncfunc_param_t.waiterr> wird auf <syncfunc_t.waitresult>
 * gesetzt.
 *
 * */
static inline int process_wakeup_list(syncrunner_t * srun)
{
   int err;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   syncqueue_t    * squeue;
   syncfunc_t     * sfunc;
   syncfunc_t     * sfunc2;
   int              isstate;
   int              cmd;
   unsigned         qidx2;
   syncfunc_opt_e   optfield2;
   uint16_t         size;
   uint16_t         size2;
   synclinkd_t      wakeup;

   if (isself_synclinkd(&srun->wakeup)) return 0;

   // this ensures that newly woken up syncfunc_t are not executed this time

   wakeup = srun->wakeup;
   relink_synclinkd(&wakeup);
   initself_synclinkd(&srun->wakeup);

   while (wakeup.next != &wakeup) {
      sfunc  = waitlistcast_syncfunc(wakeup.next, true);
      unlinkkeepself_synclinkd(wakeup.next);
      squeue = queuefromaddr_syncqueue(sfunc);
      size   = elemsize_syncqueue(squeue);
      isstate = (sfunc->optfields & syncfunc_opt_STATE);

      param.waiterr = 0;
      if (sfunc->optfields & syncfunc_opt_WAITRESULT) {
         if (sfunc->optfields & syncfunc_opt_WAITFOR_CALLED) {
            param.retcode = *addrwaitresult_syncfunc(sfunc);
         } else {
            param.waiterr = *addrwaitresult_syncfunc(sfunc);
         }
      }
      cmd = CALL_SYNCFUNC(srun, sfunc, size, isstate, param);

      switch ((synccmd_e)cmd) {
      default:
      case synccmd_RUN:
         param.contoffset = 0;
         // continue synccmd_CONTINUE;

      case synccmd_CONTINUE:
         // move from wait to run queue

         optfield2 = (param.state ? syncfunc_opt_STATE : 0) | (sfunc->optfields & syncfunc_opt_CALLER);
         qidx2   = find_run_queue(optfield2);
         sfunc2  = nextfree_syncqueue(&srun->rwqueue[qidx2]);
         size2   = elemsize_syncqueue(&srun->rwqueue[qidx2]);
         initmove_syncfunc(sfunc2, size2, param.contoffset, optfield2, param.state, sfunc, size, isstate);
         goto REMOVE_FROM_OLD_QUEUE;

      case synccmd_EXIT:
         wakeup_caller(srun, sfunc, size, isstate, param.retcode);
         err = remove_syncqueue(squeue, sfunc);
         SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
         if (err) goto ONERR;
         break;

      case synccmd_WAIT:

         if (isstate) {
            sfunc->contoffset = param.contoffset;
            *addrstate_syncfunc(sfunc, size) = param.state;

            // add optional state field ?

         } else if (param.state) {
            // calculate correct wait queue id and move into new queue
            optfield2 = (sfunc->optfields & syncfunc_opt_CALLER)
                      | (param.condition ? syncfunc_opt_STATE|syncfunc_opt_WAITLIST|syncfunc_opt_WAITFOR_CONDITION
                                         : syncfunc_opt_STATE|syncfunc_opt_WAITLIST|syncfunc_opt_WAITFOR_CALLED);
            qidx2 = find_wait_queue(optfield2);
            sfunc2 = nextfree_syncqueue(&srun->rwqueue[qidx2]);
            size2  = elemsize_syncqueue(&srun->rwqueue[qidx2]);
            initmove_syncfunc(sfunc2, size2, param.contoffset, optfield2, param.state, sfunc, size, false);
            // waitfor & waitlist are undefined ==> set it !
            link_waitfields(srun, sfunc2, &param);
            goto REMOVE_FROM_OLD_QUEUE;

         } else {
            sfunc->contoffset = param.contoffset;
         }

         // waitfor & waitlist are undefined ==> set it !
         sfunc->optfields = (uint8_t) ( (sfunc->optfields & ~syncfunc_opt_WAITFOR)
                          | (param.condition ? syncfunc_opt_WAITFOR_CONDITION : syncfunc_opt_WAITFOR_CALLED));
         link_waitfields(srun, sfunc, &param);
         break;
      }

      continue ;

      REMOVE_FROM_OLD_QUEUE: ;

      // mark nextfree as used
      //  - needed for "if (err) goto ONERR" cause nextfree is in use
      //  - needed if (srun->rwqueue[qidx2] == squeue) to make precondition valid for remove_syncqueue
      setnextfree_syncqueue(&srun->rwqueue[qidx2], 0);
      err = remove_syncqueue(squeue, sfunc);
      SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
      if (err) goto ONERR;
      err = preallocate_syncqueue(&srun->rwqueue[qidx2]);
      if (err) goto ONERR;
   }

   return 0;
ONERR:
   if (!isself_synclinkd(&wakeup)) {
      spliceprev_synclinkd(&srun->wakeup, &wakeup);
      unlinkkeepself_synclinkd(&wakeup);
   }
   return err;
}

int run2_syncrunner(syncrunner_t * srun, bool runwakeup)
{
   int err;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   queue_iterator_t iter = queue_iterator_FREE;
   syncqueue_t    * squeue;
   queue_t        * queue;
   syncfunc_t     * sfunc;
   syncfunc_t     * sfunc2;
   int              isstate;
   int              cmd;
   unsigned         qidx2;
   syncfunc_opt_e   optfield2;
   uint16_t         size;
   uint16_t         size2;

   if (srun->isrun) return EINPROGRESS;

   // prepare
   srun->isrun = true;

   // preallocate enough resources

   for (unsigned qidx = 0; qidx < lengthof(srun->rwqueue); ++qidx) {
      if (0 == nextfree_syncqueue(&srun->rwqueue[qidx])) {
         err = preallocate_syncqueue(&srun->rwqueue[qidx]);
         if (err) goto ONERR;
      }
   }

   // run every entry in run queue once
   // (if entries are moved to other run queue then qidx2 == qidx+1 ==> only run once)

   for (int runidx = WAITQUEUE_OFFSET-1; runidx >= 0; --runidx) {
      squeue = &srun->rwqueue[runidx];
      queue  = genericcast_queue(squeue);
      size   = elemsize_syncqueue(&srun->rwqueue[runidx]);

      // new entries are added to end of queue ==> do not run them during this invocation

      void * prev;
      bool isPrev;
      err = initlast_queueiterator(&iter, queue, size);
      if (err) goto ONERR;
      isPrev = prev_queueiterator(&iter, &prev);
      while (isPrev) {
         sfunc = prev;
         isPrev = prev_queueiterator(&iter, &prev); // makes calling remove_syncqueue(squeue, sfunc) safe
         if (sfunc == nextfree_syncqueue(squeue)) continue;

         isstate = (sfunc->optfields & syncfunc_opt_STATE);

         cmd = CALL_SYNCFUNC(srun, sfunc, size, isstate, param);

         switch ((synccmd_e)cmd) {
         default:
         case synccmd_RUN:
            param.contoffset = 0;
            // continue synccmd_CONTINUE;

         case synccmd_CONTINUE:
            if (isstate) {
               sfunc->contoffset = param.contoffset;
               *addrstate_syncfunc(sfunc, size) = param.state;

            // add optional state field ?

            } else if (param.state) {
               optfield2 = sfunc->optfields|syncfunc_opt_STATE;
               qidx2  = (unsigned)runidx + 1u; // optimization (size is incremented by sizeof(void*))
               sfunc2 = nextfree_syncqueue(&srun->rwqueue[qidx2]);
               size2  = elemsize_syncqueue(&srun->rwqueue[qidx2]);
               initmove_syncfunc(sfunc2, size2, param.contoffset, optfield2, param.state, sfunc, size, false);
               goto REMOVE_FROM_OLD_QUEUE;

            } else {
               sfunc->contoffset = param.contoffset;
            }

            break;

         case synccmd_EXIT:
            wakeup_caller(srun, sfunc, size, isstate, param.retcode);
            err = remove_syncqueue(squeue, sfunc);
            SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
            if (err) goto ONERR;
            break;

         case synccmd_WAIT:
            // calculate correct wait queue id and move into new queue
            optfield2 = (sfunc->optfields & syncfunc_opt_CALLER)
                      | (param.condition ? syncfunc_opt_WAITLIST|syncfunc_opt_WAITFOR_CONDITION : syncfunc_opt_WAITLIST|syncfunc_opt_WAITFOR_CALLED)
                      | (param.state ? syncfunc_opt_STATE : 0);
            qidx2 = find_wait_queue(optfield2);
            sfunc2 = nextfree_syncqueue(&srun->rwqueue[qidx2]);
            size2  = elemsize_syncqueue(&srun->rwqueue[qidx2]);
            initmove_syncfunc(sfunc2, size2, param.contoffset, optfield2, param.state, sfunc, size, isstate);
            // waitfor & waitlist are undefined ==> set it !
            link_waitfields(srun, sfunc2, &param);
            goto REMOVE_FROM_OLD_QUEUE;
         }

         continue ;

         REMOVE_FROM_OLD_QUEUE: ;

         // mark nextfree as used
         //  - needed for "if (err) goto ONERR" cause nextfree is in use
         //  - needed if (srun->rwqueue[qidx2] == squeue) to make precondition valid for remove_syncqueue
         setnextfree_syncqueue(&srun->rwqueue[qidx2], 0);
         err = remove_syncqueue(squeue, sfunc);
         SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
         if (err) goto ONERR;
         err = preallocate_syncqueue(&srun->rwqueue[qidx2]);
         if (err) goto ONERR;
      }

      err = free_queueiterator(&iter);
      if (err) goto ONERR;
   }

   if (runwakeup) {
      err = process_wakeup_list(srun);
      if (err) goto ONERR;
   }

ONUNPREPARE:
   srun->isrun = false;
   return err;

ONERR:
   free_queueiterator(&iter);
   TRACEEXIT_ERRLOG(err);
   goto ONUNPREPARE;
}

int terminate_syncrunner(syncrunner_t * srun)
{
   int err;
   syncfunc_param_t param = syncfunc_param_INIT(srun);
   queue_iterator_t iter = queue_iterator_FREE;
   syncqueue_t    * squeue;
   queue_t        * queue;
   syncfunc_t     * sfunc;
   int              isstate;
   uint16_t         size;

   if (srun->isrun) return EINPROGRESS;

   // prepare
   srun->isrun = true;

   for (int qidx = (int)lengthof(srun->rwqueue)-1; qidx >= 0; --qidx) {
      squeue = &srun->rwqueue[qidx];
      queue  = genericcast_queue(squeue);
      size   = elemsize_syncqueue(&srun->rwqueue[qidx]);

      void * prev;
      bool isPrev;
      err = initlast_queueiterator(&iter, queue, size);
      if (err) goto ONERR;
      isPrev = prev_queueiterator(&iter, &prev);
      while (isPrev) {
         sfunc = prev;
         isPrev = prev_queueiterator(&iter, &prev); // makes calling remove_syncqueue(squeue, sfunc) safe
         if (sfunc == nextfree_syncqueue(squeue)) continue;

         isstate = (sfunc->optfields & syncfunc_opt_STATE);

         param.contoffset = sfunc->contoffset;
         param.state      = isstate ? *addrstate_syncfunc(sfunc, size) : 0;
         (void) sfunc->mainfct(&param, synccmd_EXIT);
         if (isstate) *addrstate_syncfunc(sfunc, size) = 0;

         err = remove_syncqueue(squeue, sfunc);
         SETONERROR_testerrortimer(&s_syncrunner_errtimer, &err);
         if (err) goto ONERR;
      }

      err = free_queueiterator(&iter);
      if (err) goto ONERR;
   }

ONUNPREPARE:
   initself_synclinkd(&srun->wakeup);
   srun->isrun = false;
   return err;

ONERR:
   free_queueiterator(&iter);
   TRACEEXIT_ERRLOG(err);
   goto ONUNPREPARE;
}



// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int dummy_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   (void) sfparam;
   (void) sfcmd;
   return synccmd_EXIT;
}

static int test_memory(void)
{
   unsigned long src[100];
   unsigned long dest[100];
   synclink_t    caller    = synclink_FREE;
   synclink_t    condition = synclink_FREE;
   synclinkd_t   waitlist  = synclinkd_FREE;

   // TEST MOVE: check precondition
   for (unsigned i = 0; i < lengthof(s_syncrunner_rwqsize); ++i) {
      TEST(0 == s_syncrunner_rwqsize[i] % sizeof(long));
   }

   // TEST MOVE: copy memory
   for (unsigned size = 1; size <= lengthof(src); ++size) {
      for (unsigned offset = 0; offset + size <= lengthof(src); ++offset) {
         memset(src, 0, sizeof(src));
         memset(dest, 255, sizeof(dest));
         MOVE(&dest[offset], src, size*sizeof(long));
         for (unsigned i = 0; i < lengthof(src); ++i) {
            TEST(0 == src[i]);
         }
         for (unsigned i = 0; i < offset; ++i) {
            TEST(ULONG_MAX == dest[i]);
         }
         for (unsigned i = offset; i < offset+size; ++i) {
            TEST(0 == dest[i]);
         }
         for (unsigned i = offset+size; i < lengthof(dest); ++i) {
            TEST(ULONG_MAX == dest[i]);
         }
      }
   }

   // TEST move_syncfunc: Speicher wird verschoben und Links angepasst
   memset(src, 255, sizeof(src));
   memset(dest, 255, sizeof(dest));
   for (unsigned optfields = syncfunc_opt_NONE; optfields <= syncfunc_opt_ALL; ++optfields) {
      uint16_t size = getsize_syncfunc(optfields);
      for (unsigned soffset = 0; soffset <= 10; soffset += 10) {
         for (unsigned doffset = 0; doffset <= 20; doffset += 5) {
            syncfunc_t * src_func = (syncfunc_t*) &src[soffset];
            syncfunc_t * dst_func = (syncfunc_t*) &dest[doffset];
            void *       state    = (void*)(uintptr_t)(64 * soffset + doffset);
            init_syncfunc(src_func, &dummy_sf, optfields);
            if (optfields & syncfunc_opt_STATE) {
               *addrstate_syncfunc(src_func, size) = state;
            }
            if (optfields & syncfunc_opt_CALLER) {
               init_synclink(addrcaller_syncfunc(src_func, size, (optfields & syncfunc_opt_STATE)), &caller);
            }
            if (optfields & syncfunc_opt_WAITLIST) {
               init_synclinkd(addrwaitlist_syncfunc(src_func, (optfields & syncfunc_opt_WAITFOR)), &waitlist);
            }
            if (optfields & syncfunc_opt_WAITFOR) {
               init_synclink(addrwaitfor_syncfunc(src_func), &condition);
            }

            // move_syncfunc
            move_syncfunc(dst_func, src_func, size);

            // check src_func/dst_func valid & links adapted
            for (int isdst = 0; isdst <= 1; ++isdst) {
               syncfunc_t * sfunc = isdst ? dst_func : src_func;
               TEST(sfunc->mainfct    == &dummy_sf);
               TEST(sfunc->contoffset == 0);
               TEST(sfunc->optfields  == optfields);
               if (optfields & syncfunc_opt_STATE) {
                  TEST(state == *addrstate_syncfunc(sfunc, size));
               }
               if (optfields & syncfunc_opt_CALLER) {
                  TEST(&caller == addrcaller_syncfunc(sfunc, size, (optfields & syncfunc_opt_STATE))->link);
                  TEST(caller.link == addrcaller_syncfunc(dst_func, size, (optfields & syncfunc_opt_STATE))); // link --> dst_func
               }
               if (optfields & syncfunc_opt_WAITLIST) {
                  TEST(&waitlist == addrwaitlist_syncfunc(sfunc, (optfields & syncfunc_opt_WAITFOR))->prev);
                  TEST(&waitlist == addrwaitlist_syncfunc(sfunc, (optfields & syncfunc_opt_WAITFOR))->next);
                  TEST(waitlist.prev == addrwaitlist_syncfunc(dst_func, (optfields & syncfunc_opt_WAITFOR))); // prev --> dst_func
                  TEST(waitlist.next == addrwaitlist_syncfunc(dst_func, (optfields & syncfunc_opt_WAITFOR))); // next --> dst_func
               }
               if (optfields & syncfunc_opt_WAITFOR) {
                  TEST(&condition == addrwaitfor_syncfunc(sfunc)->link);
                  if (optfields & syncfunc_opt_WAITRESULT) {
                     TEST(condition.link == addrwaitfor_syncfunc(src_func)); // not adapted cause of result
                  } else {
                     TEST(condition.link == addrwaitfor_syncfunc(dst_func)); // link --> dst_func
                  }
               }
            }

            // reset memory
            memset(src_func, 255, size);
            memset(dst_func, 255, size);
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_initfree(void)
{
   syncrunner_t srun = syncrunner_FREE;

   // TEST syncrunner_FREE
   TEST(0 == srun.caller);
   TEST(0 == isvalid_synclinkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(isfree_syncqueue(&srun.rwqueue[i]));
   }

   // TEST init_syncrunner
   memset(&srun, 255, sizeof(srun));
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == srun.caller);
   TEST(srun.wakeup.prev == &srun.wakeup);
   TEST(srun.wakeup.next == &srun.wakeup);
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == isfree_syncqueue(&srun.rwqueue[i]));
      TEST(1 == size_syncqueue(&srun.rwqueue[i]));
      TEST(0 != nextfree_syncqueue(&srun.rwqueue[i]));
      TEST(&srun.rwqueue[i] == queuefromaddr_syncqueue(nextfree_syncqueue(&srun.rwqueue[i])));
      TEST(s_syncrunner_rwqsize[i] == elemsize_syncqueue(&srun.rwqueue[i]));
   }

   // TEST free_syncrunner: free queues
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(0 == isfree_syncqueue(&srun.rwqueue[i]));
   }
   TEST(0 == free_syncrunner(&srun));
   // only queues are freed
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(isfree_syncqueue(&srun.rwqueue[i]));
   }

   // TEST free_syncrunner: double free
   TEST(0 == free_syncrunner(&srun));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(isfree_syncqueue(&srun.rwqueue[i]));
   }

   // TEST free_syncrunner: ERROR
   for (unsigned tc = 1; tc < 7; ++tc) {
      TEST(0 == init_syncrunner(&srun));
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         TEST(0 == isfree_syncqueue(&srun.rwqueue[i]));
      }
      init_testerrortimer(&s_syncrunner_errtimer, tc, EINVAL);
      TEST(EINVAL == free_syncrunner(&srun));
      // only queues are freed
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         TEST(isfree_syncqueue(&srun.rwqueue[i]));
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_queuehelper(void)
{
   unsigned     maxidx;
   syncrunner_t srun = syncrunner_FREE;

   // prepare
   TEST(0 == init_syncrunner(&srun));

   // TEST find_run_queue: s_syncrunner_runqsize
   static_assert( WAITQUEUE_OFFSET < lengthof(s_syncrunner_rwqsize),
                  "index in range");
   maxidx = 0;
   for (unsigned optfields = syncfunc_opt_NONE; optfields <= syncfunc_opt_ALL; ++optfields) {
      if (  0 != (optfields & syncfunc_opt_WAITFOR)
            || 0 != (optfields & syncfunc_opt_WAITLIST)) {
         continue; // contains wait fields
      }

      const uint16_t funcsize = getsize_syncfunc(optfields);
      const unsigned qidx     = find_run_queue(optfields);

      TEST(qidx < WAITQUEUE_OFFSET);
      TEST(funcsize == s_syncrunner_rwqsize[qidx]);

      if (qidx > maxidx) maxidx = qidx;
   }
   TEST(maxidx == WAITQUEUE_OFFSET-1);

   // TEST find_wait_queue: s_syncrunner_waitqsize
   maxidx = 0;
   for (unsigned optfields = syncfunc_opt_NONE; optfields <= syncfunc_opt_ALL; ++optfields) {
      if (  0 == (optfields & syncfunc_opt_WAITFOR)
            || 0 == (optfields & syncfunc_opt_WAITLIST)) {
         continue; // no waitfor or no waitlist field
      }

      const uint16_t funcsize = getsize_syncfunc(optfields);
      const unsigned qidx     = find_wait_queue(optfields);

      TEST(qidx >= WAITQUEUE_OFFSET);
      TEST(qidx <  lengthof(s_syncrunner_rwqsize));
      TEST(funcsize == s_syncrunner_rwqsize[qidx]);

      if (qidx > maxidx) maxidx = qidx;
   }
   TEST(maxidx == lengthof(s_syncrunner_rwqsize)-1);

   for (unsigned optfields = syncfunc_opt_NONE; optfields <= syncfunc_opt_ALL; ++optfields) {
      if (  0 == (optfields & syncfunc_opt_WAITFOR)
            || 0 == (optfields & syncfunc_opt_WAITLIST)) {
         continue; // no waitfor or no waitlist field
      }

      synclink_t     waitfor = synclink_FREE;
      synclinkd_t    waitlist = synclinkd_FREE;
      synclink_t     caller  = synclink_FREE;
      const unsigned qidx    = find_wait_queue(optfields);
      syncqueue_t *  squeue  = &srun.rwqueue[qidx];
      uint16_t       size    = elemsize_syncqueue(squeue);
      void *         oldfree = nextfree_syncqueue(squeue);
      syncfunc_t *   sfunc   = nextfree_syncqueue(squeue);
      TEST(0 == preallocate_syncqueue(squeue));

      // TEST remove_syncqueue: remove element, last element is next free element
      memset(sfunc, 255, size);
      TEST(sfunc == last_queue(genericcast_queue(squeue), (uint16_t) (2*size)));
      TEST(nextfree_syncqueue(squeue) == last_queue(genericcast_queue(squeue), size));
      TEST(2 == size_syncqueue(squeue));
      TEST(0 == remove_syncqueue(squeue, sfunc));
      // check result
      TEST(1 == size_syncqueue(squeue));
      TEST(sfunc == nextfree_syncqueue(squeue)); // changed into sfunc
      TEST(sfunc == last_queue(genericcast_queue(squeue), size));
      for (uint8_t * data = (uint8_t*) sfunc; data != size + (uint8_t*)sfunc; ++data) {
         TEST(*data == 255); // content not changed
      }

      // TEST remove_syncqueue: remove element, last element is in use
      TEST(0 == preallocate_syncqueue(squeue));
      sfunc = nextfree_syncqueue(squeue);
      memset(sfunc, 255, size);
      TEST(0 == preallocate_syncqueue(squeue));
      syncfunc_t * last = nextfree_syncqueue(squeue);
      memset(last, 0, size);
      init_syncfunc(last, 0, optfields);
      init_synclink(&last->waitfor, &waitfor);
      init_synclinkd(&last->waitlist, &waitlist);
      init_synclink(&last->caller, &caller);
      setnextfree_syncqueue(squeue, oldfree);
      TEST(3 == size_syncqueue(squeue));
      TEST(0 == remove_syncqueue(squeue, sfunc));
      // check result
      TEST(2 == size_syncqueue(squeue));
      TEST(oldfree == nextfree_syncqueue(squeue)); // not changed
      TEST(sfunc == last_queue(genericcast_queue(squeue), size));
      TEST(sfunc->mainfct == 0);
      TEST(sfunc->contoffset == 0);
      TEST(sfunc->optfields  == optfields);
      TEST(sfunc->waitfor.link  == &waitfor);
      TEST(sfunc->waitlist.prev == &waitlist);
      TEST(sfunc->waitlist.next == &waitlist);
      if (optfields & syncfunc_opt_CALLER) {
         TEST(sfunc->caller.link == &caller);
         TEST(caller.link == &sfunc->caller);
      }

      // TEST remove_syncqueue: remove last element
      TEST(oldfree == nextfree_syncqueue(squeue));
      TEST(sfunc == last_queue(genericcast_queue(squeue), size));
      memset(sfunc, 255, size);
      TEST(2 == size_syncqueue(squeue));
      TEST(0 == remove_syncqueue(squeue, sfunc));
      // check result
      TEST(1 == size_syncqueue(squeue));
      TEST(oldfree == nextfree_syncqueue(squeue)); // not changed
      TEST(sfunc == (syncfunc_t*) (size + (uint8_t*)last_queue(genericcast_queue(squeue), size)));
      for (uint8_t * data = (uint8_t*) sfunc; data != size + (uint8_t*)sfunc; ++data) {
         TEST(*data == 255); // content not changed
      }

      // TEST remove_syncqueue: ERROR
      setnextfree_syncqueue(squeue, 0);
      TEST(0 == remove_syncqueue(squeue, oldfree));
      TEST(0 == size_syncqueue(squeue));
      TEST(ENODATA == remove_syncqueue(squeue, oldfree));
      TEST(0 == preallocate_syncqueue(squeue));
      TEST(1 == size_syncqueue(squeue));
   }

   for (unsigned optfields = syncfunc_opt_NONE; optfields <= syncfunc_opt_ALL; ++optfields) {
      if (  0 == (optfields & syncfunc_opt_WAITFOR)
            || 0 == (optfields & syncfunc_opt_WAITLIST)) {
         continue; // no waitfor or no waitlist field
      }

      // TEST wait_queue: OK
      const unsigned qidx   = find_wait_queue(optfields);
      syncqueue_t *  squeue = &srun.rwqueue[qidx];
      syncfunc_t  *  sfunc;
      for (unsigned i = 0; i < 100; ++i) {
         sfunc = nextfree_syncqueue(squeue);
         init_syncfunc(sfunc, 0, optfields);
         TEST(squeue == wait_queue(&srun, sfunc));
         TEST(0 == preallocate_syncqueue(squeue));
      }

      // TEST wait_queue: ERROR
      init_syncfunc(sfunc, 0, optfields ^ syncfunc_opt_CALLER);
      TEST(0 == wait_queue(&srun, sfunc));
      init_syncfunc(sfunc, 0, optfields ^ syncfunc_opt_STATE);
      TEST(0 == wait_queue(&srun, sfunc));
      syncfunc_t dummy;
      init_syncfunc(&dummy, 0, optfields);
      TEST(0 == wait_queue(&srun, &dummy));
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_query(void)
{
   syncrunner_t srun;

   // prepare
   TEST(0 == init_syncrunner(&srun));

   // TEST size_syncrunner: after init
   TEST(0 == size_syncrunner(&srun));

   // TEST size_syncrunner: size of single queue
   for (unsigned size = 1; size <= 4; ++size) {
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         for (unsigned s = 1; s <= size; ++s) {
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[i]));
            TEST(s == size_syncrunner(&srun));
         }
         TEST(0 == free_syncrunner(&srun));
         TEST(0 == init_syncrunner(&srun));
      }
   }

   // TEST size_syncrunner: size of all queues
   for (unsigned size = 1; size <= 4; ++size) {
      unsigned S = 1;
      for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
         for (unsigned s = 1; s <= size+i; ++s, ++S) {
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[i]));
            TEST(S == size_syncrunner(&srun));
         }
      }
      TEST(0 == free_syncrunner(&srun));
      TEST(0 == init_syncrunner(&srun));
      TEST(0 == size_syncrunner(&srun));
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   return EINVAL;
}

static int test_addfunc(void)
{
   syncrunner_t srun;
   queue_t *    queue;
   uint16_t     size;
   syncfunc_t * sfunc;

   static_assert(WAITQUEUE_OFFSET == 3, "code assumes 3 queues");
   // code assumes also: size_syncqueue == 1 after init (includes free entry)

   // prepare
   TEST(0 == init_syncrunner(&srun));

   // TEST addasync_syncrunner
   for (uintptr_t i = 1, s = 1; i; i <<= 1, ++s) {
      // state != 0
      // uses preallocated entry
      sfunc = nextfree_syncqueue(&srun.rwqueue[1]);
      size  = elemsize_syncqueue(&srun.rwqueue[1]);
      queue = genericcast_queue(&srun.rwqueue[1]);
      TEST(0 == addasync_syncrunner(&srun, &dummy_sf, (void*)i));
      TEST(2*s-1 == size_syncrunner(&srun));
      TEST(s+1 == size_syncqueue(&srun.rwqueue[1]));
      TEST(sfunc->mainfct    == &dummy_sf);
      TEST(sfunc->contoffset == 0);
      TEST(sfunc->optfields  == syncfunc_opt_STATE);
      TEST(*addrstate_syncfunc(sfunc, size) == (void*)i);
      TEST(nextfree_syncqueue(&srun.rwqueue[1]) == last_queue(queue, size));

      // state == 0
      sfunc = nextfree_syncqueue(&srun.rwqueue[0]);
      size  = elemsize_syncqueue(&srun.rwqueue[0]);
      queue = genericcast_queue(&srun.rwqueue[0]);
      TEST(0 == addasync_syncrunner(&srun, &dummy_sf, 0));
      TEST(2*s == size_syncrunner(&srun));
      TEST(s+1 == size_syncqueue(&srun.rwqueue[0]));
      TEST(sfunc->mainfct == &dummy_sf);
      TEST(sfunc->contoffset == 0);
      TEST(sfunc->optfields  == syncfunc_opt_NONE);
      TEST(nextfree_syncqueue(&srun.rwqueue[0]) == last_queue(queue, size));
   }

   // TEST addcall_syncrunner
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   for (uintptr_t i = 1, s = 1; i; i <<= 1, ++s) {
      srun.caller = 0;
      // state != 0
      sfunc = nextfree_syncqueue(&srun.rwqueue[2]);
      size  = elemsize_syncqueue(&srun.rwqueue[2]);
      queue = genericcast_queue(&srun.rwqueue[2]);
      TEST(0 == addcall_syncrunner(&srun, &dummy_sf, (void*)i));
      TEST(2*s-1 == size_syncrunner(&srun));
      TEST(s+1 == size_syncqueue(&srun.rwqueue[2]));
      TEST(sfunc->mainfct    == &dummy_sf);
      TEST(sfunc->contoffset == 0);
      TEST(sfunc->optfields  == (syncfunc_opt_CALLER|syncfunc_opt_STATE));
      TEST(0        == addrcaller_syncfunc(sfunc, size, true)->link);
      TEST((void*)i == *addrstate_syncfunc(sfunc, size));
      TEST(srun.caller == addrcaller_syncfunc(sfunc, size, true));
      TEST(nextfree_syncqueue(&srun.rwqueue[2]) == last_queue(queue, size));

      // state == 0
      sfunc = nextfree_syncqueue(&srun.rwqueue[1]);
      size  = elemsize_syncqueue(&srun.rwqueue[1]);
      queue = genericcast_queue(&srun.rwqueue[1]);
      TEST(0 == addcall_syncrunner(&srun, &dummy_sf, 0));
      TEST(2*s == size_syncrunner(&srun));
      TEST(s+1 == size_syncqueue(&srun.rwqueue[1]));
      TEST(sfunc->mainfct == &dummy_sf);
      TEST(sfunc->contoffset == 0);
      TEST(sfunc->optfields  == syncfunc_opt_CALLER);
      TEST(0     == addrcaller_syncfunc(sfunc, size, false)->link);
      TEST(srun.caller == addrcaller_syncfunc(sfunc, size, false));
      TEST(nextfree_syncqueue(&srun.rwqueue[1]) == last_queue(queue, size));
   }

   // TEST addasync_syncrunner: ERROR
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   for (unsigned i = 0; i < WAITQUEUE_OFFSET; ++i) {
      memset(nextfree_syncqueue(&srun.rwqueue[i]), 0, elemsize_syncqueue(&srun.rwqueue[i]));
   }
   init_testerrortimer(&s_syncrunner_errtimer, 1, ENOMEM);
   TEST(ENOMEM == addasync_syncrunner(&srun, &dummy_sf, (void*)1));
   init_testerrortimer(&s_syncrunner_errtimer, 1, ENOMEM);
   TEST(ENOMEM == addasync_syncrunner(&srun, &dummy_sf, (void*)0));
   for (unsigned i = 0; i < WAITQUEUE_OFFSET; ++i) {
      TEST(1 == size_syncqueue(&srun.rwqueue[i]));
      for (unsigned b = 0; b < elemsize_syncqueue(&srun.rwqueue[i]); ++b) {
         TEST(0 == ((uint8_t*)nextfree_syncqueue(&srun.rwqueue[i]))[b]);
      }
   }

   // TEST addcall_syncrunner: ERROR
   init_testerrortimer(&s_syncrunner_errtimer, 1, ENOMEM);
   TEST(ENOMEM == addcall_syncrunner(&srun, &dummy_sf, (void*)1));
   init_testerrortimer(&s_syncrunner_errtimer, 1, ENOMEM);
   TEST(ENOMEM == addcall_syncrunner(&srun, &dummy_sf, (void*)0));
   for (unsigned i = 0; i < WAITQUEUE_OFFSET; ++i) {
      TEST(1 == size_syncqueue(&srun.rwqueue[i]));
      for (unsigned b = 0; b < elemsize_syncqueue(&srun.rwqueue[i]); ++b) {
         TEST(0 == ((uint8_t*)nextfree_syncqueue(&srun.rwqueue[i]))[b]);
      }
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   return EINVAL;
}

static int test_wakeup(void)
{
   syncrunner_t srun;
   syncfunc_t   sfunc[10];
   syncfunc_t * qfunc[5*4];
   synccond_t   cond = synccond_FREE;

   // prepare
   TEST(0 == init_synccond(&cond));
   TEST(0 == init_syncrunner(&srun));
   TEST(isself_synclinkd(&srun.wakeup));

   // TEST link_to_wakeup
   for (unsigned i = 0; i < lengthof(sfunc); ++i) {
      link_to_wakeup(&srun, &sfunc[i].waitlist);
      TEST(sfunc[i].waitlist.prev == (i ? &sfunc[i-1].waitlist : &srun.wakeup))
      TEST(sfunc[i].waitlist.next == &srun.wakeup);
   }

   // TEST linkall_to_wakeup
   initself_synclinkd(&srun.wakeup);
   init_synclinkd(&sfunc[0].waitlist, &sfunc[1].waitlist);
   for (unsigned i = 2; i < lengthof(sfunc); ++i) {
      initnext_synclinkd(&sfunc[i].waitlist, &sfunc[i-1].waitlist);
   }
   linkall_to_wakeup(&srun, &sfunc[0].waitlist);
   for (unsigned i = 0; i < lengthof(sfunc); ++i) {
      TEST(sfunc[i].waitlist.prev == (i ? &sfunc[i-1].waitlist : &srun.wakeup))
      TEST(sfunc[i].waitlist.next == (i < lengthof(sfunc)-1? &sfunc[i+1].waitlist : &srun.wakeup))
   }

   // TEST wakeup_caller
   for (int isstate = 0; isstate <= 1; ++isstate) {
      const uint16_t size = (uint16_t) (isstate ? sizeof(sfunc[0]) : offsetof(syncfunc_t, state));
      for (int retcode = -4; retcode <= 4; ++retcode) {
         static_assert(9 < lengthof(sfunc), "make sure index si is in bounds");
         int si  = 5+retcode;
         int opt = (retcode == -5 ? syncfunc_opt_STATE : retcode == -4 ? syncfunc_opt_WAITFOR : 0);

         // wakeup_caller: (optfields & syncfunc_opt_CALLER) != 0 && caller.link != 0
         memset(&sfunc[si], 0, sizeof(sfunc[si]));
         // sfunc[si].optfields not evaluated
         sfunc[si].optfields = (uint8_t) opt;
         init_synclink(&sfunc[0].caller, &sfunc[si].waitfor);
         initself_synclinkd(&srun.wakeup);
         sfunc[0].optfields = syncfunc_opt_CALLER;
         wakeup_caller(&srun, sfunc, size, isstate, retcode);
         // sfunc[0] cleared
         TEST(!isvalid_synclink(&sfunc[0].caller));
         // sfunc[si] added to wakeup
         TEST(sfunc[si].optfields  == (opt | syncfunc_opt_WAITRESULT));
         TEST(sfunc[si].waitresult == retcode);
         TEST(sfunc[si].waitlist.prev == &srun.wakeup);
         TEST(sfunc[si].waitlist.next == &srun.wakeup);

         // wakeup_caller: (optfields & syncfunc_opt_CALLER) != 0 && caller.link == 0
         memset(&sfunc[si], 0, sizeof(sfunc[si]));
         memset(&sfunc[0], 0, sizeof(sfunc[0]));
         sfunc[si].optfields = (uint8_t) opt;
         initself_synclinkd(&srun.wakeup);
         sfunc[0].optfields = syncfunc_opt_CALLER;
         wakeup_caller(&srun, sfunc, size, isstate, retcode);
         // nothing added to wakeup
         TEST(sfunc[si].optfields  == opt);
         TEST(sfunc[si].waitresult == 0);
         TEST(sfunc[si].waitlist.prev == 0);
         TEST(sfunc[si].waitlist.next == 0);

         // wakeup_caller: (optfields & syncfunc_opt_CALLER) == 0 && caller.link != 0
         memset(&sfunc[si], 0, sizeof(sfunc[si]));
         sfunc[si].optfields = (uint8_t) opt;
         init_synclink(&sfunc[0].caller, &sfunc[si].waitfor);
         initself_synclinkd(&srun.wakeup);
         sfunc[0].optfields = 0;
         wakeup_caller(&srun, sfunc, size, isstate, retcode);
         TEST(isvalid_synclink(&sfunc[0].caller));
         // nothing added to wakeup
         TEST(sfunc[si].optfields  == opt);
         TEST(sfunc[si].waitfor.link  == &sfunc[0].caller);
         TEST(sfunc[si].waitlist.prev == 0);
         TEST(sfunc[si].waitlist.next == 0);
      }
   }

   // prepare
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   for (int isstate = 0, i = 0; isstate <= 1; ++isstate) {
      for (int iscaller = 0; iscaller <= 1; ++iscaller) {
         syncfunc_opt_e optfields = syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST
                                  | (isstate ? syncfunc_opt_STATE : 0) | (iscaller ? syncfunc_opt_CALLER : 0);
         unsigned qidx = find_wait_queue(optfields);
         for (unsigned i2 = 0; i2 < 5; ++i2, ++i) {
            TEST(i < (int)lengthof(qfunc));
            qfunc[i] = nextfree_syncqueue(&srun.rwqueue[qidx]);
            TEST(0 == preallocate_syncqueue(&srun.rwqueue[qidx]));
            memset(qfunc[i], 0, elemsize_syncqueue(&srun.rwqueue[qidx]));
            qfunc[i]->optfields = optfields;
         }
      }
      TEST(!isstate || i == (int)lengthof(qfunc));
   }

   // TEST wakeup_syncrunner: waitlist not empty / waitlist empty
   // build waiting list on cond
   link_synccond(&cond, qfunc[0]);
   initself_synclinkd(addrwaitlist_syncfunc(qfunc[0], true));
   for (unsigned i = 1; i < lengthof(qfunc); ++i) {
      initprev_synclinkd(addrwaitlist_syncfunc(qfunc[i], true), addrwaitlist_syncfunc(qfunc[0], true));
   }
   for (unsigned i = 0; i < lengthof(qfunc); ++i) {
      if (i == lengthof(qfunc)-1) { // test empty waitlist
         TEST(! isvalid_synclinkd(addrwaitlist_syncfunc(qfunc[i], true)));
      }
      // test
      TEST(0 == wakeup_syncrunner(&srun, &cond));
      // added to wakeup
      synclinkd_t * const prev = i > 0 ? addrwaitlist_syncfunc(qfunc[i-1], true) : &srun.wakeup;
      synclinkd_t * const next = &srun.wakeup;
      TEST(prev == addrwaitlist_syncfunc(qfunc[i], true)->prev);
      TEST(next == addrwaitlist_syncfunc(qfunc[i], true)->next);
      // waitfor cleared
      TEST(! isvalid_synclink(addrwaitfor_syncfunc(qfunc[i])));
      // no wait result !
      TEST(0 == (qfunc[i]->optfields & syncfunc_opt_WAITRESULT));
      if (i != lengthof(qfunc)-1) {    // not empty waitlist
         // cond points to next in waiting list
         TEST( iswaiting_synccond(&cond));
         TEST(qfunc[i+1] == waitfunc_synccond(&cond));
      } else {                         // empty waitlist
         TEST(0 == iswaiting_synccond(&cond));
      }
   }

   // TEST wakeup_syncrunner: empty condition ==> call ignored
   initself_synclinkd(&srun.wakeup);
   TEST(0 == iswaiting_synccond(&cond));
   TEST(0 == wakeup_syncrunner(&srun, &cond));
   TEST(0 != isself_synclinkd(&srun.wakeup));
   TEST(0 == iswaiting_synccond(&cond));

   // TEST wakeup_syncrunner: ERROR
   link_synccond(&cond, &sfunc[0]);
   TEST(EINVAL == wakeup_syncrunner(&srun, &cond));
   TEST(0 != isself_synclinkd(&srun.wakeup));
   TEST(&sfunc[0] == waitfunc_synccond(&cond));

   // TEST wakeupall_syncrunner
   // build waiting list on cond
   link_synccond(&cond, qfunc[0]);
   initself_synclinkd(addrwaitlist_syncfunc(qfunc[0], true));
   for (unsigned i = 1; i < lengthof(qfunc); ++i) {
      initprev_synclinkd(addrwaitlist_syncfunc(qfunc[i], true), addrwaitlist_syncfunc(qfunc[0], true));
   }
   // test
   TEST(0 == wakeupall_syncrunner(&srun, &cond));
   // cond clear
   TEST(0 == iswaiting_synccond(&cond));
   for (unsigned i = 0; i < lengthof(qfunc); ++i) {
      // added to wakeup
      synclinkd_t * const prev = i > 0 ? addrwaitlist_syncfunc(qfunc[i-1], true) : &srun.wakeup;
      synclinkd_t * const next = i < lengthof(qfunc)-1 ? addrwaitlist_syncfunc(qfunc[i+1], true) : &srun.wakeup;
      TEST(prev == addrwaitlist_syncfunc(qfunc[i], true)->prev);
      TEST(next == addrwaitlist_syncfunc(qfunc[i], true)->next);
      // waitfor cleared
      TEST(! isvalid_synclink(addrwaitfor_syncfunc(qfunc[i])));
      // no wait result !
      TEST(0 == (qfunc[i]->optfields & syncfunc_opt_WAITRESULT));
   }

   // TEST wakeupall_syncrunner: empty condition ==> call ignored
   initself_synclinkd(&srun.wakeup);
   unlink_synccond(&cond);
   TEST(0 == iswaiting_synccond(&cond));
   TEST(0 == wakeupall_syncrunner(&srun, &cond));
   TEST(0 != isself_synclinkd(&srun.wakeup));
   TEST(0 == iswaiting_synccond(&cond));

   // TEST wakeupall_syncrunner: ERROR
   link_synccond(&cond, &sfunc[0]);
   TEST(EINVAL == wakeupall_syncrunner(&srun, &cond));
   TEST(0 != isself_synclinkd(&srun.wakeup));
   TEST(&sfunc[0] == waitfunc_synccond(&cond));

   // unprepare
   TEST(0 == free_synccond(&cond));
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_synccond(&cond);
   free_syncrunner(&srun);
   return EINVAL;
}

// == in params ==
static syncrunner_t     * s_test_srun   = 0;
static int                s_test_return = synccmd_RUN;
static uint16_t           s_test_set_contoffset = 0;
static int                s_test_set_retcode = 0;
static void *             s_test_set_state   = 0;
static synccond_t *       s_test_set_condition = 0;
static synclink_t *       s_test_set_called  = 0;
static int                s_test_expect_condition = 0;
static uint16_t           s_test_expect_contoffset = 0;
static void *             s_test_expect_state = 0;
static uint32_t           s_test_expect_cmd = 0;
static int                s_test_expect_waitresult = 0;
// == out params ==
static size_t             s_test_runcount = 0;
static size_t             s_test_errcount = 0;
static syncfunc_param_t * s_test_param  = 0;
static uint32_t           s_test_cmd    = 0;

static int test_call_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   assert(s_test_srun == sfparam->syncrun);
   s_test_cmd   = sfcmd;
   s_test_param = sfparam;
   return s_test_return;
}

static int test_exec_helper(void)
{
   syncrunner_t      srun;
   syncfunc_t        sfunc;
   syncfunc_param_t  param = syncfunc_param_INIT(&srun);

   // prepare
   s_test_srun = &srun;
   TEST(0 == init_syncrunner(&srun));

   // TEST CALL_SYNCFUNC
   for (int retcode = -2; retcode <= 2; ++retcode) {
      for (int isstate = 0; isstate <= 1; ++isstate) {
         for (uint16_t contoffset = 0; contoffset <= 3; ++contoffset) {
            syncfunc_opt_e optfields = isstate ? syncfunc_opt_STATE : 0;
            uint16_t       size      = getsize_syncfunc(optfields);
            init_syncfunc(&sfunc, &test_call_sf, optfields);
            sfunc.contoffset = contoffset;
            if (optfields & syncfunc_opt_STATE) {
               *addrstate_syncfunc(&sfunc, size) = (void*)2;
            }
            srun.caller = (void*)1;
            param.state = (void*)1;
            param.contoffset = (uint16_t)-1;
            s_test_cmd   = (uint32_t) -1;
            s_test_param = 0;
            s_test_return = retcode;
            TEST(retcode == CALL_SYNCFUNC(&srun, &sfunc, size, (optfields & syncfunc_opt_STATE), param));
            // test srun
            TEST(srun.caller == 0);
            // test param
            TEST(param.state      == (isstate ? (void*)2 : (void*)0));
            TEST(param.contoffset == contoffset);
            // test function parameter
            TEST(s_test_cmd   == (contoffset ? synccmd_CONTINUE : synccmd_RUN));
            TEST(s_test_param == &param);
         }
      }
   }

   // TEST link_waitfields
   // TODO: link_waitfields

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_wakeup_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   assert(s_test_srun == sfparam->syncrun);

   s_test_runcount += 1;
   s_test_errcount += (sfcmd != s_test_expect_cmd);
   s_test_errcount += (sfparam->state != s_test_expect_state);
   s_test_errcount += (sfparam->contoffset != s_test_expect_contoffset);

   // only valid wakeup from wait operation
   // but called from process_wakeup_list which implements wakeup from wait operation
   if (s_test_expect_condition) {
      s_test_errcount += (sfparam->waiterr != s_test_expect_waitresult);
   } else {
      s_test_errcount += (sfparam->waiterr != 0);
      s_test_errcount += (sfparam->retcode != s_test_expect_waitresult);
   }

   sfparam->contoffset = s_test_set_contoffset;
   sfparam->state = s_test_set_state;

   if (s_test_return == synccmd_WAIT) {
      sfparam->condition = s_test_set_condition;
      sfparam->syncrun->caller = s_test_set_called; // fake call of another function

   } else if (s_test_return == synccmd_EXIT) {
      sfparam->retcode = s_test_set_retcode;
   }

   return s_test_return;
}

static int test_exec_wakeup(void)
{
   syncrunner_t   srun;
   syncfunc_t   * sfunc[10];
   syncfunc_opt_e optfields;
   unsigned       qidx;
   uint16_t       size;
   void *         oldfree;
   syncqueue_t *  squeue;
   synclink_t     dummy_caller;
   synclink_t     dummy_called;
   syncfunc_t     dummy_func = { .optfields = syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST|syncfunc_opt_STATE|syncfunc_opt_CALLER };
   synccond_t     scond;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == init_synccond(&scond));

   // TEST process_wakeup_list: empty list
   TEST(0 == process_wakeup_list(&srun));
   TEST(isself_synclinkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(1 == size_syncqueue(&srun.rwqueue[i]));
   }

   // TEST process_wakeup_list: synccmd_EXIT && "all possible wakeup parameter"
   s_test_return = synccmd_EXIT;
   for (int isstate = 0; isstate <= 1; ++isstate) {
      s_test_expect_state = isstate ? (void*)(uintptr_t)0x123 : 0;
      for (int iscaller = 0; iscaller <= 1; ++iscaller) {
         optfields = syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST
                   | (isstate ? syncfunc_opt_STATE : 0) | (iscaller ? syncfunc_opt_CALLER : 0);
         qidx = find_wait_queue(optfields);
         size = elemsize_syncqueue(&srun.rwqueue[qidx]);
         oldfree = nextfree_syncqueue(&srun.rwqueue[qidx]);
         squeue  = &srun.rwqueue[qidx];
         for (int isresult = 0; isresult <= 1; isresult += 1) {
            for (int waitresult = 0; waitresult <= isresult*256; waitresult += 256) {
               s_test_expect_waitresult = waitresult;
               for (int iscondition = 1-isresult; iscondition <= 1; ++iscondition) {
                  s_test_expect_condition = iscondition;
                  for (int contoffset = 0; contoffset <= 1; ++contoffset) {
                     s_test_expect_contoffset = (uint16_t) contoffset;
                     s_test_expect_cmd = contoffset ? synccmd_CONTINUE : synccmd_RUN;
                     // squeue: free-entry, sfunc[0],  ..., sfunc[9]
                     // waitlist: sfunc[0] - ...  - sfunc[9] - srun.wakeup
                     for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                        TEST(0 == preallocate_syncqueue(squeue));
                        sfunc[i] = nextfree_syncqueue(squeue);
                        memset(sfunc[i], 0, size);
                        init_syncfunc(sfunc[i], &test_wakeup_sf, (uint8_t) (
                                             (optfields ^ syncfunc_opt_WAITFOR)
                                             | (isresult ? syncfunc_opt_WAITRESULT : 0)
                                             | (iscondition ? syncfunc_opt_WAITFOR_CONDITION : syncfunc_opt_WAITFOR_CALLED)));
                        sfunc[i]->contoffset = (uint16_t) contoffset;
                        if (isstate)  *addrstate_syncfunc(sfunc[i], size) = s_test_expect_state;
                        if (isresult) *addrwaitresult_syncfunc(sfunc[i]) = s_test_expect_waitresult;
                        initprev_synclinkd(addrwaitlist_syncfunc(sfunc[i], true), &srun.wakeup);
                     }
                     setnextfree_syncqueue(squeue, oldfree);

                     // test process_wakeup_list
                     s_test_runcount = 0;
                     TEST(0 == process_wakeup_list(&srun));
                     // check result
                     TEST(0 == s_test_errcount);
                     TEST(lengthof(sfunc) == s_test_runcount);
                     TEST(1 == size_syncqueue(squeue));
                     TEST(oldfree == nextfree_syncqueue(squeue));
                     TEST(isself_synclinkd(&srun.wakeup));
                  }
               }
            }
         }
      }
   }

   // TEST process_wakeup_list: synccmd_EXIT && "wakeup waiting caller"
   s_test_return = synccmd_EXIT;
   s_test_expect_condition  = 1;
   s_test_expect_waitresult = 0;
   s_test_expect_state = 0 ;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   for (int retcode = 0; retcode <= 10; retcode += 5) {
      s_test_set_retcode = retcode;
      for (int isstate = 0; isstate <= 1; ++isstate) {
         optfields = syncfunc_opt_WAITFOR_CALLED|syncfunc_opt_WAITLIST
                   | (isstate ? syncfunc_opt_STATE : 0) | syncfunc_opt_CALLER;
         qidx = find_wait_queue(optfields);
         size = elemsize_syncqueue(&srun.rwqueue[qidx]);
         oldfree = nextfree_syncqueue(&srun.rwqueue[qidx]);
         squeue  = &srun.rwqueue[qidx];
         // squeue: free-entry, sfunc[0],  ..., sfunc[9]
         // waitlist: sfunc[0] - ...  - sfunc[9] - srun.wakeup
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == preallocate_syncqueue(squeue));
            sfunc[i] = nextfree_syncqueue(squeue);
            memset(sfunc[i], 0, size);
            init_syncfunc(sfunc[i], &test_wakeup_sf, optfields);
            initprev_synclinkd(addrwaitlist_syncfunc(sfunc[i], true), &srun.wakeup);
         }
         // squeue: free-entry, sfunc[0],  ..., sfunc[9], caller[0], ..., caller[9]
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(0 == preallocate_syncqueue(squeue));
            syncfunc_t * caller = nextfree_syncqueue(squeue);
            memset(caller, 0, size);
            init_syncfunc(caller, &test_wakeup_sf, optfields);
            init_synclink(addrwaitfor_syncfunc(caller), addrcaller_syncfunc(sfunc[i], size, isstate));
            init_synclink(addrcaller_syncfunc(caller, size, isstate), &dummy_caller);
         }
         setnextfree_syncqueue(squeue, oldfree);

         // test process_wakeup_list
         s_test_runcount = 0;
         TEST(0 == process_wakeup_list(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(lengthof(sfunc) == s_test_runcount);
         TEST(lengthof(sfunc)+1 == size_syncqueue(squeue));
         // during remove operation last entries are moved to first entries ==> caller[9] -> sfunc[0], ...
         TEST(oldfree == nextfree_syncqueue(squeue));
         // check caller added to wakeup
         TEST(srun.wakeup.next == &sfunc[9]->waitlist);
         TEST(&srun.wakeup     == sfunc[9]->waitlist.prev);
         TEST(srun.wakeup.prev == &sfunc[0]->waitlist);
         TEST(&srun.wakeup     == sfunc[0]->waitlist.next);
         for (unsigned i = 0; i < lengthof(sfunc)-1; ++i) {
            TEST(sfunc[i]->waitlist.prev == &sfunc[i+1]->waitlist);
            TEST(&sfunc[i]->waitlist     == sfunc[i+1]->waitlist.next);
         }
         // check content
         optfields |= syncfunc_opt_WAITRESULT;
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            TEST(sfunc[i]->mainfct    == &test_wakeup_sf);
            TEST(sfunc[i]->contoffset == 0);
            TEST(sfunc[i]->optfields  == optfields);
            TEST(&dummy_caller == addrcaller_syncfunc(sfunc[i], size, isstate)->link);
            if (isstate) {
               TEST(0 == *addrstate_syncfunc(sfunc[i], size));
            }
            TEST(retcode == *addrwaitresult_syncfunc(sfunc[i]));
         }
         // prepare for next run
         TEST(0 == free_syncqueue(squeue));
         TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
         initself_synclinkd(&srun.wakeup);
      }
   }

   // TEST process_wakeup_list: synccmd_RUN && synccmd_CONTINUE
   for (int retcmd = 0; retcmd <= 1; ++retcmd) {
      s_test_return = retcmd ? synccmd_CONTINUE : synccmd_RUN;
      s_test_expect_condition = 1;
      s_test_expect_state = 0;
      s_test_expect_waitresult = 0;
      s_test_expect_cmd = synccmd_RUN;
      s_test_expect_contoffset = 0;
      s_test_set_contoffset = 100;
      for (int isstate = 0; isstate <= 1; ++isstate) {
         for (int setstate = 0; setstate <= 1; ++setstate) {
            s_test_set_state = setstate ? &optfields : 0;
            for (int iscaller = 0; iscaller <= 1; ++iscaller) {
               optfields = syncfunc_opt_WAITFOR | syncfunc_opt_WAITLIST
                         | (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);;
               qidx = find_wait_queue(optfields);
               size = elemsize_syncqueue(&srun.rwqueue[qidx]);
               oldfree = nextfree_syncqueue(&srun.rwqueue[qidx]);
               squeue  = &srun.rwqueue[qidx];
               // squeue: free-entry, sfunc[0],  ..., sfunc[9]
               // waitlist: srun.wakeup - sfunc[0] - ...  - sfunc[9]
               for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                  TEST(0 == preallocate_syncqueue(squeue));
                  sfunc[i] = nextfree_syncqueue(squeue);
                  memset(sfunc[i], 0, size);
                  init_syncfunc(sfunc[i], &test_wakeup_sf, optfields);
                  if (iscaller) init_synclink(addrcaller_syncfunc(sfunc[i], size, isstate), &dummy_caller);
                  initprev_synclinkd(addrwaitlist_syncfunc(sfunc[i], true), &srun.wakeup);
               }
               setnextfree_syncqueue(squeue, oldfree);

               // test process_wakeup_list
               s_test_runcount = 0;
               TEST(0 == process_wakeup_list(&srun));
               // check result
               TEST(0 == s_test_errcount);
               TEST(lengthof(sfunc) == s_test_runcount);
               TEST(1 == size_syncqueue(squeue));
               TEST(oldfree == nextfree_syncqueue(squeue));
               TEST(isself_synclinkd(&srun.wakeup));
               // check length of queues
               optfields = (iscaller ? syncfunc_opt_CALLER : 0) | (setstate ? syncfunc_opt_STATE : 0);
               qidx   = find_run_queue(optfields);
               squeue = &srun.rwqueue[qidx];
               size   = elemsize_syncqueue(squeue);
               for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
                  TEST((i == qidx ? 1+lengthof(sfunc) : 1) == size_syncqueue(&srun.rwqueue[i]));
               }
               // check content of run queue
               unsigned i = 0;
               foreach (_queue, next, genericcast_queue(squeue), size) {
                  if (next == nextfree_syncqueue(squeue)) continue;
                  syncfunc_t * sf = next;
                  TEST(sf->mainfct    == &test_wakeup_sf);
                  TEST(sf->contoffset == (retcmd ? s_test_set_contoffset/*continue*/ : 0/*run*/));
                  TEST(sf->optfields  == optfields);
                  if (iscaller) {
                     TEST(&dummy_caller == addrcaller_syncfunc(sf, size, setstate)->link);
                  }
                  if (setstate) {
                     TEST(s_test_set_state == *addrstate_syncfunc(sf, size));
                  }
                  ++ i;
               }
               TEST(i == lengthof(sfunc));
               // prepare for next run
               TEST(0 == free_syncqueue(squeue));
               TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
               initself_synclinkd(&srun.wakeup);
            }
         }
      }
   }

   // TEST process_wakeup_list: synccmd_WAIT
   s_test_return = synccmd_WAIT;
   s_test_expect_condition = 1;
   s_test_expect_state = 0;
   s_test_expect_waitresult = 0;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   for (int condition = 0; condition <= 1; ++condition) {
      s_test_set_condition = condition ? &scond : 0;
      s_test_set_called    = &dummy_called;
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_set_contoffset = (uint16_t) contoffset;
         for (int isstate = 0; isstate <= 1; ++isstate) {
            for (int setstate = 0; setstate <= 1; ++setstate) {
               s_test_set_state = setstate ? &optfields : 0;
               for (int iscaller = 0; iscaller <= 1; ++iscaller) {
                  int isstate2 = isstate || setstate;
                  optfields = (condition ? syncfunc_opt_WAITFOR_CALLED : syncfunc_opt_WAITFOR_CONDITION)
                            | syncfunc_opt_WAITLIST | (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
                  /* syncfunc_opt_WAITFOR_? is reversed to test for correct adaption of bits */
                  qidx = find_wait_queue(optfields);
                  size = elemsize_syncqueue(&srun.rwqueue[qidx]);
                  oldfree = nextfree_syncqueue(&srun.rwqueue[qidx]);
                  squeue  = &srun.rwqueue[qidx];
                  // squeue: free-entry, sfunc[0],  ..., sfunc[9]
                  // waitlist: srun.wakeup - sfunc[0] - ...  - sfunc[9]
                  for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                     TEST(0 == preallocate_syncqueue(squeue));
                     sfunc[i] = nextfree_syncqueue(squeue);
                     memset(sfunc[i], 0, size);
                     init_syncfunc(sfunc[i], &test_wakeup_sf, optfields);
                     if (iscaller) init_synclink(addrcaller_syncfunc(sfunc[i], size, isstate), &dummy_caller);
                     initprev_synclinkd(addrwaitlist_syncfunc(sfunc[i], true), &srun.wakeup);
                  }
                  setnextfree_syncqueue(squeue, oldfree);

                  // test process_wakeup_list
                  s_test_runcount = 0;
                  TEST(0 == process_wakeup_list(&srun));
                  // check result
                  TEST(0 == s_test_errcount);
                  TEST(lengthof(sfunc) == s_test_runcount);
                  TEST(isself_synclinkd(&srun.wakeup));
                  // check length of queues
                  unsigned qidx2 = find_wait_queue(optfields|(isstate2 ? syncfunc_opt_STATE : 0));
                  if (isstate || !setstate) {
                     TEST(qidx2 == qidx);
                  } else {
                     TEST(qidx2 != qidx);
                  }
                  squeue = &srun.rwqueue[qidx2];
                  size   = elemsize_syncqueue(squeue);
                  for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
                     TEST((i == qidx2 ? 1+lengthof(sfunc) : 1) == size_syncqueue(&srun.rwqueue[i]));
                  }
                  // check content of wait queue
                  unsigned i = 0;
                  foreach (_queue, elem, genericcast_queue(squeue), size) {
                     if (elem != nextfree_syncqueue(squeue)) {
                        syncfunc_t * sf = elem;
                        TEST(sf->mainfct    == &test_wakeup_sf);
                        TEST(sf->contoffset == s_test_set_contoffset);
                        TEST(sf->optfields  == ((condition ? syncfunc_opt_WAITFOR_CONDITION : syncfunc_opt_WAITFOR_CALLED)
                                                | syncfunc_opt_WAITLIST
                                                | ((iscaller ? syncfunc_opt_CALLER : 0)|(isstate2 ? syncfunc_opt_STATE : 0))));
                        if (! condition) {
                           TEST(&dummy_called == addrwaitfor_syncfunc(sf)->link);
                           TEST(! isvalid_synclinkd(addrwaitlist_syncfunc(sf, true)));
                        }
                        if (iscaller) {
                           TEST(&dummy_caller == addrcaller_syncfunc(sf, size, isstate2)->link);
                        }
                        if (sf->optfields & syncfunc_opt_STATE) {
                           TEST(s_test_set_state == *addrstate_syncfunc(sf, size));
                        }
                        if (qidx2 == qidx) {
                           TEST(sfunc[i] == sf);
                        }
                        ++i;
                     }
                  }
                  TEST(i == lengthof(sfunc));
                  // check condition waitlist
                  if (condition) {
                     TEST(iswaiting_synccond(&scond));
                     syncfunc_t * sf = waitfunc_synccond(&scond);
                     TEST(squeue == queuefromaddr_syncqueue(sf));
                     TEST(&scond.waitfunc == addrwaitfor_syncfunc(sf)->link);
                     TEST(isvalid_synclinkd(addrwaitlist_syncfunc(sf, true)));
                     for (i = 1; i < lengthof(sfunc); ++i) {
                        TEST(addrwaitlist_syncfunc(sf, true) == addrwaitlist_syncfunc(sf, true)->next->prev);
                        sf = waitlistcast_syncfunc(addrwaitlist_syncfunc(sf, true)->next, true);
                        TEST(!isvalid_synclink(addrwaitfor_syncfunc(sf)));
                        TEST(isvalid_synclinkd(addrwaitlist_syncfunc(sf, true)));
                     }
                     TEST(addrwaitlist_syncfunc(sf, true) == addrwaitlist_syncfunc(sf, true)->next->prev);
                     sf = waitlistcast_syncfunc(addrwaitlist_syncfunc(sf, true)->next, true);
                     TEST(sf == waitfunc_synccond(&scond));
                  }
                  // prepare for next run
                  unlink_synccond(&scond);
                  TEST(0 == free_syncqueue(squeue));
                  TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx2));
               }
            }
         }
      }
   }

   // TEST process_wakeup_list: wait error ==> add function to waitlist with wait result == EINVAL
   s_test_return = synccmd_WAIT;
   s_test_expect_condition = 1;
   s_test_expect_state = 0;
   s_test_expect_waitresult = 0;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   link_synccond(&scond, &dummy_func); // error
   for (int condition = 0; condition <= 1; ++condition) {
      s_test_set_condition = condition ? &scond : 0;
      s_test_set_called    = 0; // error
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_set_contoffset = (uint16_t) contoffset;
         for (int isstate = 0; isstate <= 1; ++isstate) {
            for (int setstate = 0; setstate <= 1; ++setstate) {
               s_test_set_state = setstate ? &optfields : 0;
               for (int iscaller = 0; iscaller <= 1; ++iscaller) {
                  int isstate2 = isstate || setstate;
                  optfields = syncfunc_opt_WAITFOR | syncfunc_opt_WAITLIST
                            | (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
                  qidx = find_wait_queue(optfields);
                  size = elemsize_syncqueue(&srun.rwqueue[qidx]);
                  oldfree = nextfree_syncqueue(&srun.rwqueue[qidx]);
                  squeue  = &srun.rwqueue[qidx];
                  // squeue: free-entry, sfunc[0],  ..., sfunc[9]
                  // waitlist: srun.wakeup - sfunc[0] - ...  - sfunc[9]
                  for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                     TEST(0 == preallocate_syncqueue(squeue));
                     sfunc[i] = nextfree_syncqueue(squeue);
                     memset(sfunc[i], 0, size);
                     init_syncfunc(sfunc[i], &test_wakeup_sf, optfields);
                     if (iscaller) init_synclink(addrcaller_syncfunc(sfunc[i], size, isstate), &dummy_caller);
                     initprev_synclinkd(addrwaitlist_syncfunc(sfunc[i], true), &srun.wakeup);
                  }
                  setnextfree_syncqueue(squeue, oldfree);

                  // test process_wakeup_list
                  s_test_runcount = 0;
                  TEST(0 == process_wakeup_list(&srun));
                  // check result
                  TEST(0 == s_test_errcount);
                  TEST(lengthof(sfunc) == s_test_runcount);
                  TEST(isvalid_synclinkd(&srun.wakeup) && ! isself_synclinkd(&srun.wakeup));
                  // check length of queues
                  unsigned qidx2 = find_wait_queue(optfields|(isstate2 ? syncfunc_opt_STATE : 0));
                  if (isstate || !setstate) {
                     TEST(qidx2 == qidx);
                  } else {
                     TEST(qidx2 != qidx);
                  }
                  squeue = &srun.rwqueue[qidx2];
                  size   = elemsize_syncqueue(squeue);
                  for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
                     TEST((i == qidx2 ? 1+lengthof(sfunc) : 1) == size_syncqueue(&srun.rwqueue[i]));
                  }
                  // check content of wakeup list
                  unsigned i = 0;
                  for (synclinkd_t * next = srun.wakeup.next; next != &srun.wakeup; next = next->next) {
                     syncfunc_t * sf = waitlistcast_syncfunc(next, true);
                     TEST(squeue == queuefromaddr_syncqueue(sf));
                     TEST(sf->mainfct    == &test_wakeup_sf);
                     TEST(sf->contoffset == s_test_set_contoffset);
                     TEST(sf->optfields  ==  (syncfunc_opt_WAITFOR_CONDITION
                                             | syncfunc_opt_WAITLIST | syncfunc_opt_WAITRESULT
                                             | ((iscaller ? syncfunc_opt_CALLER : 0)|(isstate2 ? syncfunc_opt_STATE : 0))));
                     TEST(EINVAL == *addrwaitresult_syncfunc(sf)); // error
                     TEST( isvalid_synclinkd(addrwaitlist_syncfunc(sf, true)));
                     if (iscaller) {
                        TEST(&dummy_caller == addrcaller_syncfunc(sf, size, isstate2)->link);
                     }
                     if (sf->optfields & syncfunc_opt_STATE) {
                        TEST(s_test_set_state == *addrstate_syncfunc(sf, size));
                     }
                     if (qidx2 == qidx) {
                        TEST(sfunc[i] == sf);
                     }
                     ++i;
                  }
                  TEST(i == lengthof(sfunc));
                  // prepare for next run
                  TEST(0 == free_syncqueue(squeue));
                  TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx2));
                  initself_synclinkd(&srun.wakeup);
               }
            }
         }
      }
   }
   unlink_synccond(&scond);

   // TEST process_wakeup_list: EINVAL (remove_queue)
   s_test_expect_condition = 1;
   s_test_set_contoffset = 0;
   s_test_set_state = 0;
   s_test_expect_state = 0;
   s_test_expect_waitresult = 0;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   for (unsigned cmd = 0; cmd <= 1; ++cmd) {
      s_test_return = cmd ? synccmd_RUN : synccmd_EXIT;
      for (unsigned errcount = 1; errcount <= lengthof(sfunc); ++errcount) {
         optfields = syncfunc_opt_WAITFOR | syncfunc_opt_WAITLIST;
         qidx = find_wait_queue(optfields);
         size = elemsize_syncqueue(&srun.rwqueue[qidx]);
         squeue  = &srun.rwqueue[qidx];
         // build waitlist: sfunc[9] is processed first
         // last entry in queue is nextfree
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            sfunc[i] = nextfree_syncqueue(squeue);
            TEST(0 == preallocate_syncqueue(squeue));
            memset(sfunc[i], 0, size);
            init_syncfunc(sfunc[i], &test_wakeup_sf, optfields);
            initnext_synclinkd(addrwaitlist_syncfunc(sfunc[i], true), &srun.wakeup);
         }
         // test process_wakeup_list
         s_test_runcount = 0;
         init_testerrortimer(&s_syncrunner_errtimer, errcount, EINVAL);
         TEST(EINVAL == process_wakeup_list(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(errcount == s_test_runcount);
         unsigned qidx2 = cmd ? find_run_queue(optfields) : qidx;
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST((i == qidx ? 1+lengthof(sfunc)-errcount : i == qidx2 ? errcount : 1) == size_syncqueue(&srun.rwqueue[i]));
         }
         if (cmd) {
            TEST(0 /*no reallocation*/ == nextfree_syncqueue(&srun.rwqueue[qidx2]));
         }
         TEST(sfunc[lengthof(sfunc)-errcount] == nextfree_syncqueue(squeue));
         if (errcount == lengthof(sfunc)) {
            TEST(isself_synclinkd(&srun.wakeup));
         } else {
            TEST(srun.wakeup.next == addrwaitlist_syncfunc(sfunc[lengthof(sfunc)-1-errcount], true));
            for (unsigned i = lengthof(sfunc)-1-errcount; i < lengthof(sfunc); --i) {
               if (i) {
                  TEST(addrwaitlist_syncfunc(sfunc[i], true)->next == addrwaitlist_syncfunc(sfunc[i-1], true));
               } else {
                  TEST(addrwaitlist_syncfunc(sfunc[0], true)->next == &srun.wakeup);
               }
               TEST(sfunc[i]->mainfct    == &test_wakeup_sf);
               TEST(sfunc[i]->contoffset == 0);
               TEST(sfunc[i]->optfields  == optfields);
            }
         }
         // prepare for next run
         TEST(0 == free_syncqueue(squeue));
         TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
         initself_synclinkd(&srun.wakeup);
         if (cmd) {
            squeue = &srun.rwqueue[qidx2];
            size = elemsize_syncqueue(squeue);
            TEST(0 == free_syncqueue(squeue));
            TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx2));
         }
      }
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == free_synccond(&scond));

   return 0;
ONERR:
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_run_sf(syncfunc_param_t * sfparam, uint32_t sfcmd)
{
   assert(s_test_srun == sfparam->syncrun);

   s_test_runcount += 1;
   s_test_errcount += (sfparam->syncrun->isrun != 1);
   s_test_errcount += (sfparam->syncrun->caller != 0);
   s_test_errcount += (sfcmd != s_test_expect_cmd);
   s_test_errcount += (sfparam->state != s_test_expect_state);
   s_test_errcount += (sfparam->contoffset != s_test_expect_contoffset);

   sfparam->contoffset = s_test_set_contoffset;
   sfparam->state = s_test_set_state;

   sfparam->condition = s_test_set_condition;
   sfparam->syncrun->caller = s_test_set_called; // fake call of another function
   sfparam->retcode   = s_test_set_retcode;

   return s_test_return;
}

static int test_exec_run(void)
{
   syncrunner_t   srun;
   syncfunc_t   * sfunc[10];
   syncfunc_opt_e soptfields[10];
   syncfunc_opt_e optfields;
   unsigned       qidx;
   uint16_t       size;
   syncqueue_t *  squeue;
   synclink_t     dummy_caller;
   synclink_t     dummy_called;
   syncfunc_t     dummy_func = { .optfields = syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST|syncfunc_opt_STATE|syncfunc_opt_CALLER };
   synccond_t     scond;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));
   TEST(0 == init_synccond(&scond));

   // TEST run_syncrunner: EINPROGRESS
   srun.isrun = 1;
   TEST(EINPROGRESS == run_syncrunner(&srun));
   TEST(1 == srun.isrun);
   TEST(isself_synclinkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(1 == size_syncqueue(&srun.rwqueue[i]));
   }
   srun.isrun = 0;

   // TEST run_syncrunner: empty queues
   TEST(0 == run_syncrunner(&srun));
   TEST(0 == s_test_errcount);
   TEST(0 == srun.isrun);
   TEST(isself_synclinkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(1 == size_syncqueue(&srun.rwqueue[i]));
   }

   // TEST run_syncrunner: delete last entry in queue
   s_test_return = synccmd_EXIT;
   s_test_set_state = 0;
   s_test_expect_state = 0;
   s_test_expect_contoffset = 0;
   s_test_expect_cmd = synccmd_RUN;
   optfields = 0;
   qidx = find_run_queue(optfields);
   squeue = &srun.rwqueue[qidx];
   size = elemsize_syncqueue(squeue);
   sfunc[0] = nextfree_syncqueue(squeue);
   init_syncfunc(sfunc[0], &test_run_sf, optfields);
   setnextfree_syncqueue(squeue, sfunc[0]+1);
   // test
   s_test_runcount = 0;
   TEST(0 == run_syncrunner(&srun));
   // check
   TEST(0 == s_test_errcount);
   TEST(1 == s_test_runcount);
   TEST(0 == srun.isrun);
   TEST(isself_synclinkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST((i == qidx ? 0 : 1) == size_syncqueue(&srun.rwqueue[i]));
   }

   // TEST run_syncrunner: preallocate
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   s_test_return = synccmd_RUN;
   s_test_set_state = 0;
   s_test_expect_state = 0;
   s_test_expect_contoffset = 0;
   s_test_expect_cmd = synccmd_RUN;
   // set one entry per run queue (overwrite if different config map to same run queue)
   for (int isstate = 0; isstate <= 1; ++isstate) {
      for (int iscaller = 0; iscaller <= 1; ++iscaller) {
         optfields = (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
         qidx = find_run_queue(optfields);
         squeue =  &srun.rwqueue[qidx];
         size = elemsize_syncqueue(squeue);
         sfunc[qidx] = nextfree_syncqueue(squeue);
         soptfields[qidx] = optfields;
         memset(sfunc[qidx], 0, size);
         init_syncfunc(sfunc[qidx], &test_run_sf,  optfields);
      }
   }
   // clear all preallocated entries
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      setnextfree_syncqueue(&srun.rwqueue[i], 0);
   }
   // test run_syncrunner
   s_test_runcount = 0;
   TEST(0 == run_syncrunner(&srun));
   // check result
   TEST(0 == s_test_errcount);
   TEST(WAITQUEUE_OFFSET == s_test_runcount);
   TEST(0 == srun.isrun);
   TEST(isself_synclinkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      TEST(2 == size_syncqueue(&srun.rwqueue[i]));
      TEST(0 != nextfree_syncqueue(&srun.rwqueue[i]));
   }
   for (unsigned i = 0; i < WAITQUEUE_OFFSET; ++i) {
      TEST(sfunc[i] == first_queue(genericcast_queue(&srun.rwqueue[i]), elemsize_syncqueue(&srun.rwqueue[i])));
      TEST(sfunc[i]->mainfct    == &test_run_sf);
      TEST(sfunc[i]->contoffset == 0);
      TEST(sfunc[i]->optfields  == soptfields[i]);
   }

   // TEST run_syncrunner: synccmd_EXIT && "all possible run parameter""
   TEST(0 == free_syncrunner(&srun));
   TEST(0 == init_syncrunner(&srun));
   s_test_return = synccmd_EXIT;
   s_test_set_called = &dummy_called;
   s_test_set_condition = 0;
   s_test_set_contoffset = 0;
   s_test_set_retcode = 0;
   s_test_set_state = 0;
   for (int isstate = 0; isstate <= 1; ++isstate) {
      s_test_expect_state = isstate ? (void*)(uintptr_t)0x234 : 0;
      for (int iscaller = 0; iscaller <= 1; ++iscaller) {
         for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
            s_test_expect_contoffset = (uint16_t) contoffset;
            s_test_expect_cmd = contoffset ? synccmd_CONTINUE : synccmd_RUN;
            optfields = (isstate ? syncfunc_opt_STATE : 0) | (iscaller ? syncfunc_opt_CALLER : 0);
            qidx = find_run_queue(optfields);
            size = elemsize_syncqueue(&srun.rwqueue[qidx]);
            squeue  = &srun.rwqueue[qidx];
            for (unsigned i = 0; i < lengthof(sfunc); ++i) {
               sfunc[i] = nextfree_syncqueue(squeue);
               TEST(0 == preallocate_syncqueue(squeue));
               memset(sfunc[i], 0, size);
               init_syncfunc(sfunc[i], &test_run_sf, optfields);
               sfunc[i]->contoffset = (uint16_t) contoffset;
               if (isstate) *addrstate_syncfunc(sfunc[i], size) = s_test_expect_state;
            }
            // test run_syncrunner
            s_test_runcount = 0;
            TEST(0 == run_syncrunner(&srun));
            // check result
            TEST(0 == s_test_errcount);
            TEST(lengthof(sfunc) == s_test_runcount);
            TEST(0 == srun.isrun);
            TEST(isself_synclinkd(&srun.wakeup));
            TEST(sfunc[0] == nextfree_syncqueue(squeue));
            for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
               TEST(1 == size_syncqueue(&srun.rwqueue[i]));
               TEST(0 != nextfree_syncqueue(&srun.rwqueue[i]));
            }
         }
      }
   }

   // TEST run_syncrunner: synccmd_EXIT && "wakeup waiting caller"
   s_test_return = synccmd_EXIT;
   s_test_set_called = 0;
   s_test_set_condition = 0;
   s_test_set_contoffset = 0;
   s_test_set_retcode = 0;
   s_test_set_state = 0;
   s_test_expect_condition = 0;
   s_test_expect_state = 0;
   s_test_expect_contoffset = 0;
   s_test_expect_cmd = synccmd_RUN;
   for (int isstate2 = 0; isstate2 <= 1; ++isstate2) {
      for (int retcode = -5; retcode <= 5; retcode += 5) {
         s_test_set_retcode = retcode;
         s_test_expect_waitresult = retcode;
         for (int isstate = 0; isstate <= 1; ++isstate) {
            for (int iscaller = 0; iscaller <= 1; ++iscaller) {
               // generate running functions
               optfields = (isstate2 ? syncfunc_opt_STATE : 0) | syncfunc_opt_CALLER;
               qidx   = find_run_queue(optfields);
               size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
               squeue = &srun.rwqueue[qidx];
               for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                  sfunc[i] = nextfree_syncqueue(squeue);
                  TEST(0 == preallocate_syncqueue(squeue));
                  memset(sfunc[i], 0, size);
                  init_syncfunc(sfunc[i], &test_run_sf, optfields);
               }
               uint16_t size2 = size;
               // generate waiting functions
               optfields = syncfunc_opt_WAITFOR_CALLED | syncfunc_opt_WAITLIST
                         | (isstate ? syncfunc_opt_STATE : 0) | (iscaller ? syncfunc_opt_CALLER : 0);
               qidx   = find_wait_queue(optfields);
               size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
               squeue = &srun.rwqueue[qidx];
               for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                  syncfunc_t * caller = nextfree_syncqueue(squeue);
                  TEST(0 == preallocate_syncqueue(squeue));
                  memset(caller, 0, size);
                  init_syncfunc(caller, &test_wakeup_sf, optfields);
                  init_synclink(addrwaitfor_syncfunc(caller), addrcaller_syncfunc(sfunc[i], size2, isstate2));
                  if (iscaller) init_synclink(addrcaller_syncfunc(caller, size, isstate), &dummy_caller);
               }
               // test run_syncrunner
               s_test_runcount = 0;
               TEST(0 == run2_syncrunner(&srun, false));
               // check result
               TEST(0 == s_test_errcount);
               TEST(lengthof(sfunc) == s_test_runcount);
               TEST(0 == srun.isrun);
               TEST(! isself_synclinkd(&srun.wakeup) && isvalid_synclinkd(&srun.wakeup));
               for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
                  void * last = last_queue(genericcast_queue(&srun.rwqueue[i]), elemsize_syncqueue(&srun.rwqueue[i]));
                  TEST((i == qidx ? lengthof(sfunc)+1 : 1) == size_syncqueue(&srun.rwqueue[i]));
                  TEST(0 != last);
                  TEST(last == nextfree_syncqueue(&srun.rwqueue[i]));
               }
               // check content of wakeup list
               optfields |= syncfunc_opt_WAITRESULT;
               unsigned i = 0;
               for (synclinkd_t * next = srun.wakeup.next, * prev = &srun.wakeup; next != &srun.wakeup; next = next->next) {
                  syncfunc_t * sf = waitlistcast_syncfunc(next, true);
                  TEST(squeue == queuefromaddr_syncqueue(sf));
                  TEST(sf->mainfct    == &test_wakeup_sf);
                  TEST(sf->contoffset == 0);
                  TEST(sf->optfields  == optfields);
                  TEST(retcode == *addrwaitresult_syncfunc(sf));
                  TEST(prev == addrwaitlist_syncfunc(sf, true)->prev);
                  TEST(0    != addrwaitlist_syncfunc(sf, true)->next);
                  prev = addrwaitlist_syncfunc(sf, true);
                  if (iscaller) {
                     TEST(&dummy_caller == addrcaller_syncfunc(sf, size, isstate)->link);
                  }
                  if (isstate) {
                     TEST(s_test_set_state == *addrstate_syncfunc(sf, size));
                  }
                  ++i;
               }
               TEST(i == lengthof(sfunc));
               // prepare for next run
               TEST(0 == free_syncqueue(squeue));
               TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
               initself_synclinkd(&srun.wakeup);
            }
         }
      }
   }

   // TEST run_syncrunner: synccmd_RUN && synccmd_CONTINUE
   for (int retcmd = 0; retcmd <= 1; ++retcmd) {
      s_test_return = retcmd ? synccmd_CONTINUE : synccmd_RUN;
      s_test_expect_cmd = synccmd_RUN;
      s_test_expect_contoffset = 0;
      s_test_expect_state = 0;
      s_test_set_contoffset = 100;
      s_test_set_state = 0;
      for (int setstate = 0; setstate <= 1; ++setstate) {
         s_test_set_state = setstate ? &optfields : 0;
         for (int isstate = 0; isstate <= 1; ++isstate) {
            for (int iscaller = 0; iscaller <= 1; ++iscaller) {
               // generate running functions
               optfields = (isstate ? syncfunc_opt_STATE : 0) | (iscaller ? syncfunc_opt_CALLER : 0);
               qidx   = find_run_queue(optfields);
               size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
               squeue = &srun.rwqueue[qidx];
               for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                  sfunc[i] = nextfree_syncqueue(squeue);
                  TEST(0 == preallocate_syncqueue(squeue));
                  memset(sfunc[i], 0, size);
                  init_syncfunc(sfunc[i], &test_run_sf, optfields);
                  if (iscaller) init_synclink(addrcaller_syncfunc(sfunc[i], size, isstate), &dummy_caller);
               }
               // test run_syncrunner
               s_test_runcount = 0;
               TEST(0 == run_syncrunner(&srun));
               // check result
               TEST(0 == s_test_errcount);
               TEST(lengthof(sfunc) == s_test_runcount);
               TEST(0 == srun.isrun);
               TEST(isself_synclinkd(&srun.wakeup));
               int isstate2 = isstate || s_test_set_state;
               optfields = (isstate2 ? syncfunc_opt_STATE : 0) | (iscaller ? syncfunc_opt_CALLER : 0);
               qidx = find_run_queue(optfields);
               size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
               squeue = &srun.rwqueue[qidx];
               for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
                  void * last = last_queue(genericcast_queue(&srun.rwqueue[i]), elemsize_syncqueue(&srun.rwqueue[i]));
                  TEST((i == qidx ? lengthof(sfunc)+1 : 1) == size_syncqueue(&srun.rwqueue[i]));
                  TEST(0 != last);
                  TEST(last == nextfree_syncqueue(&srun.rwqueue[i]));
               }
               unsigned i = 0;
               foreach (_queue, next, genericcast_queue(squeue), size) {
                  if (next == nextfree_syncqueue(squeue)) continue;
                  syncfunc_t * sf = next;
                  if (isstate == isstate2) {
                     TEST(sf == sfunc[i]);
                  }
                  TEST(sf->mainfct    == &test_run_sf);
                  TEST(sf->contoffset == (retcmd ? s_test_set_contoffset : 0));
                  TEST(sf->optfields  == optfields);
                  if (isstate2) {
                     TEST(s_test_set_state == *addrstate_syncfunc(sf, size));
                  }
                  if (iscaller) {
                     TEST(&dummy_caller == addrcaller_syncfunc(sf, size, isstate2)->link);
                  }
                  ++i;
               }
               TEST(lengthof(sfunc) == i);
               // prepare for next run
               TEST(0 == free_syncqueue(squeue));
               TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
            }
         }
      }
   }

   // TEST run_syncrunner: synccmd_WAIT
   s_test_return = synccmd_WAIT;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   for (int condition = 0; condition <= 1; ++condition) {
      s_test_set_condition = condition ? &scond : 0;
      s_test_set_called    = &dummy_called;
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_set_contoffset = (uint16_t) contoffset;
         for (int isstate = 0; isstate <= 1; ++isstate) {
            for (int setstate = 0; setstate <= 1; ++setstate) {
               s_test_set_state = setstate ? &optfields : 0;
               for (int iscaller = 0; iscaller <= 1; ++iscaller) {
                  optfields = (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
                  qidx   = find_run_queue(optfields);
                  size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
                  squeue = &srun.rwqueue[qidx];
                  for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                     sfunc[i] = nextfree_syncqueue(squeue);
                     TEST(0 == preallocate_syncqueue(squeue));
                     memset(sfunc[i], 0, size);
                     init_syncfunc(sfunc[i], &test_run_sf, optfields);
                     if (iscaller) init_synclink(addrcaller_syncfunc(sfunc[i], size, isstate), &dummy_caller);
                  }
                  // test run_syncrunner
                  s_test_runcount = 0;
                  TEST(0 == run_syncrunner(&srun));
                  // check result
                  TEST(0 == s_test_errcount);
                  TEST(lengthof(sfunc) == s_test_runcount);
                  TEST(0 == srun.isrun);
                  TEST(isself_synclinkd(&srun.wakeup));
                  // check length of queues
                  optfields = (condition ? syncfunc_opt_WAITFOR_CONDITION : syncfunc_opt_WAITFOR_CALLED)
                            | syncfunc_opt_WAITLIST | (iscaller ? syncfunc_opt_CALLER : 0) | (setstate ? syncfunc_opt_STATE : 0);
                  qidx   = find_wait_queue(optfields);
                  squeue = &srun.rwqueue[qidx];
                  size   = elemsize_syncqueue(squeue);
                  for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
                     TEST((i == qidx ? 1+lengthof(sfunc) : 1) == size_syncqueue(&srun.rwqueue[i]));
                  }
                  // check content of wait queue
                  unsigned i = 0;
                  foreach (_queue, next, genericcast_queue(squeue), size) {
                     if (next == nextfree_syncqueue(squeue)) continue;
                     syncfunc_t * sf = next;
                     TEST(sf->mainfct    == &test_run_sf);
                     TEST(sf->contoffset == s_test_set_contoffset);
                     TEST(sf->optfields  == optfields);
                     if (! condition) {
                        TEST(&dummy_called == addrwaitfor_syncfunc(sf)->link);
                        TEST(! isvalid_synclinkd(addrwaitlist_syncfunc(sf, true)));
                     }
                     if (iscaller) {
                        TEST(&dummy_caller == addrcaller_syncfunc(sf, size, setstate)->link);
                     }
                     if (setstate) {
                        TEST(s_test_set_state == *addrstate_syncfunc(sf, size));
                     }
                     ++i;
                  }
                  TEST(i == lengthof(sfunc));
                  // check condition waitlist
                  if (condition) {
                     TEST(iswaiting_synccond(&scond));
                     syncfunc_t * sf = waitfunc_synccond(&scond);
                     TEST(squeue == queuefromaddr_syncqueue(sf));
                     TEST(&scond.waitfunc == addrwaitfor_syncfunc(sf)->link);
                     TEST(isvalid_synclinkd(addrwaitlist_syncfunc(sf, true)));
                     for (i = 1; i < lengthof(sfunc); ++i) {
                        TEST(addrwaitlist_syncfunc(sf, true) == addrwaitlist_syncfunc(sf, true)->next->prev);
                        sf = waitlistcast_syncfunc(addrwaitlist_syncfunc(sf, true)->next, true);
                        TEST(!isvalid_synclink(addrwaitfor_syncfunc(sf)));
                        TEST(isvalid_synclinkd(addrwaitlist_syncfunc(sf, true)));
                     }
                     TEST(addrwaitlist_syncfunc(sf, true) == addrwaitlist_syncfunc(sf, true)->next->prev);
                     sf = waitlistcast_syncfunc(addrwaitlist_syncfunc(sf, true)->next, true);
                     TEST(sf == waitfunc_synccond(&scond));
                  }
                  // prepare for next run
                  unlink_synccond(&scond);
                  TEST(0 == free_syncqueue(squeue));
                  TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
               }
            }
         }
      }
   }

   // TEST run_syncrunner: wait error ==> add function to waitlist with wait result == EINVAL
   s_test_return = synccmd_WAIT;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   link_synccond(&scond, &dummy_func); // error
   for (int condition = 0; condition <= 1; ++condition) {
      s_test_set_condition = condition ? &scond : 0;
      s_test_set_called    = 0; // error
      for (int contoffset = 0; contoffset <= 256; contoffset += 128) {
         s_test_set_contoffset = (uint16_t) contoffset;
         for (int isstate = 0; isstate <= 1; ++isstate) {
            for (int setstate = 0; setstate <= 1; ++setstate) {
               s_test_set_state = setstate ? &optfields : 0;
               for (int iscaller = 0; iscaller <= 1; ++iscaller) {
                  optfields = (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
                  qidx   = find_run_queue(optfields);
                  size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
                  squeue = &srun.rwqueue[qidx];
                  for (unsigned i = 0; i < lengthof(sfunc); ++i) {
                     sfunc[i] = nextfree_syncqueue(squeue);
                     TEST(0 == preallocate_syncqueue(squeue));
                     memset(sfunc[i], 0, size);
                     init_syncfunc(sfunc[i], &test_run_sf, optfields);
                     if (iscaller) init_synclink(addrcaller_syncfunc(sfunc[i], size, isstate), &dummy_caller);
                  }
                  // test run_syncrunner
                  s_test_runcount = 0;
                  TEST(0 == run2_syncrunner(&srun, false));
                  // check result
                  TEST(0 == s_test_errcount);
                  TEST(lengthof(sfunc) == s_test_runcount);
                  TEST(0 == srun.isrun);
                  TEST(isvalid_synclinkd(&srun.wakeup) && ! isself_synclinkd(&srun.wakeup));
                  // check length of queues
                  optfields = syncfunc_opt_WAITFOR_CONDITION | syncfunc_opt_WAITLIST | syncfunc_opt_WAITRESULT
                            | (iscaller ? syncfunc_opt_CALLER : 0) | (setstate ? syncfunc_opt_STATE : 0);
                  qidx   = find_wait_queue(optfields);
                  squeue = &srun.rwqueue[qidx];
                  size   = elemsize_syncqueue(squeue);
                  for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
                     TEST((i == qidx ? 1+lengthof(sfunc) : 1) == size_syncqueue(&srun.rwqueue[i]));
                  }
                  // check content of wakeup list
                  unsigned i = 0;
                  for (synclinkd_t * next = srun.wakeup.next, * prev = &srun.wakeup; next != &srun.wakeup; next = next->next) {
                     syncfunc_t * sf = waitlistcast_syncfunc(next, true);
                     TEST(squeue == queuefromaddr_syncqueue(sf));
                     TEST(sf->mainfct    == &test_run_sf);
                     TEST(sf->contoffset == s_test_set_contoffset);
                     TEST(sf->optfields  == optfields);
                     TEST(EINVAL == *addrwaitresult_syncfunc(sf)); // error
                     TEST(prev == addrwaitlist_syncfunc(sf, true)->prev);
                     TEST(0    != addrwaitlist_syncfunc(sf, true)->next);
                     prev = addrwaitlist_syncfunc(sf, true);
                     if (iscaller) {
                        TEST(&dummy_caller == addrcaller_syncfunc(sf, size, setstate)->link);
                     }
                     if (setstate) {
                        TEST(s_test_set_state == *addrstate_syncfunc(sf, size));
                     }
                     ++i;
                  }
                  TEST(i == lengthof(sfunc));
                  // prepare for next run
                  TEST(0 == free_syncqueue(squeue));
                  TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
                  initself_synclinkd(&srun.wakeup);
               }
            }
         }
      }
   }
   unlink_synccond(&scond);

   // TEST run_syncrunner: EINVAL (remove_queue)
   s_test_set_contoffset = 0;
   s_test_set_state = (void*)1;
   s_test_expect_cmd = synccmd_RUN;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   for (unsigned cmd = 0; cmd <= 1; ++cmd) {
      s_test_return = cmd ? synccmd_RUN : synccmd_EXIT;
      for (unsigned errcount = 1; errcount <= lengthof(sfunc); ++errcount) {
         optfields = 0;
         qidx = find_run_queue(optfields);
         size = elemsize_syncqueue(&srun.rwqueue[qidx]);
         squeue  = &srun.rwqueue[qidx];
         for (unsigned i = 0; i < lengthof(sfunc); ++i) {
            sfunc[i] = nextfree_syncqueue(squeue);
            TEST(0 == preallocate_syncqueue(squeue));
            memset(sfunc[i], 0, size);
            init_syncfunc(sfunc[i], &test_run_sf, optfields);
         }
         // test run_syncrunner
         s_test_runcount = 0;
         init_testerrortimer(&s_syncrunner_errtimer, errcount, EINVAL);
         TEST(EINVAL == run_syncrunner(&srun));
         // check result
         TEST(0 == s_test_errcount);
         TEST(errcount == s_test_runcount);
         unsigned qidx2 = find_run_queue(syncfunc_opt_STATE);
         for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
            TEST((i == qidx ? 1+lengthof(sfunc)-errcount : cmd && qidx2== i ? errcount : 1) == size_syncqueue(&srun.rwqueue[i]));
         }
         TEST(sfunc[lengthof(sfunc)-errcount] == nextfree_syncqueue(squeue));
         for (unsigned i = lengthof(sfunc)-1-errcount; i < lengthof(sfunc); --i) {
            TEST(sfunc[i]->mainfct    == &test_run_sf);
            TEST(sfunc[i]->contoffset == 0);
            TEST(sfunc[i]->optfields  == optfields);
         }
         if (cmd) {
            TEST(0 /*no reallocation*/ == nextfree_syncqueue(&srun.rwqueue[qidx2]));
            foreach (_queue, next, genericcast_queue(&srun.rwqueue[qidx2]), elemsize_syncqueue(&srun.rwqueue[qidx2])) {
               syncfunc_t * sf = next;
               TEST(sf->mainfct      == &test_run_sf);
               TEST(sf->contoffset   == 0);
               TEST(sf->optfields    == syncfunc_opt_STATE);
               TEST(s_test_set_state == *addrstate_syncfunc(sf, elemsize_syncqueue(&srun.rwqueue[qidx2])));
            }
         }
         // prepare for next run
         TEST(0 == free_syncqueue(squeue));
         TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
         initself_synclinkd(&srun.wakeup);
         if (cmd) {
            squeue = &srun.rwqueue[qidx2];
            size = elemsize_syncqueue(squeue);
            TEST(0 == free_syncqueue(squeue));
            TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx2));
         }
      }
   }

   // unprepare
   TEST(0 == free_synccond(&scond));
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_synccond(&scond);
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_exec_terminate(void)
{
   syncrunner_t   srun;
   syncfunc_opt_e optfields;
   unsigned       qidx;
   uint16_t       size;
   syncqueue_t *  squeue;
   unsigned       MAX_PER_QUEUE = 5000;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));

   // TEST terminate_syncrunner: 1 wait and 1 run queue
   s_test_return = synccmd_EXIT;
   s_test_expect_cmd = synccmd_EXIT;
   s_test_set_called = 0;
   s_test_set_condition = 0;
   s_test_set_contoffset = 1;
   s_test_set_retcode = 100;
   s_test_set_state = &optfields;
   for (int contoffset = 0; contoffset <= 256; contoffset += 256) {
      s_test_expect_contoffset = (uint16_t) contoffset;
      for (int isstate = 0; isstate <= 1; ++isstate) {
         s_test_expect_state = isstate ? &srun : 0;
         for (int iscaller = 0; iscaller <= 1; ++iscaller) {
            optfields = (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
            qidx   = find_run_queue(optfields);
            size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
            squeue = &srun.rwqueue[qidx];
            for (unsigned i = 0; i < MAX_PER_QUEUE; ++i) {
               syncfunc_t * sf = nextfree_syncqueue(squeue);
               TEST(0 == preallocate_syncqueue(squeue));
               memset(sf, 0, size);
               init_syncfunc(sf, &test_run_sf, optfields);
               sf->contoffset = s_test_expect_contoffset;
               if (isstate) *addrstate_syncfunc(sf, size) = s_test_expect_state;
            }
            optfields = syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST
                      | (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
            qidx   = find_wait_queue(optfields);
            size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
            squeue = &srun.rwqueue[qidx];
            for (unsigned i = 0; i < MAX_PER_QUEUE; ++i) {
               syncfunc_t * sf = nextfree_syncqueue(squeue);
               TEST(0 == preallocate_syncqueue(squeue));
               memset(sf, 0, size);
               init_syncfunc(sf, &test_run_sf, optfields);
               sf->contoffset = s_test_expect_contoffset;
               if (isstate) *addrstate_syncfunc(sf, size) = s_test_expect_state;
               initprev_synclinkd(addrwaitlist_syncfunc(sf, true), &srun.wakeup);
            }
            // test terminate_syncrunner
            s_test_runcount = 0;
            TEST(0 == terminate_syncrunner(&srun));
            // check result
            TEST(0 == s_test_errcount);
            TEST(2*MAX_PER_QUEUE == s_test_runcount);
            TEST(isself_synclinkd(&srun.wakeup));
            for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
               void * last = last_queue(genericcast_queue(&srun.rwqueue[i]), elemsize_syncqueue(&srun.rwqueue[i]));
               TEST(1 == size_syncqueue(&srun.rwqueue[i]));
               TEST(0 != last);
               TEST(last == nextfree_syncqueue(&srun.rwqueue[i]));
            }
         }
      }
   }

   // TEST terminate_syncrunner: all queues
   s_test_return = synccmd_EXIT;
   s_test_expect_cmd = synccmd_EXIT;
   s_test_expect_contoffset = 0;
   s_test_expect_state = 0;
   // fill all queues
   for (int isstate = 0; isstate <= 1; ++isstate) {
      for (int iscaller = 0; iscaller <= 1; ++iscaller) {
         optfields = (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
         qidx   = find_run_queue(optfields);
         size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
         squeue = &srun.rwqueue[qidx];
         for (unsigned i = 0; i < MAX_PER_QUEUE; ++i) {
            syncfunc_t * sf = nextfree_syncqueue(squeue);
            TEST(0 == preallocate_syncqueue(squeue));
            memset(sf, 0, size);
            init_syncfunc(sf, &test_run_sf, optfields);
         }
         optfields = syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST
                   | (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
         qidx   = find_wait_queue(optfields);
         size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
         squeue = &srun.rwqueue[qidx];
         for (unsigned i = 0; i < MAX_PER_QUEUE; ++i) {
            syncfunc_t * sf = nextfree_syncqueue(squeue);
            TEST(0 == preallocate_syncqueue(squeue));
            memset(sf, 0, size);
            init_syncfunc(sf, &test_run_sf, optfields);
            initprev_synclinkd(addrwaitlist_syncfunc(sf, true), &srun.wakeup);
         }
      }
   }
   // test terminate_syncrunner
   s_test_runcount = 0;
   TEST(0 == terminate_syncrunner(&srun));
   // check result
   TEST(0 == s_test_errcount);
   TEST(8*MAX_PER_QUEUE == s_test_runcount);
   TEST(isself_synclinkd(&srun.wakeup));
   for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
      void * last = last_queue(genericcast_queue(&srun.rwqueue[i]), elemsize_syncqueue(&srun.rwqueue[i]));
      TEST(1 == size_syncqueue(&srun.rwqueue[i]));
      TEST(0 != last);
      TEST(last == nextfree_syncqueue(&srun.rwqueue[i]));
   }

   // TEST terminate_syncrunner: EINVAL (remove_queue)
   s_test_return = synccmd_EXIT;
   s_test_expect_cmd = synccmd_EXIT;
   s_test_expect_contoffset = 0;
   for (unsigned iswait = 0; iswait <= 1; ++iswait) {
      for (unsigned errcount = 1; errcount <= 3; ++errcount) {
         for (int isstate = 0; isstate <= 1; ++isstate) {
            s_test_expect_state = isstate ? &srun : 0;
            for (int iscaller = 0; iscaller <= 1; ++iscaller) {
               optfields = (iscaller ? syncfunc_opt_CALLER : 0) | (isstate ? syncfunc_opt_STATE : 0);
               if (iswait) optfields |= syncfunc_opt_WAITFOR|syncfunc_opt_WAITLIST;
               qidx   = iswait ? find_wait_queue(optfields) : find_run_queue(optfields);
               size   = elemsize_syncqueue(&srun.rwqueue[qidx]);
               squeue = &srun.rwqueue[qidx];
               for (unsigned i = 0; i < 3; ++i) {
                  syncfunc_t * sf = nextfree_syncqueue(squeue);
                  TEST(0 == preallocate_syncqueue(squeue));
                  memset(sf, 0, size);
                  init_syncfunc(sf, &test_run_sf, optfields);
                  if (isstate) *addrstate_syncfunc(sf, size) = s_test_expect_state;
                  if (iswait) initprev_synclinkd(addrwaitlist_syncfunc(sf, true), &srun.wakeup);
               }
               // test terminate_syncrunner
               init_testerrortimer(&s_syncrunner_errtimer, errcount, EINVAL);
               s_test_runcount = 0;
               TEST(EINVAL == terminate_syncrunner(&srun));
               // check result
               TEST(0 == s_test_errcount);
               TEST(errcount == s_test_runcount);
               TEST(isself_synclinkd(&srun.wakeup));
               for (unsigned i = 0; i < lengthof(srun.rwqueue); ++i) {
                  TEST((i == qidx ? 4-errcount : 1) == size_syncqueue(&srun.rwqueue[i]));
               }
               // prepare for next run
               TEST(0 == free_syncqueue(squeue));
               TEST(0 == init_syncqueue(squeue, size, (uint8_t) qidx));
            }
         }
      }
   }

   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncrunner(&srun);
   return EINVAL;
}

static int test_examples(void)
{
   syncrunner_t   srun;

   // prepare
   s_test_srun = &srun;
   s_test_errcount = 0;
   TEST(0 == init_syncrunner(&srun));

   // test functions calling functions
   // test waiting on condition
   // test terminating

   // TEST run_syncrunner
   // TODO: TEST run_syncrunner


   // TEST terminate_syncrunner
   // TODO: TEST terminate_syncrunner


   // unprepare
   TEST(0 == free_syncrunner(&srun));

   return 0;
ONERR:
   free_syncrunner(&srun);
   return EINVAL;
}

int unittest_task_syncrunner()
{
   if (test_memory())         goto ONERR;
   if (test_initfree())       goto ONERR;
   if (test_queuehelper())    goto ONERR;
   if (test_query())          goto ONERR;
   if (test_addfunc())        goto ONERR;
   if (test_wakeup())         goto ONERR;
   if (test_exec_helper())    goto ONERR;
   if (test_exec_wakeup())    goto ONERR;
   if (test_exec_run())       goto ONERR;
   if (test_exec_terminate()) goto ONERR;
   if (test_examples())       goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
