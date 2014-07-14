/* title: SyncRunner

   Verwaltet ein Menge von <syncfunc_t> in
   einer Reihe von Run- und Wait-Queues.

   Jeder Thread verwendet seinen eigenen <syncrunner_t>.

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
#ifndef CKERN_TASK_SYNCRUNNER_HEADER
#define CKERN_TASK_SYNCRUNNER_HEADER

#include "C-kern/api/task/synccmd.h"
#include "C-kern/api/task/syncfunc.h"
#include "C-kern/api/task/syncqueue.h"

// forward
struct syncfunc_t;

/* typedef: struct syncrunner_t
 * Export <syncrunner_t> into global namespace. */
typedef struct syncrunner_t syncrunner_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncrunner
 * Test <syncrunner_t> functionality. */
int unittest_task_syncrunner(void);
#endif


/* struct: syncrunner_t
 * TODO: describe type
 *
 * TODO: make queue_t adaptable to different page sizes !
 * TODO: syncqueue_t uses 512 or 1024 bytes (check in table with different sizes !!) !
 *
 * */
struct syncrunner_t {
   /* variable: caller
    * Zeigt auf <syncfunc_t.caller> der zuletzt mit <addcall_syncrunner> hinzugefügten Funktion.
    * Falls 0, dann wurde <addcall_syncrunner> noch nicht von der gerade ablaufenden <syncfunc_t> aufgerufen. */
   synclink_t   * caller;
   /* variable: wakeup
    * Verlinkt Einträge in <waitqueue>. Die Felder <syncfunc.waitresult> und <syncfunc.waitlist>ist vorhanden. */
   synclinkd_t    wakeup;
   /* variable: rwqueue
    * Speichert ausführbare und wartende <syncfunc_t> verschiedener Bytegrößen.
    * Die Größe in Bytes einer syncfunc_t bestimmet sich aus dem Vorhandensein optionaler Felder. */
   syncqueue_t    rwqueue[3+3];
   /* variable: isrun
    * Falls true, wird <run_syncrunner> bzw. <teminate_syncrunner> ausgeführt. */
   bool           isrun;
};

// group: lifetime

/* define: syncrunner_FREE
 * Static initializer. */
#define syncrunner_FREE \
         {  0, synclinkd_FREE, \
            { syncqueue_FREE, syncqueue_FREE, syncqueue_FREE, syncqueue_FREE, syncqueue_FREE, syncqueue_FREE }, \
            false, \
         }

/* function: init_syncrunner
 * Initialisiere srun, insbesondere die Warte- und Run-Queues. */
int init_syncrunner(/*out*/syncrunner_t * srun);

/* function: free_syncrunner
 * Gib Speicher frei, insbesondere den SPeicher der Warte- und Run-Queues.
 * Die Ressourcen noch auszuführender <syncfunc_t> und wartender <syncfunc_t>
 * werden nicht freigegeben! Falls dies erforderlich ist, bitte vorher
 * <teminate_syncrunner> aufrufen. */
int free_syncrunner(syncrunner_t * srun);

// group: query

/* function: size_syncrunner
 * Liefere Anzahl wartender und auszuführender <syncfunc_t>. */
size_t size_syncrunner(const syncrunner_t * srun);

// group: update

/* function: addasync_syncrunner
 * TODO: */
int addasync_syncrunner(syncrunner_t * srun, syncfunc_f mainfct, void * state);

/* function: addcall_syncrunner
 * TODO: */
int addcall_syncrunner(syncrunner_t * srun, syncfunc_f mainfct, void * state);

/* function: wakeup_syncrunner
 * Wecke die erste wartende <syncfunc_t> auf.
 * Falls <iswaiting_synccond>(scond)==false, wird nichts getan.
 * Return EINVAL, falls scond zu einem anderen srun gehört. */
int wakeup_syncrunner(syncrunner_t * srun, struct synccond_t * scond);

/* function: wakeupall_syncrunner
 * Wecke alle auf scond wartenden <syncfunc_t> auf.
 * Falls <iswaiting_synccond>(scond)==false, wird nichts getan.
 * Return EINVAL, falls scond zu einem anderen srun gehört. */
int wakeupall_syncrunner(syncrunner_t * srun, struct synccond_t * scond);

// group: execute

/* function: run_syncrunner
 * TODO: */
int run_syncrunner(syncrunner_t * srun);

/* function: teminate_syncrunner
 * TODO: */
void teminate_syncrunner(syncrunner_t * srun);



// section: inline implementation

// group: syncrunner_t

#if !defined(KONFIG_SUBSYS_SYNCRUNNER)

/* define: init_syncrunner
 * ! defined(KONFIG_SUBSYS_SYNCRUNNER) ==> Implementiert <syncrunner_t.init_syncrunner> als No-Op. */
#define init_syncrunner(srun) \
         (0)

/* define: free_syncrunner
 * ! defined(KONFIG_SUBSYS_SYNCRUNNER) ==> Implementiert <syncrunner_t.free_syncrunner> als No-Op. */
#define free_syncrunner(srun) \
         (0)

#endif

#endif
