/* title: Test-Errortimer
   Implements simple count down until error code is returned.
   All exported functions are implemented inline.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2011 Jörg Seebohn

   file: C-kern/api/test/errortimer.h
    Header file of <Test-Errortimer>.
*/
#ifndef CKERN_TEST_ERRORTIMER_HEADER
#define CKERN_TEST_ERRORTIMER_HEADER

// === exported types
struct test_errortimer_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_test_errortimer
 * Unittest for <test_errortimer_t>. */
int unittest_test_errortimer(void);
#endif

/* struct: test_errortimer_t
 * Holds a timer value and an error code.
 * The function <process_testerrortimer> returns
 * a stored error code if timercount has reached zero. */
typedef struct test_errortimer_t {
   /* variable: timercount
    * The number of times <process_testerrortimer> returns success. */
   uint32_t    timercount;
   /* variable: errcode
    * The error code which is returned by <process_testerrortimer>. */
   int         errcode;
} test_errortimer_t;

// group: lifetime

/* define: test_errortimer_FREE
 * Static initializer. Initializes timer disarmed. */
#define test_errortimer_FREE { 0, 0 }

/* function: init_testerrortimer
 * Inits errtimer with timercount and errcode.
 *
 * Parameter:
 * errtimer    - Pointer to object of type <test_errortimer_t> which is initialized.
 * timercount  - The number of times after <process_testerrortimer> returns an error.
 *               A value of 0 disables the timer.
 * errcode     - The errorcode <process_testerrortimer> returns in timer has fired.
 * */
void init_testerrortimer(/*out*/test_errortimer_t* errtimer, uint32_t timercount, int errcode);

/* function: free_testerrortimer
 * Sets errtimer to <test_errortimer_FREE>. */
void free_testerrortimer(test_errortimer_t* errtimer);

// group: query

/* function: isenabled_testerrortimer
 * Returns true if timer has not fired. */
bool isenabled_testerrortimer(const test_errortimer_t* errtimer);

/* function: errcode_testerrortimer
 * Returns the error code of the timer.
 * The returned value is the one set with <init_testerrortimer>
 * independent if the timer is enabled or not. */
int errcode_testerrortimer(const test_errortimer_t* errtimer);

// group: update

/* function: process_testerrortimer
 * Returns stored error code if timer has elapsed else 0.
 * If <test_errortimer_t.timercount> is 0 nothing is done and 0 is returned
 * else timercount is decremented. If timercount is zero after the decrement
 * <test_errortimer_t.errcode> is returned else 0. The value err is only set to
 * the returned value if the timer has expired. */
int process_testerrortimer(test_errortimer_t * errtimer, /*err*/int* err);

/* function: PROCESS_testerrortimer
 * This function calls <process_testerrortimer> and returns its value.
 * This function calls <NOOP_testerrortimer> if KONFIG_UNITTEST is not defined. */
int PROCESS_testerrortimer(test_errortimer_t * errtimer, /*err*/int* err);

/* function: NOOP_testerrortimer
 * This function returns always 0 and does nothing. */
int NOOP_testerrortimer(test_errortimer_t * errtimer, /*err*/int* err);


// section: inline implementation

/* define: errcode_testerrortimer
 * Implements <test_errortimer_t.errcode_testerrortimer>. */
#define errcode_testerrortimer(errtimer) \
         ((errtimer)->errcode)

/* define: free_testerrortimer
 * Implements <test_errortimer_t.free_testerrortimer>. */
#define free_testerrortimer(errtimer)  \
         ((void)(*(errtimer) = (test_errortimer_t) test_errortimer_FREE))

/* define: isenabled_testerrortimer
 * Implements <test_errortimer_t.isenabled_testerrortimer>. */
#define isenabled_testerrortimer(errtimer)   \
         ((errtimer)->timercount > 0)

/* define: init_testerrortimer
 * Implements <test_errortimer_t.init_testerrortimer>. */
#define init_testerrortimer(errtimer, timercount, errcode)  \
         ((void) (*(errtimer) = (test_errortimer_t){ timercount, errcode }))

/* define: process_testerrortimer
 * Implements <test_errortimer_t.process_testerrortimer>. */
#define process_testerrortimer(errtimer, err) \
         ( __extension__ ({                                    \
            test_errortimer_t* _tm = errtimer;                 \
            typeof(err) _err = err;                            \
            int _r = 0;                                        \
            if (_tm->timercount && 0 == --_tm->timercount) {   \
               _r = _tm->errcode;                              \
               *_err = _r;                                     \
            }                                                  \
            _r;                                                \
         }))

#define NOOP_testerrortimer(errtimer, err) \
         (0)

#ifdef KONFIG_UNITTEST

/* define: PROCESS_testerrortimer
 * Implements <test_errortimer_t.PROCESS_testerrortimer>. */
#define PROCESS_testerrortimer(errtimer, err) \
         (process_testerrortimer(errtimer, err))

#else

#define PROCESS_testerrortimer(errtimer, err) \
         NOOP_testerrortimer(errtimer, err)

#endif


#endif
