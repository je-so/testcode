/* title: Test-Mailbox impl

   Implementation of interface <Test-Mailbox>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2016 Jörg Seebohn

   file: mailbox.h
    Header file <Test-Mailbox>.

   file: mailbox.c
    Implementation file <Test-Mailbox impl>.
*/
#include <stdint.h>
#include "mailbox.h"

#define _STR2(str)   #str
#define _STR(str)    _STR2(str)

__attribute__((naked))
int send_mailbox(mailbox_t *mbox, uint32_t value)
{
   __asm volatile(
      "movs    r2, #2\n"            // r2 = 2;
      "1: ldrex   r3, [r0]\n"       // 1: r3 = mbox->state;
      "tst     r3, r3\n"            // if (mbox->state != 0) goto forward to label 3:
      "bne     3f\n"
      "strex   r3, r2, [r0]\n"      // try { mbox->state = 2; }
      "tst     r3, r3\n"
      "bne     1b\n"                // if (mbox->state!=2) goto back to label 1:
      "str     r1, [r0, #4]\n"      // mbox->value = value;
      "movs    r2, #1\n"            // r2 = 1;
      /* "dmb\n" */                 // needed in case of multi-core and cached RAM with no ordered-constraints
      "str     r2, [r0]\n"          // mbox->state = 1;
      "movs    r0, r3\n"            // r0 = 0
      "bx      lr\n"                // return 0
      "3:\n"                        // 3: ERROR RETURN
      "movs r0, #"_STR(ERRFULL)"\n" // r0 = ERRFULL
      "bx      lr\n"                // return ERRFULL
   );
}

__attribute__((naked))
uint32_t recv_mailbox(mailbox_t *mbox)
{
   __asm volatile(
      "movs    r2, #2\n"            // r2 = 2;
      "1: ldrex   r3, [r0]\n"       // 1: r3 = mbox->state;
      "cmp     r3, #1\n"            // if (mbox->state != 1) goto backward to label 1:
      "bne     1b\n"
      "strex   r3, r2, [r0]\n"      // try { mbox->state = 2; }
      "tst     r3, r3\n"
      "bne     1b\n"                // if (mbox->state!=2) goto back to label 1:
      "ldr     r2, [r0, #4]\n"      // r2 = mbox->value;
      "str     r3, [r0]\n"          // mbox->state = 0
      "movs    r0, r2\n"            // r0 = mbox->value;
      "bx      lr\n"                // return mbox->value;
   );
}

#undef _STR2
#undef _STR

