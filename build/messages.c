/*==============================================================

  MODULE   MESSAGES.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains error message support.

================================================================*/

#include "messages.h"

error_type_strings_type error_type_strings[] = {
  {NC_NORMAL, "Normal Return"},
  {NC_FATAL, "Fatal Error"},
  {NC_STARTUP_ERROR, "Error during startup"},
  {NC_BREAK, "Break"},
  {NC_TERMINATE, "Normal Exit"},
  {NC_WARNING, "Warning Message"},
  {NC_INTERNAL_ERROR, "Internal Error"},
  {NC_BAD_ERROR_TYPE, "Invalid Error Type"},
  {0, (char *) 0}
};

error_strings_type error_strings[] = {
  {NC_NO_ERR_N, NC_NORMAL, "Normal return from call"},
  {NC_ALREADY_RUNNING_W, NC_WARNING, "Interpreter already running!"},
  {NC_NOT_RUNNING_W, NC_WARNING, "Interpreter not started yet!"},
  {NC_ABORT_F, NC_FATAL, "DEBUG abort"},
  {NC_MEM_ERR_F, NC_FATAL, "Not enough memeory to continue"},
  {NC_IO_BUFFER_MEM_ERR_F, NC_FATAL, "Failed to create/expand Internal IO Buffer"},
  {NC_IO_BUFFER_OVERFLOW_W, NC_WARNING, "External IO Buffer Overflow"},
  {NC_NPRINTF_BUFFER_OVERFLOW_I, NC_INTERNAL_ERROR, "nprintf local buffer overwritten"},
  {NC_ATOM_TABLE_EXPAND_ERR_F, NC_FATAL, "Atom table cannot expand"},
  {NC_USER_BREAK_B, NC_BREAK, "User Break"},
  {NC_PROGRAM_BREAK_B, NC_BREAK, "Program Break"},
  {NC_MEM_EXPAND_FAILED_W, NC_WARNING, "workspace full"},
  {NC_MEM_EXPAND_NOT_ALLOWED_F, NC_FATAL, "workspace expansion not allowed"},
  {NC_MEM_EXPAND_NOT_TRIED_F, NC_FATAL, "workspace cannot expand"},
  {NC_INVALID_SELECTOR_ERR_I, NC_INTERNAL_ERROR, "Invalid Selector, call aborted"},
  {NC_INVALID_VALUE_ERR_I, NC_INTERNAL_ERROR, "Invalid value, call aborted"},
  {NC_INVALID_STRUCT_ERR_I, NC_INTERNAL_ERROR, "Invalid struct, call aborted"},
  {NC_RANGE_ERR_I, NC_INTERNAL_ERROR, "Value out of ranger"},
  {NC_WS_OPEN_FAILED_S, NC_STARTUP_ERROR, "Failed to open specified workspace"},
  {NC_WS_READ_ERR_S, NC_STARTUP_ERROR, "Error reading specified workspace"},
  {NC_WS_WRITE_ERR_W, NC_WARNING, "Error writing specified workspace"},
  {NC_RESERVED_OPEN_FAILED_S, NC_STARTUP_ERROR, "Failed to open 'reserverd.h'"},
  {NC_NH_OPEN_FAILED_S, NC_STARTUP_ERROR, "Failed to open an *.nh files"},
  {NC_DEFS_LOAD_FAILED_S, NC_STARTUP_ERROR, "Failed to load defs.ndf"},
  {NC_SOCKET_ERR_F, NC_FATAL, "Socket Error!"},
  {NC_BYE_T, NC_TERMINATE, "System terminated with the 'BYE' command"},
  {NC_EOF_T, NC_TERMINATE, "System terminated with by receiving and EOF"},
  {NC_FAULT_W, NC_WARNING, "Last command returned a fault"},
  {NC_WS_LOAD_ERR_F, NC_FATAL, "Workspace failed to load correctly"},
  {NC_STACK_NOT_EMPTY_I, NC_INTERNAL_ERROR, "Nial Stack not empty"},
  {NC_WS_WRONG_VERSION_S, NC_STARTUP_ERROR, "Workspace not valid for this version"},
  {NC_USER_WS_WRONG_VERSION_W, NC_WARNING, "Workspace not valid for this version"},
  {NC_STACK_GROWN_I, NC_INTERNAL_ERROR, "Stack has grown during loaddefs"},
  {NC_C_STACK_OVERFLOW_W, NC_WARNING, "C stack overflow"},
  {NC_PROFILE_MEM_W, NC_WARNING, "Could not allocate space for a new node"},
  {NC_ERASERECORD_W, NC_WARNING, "direct access eraserecord failed"},
  {NC_FILEPTR_W, NC_WARNING, "error in fileptr"},
  {NC_PROFILE_FILE_W, NC_WARNING, "failed to open profile output file"},
  {NC_FLOAT_EXCEPTION_W, NC_WARNING, "floating point exception"},
  {NC_INTEGER_OVERFLOW_APPEND_W, NC_WARNING, "integer overflow in append"},
  {NC_INTEGER_OVERFLOW_HITCH_W, NC_WARNING, "integer overflow in hitch"},
  {NC_INTEGER_OVERFLOW_CREATE_W, NC_WARNING, "integer overflow in size for array creation"},
  {NC_INTEGER_OVERFLOW_CART_W, NC_WARNING, "integer overflow in size of a cart"},
  {NC_INTEGER_OVERFLOW_LINK_W, NC_WARNING, "integer overflow in size of a link"},
  {NC_INTEGER_OVERFLOW_RESHAPE_W, NC_WARNING, "integer overflow in size of a reshape"},
  {NC_INTEGER_OVERFLOW_PICTURE_W, NC_WARNING, "integer overflow in size of picture"},
  {NC_INTEGER_OVERFLOW_TAKE_W, NC_WARNING, "integer overflow in take"},
  {NC_MATH_LIB_DOMAIN_ERROR_W, NC_WARNING, "Math Library Domain Error"},
  {NC_MATH_LIB_SING_ERROR_W, NC_WARNING, "Math Library Singularity Error"},
  {NC_MATH_LIB_OVERFLOW_ERROR_W, NC_WARNING, "Math Library Overflow Error"},
  {NC_MATH_LIB_UNDERFLOW_ERROR_W, NC_WARNING, "Math Library UnderFLow Error"},
  {NC_MATH_LIB_TLOSS_ERROR_W, NC_WARNING, "Math Library Partial Loss Error"},
  {NC_MATH_LIB_PLOSS_ERROR_W, NC_WARNING, "Math Library Total Loss Error"},
  {NC_PARSE_STACK_OVERFLOW_W, NC_WARNING, "internal state stack overflow"},
  {NC_SYMTAB_ERROR_I, NC_INTERNAL_ERROR, "invalid entry in symbol table"},
  {NC_CALL_STACK_MEM_W, NC_WARNING, "no space for call stack record"},
  {NC_PROFILE_SYNCH_W, NC_WARNING, "profiling end does not match begining"},
  {NC_STACK_EMPTY_W, NC_WARNING, "stack empty"},
  {NC_RANK_MEM_W, NC_WARNING, "unable to allocate space in RANK"},
  {NC_DIRECT_ACCESS_WRITE_W, NC_WARNING, "write error in direct access"},
  {NC_LOG_OPEN_FAILED_W, NC_WARNING, "failed to open log file"},
  {NC_RECOVER_N, NC_NORMAL, "Normal return to apply recover"},
  {NC_LATENT_N, NC_NORMAL, "Normal return to do Latent"},
  {NC_CHECKPOINT_N, NC_NORMAL, "Normal return to do Checkpoint"},
  {NC_RESTART_N, NC_NORMAL, "Normal return to do Restart"},
  {NC_CLEARWS_N, NC_NORMAL, "Normal return to do Clearws"},
  {NC_ERROR_LOOP_F, NC_FATAL, "exit_cover is in a loop"},
  {NC_RECOVERY_LOOP_W, NC_WARNING, "recover mechanism looping"},
  {NC_BREAKLIST_ERR_W, NC_WARNING, "breaklist error"},
  {NC_INVALID_ATOM_F, NC_FATAL, "Invalid Atom"},
  {NC_CBUFFER_ALLOCATION_FAILED_W, NC_WARNING, "Cbuffer allocation failed"},
  {NC_VALENCE_OVERFLOW_CREATE_W, NC_WARNING, "valence too large"},
  {0, 0, (char *) 0}
};



int
NC_ErrorType(int code)
{
  int         i = 0;

  while (error_strings[i].error_message) {
    if (error_strings[i].error_code == code)
      return (error_strings[i].error_type);
    i++;
  }
  return (NC_BAD_ERROR_TYPE);
}


char       *
NC_ErrorTypeName(int code)
{
  int         i = 0;
  int         type = NC_ErrorType(code);

  while (error_type_strings[i].error_type_name) {
    if (type == error_type_strings[i].error_type)
      return (error_type_strings[i].error_type_name);
    i++;
  }
  return ("Unknown Error Type");
}

char       *
NC_ErrorMessage(int code)
{
  int         i = 0;

  while (error_strings[i].error_message) {
    if (error_strings[i].error_code == code)
      return (error_strings[i].error_message);
    i++;
  }
  return ("Unknown Error Code");
}
