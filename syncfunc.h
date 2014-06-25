/* title: SyncFunc

   A function execution context for calling functions
   with signature <syncfunc_f> in a cooperative way.

   In the simplest case a <syncfunc_t> is a single pointer <syncfunc_f>.

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
#ifndef CKERN_TASK_SYNCFUNC_HEADER
#define CKERN_TASK_SYNCFUNC_HEADER

#include "C-kern/api/task/syncwait_node.h"

// forward
struct syncrunner_t;

/* typedef: struct syncfunc_t
 * Export <syncfunc_t> into global namespace. */
typedef struct syncfunc_t syncfunc_t;

/* typedef: struct syncfunc_param_t
 * Export <syncfunc_param_t> into global namespace. */
typedef struct syncfunc_param_t syncfunc_param_t;

/* typedef: syncfunc_f
 * Defines signature of repeatedly executed cooperative function implementation.
 * All function pointers pertaining to the same <syncrunner_t> are executed in
 * a synchronous and cooperative way.
 *
 * Parameter:
 * sfparam  - Points to in and out parameter.
 *            Certain values are only valid on entry if sfcmd has a certain value.
 *            See <syncfunc_param_t> for a description of all possible in and out parameter.
 * sfcmd    - A value from <syncfunc_cmd_e> which describes the action the function
 *            has to undertake.
 *
 * Return:
 * The return value of the function is a value from <syncfunc_cmd_e>. */
typedef int (* syncfunc_f) (syncfunc_param_t * sfparam, uint32_t sfcmd);

/* enums: syncfunc_cmd_e
 * Describes the meaning of the sfcmd (sync-func-command) parameter.
 * It is expected as second parameter sfcmd in sync functions with signature <syncfunc_f>.
 *
 * syncfunc_cmd_RUN         - The execution should start from the beginning or at some fixed place.
 * syncfunc_cmd_CONTINUE    - The execution should continue at <syncfunc_t.contlabel>.
 * syncfunc_cmd_EXIT        - Receiving this command means that the function should free all resources and
 *                            return <syncfunc_cmd_EXIT> to indicate end of execution.
 *                            The value returned in <syncfunc_param_t.retcode> is used as return value of the function.
 *                            If <syncfunc_cmd_TERMINATE> is returned the function indicates that it
 *                            has not completed but the value in <syncfunc_param_t.retcode> is also used as return code.
 *                            Any other returned value is interpreted as <syncfunc_cmd_EXIT>.
 * syncfunc_cmd_TERMINATE   - Receiving this command means either an outside error occurred which made it impossible
 *                            to continue or another function requested to terminate this function. Either way
 *                            the called function should free all resources and return <syncfunc_cmd_TERMINATE> or
 *                            <syncfunc_cmd_EXIT>. Any other returned value is interpreted as <syncfunc_cmd_TERMINATE>.
 *                            The value stored in <syncfunc_param_t.retcode> is used as return value of the function.
 * */
enum syncfunc_cmd_e {
   syncfunc_cmd_RUN,
   syncfunc_cmd_CONTINUE,
   syncfunc_cmd_EXIT,
   syncfunc_cmd_TERMINATE
} ;

typedef enum syncfunc_cmd_e syncfunc_cmd_e;

/* enums: syncfunc_opt_e
 * Bitfield which encodes which optional fields in <syncfunc_t> are valid. */
enum syncfunc_opt_e {
   syncfunc_opt_NONE      = 0,
   syncfunc_opt_STATE     = 1,
   syncfunc_opt_CONTLABEL = 2,
   syncfunc_opt_CALLER    = 4
};

typedef enum syncfunc_opt_e syncfunc_opt_e;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_task_syncfunc
 * Test <syncfunc_t> functionality. */
int unittest_task_syncfunc(void);
#endif


/* struct: syncfunc_param_t
 * First parameter of <syncfunc_f>. */
struct syncfunc_param_t {
   /* variable: syncrun
    * Context of the called function.
    * This value serves only as in parameter. */
   struct syncrunner_t * syncrun;
   /* variable: state
    * Value of <syncfunc_t.state>.
    * This value serves as inout parameter;
    * the value stored after return is provided as input to the next invocation of the function.
    * In first entry this value is 0 - except if during creation of a new <syncfunc_t> a value
    * has been set - see <syncrunner_t>. */
   void *      state;
   /* variable: contlabel
    * Value of <syncfunc_t.contlabel>.
    * This value serves as inout parameter.
    * The value is only valid on function entry if parameter sfcmd is set to <syncfunc_cmd_CONTINUE>.
    * The value stored after return is only used if the return value of the function is <syncfunc_cmd_CONTINUE>. */
   void *      contlabel;
   /* variable: retcode
    * The value serves as return value of the function.
    * This value is an out parameter.
    * It is only used if the returned value of the function is either <syncfunc_cmd_EXIT> or <syncfunc_cmd_TERMINATE>. */
   int         retcode;
};

// group: lifetime

/* define: syncfunc_param_FREE
 * Static initializer. */
#define syncfunc_param_FREE \
         { 0, 0, 0, 0 }


/* struct: syncfunc_t
 * Describes a cooperative synchronous function context.
 * The meaning of synchronous is that all functions managed
 * by a single <syncrunner_t> are guaranteed run synchronously
 * one after the other. So there is no need to synchronize access
 * to data structures with locks (see <mutex_t>).
 *
 * Optional Data Fields:
 * The variables <state>, <contlabel>, and <caller> are all optional.
 *
 * In the simplest case a function is described with only <mainfct>.
 * Fields which contains the value 0 are considered invalid.
 *
 * To describe which optional fields are valid either value 0 could be used
 * to mark a value as invalid or an additional bitfield could be used
 * to encode the available optional fields.
 *
 * Another very simple possibility is to use a complex of 8 run queues and 8 wait queues
 * (see <syncwait_func_t>) to store every possible combination of valid optional
 * fields. This would safe memory if a very large number of functions is used and
 * would make handling simple. But it wastes memory in case of a very a small
 * number of functions if the queues are implemented with linked pages with
 * a very large pagesize.
 *
 * See <syncrunner_t> for this implementation strategy. */
struct syncfunc_t {
   // group: private fields
   /* variable: mainfct
    * Function which has to be executed repeatedly. */
   syncfunc_f  mainfct;
   /* variable: state
    * Points to memory which carries the inout parameter values and the functions state.
    * Memory must be managed by <mainfct>. */
   void *      state;   // OPTIONAL
   /* variable: contlabel
    * GCC extension which describes address of a label.
    * See https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html.
    * With
    * > goto * contlabel;
    * the execution continues at the last place where execution
    * left off cause of waiting to some condition. The execution of
    * the goto must be handled at function entry and is wrapped in
    * a convenience macro.
    *
    * With another GCC extension "Locally Declared Labels"
    * The writing of macros to wait for some condition is very simple.
    * > int test_syncfunc(syncfunc_param_t * sfparam, uint32_t sfcmd)
    * > {
    * >    ...
    * >    {  __label__ continue_after_wait;
    * >       sfparam->contlabel = &&continue_after_wait;
    * >       // yield processor to other cooperative functions.
    * >       return syncfunc_cmd_CONTINUE;
    * >       // execution continues after here
    * >       continue_after_wait: ;
    * >    }
    * >    ...
    * > } */
   void *      contlabel; // OPTIONAL
   /* variable: caller
    * Points to the caller of this function which waits for the end of execution.
    * This double linked allows to build a chain of functions waiting for end of
    * a called function (call stack). The double linked list allows to move a waiting
    * function in memory (compact memory) and to adapt other nodes in the chain. */
   syncwait_node_t   caller; // OPTIONAL
};

// group: lifetime

/* define: syncfunc_FREE
 * Static initializer. */
#define syncfunc_FREE \
         { 0, 0, 0, syncwait_node_FREE }

// group: query

/* function: getstate_syncfunc
 * Gets state of current <syncfunc_t>.
 * The first parameter is of type <syncfunc_param_t> which
 * carries a copy of the state value. This tweak makes it
 * possible to provide additional information to a <syncfunc_f>
 * which is not stored in <syncfunc_t>. */
void * getstate_syncfunc(const syncfunc_param_t * sfparam);

/* function: getsize_syncfunc
 * Returns the size in bytes of a <syncfunc_t> with optional fields encoded in optfields.
 * See also <syncfunc_opt_e>. */
size_t getsize_syncfunc(const syncfunc_opt_e optfields);

// group: update

/* function: setstate_syncfunc
 * Sets state of current <syncfunc_t> to new_state.
 * The first parameter is of type <syncfunc_param_t> which
 * carries a copy of the state value. This tweak makes it
 * possible to provide additional information to a <syncfunc_f>
 * which is not stored in <syncfunc_t>. */
void setstate_syncfunc(syncfunc_param_t * sfparam, void * new_state);

/* function: setall_syncfunc
 * Sets alle fields in <syncfunc_t> to the provided values.
 * The parameter optfields encodes which optional fields are available.
 * Unavailable fields are ignored. */
void setall_syncfunc(syncfunc_t * sfunc, const syncfunc_opt_e optfields, syncfunc_f mainfct, void * state, void * contlabel, const syncwait_node_t caller);

// group: implementation-support
// Macros which makes implementing a <syncfunc_f> possible.

/* function: execmd_syncfunc
 * Jumps to onrun, or to onexit, to onterminate, or continues execution where the previous execution left off.
 * The first and second parameter should be same as in <syncfunc_f>.
 * The behaviour depends on value of sfcmd. The first parameter is only used if sfcmd equals <syncfunc_cmd_CONTINUE>.
 *
 * Command Values:
 * syncfunc_cmd_RUN         - The execution continues at label onrun.
 *                            This should be the normal running mode.
 * syncfunc_cmd_CONTINUE    - The execution continues at a place remembered during
 *                            exit of the function's last execution cycle.
 * syncfunc_cmd_EXIT        - The execution continues at label onexit.
 *                            The code at label onexit should try to end execution, free all resources,
 *                            and return the value <syncfunc_cmd_EXIT> or <syncfunc_cmd_TERMINATE>.
 *                            The returned value <syncfunc_cmd_EXIT> indicates that execution has been completed.
 * syncfunc_cmd_TERMINATE   - The execution continues at label onterminate.
 *                            The code at label onterminate should try to free all resources, abort any transactions
 *                            and return the value <syncfunc_cmd_TERMINATE> or <syncfunc_cmd_EXIT>.
 *                            The returned value <syncfunc_cmd_EXIT> indicates that execution has been completed.
 * All other values         - The function does nothing and execution continues after it.
 * */
void execmd_syncfunc(const syncfunc_param_t * sfparam, uint32_t sfcmd, LABEL onrun, LABEL onexit, IDNAME onterminate);

/* function: yield_syncfunc
 * Yields processor to other <syncfunc_t> in the same thread.
 * Execution continues after yield only if <execmd_syncfunc> is called as first function
 * in the current sync function. */
void yield_syncfunc(const syncfunc_param_t * sfparam);

/* function: exit_syncfunc
 * Exits this function and returns retcode as return code.
 * The caller assumes that computation ended normally.
 * Convention: A retcode of 0 means OK, values > 0 correspond to system errors and application errors. */
void exit_syncfunc(const syncfunc_param_t * sfparam, int retcode);

/* function: terminate_syncfunc
 * Terminates this function and returns retcode as return code.
 * The caller assumes that computation terminated before
 * execution could produce the desired result.
 * Convention: A retcode of 0 means that the function received command <syncfunc_cmd_TERMINATE>,
 * a retcode != 0 means the function terminated itself cause of the error returned in retcode.
 * Values > 0 correspond to system and application errors. */
void terminate_syncfunc(const syncfunc_param_t * sfparam, int retcode);



// section: inline implementation

/* define: execmd_syncfunc
 * Implements <syncfunc_t.execmd_syncfunc>. */
#define execmd_syncfunc(sfparam, sfcmd, onrun, onexit, onterminate) \
         ( __extension__ ({               \
         switch ( (syncfunc_cmd_e)        \
                  (sfcmd)) {              \
         case syncfunc_cmd_RUN:           \
            (void) (sfparam);             \
            goto onrun;                   \
         case syncfunc_cmd_CONTINUE:      \
            goto * (sfparam)->contlabel;  \
         case syncfunc_cmd_EXIT:          \
            (void) (sfparam);             \
            goto onexit;                  \
         case syncfunc_cmd_TERMINATE:     \
            (void) (sfparam);             \
            goto onterminate;             \
         }}))

/* define: exit_syncfunc
 * Implements <syncfunc_t.exit_syncfunc>. */
#define exit_syncfunc(sfparam, _rc) \
         {                             \
            (sfparam)->retcode = (_rc);  \
            return syncfunc_cmd_EXIT;  \
         }

/* define: getsize_syncfunc
 * Implements <syncfunc_t.getsize_syncfunc>. */
#define getsize_syncfunc(optfields) \
         (  sizeof(syncfunc_f)                  \
            + ((_opt & syncfunc_opt_STATE)      \
              ? sizeof(void*) : 0);             \
            + ((_opt & syncfunc_opt_CONTLABEL)  \
              ? sizeof(void*) : 0);             \
            + ((_opt & syncfunc_opt_CALLER)     \
              ? sizeof(syncwait_node_t) : 0)    \
         )


/* define: getstate_syncfunc
 * Implements <syncfunc_t.getstate_syncfunc>. */
#define getstate_syncfunc(sfparam) \
         ((sfparam)->state)

/* define: setall_syncfunc
 * Implements <syncfunc_t.setall_syncfunc>. */
#define setall_syncfunc(sfunc, optfields, mainfct, state, contlabel, caller) \
         do {                                   \
            void * _sf = (sfunc);               \
            syncfunc_opt_e _opt = (optfields);  \
            *(syncfunc_f**)_sf = mainfct;       \
            _sf = (uint8_t*)_sf                 \
                + sizeof(syncfunc_f);           \
            if (_opt & syncfunc_opt_STATE) {    \
               *(void**)_sf = state;            \
               _sf = (uint8_t*)_sf              \
                   + sizeof(void*);             \
            }                                   \
            if (_opt & syncfunc_opt_CONTLABEL){ \
               *(void**)_sf = contlabel;        \
               _sf = (uint8_t*)_sf              \
                   + sizeof(void*);             \
            }                                   \
            if (_opt & syncfunc_opt_CALLER) {   \
               *(syncwait_node_t*)_sf = caller; \
            }                                   \
         } while(0)

/* define: setstate_syncfunc
 * Implements <syncfunc_t.setstate_syncfunc>. */
#define setstate_syncfunc(sfparam, new_state) \
         do { (sfparam)->state = (new_state) ; } while(0)

/* define: terminate_syncfunc
 * Implements <syncfunc_t.terminate_syncfunc>. */
#define terminate_syncfunc(sfparam, _rc) \
         {                                   \
            (sfparam)->retcode = (_rc);      \
            return syncfunc_cmd_TERMINATE;   \
         }

/* define: yield_syncfunc
 * Implements <syncfunc_t.yield_syncfunc>. */
#define yield_syncfunc(sfparam) \
         ( __extension__ ({                                 \
            __label__ continue_after_yield;                 \
            (sfparam)->contlabel = &&continue_after_yield;  \
            return syncfunc_cmd_CONTINUE;                   \
            continue_after_yield: ;                         \
         }))

#endif
