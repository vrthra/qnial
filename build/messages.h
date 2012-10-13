/*==============================================================

  MESSAGES.H: header for MESSAGES.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  The header file for error message routines

================================================================*/

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

/* We originally allowed for compiling messages.c with C++. This is not
   done so we can set C_COREAPI here. 
*/



#if      defined(__cplusplus)
extern      "C"
{
#endif


  extern char *NC_ErrorTypeName(int code);
  extern char *NC_ErrorTypeMessage(int code);
  extern char *NC_ErrorMessage(int);
  extern int  NC_ErrorType(int);

/* type of checksignal calls */
  enum {
    NC_CS_STARTUP, NC_CS_NORMAL, NC_CS_OUTPUT, NC_CS_INPUT
  };

/* types of messages */
  enum {
    NC_NORMAL = 1, NC_FATAL, NC_STARTUP_ERROR, NC_BREAK,
    NC_TERMINATE, NC_WARNING, NC_INTERNAL_ERROR, NC_BAD_ERROR_TYPE
  };

  typedef struct {
    int         error_type;
    char       *error_type_name;
  }           error_type_strings_type;

  extern error_type_strings_type error_type_strings[];



/* message codes */
  enum {
    NC_NO_ERR_N = 1,         /* cannot start at 0 because of use in setjmp */
    NC_ALREADY_RUNNING_W,
    NC_NOT_RUNNING_W,
    NC_ABORT_F,
    NC_MEM_ERR_F,
    NC_IO_BUFFER_MEM_ERR_F,
    NC_IO_BUFFER_OVERFLOW_W,
    NC_NPRINTF_BUFFER_OVERFLOW_I,
    NC_ATOM_TABLE_EXPAND_ERR_F,
    NC_USER_BREAK_B,
    NC_PROGRAM_BREAK_B,
    NC_MEM_EXPAND_FAILED_W,
    NC_MEM_EXPAND_NOT_ALLOWED_F,
    NC_MEM_EXPAND_NOT_TRIED_F,
    NC_INVALID_SELECTOR_ERR_I,
    NC_INVALID_VALUE_ERR_I,
    NC_INVALID_STRUCT_ERR_I,
    NC_RANGE_ERR_I,
    NC_WS_OPEN_FAILED_S,
    NC_WS_READ_ERR_S,        /* fatal */
    NC_WS_WRITE_ERR_W,       /* non-fatal */
    NC_RESERVED_OPEN_FAILED_S,
    NC_NH_OPEN_FAILED_S,
    NC_DEFS_LOAD_FAILED_S,
    NC_SOCKET_ERR_F,
    NC_BYE_T,
    NC_EOF_T,
    NC_FAULT_W,
    NC_WS_LOAD_ERR_F,
    NC_STACK_NOT_EMPTY_I,
    NC_WS_WRONG_VERSION_S,
    NC_USER_WS_WRONG_VERSION_W,
    NC_STACK_GROWN_I,
    NC_C_STACK_OVERFLOW_W,
    NC_PROFILE_MEM_W,
    NC_ERASERECORD_W,
    NC_FILEPTR_W,
    NC_PROFILE_FILE_W,
    NC_FLOAT_EXCEPTION_W,
    NC_INTEGER_OVERFLOW_APPEND_W,
    NC_INTEGER_OVERFLOW_HITCH_W,
    NC_INTEGER_OVERFLOW_CREATE_W,
    NC_INTEGER_OVERFLOW_CART_W,
    NC_INTEGER_OVERFLOW_LINK_W,
    NC_INTEGER_OVERFLOW_RESHAPE_W,
    NC_INTEGER_OVERFLOW_PICTURE_W,
    NC_INTEGER_OVERFLOW_TAKE_W,
    NC_MATH_LIB_DOMAIN_ERROR_W,
    NC_MATH_LIB_SING_ERROR_W,
    NC_MATH_LIB_OVERFLOW_ERROR_W,
    NC_MATH_LIB_UNDERFLOW_ERROR_W,
    NC_MATH_LIB_TLOSS_ERROR_W,
    NC_MATH_LIB_PLOSS_ERROR_W,
    NC_PARSE_STACK_OVERFLOW_W,
    NC_SYMTAB_ERROR_I,
    NC_CALL_STACK_MEM_W,
    NC_PROFILE_SYNCH_W,
    NC_STACK_EMPTY_W,
    NC_RANK_MEM_W,
    NC_DIRECT_ACCESS_WRITE_W,
    NC_LOG_OPEN_FAILED_W,
    NC_RECOVER_N,
    NC_LATENT_N,
    NC_CHECKPOINT_N,
    NC_RESTART_N,
    NC_CLEARWS_N,
    NC_ERROR_LOOP_F,
    NC_RECOVERY_LOOP_W,
    NC_MAX_ERROR_NUM,
    NC_BREAKLIST_ERR_W,
    NC_INVALID_ATOM_F,
    NC_CBUFFER_ALLOCATION_FAILED_W,
    NC_VALENCE_OVERFLOW_CREATE_W
  };

  typedef struct {
    int         error_code;
    int         error_type;
    char       *error_message;
  }           error_strings_type;

  extern error_strings_type error_strings[];


#define NC_IsFatal(code)        (NC_ErrorType(code) == NC_FATAL)
#define NC_IsWarning(code)      (NC_ErrorType(code) == NC_WARNING)
#define NC_IsStartupError(code) (NC_ErrorType(code) == NC_STARTUP_ERROR)
#define NC_IsBreak(code)        (NC_ErrorType(code) == NC_BREAK)
#define NC_IsInternal(code)     (NC_ErrorType(code) == NC_INTERNAL_ERROR)
#define NC_IsTerminate(code)    (NC_ErrorType(code) == NC_TERMINATE)
#define NC_IsNoError(code)      (NC_ErrorType(code) == NC_NORMAL)


/* These are the base values for the structure.  The values
   are set so that there is plenty of room for new ID within
   each area, but also that each area has unique IDs
*/

#define NC_SESSION_BASE 100
#define NC_WINDOW_BASE  400
#define NC_IO_BASE      800

/* This are always set in the structures to assure that 
   structures have a unique identifier
*/

#define	SESSION_STRUCT_VERSION	1
#define	WINDOW_STRUCT_VERSION	10
#define	IOCONTEXT_STRUCT_VERSION	100

/* Features that may be set */

  enum {
    NC_WORKSPACE_SIZE = NC_SESSION_BASE,
    NC_INITIAL_DEFS,
    NC_QUIET,
    NC_EXPANSION,
    NC_INITIAL_WORKSPACE,
    NC_DEBUGGING_ON,
    NC_SESSION_STRUCT_VERSION,
    NC_HTML_FMSW,
    NC_LAST_SESSION_SETTING
  };




/* Features that may be set */
  enum {
    NC_TRIGGERED = NC_WINDOW_BASE,
    NC_NOUSERINTERRUPTS,
    NC_SKETCH,
    NC_DECOR,
    NC_MESSAGES,
    NC_DEBUG_MESSAGES,
    NC_TRACE,
    NC_BOX_CHARS,
    NC_LOG,
    NC_LOG_NAME,             /* (char *) */
    NC_FORMAT,               /* (char *) */
    NC_PROMPT,               /* (char *) */
    NC_SCREEN_WIDTH,
    NC_WINDOW_STRUCT_VERSION,
    NC_SCREEN_HEIGHT,
    NC_USE_HISTORY,
    NC_LAST_WINDOW_SETTING
  };





/* Available buffer modes */

  enum {
    NC_INTERNAL_BUFFER_MODE = 1,
    NC_BUFFER_MODE,
    NC_EXTERNAL_BUFFER_MODE,
    NC_IO_MODE,
    NC_NO_IO_MODE,
    NC_IO_STRUCT_VERSION,
    NC_LAST_MODE
  };


#define NC_BUFFER_SIZE  1024
#define NC_BUFFER_INC   512

#if      defined(__cplusplus)
}
#endif

#endif             /* _MESSAGES_H_ */
