/*=============================================================

  MODULE   COREIF.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the interface functions to the core interpreter
  except for NC_InitNial which is in lib_main.c

================================================================*/


#include "switches.h"


#define IOLIB
#define SJLIB
#define STLIB
#define MALLOCLIB

#ifdef WINDOWS_SYS
#define WINDOWSLIB
#endif

#include "sysincl.h"

#include "qniallim.h"
#include "coreif_p.h"
#include "lib_main.h"
#include "absmach.h"
#include "basics.h"

#include "mainlp.h"          /* for do1commmand */
#include "eval.h"            /* for destroy_call_stack */
#include "fileio.h"          /* for nprintf etc. */
#include "picture.h"         /* for get and set format string functions */
#include "messages.h"        /* for message codes */
#include "wsmanage.h"        /* for loaddefs */


/* The core interpreter is called directly from main_stu.c in a console version,
   via DLL calls in uses of the Nial Data Engine, and from the GUI interface code
   in the Q'Nial for Windows programming environment.

   The interface is very general and only some of its features are currently used.
   Three arrays are used to record the state of the interpreter settings in order
   to permit multiple simultaneous interpreter windows in the GUI interface.
   These are:
     WindowSettingsTable
        this holds all settings that can be modified under function control
        or by the windowing environment. It is updated on each loop around
        CommandInterpret
     SessionSettingsTable
        this holds all settings that remain fixed through a Session.
     IOContextTable
        this holds the IO mode and function pointers and streams for the 
        routines that do the I/O directly when mode = NC_IO_MODE.

   There are routines to Create and Destroy entries in the tables and to set and
   get values within them.

   The interface has routines to
      initialize the Nial interpreter
      interpret a Nial command
      stop the Nial interpreter
      write a string
      read a string
      read a character
*/

/* declarations for the buffer area for output string */
static char *NC_buffer = NULL;
static int  NC_buffer_pos = 0;
static int  NC_buffer_size = NC_BUFFER_SIZE;
static int  NC_buffer_init_size = NC_BUFFER_SIZE;
static int  NC_buffer_increment = NC_BUFFER_INC;
static int  sv_nui;    /* flag to save the no user interrupts flag during 
                          interpreter startup */


/* globals used to hold indicies for the settings as integers */
static long gDefaultWindSettings;
static long gDefaultIOContext;
static long gSessionSettings;
static long gIOContext;



/* This define sets the maximum number of interpreter windows that 
   could be open at a time from the GUI interface. */

#ifdef CONSOLE
  #define NC_MAX_SETTINGS_ARRAY_SIZE  1
#else
  #define NC_MAX_SETTINGS_ARRAY_SIZE  32
#endif

/*-- Arrays to hold the lists of internal contexts and settings */
static NC_WindowSettings *WindowSettingsTable[NC_MAX_SETTINGS_ARRAY_SIZE];
static NC_SessionSettings *SessionSettingsTable[NC_MAX_SETTINGS_ARRAY_SIZE];
static NC_IOContext *IOContextTable[NC_MAX_SETTINGS_ARRAY_SIZE];

/* index position in the arrays for next entry */
static int  WindowSettingsTablePos = 0;
static int  IOContextTablePos = 0;
static int  SessionSettingsTablePos = 0;

/* prototypes */

static int  process_CI_recovery(int error_rc);
static void process_latent_defs(int trydefs);
static void NC_InstallSessionSettings(long);
static void NC_InstallWindowSettings(long);
static void NC_UpdateWindowSettings(long);
static char * expand_internal_buffer(char *oldbuf);


/* NC_SetDebugLevel is the function used by calling app 
   to debug DLL or Core calls.
  The argument is the desired level (0 or 1).
   It returns NC_NO_ERR_N indicating a successful call.
*/

int         _EXPORT_
NC_SetDebugLevel(int level)
{
  diag_messages_on = level;
  return (NC_NO_ERR_N);
}


/* NC_DestroyBuffer is the function used by lib_main.c to free 
   the IO buffer area.
   It has no arguments.
   It returns NC_NO_ERR_N indicating a successful call.
*/

int
NC_DestroyBuffer(void)
{
  if (NC_buffer) {
    free(NC_buffer);
    NC_buffer = NULL;
  }
  NC_buffer_size = 0;
  NC_buffer_pos = 0;
  return (NC_NO_ERR_N);
}



/* NC_InitBuffer is the function used here and in lib_main.c to
   initialize the IO buffer area.
   It has no arguments.
   The buffer's initial size is determined by the static local 
   variable NC_buffer_init_size.
   It returns the error code NC_IO_BUFFER_MEM_ERR_F if the buffer 
   cannot be allocated and NC_NO_ERR_N if successful.
*/

int
NC_InitBuffer(void)
{
  /* test existence in case InitBuffer gets called twice due 
     to the possibly asynchronous interface */
  if (!NC_buffer) {
    NC_buffer = (char *) malloc(NC_buffer_init_size * sizeof(char));

    if (!NC_buffer) {
      NC_buffer_size = 0;
      return (NC_IO_BUFFER_MEM_ERR_F);
    }
  }
  NC_buffer_size = NC_buffer_init_size;
  return (NC_NO_ERR_N);
}


/* NC_SetBufferSize is the function used to alter the default initial 
   size of the IO buffer and it's subsequent increment size.
   The routine does not change the actual buffer. It should be called 
   prior to NC_InitBuffer.
   It has two arguments:
      inSize - initial size (this is ignored if this call is made
         subsequent to buffer initialization ie. after InitNial)
      inIncrement - The number of characters the buffer
         will grow by when expansion is needed.
   It returns the error code NC_RANGE_ERR_I if either argument
   is negative and NC_NO_ERR_N otherwise.
*/

int         _EXPORT_
NC_SetBufferSize(int inSize, int inIncrement)
{
  DIAG_MSG2("SetBufferSize", inSize, inIncrement);

  if ((inSize < 0) || (inIncrement < 0))
    return (NC_RANGE_ERR_I); /* error code */

  NC_buffer_init_size = inSize;
  NC_buffer_increment = inIncrement;

  return (NC_NO_ERR_N);
}


/* NC_IWrite is the function that does ALL writes by the CORE
   interpreter.  This is the only function that knows how and where 
   to print the output.
   It has three arguments:
      str (char *) - the string to be output (may be non-null terminated).
      numchars (int) - the count of valid characters in str.
      nlflag (int) - indicates whether a newline (\n) is to be appended 
        to the output?
   If errors occur exit_cover is called to do a longjump.
   NOTE: This function needs to be able to do newline translation depending 
   on the platform being compiled to.  This is achieved by setting the newline 
   string NLS appropriately in coreif_p.h.
*/


/* the IOContextTable[]->mode field indicates the kind of IO
   being supported. This can be:
     NC_BUFFER_MODE:  - i/o to a provided buffer
     NC_INTERNAL_BUFFER_MODE: - i/o to an internal buffer
     NC_IO_MODE: - i/o directly to interface using a provided writefunc()
     NC_NO_IO_MODE: - no i/o done, output is thrown away.
*/

void
NC_Write(char *str, int numchars, int nlflag)
{
  switch (IOContextTable[gIOContext - 1]->mode) {
 case NC_BUFFER_MODE:  /* used for DLL */
 case NC_INTERNAL_BUFFER_MODE:  /* used in debugging BUFFER_MODE via console */
        {
          int         i;
          int         NSLlen = strlen(NLS);

          for (i = 0; i < numchars; i++) {
            int         osize = ((str[i] == '\n') ? NSLlen : 1);

            if ((NC_buffer_pos + osize) >= NC_buffer_size)
              NC_buffer = expand_internal_buffer(NC_buffer);

            if (str[i] == '\n') { 
              /* substitute NLS for '\n' */
              memcpy(NC_buffer + NC_buffer_pos, NLS, NSLlen);
              NC_buffer_pos += NSLlen;
            }
            else
              NC_buffer[NC_buffer_pos++] = str[i];
          }

          if (nlflag) {
            if ((NC_buffer_pos + NSLlen) >= NC_buffer_size)
              NC_buffer = expand_internal_buffer(NC_buffer);
            memcpy(NC_buffer + NC_buffer_pos, NLS, NSLlen);
            NC_buffer_pos += NSLlen;
          }
        }
        NC_buffer[NC_buffer_pos] = '\0';
        break;

    case NC_IO_MODE:
        {
          /* the magic '3' is for the NULL + [up to] two chars for the
           * newline */
          char       *str_copy = (char *) malloc((numchars + 3) * sizeof(char));

          if (!str_copy)
            exit_cover(NC_MEM_ERR_F);

          strncpy(str_copy, str, numchars); /* redundant for WINDOWS_SYS */
          str_copy[numchars] = '\0';

          /* now we must adjust the '\n's for the appropriate platform */
#if defined(WINDOWS_SYS) && defined(WIN32GUI)
          {
            int         str_size = numchars + 3;
            int         j = 0,
                        i = 0;
            int         c;

            while (str[i] && i < numchars) {
              if (str[i] == '\n') {
                str_copy = (char *) realloc(str_copy, ++str_size);
                if (!str_copy)
                  exit_cover(NC_MEM_ERR_F);
                str_copy[j++] = '\r';
                str_copy[j] = '\n';
              }
              else
                str_copy[j] = str[i];

              i++;
              j++;
            }
            str_copy[j] = '\0';
          }
#endif

          /* if requested print a newline */
          if (nlflag)
            strcat(str_copy, NLS);

          /* call the user supplied print function */
          (IOContextTable[gIOContext - 1]->write_func) (IOContextTable[gIOContext - 1]->write_arg, str_copy);

          free(str_copy);
        }
        break;

    case NC_NO_IO_MODE:
        /* do nothing */
        break;

    default:
        /* internal error */
        break;
  }                          /* switch () */
}

/* support routine used by NC_Write to expand buffer */

static char *
expand_internal_buffer(char *oldbuf)
{
  oldbuf = (char *) realloc(oldbuf,
     (NC_buffer_size + NC_buffer_increment) * sizeof(char));
  if (oldbuf)
    NC_buffer_size += NC_buffer_increment;
  else
    exit_cover(NC_IO_BUFFER_MEM_ERR_F);
  return (oldbuf);
}


/* NC_EndBuffer is the function that places a null terminator at the
   current buffer position to ensure that left over data is not seen.
   It has no arguments and no results.
*/


void
NC_EndBuffer(void)
{
  if (NC_buffer) {
    NC_Write("\0",1,0);
  }
  else /* internal error */
    exit_cover(NC_IO_BUFFER_MEM_ERR_F);
}


/* NC_ResetBuffer is the function prepares the buffer so that it is 
   ready for mode input.  Subsequent input overwrites current contents, 
   but this call terminates but does not itself destroy the buffer
   contents.
   It has no arguments and no results.
*/


void        _EXPORT_
NC_ResetBuffer(void)
{
  /* insert a NULL character at the end of all generated output */
  NC_EndBuffer();

  /* reset the buffer start position for next call */
  NC_buffer_pos = 0;
}



/* NC_GetBuffer returns a pointer to a null terminated string
   containing the current buffer contents.
   It has no arguments. It returns the pointer to the output buffer.
*/

char       *_EXPORT_
NC_GetBuffer(void)
{
  DIAG_MSG("GetBuffer");
  return (NC_buffer);
}



/* NC_CopyBuffer copies a null terminated from the internal buffer
   into the supplied buffer.  The size of the supplied buffer
   is also specified by the caller.
   It has two arguments:
      exbuf - the external buffer
      exbufsize - the size of the external buffer
   It returns 0 on success and the size of buffer needed if it fails.
*/


int         _EXPORT_
NC_CopyBuffer(char *exbuf, int exbufsize)
{
  long        i;

  DIAG_MSG2("CopyBuffer", (int) exbuf, exbufsize);

  if (!NC_buffer) { /* no NC_buffer, place empty string into buffer */
    strncpy(exbuf, "\0", exbufsize);
    return (0);
  }
  if ((i = strlen(NC_buffer)) < exbufsize) {
    /* used portion of NC_buffer fits, copy it */
    strcpy(exbuf, NC_buffer);
    return (0);
  }
  else { /* copy as much as fits, return needed size */
    strncpy(exbuf, NC_buffer, exbufsize);
    return (i);
  }
}

/* routine to install the session settings. */

static void
NC_InstallSessionSettings(long SS)
{
  if (SessionSettingsTable[SS - 1]->defs_name) {
    /* set up copy of name for a supplied definition file name */
    defssw = true;
    defsfnm = strdup(SessionSettingsTable[SS - 1]->defs_name);
  }
  else {
    defssw = false;
    /* if this is needed here, should it also be in the above case? */
    if (defsfnm)
      free(defsfnm);
    defsfnm = NULL;
  }
#ifdef OMITTED
  HTMLfmsw = SessionSettingsTable[SS - 1]->form_switch;
#endif

  initmemsize = SessionSettingsTable[SS - 1]->ws_size;
  quiet = SessionSettingsTable[SS - 1]->squiet;
  expansion = SessionSettingsTable[SS - 1]->sexpansion;
  debugging_on = SessionSettingsTable[SS - 1]->sdebugging_on;

  if (SessionSettingsTable[SS - 1]->ws_name) {
    loadsw = true;
    loadfnm = strdup(SessionSettingsTable[SS - 1]->ws_name);
  }
  else {
    loadsw = false;
    if (loadfnm)
      free(loadfnm);
    loadfnm = NULL;

  }
}

/* routine to set the window specific settings. This is done
   on each route through the CI routine. */

static void
NC_InstallWindowSettings(long WindowSettings)
{
  /* set the window specific attributes */
  triggered = WindowSettingsTable[WindowSettings - 1]->triggered_on;
  nouserinterrupts = WindowSettingsTable[WindowSettings - 1]->nouserinterrupts_on;
  sketch = WindowSettingsTable[WindowSettings - 1]->sketch_on;
  decor = WindowSettingsTable[WindowSettings - 1]->decor_on;
  messages_on = WindowSettingsTable[WindowSettings - 1]->msgs_on;
#ifdef DEBUG
  debug_messages_on = WindowSettingsTable[WindowSettings - 1]->debug_msgs_on;
#endif
  trace = WindowSettingsTable[WindowSettings - 1]->trace_on;
  keeplog = WindowSettingsTable[WindowSettings - 1]->log_on;

  /*-- Set the box chars each time we get in here */
  initboxchars(WindowSettingsTable[WindowSettings - 1]->boxchars_on);
  strncpy(logfnm, WindowSettingsTable[WindowSettings - 1]->log_name, MAXLOGFNMSIZE);
  setformat(WindowSettingsTable[WindowSettings - 1]->format);
  strncpy(prompt, WindowSettingsTable[WindowSettings - 1]->wprompt, MAXPROMPTSIZE);
  ssizew = WindowSettingsTable[WindowSettings - 1]->screenWidth;
  usehistory = WindowSettingsTable[WindowSettings - 1]->useHistory;
}

/* routine to update the window specific settings in case they
   have changed under program execution. This is done
   on each route through the CI routine. */

static void
NC_UpdateWindowSettings(long WindowSettings)
{
  /* set variables in case they have changed */
  WindowSettingsTable[WindowSettings - 1]->triggered_on = triggered;
  WindowSettingsTable[WindowSettings - 1]->nouserinterrupts_on = nouserinterrupts;
  WindowSettingsTable[WindowSettings - 1]->sketch_on = sketch;
  WindowSettingsTable[WindowSettings - 1]->decor_on = decor;
  WindowSettingsTable[WindowSettings - 1]->msgs_on = messages_on;
#ifdef DEBUG
  WindowSettingsTable[WindowSettings - 1]->debug_msgs_on = debug_messages_on;
#endif
  WindowSettingsTable[WindowSettings - 1]->trace_on = trace;
  WindowSettingsTable[WindowSettings - 1]->log_on = keeplog;

  NC_SetWindowSetting(WindowSettings, NC_LOG_NAME, (int) logfnm);
  NC_SetWindowSetting(WindowSettings, NC_FORMAT, (int) getformat());
  NC_SetWindowSetting(WindowSettings, NC_PROMPT, (int) prompt);
  WindowSettingsTable[WindowSettings - 1]->screenWidth = ssizew;
  WindowSettingsTable[WindowSettings - 1]->useHistory = usehistory;
}


/* the main routine to support interpretation of actions */

int         _EXPORT_
NC_CommandInterpret(char *inCommand, long ioParams,
                    long inIOContext)
{
  int         result;
  int         error_rc;

  recoveryno = 0;            /* reset on each entry */
  DIAG_MSG3("CommandInterpret", (int) inCommand, ioParams, inIOContext);

  /* Only proceed if the interpreter is running */
  if (!interpreter_running)
    return (NC_NOT_RUNNING_W);

  /* long jump point for all interrupted computations */
  error_rc = setjmp(error_env);
  if (error_rc) {
    return process_CI_recovery(error_rc);
  }


  /* normal entry point for NC_CommandINterpret()  called CI in comments */

  /* Check that the Window Settings are ok */
  if (!ioParams)
    ioParams = gDefaultWindSettings;
  else
    CHECK_WINDOWSETTINGS(ioParams);

  /* Check that the IO Context is ok */
  if (!inIOContext)
    gIOContext = gDefaultIOContext;
  else {
    CHECK_IOCONTEXT(inIOContext);
    gIOContext = inIOContext;
  }


  /* install settings into the core interpreter */
  NC_InstallWindowSettings(ioParams);

  /* is this the best place to put the command out to the log? */
  if (keeplog) {
    writelog((char *) "\n", strlen("\n"), false);
    writelog((char *) prompt, strlen(prompt), false);
    writelog((char *) inCommand, strlen(inCommand), true);
  }

  /* execute the command */

  result = do1command(inCommand);

  /* copy setting from core interpreter back into settings */
  NC_UpdateWindowSettings(ioParams);
  NC_ResetBuffer();

  return (result);
}

/* skeleton routines for future interface needs */

char       *_EXPORT_
NC_LoadLibrary(void)
{
  return (NULL);
}

int
NC_UnloadLibrary(void)
{
  return (0);
}


/* NC_SetNialRoot is the routine to set the nialrootname variable
   to the path of the Mial Root Directory.
   The argument is the path.
   It returns NC_NO_ERR_N to indicate success.
*/


int         _EXPORT_
NC_SetNialRoot(char *inRoot)
{
  DIAG_MSG1("SetNialRoot", (int) inRoot);
  strcpy(nialrootname, inRoot);

  return (NC_NO_ERR_N);
}


/* NC_CreateSessionSettings is the routine that creates a 
   NC_SessionSettings struct and fills in the default values.
   It has no arguments.
   It returns an integer, which is one higher than the position
   of the NC_SessionSettings struct in the NC_SessionSettings table.
*/


long        _EXPORT_
NC_CreateSessionSettings(void)
{
  NC_SessionSettings *theSettings;
  int         i;

  DIAG_MSG("CreateSessionSettings");

  theSettings = (NC_SessionSettings *) malloc(sizeof(NC_SessionSettings));

  if (!theSettings)
    return (NC_MEM_ERR_F); 

  theSettings->struct_version = SESSION_STRUCT_VERSION;

  theSettings->ws_size = dfmemsize;
  theSettings->sexpansion = true;
#ifdef DEBUG
  theSettings->sdebugging_on = true;
#else
  theSettings->sdebugging_on = false;
#endif
  theSettings->defs_name = NULL;
  theSettings->form_switch = false;
  theSettings->squiet = false;

  theSettings->ws_name = NULL;
  theSettings->CheckUserBreak = NULL;

  /*-- Find a slot that might have been freed already */
  for (i = 0; i < SessionSettingsTablePos; i++) {
    if (!SessionSettingsTable[i]) {
      SessionSettingsTable[i] = theSettings;
      return (i + 1);  /* always give the user 1-n (not 0-(n-1)) */
    }
  }
  /*-- Are we out of array space */
  if (SessionSettingsTablePos >= (NC_MAX_SETTINGS_ARRAY_SIZE)) {
    free(theSettings);
    return (NC_MEM_ERR_F);
  }
  SessionSettingsTable[SessionSettingsTablePos++] = theSettings;

  return (SessionSettingsTablePos);
}

/* NC_SetSessionSetting is the routine that sets an individual feature
   setting in a NC_SessionSettings struct. 
   It has three arguments:
      ioSettings - the index + 1 to the struct in the table
      inFeature - the name of the feature
      inValue - the value to set it to
   It returns NC_NO_ERR_N to indicate success.
*/

int         _EXPORT_
NC_SetSessionSetting(long ioSettings, int inFeature, long* inValue)
{
  int         len;

  DIAG_MSG3("SetSessionSettings", ioSettings, inFeature, inValue);

  /* check if ioSettings actually exist... */
  if (!ioSettings || (SessionSettingsTable[ioSettings - 1]->struct_version > SESSION_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  switch (inFeature) {
    case NC_WORKSPACE_SIZE:
        SessionSettingsTable[ioSettings - 1]->ws_size = inValue;
        break;

    case NC_DEBUGGING_ON:
        SessionSettingsTable[ioSettings - 1]->sdebugging_on = inValue;
        break;
    case NC_HTML_FMSW:
        SessionSettingsTable[ioSettings - 1]->form_switch = inValue;
        break;
    case NC_INITIAL_DEFS:
        if (inValue) {
          len = strlen((char *) inValue);

          /* if we haven't allocated it, do so */
          if (!SessionSettingsTable[ioSettings - 1]->defs_name)
            SessionSettingsTable[ioSettings - 1]->defs_name = (char *) malloc(len + 1);
          else               /* otherwise, realloc it */
            SessionSettingsTable[ioSettings - 1]->defs_name =
              (char *) realloc(SessionSettingsTable[ioSettings - 1]->defs_name, len + 1);

          if (!SessionSettingsTable[ioSettings - 1]->defs_name)
            return (NC_MEM_ERR_F);
          strcpy(SessionSettingsTable[ioSettings - 1]->defs_name, (char *) inValue);
        }
        else {
          /* we have passed in a NULL, then clear the old value */
          if (SessionSettingsTable[ioSettings - 1]->defs_name)
            free(SessionSettingsTable[ioSettings - 1]->defs_name);
          SessionSettingsTable[ioSettings - 1]->defs_name = NULL;
        }
        break;

    case NC_QUIET:
        SessionSettingsTable[ioSettings - 1]->squiet = inValue;
        break;

    case NC_EXPANSION:
        SessionSettingsTable[ioSettings - 1]->sexpansion = inValue;
        break;

    case NC_INITIAL_WORKSPACE:
        if (inValue) {
          len = strlen((char *) inValue);

          /* if we haven't allocated it, do so */
          if (!SessionSettingsTable[ioSettings - 1]->ws_name)
            SessionSettingsTable[ioSettings - 1]->ws_name = (char *) malloc(len + 1);
          else               /* otherwise, realloc it */
            SessionSettingsTable[ioSettings - 1]->ws_name =
              (char *) realloc(SessionSettingsTable[ioSettings - 1]->ws_name, len + 1);

          if (!SessionSettingsTable[ioSettings - 1]->ws_name)
            return (NC_MEM_ERR_F);
          strcpy(SessionSettingsTable[ioSettings - 1]->ws_name, (char *) inValue);
        }
        else {
          /* we have passed in a NULL, then clear the old value */
          if (SessionSettingsTable[ioSettings - 1]->ws_name)
            free(SessionSettingsTable[ioSettings - 1]->ws_name);
          SessionSettingsTable[ioSettings - 1]->ws_name = NULL;
        }
        break;

    default:
        /* programmer's error */
        return (NC_INVALID_SELECTOR_ERR_I);
  }

  return (NC_NO_ERR_N);
}

/* NC_GetSessionSetting is the routine that gets an individual feature
   setting in a NC_SessionSettings struct. 
   It has three arguments:
      ioSettings - the index + 1 to the struct in the table
      inFeature - the name of the feature
      inValue - a pointer to a variable where the value to be placed
   It returns NC_NO_ERR_N to indicate success.
*/


int         _EXPORT_
NC_GetSessionSetting(long ioSettings, int inFeature, int *inValue)
{
  DIAG_MSG3("GetSessionSettings", ioSettings, inFeature, (int) inValue);
  /* placing this before the check allows us to debug the struct */
  if (inFeature == NC_SESSION_STRUCT_VERSION) {
    if (SessionSettingsTable[ioSettings - 1])
      *inValue = SessionSettingsTable[ioSettings - 1]->struct_version;
    else
      *inValue = NC_INVALID_STRUCT_ERR_I;
    return (NC_NO_ERR_N);
  }
  /* check if ioSettings actually exist... */
  if (!ioSettings || (SessionSettingsTable[ioSettings - 1]->struct_version > SESSION_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  switch (inFeature) {
    case NC_WORKSPACE_SIZE:
        *inValue = SessionSettingsTable[ioSettings - 1]->ws_size;
        break;

    case NC_DEBUGGING_ON:
        *inValue = SessionSettingsTable[ioSettings - 1]->sdebugging_on;
        break;

    case NC_HTML_FMSW:
        *inValue = (int) SessionSettingsTable[ioSettings - 1]->form_switch;
        break;

    case NC_INITIAL_DEFS:
        *inValue = (int) SessionSettingsTable[ioSettings - 1]->defs_name;
        break;

    case NC_QUIET:
        *inValue = SessionSettingsTable[ioSettings - 1]->squiet;
        break;

    case NC_EXPANSION:
        *inValue = SessionSettingsTable[ioSettings - 1]->sexpansion;
        break;

    case NC_INITIAL_WORKSPACE:
        *inValue = (int) SessionSettingsTable[ioSettings - 1]->ws_name;
        break;

    default:
        /* programmer's error */
        return (NC_INVALID_SELECTOR_ERR_I);
  }

  return (NC_NO_ERR_N);
}

/* NC_DestroySessionSettings is the routine frees a SessionSettings item
   in the table.
   It has one argument:
      ioSettings - the index + 1 of the position of the struct.
   It returns NC_NO_ERR_N to indicate success.
   Note: The routine has to free the space allocated to members of the struct
   as well as the struct itself.
*/


int         _EXPORT_
NC_DestroySessionSettings(long ioSettings)
{
  DIAG_MSG1("DestroySessionSettings", ioSettings);

  if (!ioSettings ||
      (SessionSettingsTable[ioSettings - 1]->struct_version > SESSION_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  free(SessionSettingsTable[ioSettings - 1]->defs_name);
  free(SessionSettingsTable[ioSettings - 1]->ws_name);
  free(SessionSettingsTable[ioSettings - 1]);
  SessionSettingsTable[ioSettings - 1] = NULL;

  return (NC_NO_ERR_N);
}



/* NC_CreateWindowSettings is the routine that creates a 
   NC_WindowSettings struct and fills in the default values.
   It has no arguments.
   It returns an integer, which is one higher than the position
   of the NC_WindowSettings struct in the NC_WindowSettings table.
*/

long        _EXPORT_
NC_CreateWindowSettings(void)
{
  int         i;
  NC_WindowSettings *theSettings;

  DIAG_MSG("CreateWindowSettings");

  theSettings = (NC_WindowSettings *) malloc(sizeof(NC_WindowSettings));

  if (!theSettings)
    return ((long) NULL);

  theSettings->struct_version = WINDOW_STRUCT_VERSION;

  /* set up some default values */
  theSettings->triggered_on = true;
  theSettings->nouserinterrupts_on = false;
  theSettings->sketch_on = true;
  theSettings->decor_on = false;
#ifdef DEBUG
  theSettings->msgs_on = true;
  theSettings->debug_msgs_on = true;
#else
  theSettings->msgs_on = false;
  theSettings->debug_msgs_on = false;
#endif
  theSettings->trace_on = false;
#ifdef IBMPCCHARS
  theSettings->boxchars_on = true;
#else
  theSettings->boxchars_on = false;
#endif
  theSettings->log_on = false;

  theSettings->log_name = strdup("auto.nlg");

  theSettings->format = strdup("%g");

  theSettings->wprompt = strdup("     ");

  theSettings->screenWidth = 80;

  theSettings->useHistory = 1;

  /*-- Find a place that might have been free'd already */
  for (i = 0; i < WindowSettingsTablePos; i++) {
    if (!WindowSettingsTable[i]) {
      WindowSettingsTable[i] = theSettings;
      return (i + 1);        /* always give the user 1-n (not 0-(n-1)) */
    }
  }
  /*-- Are we out of array space */
  if (WindowSettingsTablePos >= (NC_MAX_SETTINGS_ARRAY_SIZE)) {
    free(theSettings);
    return (0);
  }
  WindowSettingsTable[WindowSettingsTablePos++] = theSettings;

  return (WindowSettingsTablePos);
}

/* NC_SetWindowSetting is the routine that sets an individual feature
   setting in a NC_WindowSettings struct. 
   It has three arguments:
      ioSettings - the index + 1 to the struct in the table
      inFeature - the name of the feature
      inValue - the value to set it to
   It returns NC_NO_ERR_N to indicate success.
*/

int         _EXPORT_
NC_SetWindowSetting(long ioSettings, int inFeature, int inValue)
{
  int         len;

  DIAG_MSG3("SetWindowSetting", ioSettings, inFeature, (int) inValue);

  /* check if ioSettings actually exist... */
  if (!ioSettings ||
      (WindowSettingsTable[ioSettings - 1]->struct_version > WINDOW_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  switch (inFeature) {
    case NC_TRIGGERED:
        WindowSettingsTable[ioSettings - 1]->triggered_on = inValue;
        break;

    case NC_NOUSERINTERRUPTS:
        WindowSettingsTable[ioSettings - 1]->nouserinterrupts_on = inValue;
        break;

    case NC_SKETCH:
        WindowSettingsTable[ioSettings - 1]->sketch_on = inValue;
        break;

    case NC_DECOR:
        WindowSettingsTable[ioSettings - 1]->decor_on = inValue;
        break;

    case NC_MESSAGES:
        WindowSettingsTable[ioSettings - 1]->msgs_on = inValue;
        break;

    case NC_DEBUG_MESSAGES:
        WindowSettingsTable[ioSettings - 1]->debug_msgs_on = inValue;
        break;

    case NC_TRACE:
        WindowSettingsTable[ioSettings - 1]->trace_on = inValue;
        break;

    case NC_BOX_CHARS:
        WindowSettingsTable[ioSettings - 1]->boxchars_on = inValue;
        break;

    case NC_LOG:
        WindowSettingsTable[ioSettings - 1]->log_on = inValue;
        break;

    case NC_LOG_NAME:
        if (inValue) {
          len = strlen((char *) inValue);
          if (!len)
            return (NC_INVALID_VALUE_ERR_I);

          if (WindowSettingsTable[ioSettings - 1]->log_name)
            free(WindowSettingsTable[ioSettings - 1]->log_name);
          WindowSettingsTable[ioSettings - 1]->log_name = strdup((char *) inValue);
        }
        else {
          if (WindowSettingsTable[ioSettings - 1]->log_name) {
            free(WindowSettingsTable[ioSettings - 1]->log_name);
            WindowSettingsTable[ioSettings - 1]->log_name = NULL;
          }
        }
        break;

    case NC_FORMAT:
        if (inValue) {
          len = strlen((char *) inValue);
          if (!len)
            return (NC_INVALID_VALUE_ERR_I);

          if (WindowSettingsTable[ioSettings - 1]->format)
            free(WindowSettingsTable[ioSettings - 1]->format);
          WindowSettingsTable[ioSettings - 1]->format = strdup((char *) inValue);
        }
        else {
          if (WindowSettingsTable[ioSettings - 1]->format) {
            free(WindowSettingsTable[ioSettings - 1]->format);
            WindowSettingsTable[ioSettings - 1]->format = NULL;
          }
        }
        break;

    case NC_PROMPT:
        if (inValue) {
          if (WindowSettingsTable[ioSettings - 1]->wprompt)
            free(WindowSettingsTable[ioSettings - 1]->wprompt);
          WindowSettingsTable[ioSettings - 1]->wprompt = strdup((char *) inValue);
        }
        else {
          if (WindowSettingsTable[ioSettings - 1]->wprompt) {
            free(WindowSettingsTable[ioSettings - 1]->wprompt);
            WindowSettingsTable[ioSettings - 1]->wprompt = NULL;
          }
        }
        break;

    case NC_SCREEN_WIDTH:
        /* NOTE: setting this to 0 gives an 'infinite' screen width i.e. no
         * newlines will be inserted */
        WindowSettingsTable[ioSettings - 1]->screenWidth = inValue;
        break;

    case NC_USE_HISTORY:
        WindowSettingsTable[ioSettings - 1]->useHistory = inValue;
        break;

    default:
        /* programmer's error */
        return (NC_INVALID_SELECTOR_ERR_I);
  }

  return (NC_NO_ERR_N);
}

/* NC_GetWindowSetting is the routine that gets an individual feature
   setting in a NC_WindowSettings struct. 
   It has three arguments:
      ioSettings - the index + 1 to the struct in the table
      inFeature - the name of the feature
      inValue - the value to set it to
   It returns NC_NO_ERR_N to indicate success.
*/

int         _EXPORT_
NC_GetWindowSetting(long ioSettings, int inFeature, int *outValue)
{
  DIAG_MSG3("GetWindowSetting", ioSettings, inFeature, (int) outValue);
  /* placing this before the check allows us to debug the struct */
  if (inFeature == NC_WINDOW_STRUCT_VERSION) {
    *outValue = WindowSettingsTable[ioSettings - 1]->struct_version;
    return (NC_NO_ERR_N);
  }
  /* check if ioSettings actually exist... */
  if (!ioSettings ||
      (WindowSettingsTable[ioSettings - 1]->struct_version > WINDOW_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  switch (inFeature) {
    case NC_TRIGGERED:
        *outValue = WindowSettingsTable[ioSettings - 1]->triggered_on;
        break;

    case NC_NOUSERINTERRUPTS:
        *outValue =
          WindowSettingsTable[ioSettings - 1]->nouserinterrupts_on;
        break;

    case NC_SKETCH:
        *outValue = WindowSettingsTable[ioSettings - 1]->sketch_on;
        break;

    case NC_DECOR:
        *outValue = WindowSettingsTable[ioSettings - 1]->decor_on;
        break;

    case NC_MESSAGES:
        *outValue = WindowSettingsTable[ioSettings - 1]->msgs_on;
        break;

    case NC_DEBUG_MESSAGES:
        *outValue = WindowSettingsTable[ioSettings - 1]->debug_msgs_on;
        break;

    case NC_TRACE:
        *outValue = WindowSettingsTable[ioSettings - 1]->trace_on;
        break;

    case NC_BOX_CHARS:
        *outValue = WindowSettingsTable[ioSettings - 1]->boxchars_on;
        break;

    case NC_LOG:
        *outValue = WindowSettingsTable[ioSettings - 1]->log_on;
        break;

    case NC_SCREEN_WIDTH:
        *outValue = WindowSettingsTable[ioSettings - 1]->screenWidth;
        break;

    case NC_LOG_NAME:
        *outValue = (int) WindowSettingsTable[ioSettings - 1]->log_name;
        break;
    case NC_PROMPT:
        *outValue = (int) WindowSettingsTable[ioSettings - 1]->wprompt;
        break;
    case NC_FORMAT:
        *outValue = (int) WindowSettingsTable[ioSettings - 1]->format;
        break;
    case NC_USE_HISTORY:
        *outValue = (int) WindowSettingsTable[ioSettings - 1]->useHistory;
        break;
    default:
        /* programmer's error */
        return (NC_INVALID_SELECTOR_ERR_I);
  }

  return (NC_NO_ERR_N);
}

/* separate routine to get the prompt since this is needed frequently */

char       *_EXPORT_
NC_GetPrompt(long inSettings)
{
  DIAG_MSG1("GetPrompt", inSettings);

  if (!inSettings || (WindowSettingsTable[inSettings - 1]->struct_version > WINDOW_STRUCT_VERSION))
    return (NULL);

  return (WindowSettingsTable[inSettings - 1]->wprompt);
}

/* NC_DestroyWindowSettings is the routine frees a WindowSettings item
   in the table.
   It has one argument:
      ioSettings - the index + 1 of the position of the struct.
   It returns NC_NO_ERR_N to indicate success.
   Note: The routine has to free the space allocated to members of the struct
   as well as the struct itself.
*/

int         _EXPORT_
NC_DestroyWindowSettings(long ioSettings)
{
  DIAG_MSG1("DestroyWindowSettings", ioSettings);
  if (!ioSettings || (WindowSettingsTable[ioSettings - 1]->struct_version > WINDOW_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  free(WindowSettingsTable[ioSettings - 1]->log_name);
  free(WindowSettingsTable[ioSettings - 1]->format);
  free(WindowSettingsTable[ioSettings - 1]->wprompt);
  free(WindowSettingsTable[ioSettings - 1]);
  WindowSettingsTable[ioSettings - 1] = NULL;

  return (NC_NO_ERR_N);
}



/* NC_CreateIOContext creates a NC_IOContext struct and fills in the
   default values.
   It has no arguments.
   It returns an integer, which is one higher than the position
   of the NC_IOContext struct in the NC_IOContext table.
*/


long        _EXPORT_
NC_CreateIOContext(void)
{
  int         i;
  NC_IOContext *theContext;

  DIAG_MSG("CreateIOContext");

  theContext = (NC_IOContext *) malloc(sizeof(NC_IOContext));

  if (!theContext)
    return ((long) NULL);

  theContext->struct_version = IOCONTEXT_STRUCT_VERSION;

  theContext->mode = NC_BUFFER_MODE; /* needed for DLL case */

  theContext->write_func = NULL;
  theContext->write_arg = NULL;

  theContext->readstr_func = NULL;
  theContext->readstr_arg = NULL;

  theContext->readchar_func = NULL;
  theContext->readchar_arg = NULL;

  /*-- Find a place that might have been free'd already */
  for (i = 0; i < IOContextTablePos; i++) {
    if (!IOContextTable[i]) {
      IOContextTable[i] = theContext;
      return (i + 1);        /* always give the user 1-n (not 0-(n-1)) */
    }
  }
  /*-- Are we out of array space */
  if (IOContextTablePos >= (NC_MAX_SETTINGS_ARRAY_SIZE)) {
    free(theContext);
    return (0);
  }
  IOContextTable[IOContextTablePos++] = theContext;

  return (IOContextTablePos);
}

/* NC_SetIOMode sets the mode field in an IOContext struct.
   It has two arguments:
      ioIOContext - the index + 1 of the struct in the table
      inMode - the mode to be assigned
   It returns NC_NO_ERR_N if successful and NC_INVALID_STRUCT_ERR_I
   otherwise.
*/

int         _EXPORT_
NC_SetIOMode(long ioIOContext, int inMode)
{
  DIAG_MSG2("CreateIOContext", ioIOContext, inMode);
  if (!ioIOContext ||
      (IOContextTable[ioIOContext - 1]->struct_version > IOCONTEXT_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  if (inMode < 0 || inMode >= NC_LAST_MODE)
    return (NC_INVALID_SELECTOR_ERR_I);

  IOContextTable[ioIOContext - 1]->mode = inMode;
  return (NC_NO_ERR_N);
}

/* NC_SetWriteCommand sets the function pointer and stream to set up
   direct output.
   It has three arguments:
      ioIOContext - the index + 1 of the struct in the table
      inFunc - the function pointer to the write routine
      inTheArg - the stream to be used
   It returns NC_NO_ERR_N if successful and NC_INVALID_STRUCT_ERR_I
   otherwise.
*/

int         _EXPORT_
NC_SetWriteCommand(long ioIOContext,
                   void (*inFunc) (void *, char *), void *inTheArg)
{
  DIAG_MSG3("SetWriteCommand", ioIOContext, (int) inFunc, (int) inTheArg);
  if (!ioIOContext ||
      (IOContextTable[ioIOContext - 1]->struct_version > IOCONTEXT_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);


  IOContextTable[ioIOContext - 1]->write_func = inFunc;
  IOContextTable[ioIOContext - 1]->write_arg = inTheArg;

  return (NC_NO_ERR_N);
}

/* NC_SetReadStringCommand sets the function pointer and stream to set up
   reading a character string.
   It has three arguments:
      ioIOContext - the index + 1 of the struct in the table
      inFunc - the function pointer to the read routine
      inTheArg - the stream to be used
   It returns NC_NO_ERR_N if successful and NC_INVALID_STRUCT_ERR_I
   otherwise.
*/

int         _EXPORT_
NC_SetReadStringCommand(long ioIOContext,
                        char *(*inFunc) (void *), void *inTheArg)
{
  DIAG_MSG3("SetReadStringCommand", ioIOContext, (int) inFunc, (int) inTheArg);
  if (!ioIOContext ||
      (IOContextTable[ioIOContext - 1]->struct_version > IOCONTEXT_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  IOContextTable[ioIOContext - 1]->readstr_func = inFunc;
  IOContextTable[ioIOContext - 1]->readstr_arg = inTheArg;

  return (NC_NO_ERR_N);
}

/* NC_SetReadCharCommand sets the function pointer and stream to set up
   reading a single character.
   It has three arguments:
      ioIOContext - the index + 1 of the struct in the table
      inFunc - the function pointer to the read routine
      inTheArg - the stream to be used
   It returns NC_NO_ERR_N if successful and NC_INVALID_STRUCT_ERR_I
   otherwise.
*/

int         _EXPORT_
NC_SetReadCharCommand(long ioIOContext,
                      char (*inFunc) (void *), void *inTheArg)
{
  DIAG_MSG3("SetReadCharCommand", ioIOContext, (int) inFunc, (int) inTheArg);
  if (!ioIOContext ||
      (IOContextTable[ioIOContext - 1]->struct_version > IOCONTEXT_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  IOContextTable[ioIOContext - 1]->readchar_func = inFunc;
  IOContextTable[ioIOContext - 1]->readchar_arg = inTheArg;

  return (NC_NO_ERR_N);
}

/* NC_SetCheckUserBreak sets the function pointer to the routine that
   checks for user breaks.
   It has two arguments:
      inSessionSettings - the index + 1 of the struct in the SessionSettings
         table
      inFunc - the function pointer to the checking routine
   It returns NC_NO_ERR_N if successful and NC_INVALID_STRUCT_ERR_I
   otherwise.
*/



int         _EXPORT_
NC_SetCheckUserBreak(long inSessionSettings,
                     int (*inFunc) (int))
{
  DIAG_MSG2("SetCheckUserBreak", inSessionSettings, (int) inFunc);
  if (!inSessionSettings ||
      (SessionSettingsTable[inSessionSettings - 1]->struct_version >
       SESSION_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  SessionSettingsTable[inSessionSettings - 1]->CheckUserBreak = inFunc;

  return (NC_NO_ERR_N);
}

/* NC_DestroyIOContext is the routine that frees an IOContext item
   in the table.
   It has one argument:
      ioIOContext - the index + 1 of the position of the struct.
   It returns NC_NO_ERR_N to indicate success.
   Note: The routine has to free the space allocated to members of the struct
   as well as the struct itself.
*/

int         _EXPORT_
NC_DestroyIOContext(long ioIOContext)
{
  DIAG_MSG1("DestroyIOContext", ioIOContext);
  if (!ioIOContext ||
      (IOContextTable[ioIOContext - 1]->struct_version > IOCONTEXT_STRUCT_VERSION))
    return (NC_INVALID_STRUCT_ERR_I);

  free(IOContextTable[ioIOContext - 1]);
  IOContextTable[ioIOContext - 1] = NULL;

  return (NC_NO_ERR_N);
}

/* cheksignal is the routine that checks whether the user has requested 
   a program break.
   In a GUI interface the routine that is called will check for the
   break on some frequency of calls from here. Calls to checksignal
   are embedded in long computations in Nial primitives to give
   the system a chance to break. */

void
checksignal(int code)
{
  if (!nouserinterrupts) {
    if (SessionSettingsTable[gSessionSettings - 1]->CheckUserBreak != NULL)
      if (SessionSettingsTable[gSessionSettings - 1]->CheckUserBreak(code))
        /* if a signal has been indicated use exit_cover to interrupt */
        exit_cover(NC_USER_BREAK_B);
  }
}


/* routine to process the Latent expression in the workspace loaded on startup
  (given, clearws or continue) and to process a given defs file.
  It is called by InitNial to handle the orginal start up
  and by process_CI_recovery for uses of the clearws and restart primitives.
  
NOTE: This code supports a call to cginial.exe with *.nfm form file as
  argument. It assumes that it can load the library definition file
  cgi-lib.ndf and that "process_script" is the routine that will execute the
  form. This code is obsolete. It assumes an older version of cgi-lib.ndf
  and that the latter is in the local directory. Need to redesign the interface 
  or omit it. I have OMITTED it for now MAJ.
*/

static void
process_latent_defs(int trydefs)
{
  nouserinterrupts = false;  /* only want to reset this first time */
  topstack = (-1);
  if (latenttest()) {        /* Latent exists, execute it from here */
    do1command("Latent");
  }

  if (trydefs && defssw) {  
  /* loads the initial definitions requested or defaulted by command line options. */
    char        defstr[256];

    if (defssw) {
      strcpy(defstr, defsfnm);
      check_ext(defstr, ".ndf",NOFORCE_EXTENSION);
      if (!loaddefs(true, defstr, 0))
        nprintf(OF_NORMAL_LOG, "loaddefs failed on %s\n", defstr);
    }
  }
}


/* process_CI_recovery determines what action to take when a 
   command execution has been terminated.
   all non-standard exits from the evaluator come here except during
   loaddefs of defs.ndf in startup. */

static int
process_CI_recovery(int error_rc)
{
  int         result;
  int         wsgiven;
  int         trydefs;
  int         clearsw;
  char       *inCommand;

#ifdef DEBUG
nprintf(OF_DEBUG,"in CI recovery: error msg %s\n",NC_ErrorMessage(error_rc));
nprintf(OF_DEBUG,"recoveryno %d\n",recoveryno);
#endif

  switch (error_rc) {
        /* Latent, Checkpoint and recover share code to do the execution */
    case NC_LATENT_N:
        inCommand = "Latent";
        goto doit;
    case NC_CHECKPOINT_N:
        inCommand = "Checkpoint";
        goto doit;
    case NC_RECOVER_N:
        {
          char        temp[100];

          recoveryno++;
          /* the recovery mechanism can easily be made to loop if an error
           * occurs in its code. So the number of times it can be called is
           * limited. */
          if (recoveryno >= MAXNORECOVERS) {
            nprintf(OF_NORMAL, "recovery mechanism is looping\n");
            NC_ResetBuffer();
            return NC_RECOVERY_LOOP_W;
          }
          /* set up string with argument to recover provided 
             from global variable toplevelmsg */
          strcpy(temp, "recover '");
          strcat(temp, toplevelmsg);
          strcat(temp, "'");
          inCommand = temp;
      doit:
          /* execute the command */
          result = do1command(inCommand);
          NC_ResetBuffer();
          return (result);
        }

        /* the Clearws and Restart expressions share code */
    case NC_CLEARWS_N:
        wsgiven = false;
        trydefs = false;
        clearsw = true;
        goto joinrestart;
    case NC_RESTART_N:
        wsgiven = loadsw;
        trydefs = true;
        clearsw = false;
    joinrestart:
        instartup = true;
        cleanup_ws();        /* prepare to restart */
        nialboot(wsgiven, clearsw, false);  /* do the reboot */
        instartup = false;   /* reset instartup */
        firstcommand = true; /* pretend it is first time through so that
                                defssw is looked at. */
        process_latent_defs(trydefs);
        NC_ResetBuffer();
        return NC_NO_ERR_N;

    /* mem expand fatal error cases that come here rather than exit_recover */
    case NC_MEM_EXPAND_NOT_TRIED_F:
    case NC_MEM_EXPAND_NOT_ALLOWED_F:
        /*  no need to clear stack and heap since we are terminating. */
        NC_ResetBuffer();
        return (error_rc);

    /* come here to avoid call to exit_cover for C stack overflow and
       FAULT result warnings. Do not want to call recover since it might
       loop. */
    case NC_C_STACK_OVERFLOW_W:
        nprintf(OF_MESSAGE, "Out of C stack space\n");
    case NC_FAULT_W:
        cleanup_ws();
        /* fall through to default case for all other errors */
    default:
        NC_ResetBuffer();
        return (error_rc);
  }
}

/* NC_StopNial() is used to stop the interpreter and do cleanups */

int         _EXPORT_
NC_StopNial()
{ 
/* If the interpreter is running set the flag to false,
  close the user files and clear the abstract machine.
  In either case, destroy the IO buffer and the call stack.
*/
  if (interpreter_running) {
    interpreter_running = 0;
    closeuserfiles();        /* close all user files */

    clear_abstract_machine();  /* this will call DLLclean */

  }
  NC_DestroyBuffer();        /* Throw away the Core IO buffer */
  destroy_call_stack();      /* get rid of call_stack, it will be recreated
                                if Nial is initiated again. */
  return (NC_NO_ERR_N);
}



/* NC_InitNial is the routine that initializes the core interpreter.
  It has 3 arguments:
    inSessionSettings - the Session settings
    inDefaultWindSettings - default Window settings
    inDefaultIOContext - the default IO context
 */

int         _EXPORT_
NC_InitNial(long inSessionSettings,
            long inDefaultWindSettings,
            long inDefaultIOContext)
{
  int         rc;
  int         error_rc;      /* return code holder if a longjmp error_env is
                              * done */
  int         loadsavesw;    /* return code holder if a longjmp to
                              * loadsave_env is done */

  DIAG_MSG3("InitNial", inSessionSettings, inDefaultWindSettings, inDefaultIOContext);

  /*-- Don't start up twice */
  if (interpreter_running)
    return (NC_ALREADY_RUNNING_W);

  interpreter_running = 0;   /* set to false until we are sure we are going */
 
 /* save a copy of the user interrupts status and turn it off durring startup */ 
  sv_nui = nouserinterrupts;   
  nouserinterrupts = true;   

  instartup = true;

#ifdef HISTORY
  lasteval = -1;       /* to ensure this is always cleared on startup */
#endif

  /* initialize variables that can be changed by args to Nial */

#ifdef DEBUG
  debug = false;      /* switched with set "debug/set "nodebug if
                              DEBUG is defined */
#endif

  continuefound = false;

  error_rc = setjmp(init_buf);
  if (error_rc) {
    /* all non-standard exits during initialization come here */

#ifdef DEBUG
/*  insert this print statement if needed
nprintf(OF_DEBUG,"in lib_main init_buf: error msg %s\n",NC_ErrorMessage(error_rc));
*/
#endif

    switch (error_rc) {

        /* mem expand warning cases that come here rather than exit_recover */
      case NC_MEM_EXPAND_FAILED_W:
          clearstack();
          clearheap();
          NC_EndBuffer();
          return (error_rc);

        /* mem expand fatal error cases that come here rather than exit_recover */
      case NC_MEM_EXPAND_NOT_TRIED_F:
      case NC_MEM_EXPAND_NOT_ALLOWED_F:
          NC_ResetBuffer();
          return (error_rc);

        /* come here to avoid call to exit_cover for C stack overflow and
           FAULT result warnings. Do not want to call recover since it
           might loop. */
      case NC_C_STACK_OVERFLOW_W:
          nprintf(OF_MESSAGE, "Out of C stack space\n");
      case NC_FAULT_W:
          cleanup_ws();
          /* fall through to default case for all other errors */
      default:
          NC_ResetBuffer();
          return (error_rc);
    }

  }

  /* long jump point for all interrupted computations */
  rc = setjmp(error_env);
  if (rc)
    return process_CI_recovery(rc);

  /* isave and iload will do a longjmp to here to do the reading and writing
   with workspace in a normal state */
  loadsavesw = setjmp(loadsave_env);
  if (loadsavesw)
    return process_loadsave_recovery(loadsavesw);

  /* if some settings were passed in AND the struct passed is valid */
  if (inSessionSettings) {
    CHECK_SESSIONSETTINGS(inSessionSettings);
    gSessionSettings = inSessionSettings;
  }
  else                       /* Create The defaults */
    gSessionSettings = NC_CreateSessionSettings();
  
  /* install session settings */
  NC_InstallSessionSettings(gSessionSettings);


  /* initialize the default window settings struct */
  if (inDefaultWindSettings) {
    CHECK_WINDOWSETTINGS(inDefaultWindSettings);
    gDefaultWindSettings = inDefaultWindSettings;
  }
  else
    gDefaultWindSettings = NC_CreateWindowSettings();

  /* Install window settings in case anything need then in startup */
  NC_InstallWindowSettings(gDefaultWindSettings);


  /* initialize the default IOContext struct */
  if (inDefaultIOContext) {
    CHECK_IOCONTEXT(inDefaultIOContext);
    gDefaultIOContext = inDefaultIOContext;
  }
  else
    gDefaultIOContext = NC_CreateIOContext();

  /* gIOContext is the current IOContext */
  gIOContext = gDefaultIOContext;


  NC_InitBuffer();

  /* call the main initialization routines.
     return code is nonzero if error occurs */
  rc = do_init_work();
  if (rc)
    return rc;

  /* do the intializations that use the workspace */

  nouserinterrupts = sv_nui; /* restore the desired setting */
  interpreter_running = true;/* Things are now safe */

  recoveryno = 0;


  process_latent_defs(true);

  NC_ResetBuffer();

  return NC_NO_ERR_N;
}

/* routine to use the installed read string function to get a string.
   The return value indicates whether the input routine
   was installed. */

int
NC_readstr(char **str)
{
  if (IOContextTable[gIOContext - 1]->readstr_func) {
    *str = (IOContextTable[gIOContext - 1]->readstr_func)
    (IOContextTable[gIOContext - 1]->readstr_arg);
    return true;
  }
  return false;
}


/* routine to use the installed read char function to get a character.
   Called with a char variable that the character is copied to.
   The result indicates whether the input routine was installed. */

int
NC_readchar(char *ch)
{
  if (IOContextTable[gIOContext - 1]->readchar_func) {
    /* raw read done using the installed function directly */
    *ch = (IOContextTable[gIOContext - 1]->readchar_func)
    (IOContextTable[gIOContext - 1]->readchar_arg);
    return true;
  }
  return false;
}
