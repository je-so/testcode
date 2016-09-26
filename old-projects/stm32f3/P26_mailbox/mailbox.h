/* title: Test-Mailbox

   Implements simple data structure for communicating
   a single value of type uint32_t between two threads.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: mailbox.h
    Header file <Test-Mailbox>.

   file: mailbox.c
    Implementation file <Test-Mailbox impl>.
*/
#ifndef CORTEXM4_MAILBOX_HEADER
#define CORTEXM4_MAILBOX_HEADER

// == exported constants

#ifndef NOERROR
#define NOERROR   0
#endif

#ifndef ERRFULL
#define ERRFULL   1024
#endif

// == exported objects

typedef struct mailbox_t {
   uint32_t state;   // 0: invalid. 1: valid. 2: locked.
   uint32_t value;   // written value only valid if (state == 1)
} mailbox_t;

// == lifetime
#define mailbox_INIT { 0, 0 }

// == send / receive
int      send_mailbox(mailbox_t *mbox, uint32_t value);  // returns 0: mbox->value set to value. ERRFULL: mbox->value already set and not read since last call.
uint32_t recv_mailbox(mailbox_t *mbox);                  // returns mbox->value. Waits in a spinning loop until mbox->value is set.


#endif
