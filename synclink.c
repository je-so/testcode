/* title: SyncLink impl

   Implements <SyncLink>.

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

   file: C-kern/api/task/synclink.h
    Header file <SyncLink>.

   file: C-kern/task/synclink.c
    Implementation file <SyncLink impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/task/synclink.h"
#include "C-kern/api/err.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif


// section: synclink_t

// group: lifetime


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

static int test_initfree(void)
{
   synclink_t slink  = synclink_FREE;
   synclink_t slink2 = synclink_FREE;
   synclink_t slink3 = synclink_FREE;
   synclinkd_t slinkd  = synclinkd_FREE;
   synclinkd_t slinkd2 = synclinkd_FREE;
   synclinkd_t slinkd3 = synclinkd_FREE;
   synclinkd_t slinkd4 = synclinkd_FREE;

   // === synclink_t ===

   // TEST synclink_FREE
   TEST(0 == slink.link);

   // TEST init_synclink: slink is free
   init_synclink(&slink, &slink2);
   TEST(&slink2 == slink.link);
   TEST(&slink  == slink2.link);

   // TEST init_synclink: slink is valid
   init_synclink(&slink, &slink3);
   TEST(&slink3 == slink.link);
   TEST(&slink  == slink3.link);
   TEST(&slink  == slink2.link); // not changed !

   // TEST free_synclink: slink is valid
   free_synclink(&slink);
   TEST(0 == slink.link);
   TEST(0 == slink3.link);

   // TEST free_synclink: slink is free
   TEST(0 == slink.link);
   free_synclink(&slink);
   TEST(0 == slink.link);

   // TEST free_synclink: free slink2 - the other side
   init_synclink(&slink, &slink2);
   free_synclink(&slink2);
   TEST(0 == slink.link);
   TEST(0 == slink2.link);

   // === synclinkd_t ===

   // TEST synclinkd_FREE
   TEST(0 == slinkd.prev);
   TEST(0 == slinkd.next);

   // TEST init_synclinkd
   init_synclinkd(&slinkd, &slinkd2);
   TEST(&slinkd2 == slinkd.prev);
   TEST(&slinkd2 == slinkd.next);
   TEST(&slinkd  == slinkd2.prev);
   TEST(&slinkd  == slinkd2.next);
   init_synclinkd(&slinkd, &slinkd3);
   TEST(&slinkd3 == slinkd.prev);
   TEST(&slinkd3 == slinkd.next);
   TEST(&slinkd  == slinkd3.prev);
   TEST(&slinkd  == slinkd3.next);

   // TEST initprev_synclinkd: in chain of 2
   init_synclinkd(&slinkd, &slinkd3);
   initprev_synclinkd(&slinkd2, &slinkd3);
   TEST(&slinkd3 == slinkd.prev);
   TEST(&slinkd2 == slinkd.next);
   TEST(&slinkd  == slinkd2.prev);
   TEST(&slinkd3 == slinkd2.next);
   TEST(&slinkd2 == slinkd3.prev);
   TEST(&slinkd  == slinkd3.next);

   // TEST initprev_synclinkd: in chain of 3
   initprev_synclinkd(&slinkd4, &slinkd);
   TEST(&slinkd4 == slinkd.prev);
   TEST(&slinkd2 == slinkd.next);
   TEST(&slinkd  == slinkd2.prev);
   TEST(&slinkd3 == slinkd2.next);
   TEST(&slinkd2 == slinkd3.prev);
   TEST(&slinkd4 == slinkd3.next);
   TEST(&slinkd3 == slinkd4.prev);
   TEST(&slinkd  == slinkd4.next);

   // TEST initnext_synclinkd: in chain of 2
   init_synclinkd(&slinkd, &slinkd3);
   initnext_synclinkd(&slinkd2, &slinkd);
   TEST(&slinkd3 == slinkd.prev);
   TEST(&slinkd2 == slinkd.next);
   TEST(&slinkd  == slinkd2.prev);
   TEST(&slinkd3 == slinkd2.next);
   TEST(&slinkd2 == slinkd3.prev);
   TEST(&slinkd  == slinkd3.next);

   // TEST initnext_synclinkd: in chain of 3
   initnext_synclinkd(&slinkd4, &slinkd3);
   TEST(&slinkd4 == slinkd.prev);
   TEST(&slinkd2 == slinkd.next);
   TEST(&slinkd  == slinkd2.prev);
   TEST(&slinkd3 == slinkd2.next);
   TEST(&slinkd2 == slinkd3.prev);
   TEST(&slinkd4 == slinkd3.next);
   TEST(&slinkd3 == slinkd4.prev);
   TEST(&slinkd  == slinkd4.next);

   // TEST initself_synclinkd
   initself_synclinkd(&slinkd);
   TEST(&slinkd == slinkd.prev);
   TEST(&slinkd == slinkd.next);
   // initself_synclinkd can be used to add elements without checking for valid links
   initprev_synclinkd(&slinkd2, &slinkd);
   TEST(&slinkd2 == slinkd.prev);
   TEST(&slinkd2 == slinkd.next);
   TEST(&slinkd  == slinkd2.prev);
   TEST(&slinkd  == slinkd2.next);

   // TEST free_synclinkd: link is already free
   slinkd = (synclinkd_t) synclinkd_FREE;
   free_synclinkd(&slinkd);
   TEST(0 == slinkd.prev);
   TEST(0 == slinkd.next);

   // TEST free_synclinkd: 2 nodes connected
   init_synclinkd(&slinkd, &slinkd2);
   free_synclinkd(&slinkd);
   TEST(0 == slinkd.prev);
   TEST(0 == slinkd.next);
   TEST(0 == slinkd2.prev);
   TEST(0 == slinkd2.next);

   // TEST free_synclinkd: 3 nodes connected
   init_synclinkd(&slinkd, &slinkd2);
   initnext_synclinkd(&slinkd3, &slinkd2);
   free_synclinkd(&slinkd);
   TEST(0 == slinkd.prev);
   TEST(0 == slinkd.next);
   TEST(&slinkd3 == slinkd2.prev);
   TEST(&slinkd3 == slinkd2.next);
   TEST(&slinkd2 == slinkd3.prev);
   TEST(&slinkd2 == slinkd3.next);

   return 0;
ONERR:
   return EINVAL;
}

static int test_query(void)
{
   synclink_t  slink   = synclink_FREE;
   synclinkd_t slinkd  = synclinkd_FREE;
   synclinkd_t slinkd2 = synclinkd_FREE;

   // === synclink_t ===

   // TEST isvalid_synclink: synclink_FREE
   TEST(0 == isvalid_synclink(&slink));

   // TEST isvalid_synclink: != synclink_FREE
   slink.link = &slink;
   TEST(0 != isvalid_synclink(&slink));

   // === synclinkd_t ===

   // TEST isvalid_synclinkd: synclinkd_FREE
   TEST(0 == isvalid_synclinkd(&slinkd));

   // TEST isself_synclinkd: synclinkd_FREE
   TEST(0 == isself_synclinkd(&slinkd));

   // TEST isvalid_synclinkd: != synclinkd_FREE
   init_synclinkd(&slinkd, &slinkd2);
   TEST(0 != isvalid_synclinkd(&slinkd));
   TEST(0 != isvalid_synclinkd(&slinkd2));

   // TEST isself_synclinkd: true
   initself_synclinkd(&slinkd);
   TEST(1 == isself_synclinkd(&slinkd));

   // TEST isself_synclinkd: false
   init_synclinkd(&slinkd, &slinkd2);
   TEST(0 == isself_synclinkd(&slinkd));
   TEST(0 == isself_synclinkd(&slinkd2));

   return 0;
ONERR:
   return EINVAL;
}

static int test_update(void)
{
   synclink_t slink;
   synclink_t slink2;
   synclink_t slink3;
   synclinkd_t slinkd[6];

   // === synclink_t ===

   // TEST relink_synclink: other side 0
   slink.link  = &slink2;
   slink2.link = 0;
   relink_synclink(&slink);
   TEST(&slink == slink2.link);

   // TEST relink_synclink: simulate move in memory
   slink3 = slink; // moved in memory
   relink_synclink(&slink3);
   TEST(&slink3 == slink2.link);
   TEST(&slink2 == slink.link); // not changed

   // TEST unlink_synclink: connected
   init_synclink(&slink, &slink2);
   unlink_synclink(&slink);
   TEST(0 == slink2.link);
   TEST(&slink2 == slink.link); // not changed

   // === synclinkd_t ===

   // TEST relink_synclinkd
   init_synclinkd(&slinkd[0], &slinkd[1]);
   initnext_synclinkd(&slinkd[2], &slinkd[1]);
   slinkd[3] = slinkd[0];        // moved in memory
   relink_synclinkd(&slinkd[3]); // adapt neighbours
   TEST(&slinkd[2] == slinkd[0].prev); // not changed
   TEST(&slinkd[1] == slinkd[0].next); // not changed
   TEST(&slinkd[3] == slinkd[1].prev);
   TEST(&slinkd[2] == slinkd[1].next);
   TEST(&slinkd[1] == slinkd[2].prev);
   TEST(&slinkd[3] == slinkd[2].next);
   TEST(&slinkd[2] == slinkd[3].prev);
   TEST(&slinkd[1] == slinkd[3].next);

   // TEST unlink_synclinkd: self connected node
   initself_synclinkd(&slinkd[0]);
   unlink_synclinkd(&slinkd[0]);
   TEST(0 == slinkd[0].prev);
   TEST(0 == slinkd[0].next);

   // TEST unlink_synclinkd: 2 nodes connected
   init_synclinkd(&slinkd[0], &slinkd[2]);
   unlink_synclinkd(&slinkd[0]);
   TEST(0 == slinkd[2].prev);
   TEST(0 == slinkd[2].next);
   TEST(&slinkd[2] == slinkd[0].prev); // not changed
   TEST(&slinkd[2] == slinkd[0].next); // not changed

   // TEST unlink_synclinkd: 3 nodes connected
   init_synclinkd(&slinkd[0], &slinkd[1]);
   initprev_synclinkd(&slinkd[2], &slinkd[0]);
   unlink_synclinkd(&slinkd[0]);
   TEST(&slinkd[2] == slinkd[1].prev);
   TEST(&slinkd[2] == slinkd[1].next);
   TEST(&slinkd[1] == slinkd[2].prev);
   TEST(&slinkd[1] == slinkd[2].next);
   TEST(&slinkd[2] == slinkd[0].prev); // not changed
   TEST(&slinkd[1] == slinkd[0].next); // not changed

   // TEST unlinkkeepself_synclinkd: self connected node
   initself_synclinkd(&slinkd[0]);
   unlinkkeepself_synclinkd(&slinkd[0]);
   TEST(&slinkd[0] == slinkd[0].prev); // links to self !!
   TEST(&slinkd[0] == slinkd[0].next);

   // TEST unlinkkeepself_synclinkd: 2 nodes connected
   init_synclinkd(&slinkd[0], &slinkd[2]);
   unlinkkeepself_synclinkd(&slinkd[0]);
   TEST(&slinkd[2] == slinkd[2].prev); // links to self !!
   TEST(&slinkd[2] == slinkd[2].next);
   TEST(&slinkd[2] == slinkd[0].prev); // not changed
   TEST(&slinkd[2] == slinkd[0].next); // not changed

   // TEST unlinkkeepself_synclinkd: 3 nodes connected
   init_synclinkd(&slinkd[0], &slinkd[1]);
   initprev_synclinkd(&slinkd[2], &slinkd[0]);
   unlinkkeepself_synclinkd(&slinkd[0]);
   TEST(&slinkd[2] == slinkd[1].prev);
   TEST(&slinkd[2] == slinkd[1].next);
   TEST(&slinkd[1] == slinkd[2].prev);
   TEST(&slinkd[1] == slinkd[2].next);
   TEST(&slinkd[2] == slinkd[0].prev); // not changed
   TEST(&slinkd[1] == slinkd[0].next); // not changed

   // TEST spliceprev_synclinkd: self connected nodes
   initself_synclinkd(&slinkd[0]);
   initself_synclinkd(&slinkd[1]);
   spliceprev_synclinkd(&slinkd[0], &slinkd[1]);
   TEST(&slinkd[1] == slinkd[0].prev);
   TEST(&slinkd[1] == slinkd[0].next);
   TEST(&slinkd[0] == slinkd[1].prev);
   TEST(&slinkd[0] == slinkd[1].next);

   // TEST spliceprev_synclinkd: self connected node + list and vice versa
   for (int isswitch = 0; isswitch <= 1; ++isswitch) {
      initself_synclinkd(&slinkd[0]);
      init_synclinkd(&slinkd[1], &slinkd[2]);
      spliceprev_synclinkd(&slinkd[isswitch], &slinkd[1-isswitch]);
      TEST(&slinkd[2] == slinkd[0].prev);
      TEST(&slinkd[1] == slinkd[0].next);
      TEST(&slinkd[0] == slinkd[1].prev);
      TEST(&slinkd[2] == slinkd[1].next);
      TEST(&slinkd[1] == slinkd[2].prev);
      TEST(&slinkd[0] == slinkd[2].next);
   }

   // TEST spliceprev_synclinkd: each list has 3 nodes
   init_synclinkd(&slinkd[0], &slinkd[1]);
   initnext_synclinkd(&slinkd[2], &slinkd[1]);
   init_synclinkd(&slinkd[3], &slinkd[4]);
   initnext_synclinkd(&slinkd[5], &slinkd[4]);
   spliceprev_synclinkd(&slinkd[0], &slinkd[3]);
   for (int i = 0; i < 6; ++i) {
      int n = (i+1) % 6;
      int p = (i+5) % 6;
      TEST(&slinkd[p] == slinkd[i].prev);
      TEST(&slinkd[n] == slinkd[i].next);
   }

   return 0;
ONERR:
   return EINVAL;
}

int unittest_task_synclink()
{
   if (test_initfree())       goto ONERR;
   if (test_query())          goto ONERR;
   if (test_update())         goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
