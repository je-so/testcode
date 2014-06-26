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

// forward
struct syncrunner_t;
struct syncwait_condition_t;

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
 * syncfunc_cmd_RUN         - In: Receiving this command means execution should start from the beginning
 *                            or at some fixed place.
 *                            Out: Returning this value means that <syncfunc_t.contlabel> is invalid and
 *                            at the next invocation of this sync function <syncfunc_cmd_RUN> is given as command.
 * syncfunc_cmd_CONTINUE    - In: Receiving this command means the execution should continue at <syncfunc_t.contlabel>.
 *                            Out: Returning this value means that the value set in <syncfunc_t.contlabel> is valid
 *                            and the execution should continue at contlabel the next time this function is called.
 * syncfunc_cmd_EXIT        - In: Receiving this command means an outside error occurred which made it impossible
 *                            to continue. This function should free all resources and return. Any returned value
 *                            is interpreted as <syncfunc_cmd_EXIT>. The value set in <syncfunc_param_t.retcode>
 *                            is used as return value.
 *                            Out: Returning this value indicates that execution has ended with or without an error.
 *                            The value set in <syncfunc_param_t.retcode> is used as return value and a value != 0 indicates
 *                            an error. Any execution context for this function is removed from <syncrunner_t>.
 *                            The function is never called again and if there is a function waiting for this function to exit
 *                            it is woken up.
 * syncfunc_cmd_WAIT       -  In: This command is never recieved.
 *                            Out: Returning this value indicates that execution should halt until a condition is met.
 *                            The value <syncfunc_param_t.contlabel> must be set to indicate where execution should
 *                            continue. Also the <syncfunc_param_t.condition> must be set to the address of the condition
 *                            for which this function should be waiting.
 *                            If <syncfunc_param_t.condition> is set to 0 the implicit condition is to wait for
 *                            exit of the last created <syncfunc_t>.
 *                            If the condition is true the function is woken up with command <syncfunc_cmd_CONTINUE>
 *                            and <syncfunc_param_t.waiterr> set to 0. In case condition was 0 <syncfunc_param_t.retcode>
 *                            carries the return code of the exited function.
 *                            If an out of memory error occurred and the function could not be added to the wait list
 *                            of the condition variable the variable <syncfunc_param_t.waiterr> is set to ENOMEM or
 *                            another error value > 0.
 * */
enum syncfunc_cmd_e {
   syncfunc_cmd_RUN,
   syncfunc_cmd_CONTINUE,
   syncfunc_cmd_EXIT
} ;

typedef enum syncfunc_cmd_e syncfunc_cmd_e;

/* enums: syncfunc_opt_e
 * Bitfield which encodes which optional fields in <syncfunc_t> are valid. */
enum syncfunc_opt_e {
   syncfunc_opt_NONE      = 0,
   syncfunc_opt_STATE     = 1,
   syncfunc_opt_CONTLABEL = 2,
   syncfunc_opt_ALL       = 3
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
    * This value serves as inout parameter;
    * The value stored after return is provided as input to the next invocation of the function. */
   void *      state;
   /* variable: contlabel
    * This value serves as inout parameter.
    * The value is only valid on function entry if parameter sfcmd is set to <syncfunc_cmd_CONTINUE>.
    * The value stored after return is only used if the return value of the function is <syncfunc_cmd_CONTINUE>
    * or <syncfunc_cmd_WAIT>. */
   void *      contlabel;
   /* variable: retcode
    * The value is the return value of the function or retcode of waited for function.
    * It serves as an inout parameter.
    * It is only used as out value if the return value of the function is <syncfunc_cmd_EXIT>.
    * The in value is only valid if the last execution of the function returned with <syncfunc_cmd_WAIT>
    * and <condition> was set to 0 (waiting for exit of a function). */
   int         retcode;
   /* variable: condition
    * The value is the condition the function wants to wait for.
    * This value serves as out parameter.
    * It is only used if the returned value of the function is <syncfunc_cmd_WAIT>. */
   struct syncwait_condition_t
             * condition;
   /* variable: waiterr
    * The value is the result of a wait operation.
    * This value serves as in parameter.
    * It is only valid if the last execution of the function returned with <syncfunc_cmd_WAIT>.
    * A value != 0 indicates that the wait operation failed and waiterr is the error code. */
    int        waiterr;
};

// group: lifetime

/* define: syncfunc_param_FREE
 * Static initializer. */
#define syncfunc_param_FREE \
         { 0, 0, 0, 0, 0, 0 }


/* struct: syncfunc_t
 * Describes a cooperative synchronous function context.
 * The meaning of synchronous is that all functions managed
 * by a single <syncrunner_t> are guaranteed run synchronously
 * one after the other. So there is no need to synchronize access
 * to data structures with locks (see <mutex_t>).
 *
 * Optional Data Fields:
 * The variables <state>, and <contlabel> are optional.
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
};

// group: lifetime

/* define: syncfunc_FREE
 * Static initializer. */
#define syncfunc_FREE \
         { 0, 0, 0 }

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

/* function: optstate_syncfunc
 * Returns value of optional <state> field or 0.
 * The bitfield optfields determines which fields are available. */
void * optstate_syncfunc(const syncfunc_t * sfunc, const syncfunc_opt_e optfields);

/* function: optcontlabel_syncfunc
 * Returns value of optional <contlabel> field or 0.
 * The bitfield optfields determines which fields are available. */
void * optcontlabel_syncfunc(const syncfunc_t * sfunc, const syncfunc_opt_e optfields);

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
void setall_syncfunc(syncfunc_t * sfunc, const syncfunc_opt_e optfields, syncfunc_f mainfct, void * state, void * contlabel);

// group: implementation-support
// Macros which makes implementing a <syncfunc_f> possible.

/* function: execmd_syncfunc
 * Jumps to onrun, or to onexit, or continues execution where the previous execution left off.
 * The first and second parameter should be same as in <syncfunc_f>.
 * The behaviour depends on value of sfcmd. The first parameter is only used if sfcmd equals <syncfunc_cmd_CONTINUE>.
 *
 * Command Values:
 * syncfunc_cmd_RUN         - The execution continues at label onrun.
 *                            This should be the normal running mode.
 * syncfunc_cmd_CONTINUE    - The execution continues at a place remembered during
 *                            exit of the function's last execution cycle.
 * syncfunc_cmd_EXIT        - The execution continues at label onexit.
 *                            The code at this label should try to free all resources and call <exit_syncfunc>.
 *                            Or set the return code in <syncfunc_param_t.retcode> and return <syncfunc_cmd_EXIT>.
 * All other values         - The function does nothing and execution continues after it.
 * */
void execmd_syncfunc(const syncfunc_param_t * sfparam, uint32_t sfcmd, LABEL onrun, IDNAME onexit);

/* function: yield_syncfunc
 * Yields processor to other <syncfunc_t> in the same thread.
 * Execution continues after yield only if <execmd_syncfunc> is called as first function
 * in the current sync function. */
void yield_syncfunc(const syncfunc_param_t * sfparam);

/* function: exit_syncfunc
 * Exits this function and returns retcode as return code.
 * Convention: A retcode of 0 means OK, values > 0 correspond to system errors and application errors. */
void exit_syncfunc(const syncfunc_param_t * sfparam, int retcode);

/* function: wait_syncfunc
 * Waits until condition is true.
 * A return value of 0 indicates success.
 * The return value ENOMEM (or any other value != 0) means that
 * not enough resources were available to add the caller to the
 * wait list if condition. */
int wait_syncfunc(const syncfunc_param_t * sfparam, struct syncwait_condition_t * condition);

/* function: waitexit_syncfunc
 * Wait until the last created sync function exits.
 * The out param retcode is set to the return code of the function (see <exit_syncfunc>).
 * It is also set in case waitexit_syncfunc does not succeed.
 * This function returns 0 in case of success.
 * It returns value ENOMEM (or any other value != 0) to state that there were
 * not enough resources to add the caller to the wait list of the last created
 * sync function (see <syncrunner_t>). */
int waitexit_syncfunc(const syncfunc_param_t * sfparam, /*out;err*/int * retcode);



// section: inline implementation

/* define: execmd_syncfunc
 * Implements <syncfunc_t.execmd_syncfunc>. */
#define execmd_syncfunc(sfparam, sfcmd, onrun, onexit) \
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
         default: /*ignoring all other*/  \
            break;                        \
         }}))

/* define: exit_syncfunc
 * Implements <syncfunc_t.exit_syncfunc>. */
#define exit_syncfunc(sfparam, _rc) \
         {                                \
            (sfparam)->retcode = (_rc);   \
            return syncfunc_cmd_EXIT;     \
         }

/* define: getsize_syncfunc
 * Implements <syncfunc_t.getsize_syncfunc>. */
#define getsize_syncfunc(optfields) \
         ( __extension__ ({                     \
            syncfunc_opt_e _opt = (optfields);  \
            sizeof(syncfunc_f)                  \
            + ((_opt & syncfunc_opt_STATE)      \
              ? sizeof(void*) : 0)              \
            + ((_opt & syncfunc_opt_CONTLABEL)  \
              ? sizeof(void*) : 0);             \
         }))


/* define: getstate_syncfunc
 * Implements <syncfunc_t.getstate_syncfunc>. */
#define getstate_syncfunc(sfparam) \
         ((sfparam)->state)

/* define: optcontlabel_syncfunc
 * Implements <syncfunc_t.optcontlabel_syncfunc>. */
#define optcontlabel_syncfunc(sfunc, optfields) \
         ( __extension__ ({                     \
            void ** _sf = &(sfunc)->state;      \
            syncfunc_opt_e _opt = (optfields);  \
            if (_opt & syncfunc_opt_STATE) {    \
               ++ _sf;                          \
            }                                   \
            (_opt & syncfunc_opt_CONTLABEL)     \
            ? *_sf : (void*)0;                  \
         }))

/* define: optstate_syncfunc
 * Implements <syncfunc_t.optstate_syncfunc>. */
#define optstate_syncfunc(sfunc, optfields) \
         ( __extension__ ({                     \
            void ** _sf = &(sfunc)->state;      \
            syncfunc_opt_e _opt = (optfields);  \
            (_opt & syncfunc_opt_STATE)         \
            ? *_sf : (void*)0;                  \
         }))

/* define: setall_syncfunc
 * Implements <syncfunc_t.setall_syncfunc>. */
#define setall_syncfunc(sfunc, optfields, _mainfct, _state, _contlabel) \
         do {                                   \
            syncfunc_t * _sf0 = (sfunc);        \
            void ** _sf = (void**)_sf0;         \
            syncfunc_opt_e _opt = (optfields);  \
            _sf0->mainfct = _mainfct;           \
            _sf = (void**)                      \
                  &((syncfunc_t*)_sf)->state;   \
            if (_opt & syncfunc_opt_STATE) {    \
               *_sf = _state;                   \
               ++_sf;                           \
            }                                   \
            if (_opt & syncfunc_opt_CONTLABEL){ \
               *_sf = _contlabel;               \
            }                                   \
         } while(0)

/* define: setstate_syncfunc
 * Implements <syncfunc_t.setstate_syncfunc>. */
#define setstate_syncfunc(sfparam, new_state) \
         do { (sfparam)->state = (new_state) ; } while(0)

/* define: wait_syncfunc
 * Implements <syncfunc_t.wait_syncfunc>. */
#define wait_syncfunc(sfparam, _condition) \
         ( __extension__ ({                                 \
            __label__ continue_after_wait;                  \
            (sfparam)->condition = _condition;              \
            (sfparam)->contlabel = &&continue_after_wait;   \
            return syncfunc_cmd_WAIT;                       \
            continue_after_wait: ;                          \
            (sfparam)->waiterr;                             \
         }))

/* define: waitexit_syncfunc
 * Implements <syncfunc_t.waitexit_syncfunc>. */
#define waitexit_syncfunc(sfparam, _retcode) \
         ( __extension__ ({                                 \
            __label__ continue_after_wait;                  \
            (sfparam)->condition = 0;                       \
            (sfparam)->contlabel = &&continue_after_wait;   \
            return syncfunc_cmd_WAIT;                       \
            continue_after_wait: ;                          \
            *(_retcode) = (sfparam)->retcode;               \
            (sfparam)->waiterr;                             \
         }))

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
