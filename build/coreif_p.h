/*==============================================================

  HEADER   COREIF_P.H

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains the internal interface to the core interface routines.

================================================================*/

/* NOTE:  Any additions to this file should be considered for addition
  in the public interface "coreif.h".
*/


#ifndef _COREIF_P_H_
#define _COREIF_P_H_

#include "messages.h"

/*  The following defines sets up the correct line end for the particular
 system we are on.  This is for output translation only.
 */
#if defined(UNIXSYS)
#define NLS     "\n"
#elif (defined(WINDOWS_SYS) && defined(CONSOLE))
#define NLS     "\n"
#elif defined(WINDOWS_SYS)
#define NLS     "\r\n"
#else
#error A newline combination should be defined here 
#endif

/* Quick check macros to assure what was passed was the correct type of struct */
#define CHECK_WINDOWSETTINGS(X) {if (WindowSettingsTable[X-1]->struct_version > WINDOW_STRUCT_VERSION)\
	 return (NC_INVALID_STRUCT_ERR_I);}

#define CHECK_IOCONTEXT(X) {if (IOContextTable[X-1]->struct_version > IOCONTEXT_STRUCT_VERSION)\
       return (NC_INVALID_STRUCT_ERR_I);}

#define CHECK_SESSIONSETTINGS(X) {if (SessionSettingsTable[X-1]->struct_version > SESSION_STRUCT_VERSION)\
	 return (NC_INVALID_STRUCT_ERR_I);}

/* If we request API debugging and we are under windows.  This should
   be expanded to the ability to log to a file the API calls and their
   results.
*/

#if defined(WINDOWS_SYS) && defined(API_DEBUG)


#define DIAG_MSG(fun) {\
	 if (diag_messages_on) {\
		char buf[512];\
		sprintf(buf,"%s :(void)",fun);\
		MessageBox(0,buf,"Core DLL Diagnostics",MB_OK);\
	 }\
  }

#define DIAG_MSG1(fun,a1) {\
	 if (diag_messages_on) {\
		char buf[512];\
		sprintf(buf,"%s :(%d)",fun,a1);\
		MessageBox(0,buf,"Core DLL Diagnostics",MB_OK);\
	 }\
  }


#define DIAG_MSG2(fun,a1,a2) {\
	 if (diag_messages_on) {\
		char buf[512];\
		sprintf(buf,"%s :(%d,%d)",fun,a1,a2);\
		MessageBox(0,buf,"Core DLL Diagnostics",MB_OK);\
	 }\
  }

#define DIAG_MSG3(fun,a1,a2,a3) {\
	 if (diag_messages_on) {\
		char buf[512];\
		sprintf(buf,"%s :(%d,%d,%d)",fun,a1,a2,a3);\
		MessageBox(0,buf,"Core DLL Diagnostics",MB_OK);\
	 }\
  }
#else              /* not WINDOWS_SYS && API_DEBUG */

#define DIAG_MSG(fun)        /* */
#define DIAG_MSG1(fun,a1)    /* */
#define DIAG_MSG2(fun,a1,a2) /* */
#define DIAG_MSG3(fun,a1,a2,a3) /* */
#endif

/* settings for a session */

typedef struct {
  int         struct_version;
  int         ws_size;
  char       *defs_name;
  int         squiet;       
  int         sexpansion;
  int         sdebugging_on;
  char       *ws_name;
  int         form_switch;
  int         (*CheckUserBreak) (int);
}           NC_SessionSettings;


/* settings for a given window */
typedef struct {
  int         struct_version;

  /* BOOLEANS */
  int         triggered_on;
  int         nouserinterrupts_on;
  int         sketch_on;
  int         decor_on;
  int         msgs_on;
  int         debug_msgs_on;
  int         trace_on;
  int         boxchars_on;
  int         log_on;

  char       *log_name;
  char       *format;
  char       *wprompt;
  int         screenWidth;
  int         screenHeight;
  int         useHistory;
}           NC_WindowSettings;


/* method of conducting I/O from within the core */
typedef struct {
  int         struct_version;
  char        mode;
  void        (*write_func) (void *write_arg, char *theStr);
  void       *write_arg;
  char       *(*readstr_func) (void *read_arg);
  void       *readstr_arg;
  char        (*readchar_func) (void *read_arg);
  void       *readchar_arg;
}           NC_IOContext;

/* non-exported API routines for internal use */
extern int  NC_SetExternalBuffer(char *buffer, int size);
extern int  NC_InitBuffer(void);
extern int  NC_DestroyBuffer(void);
extern void NC_Write(char *str, int numchars, int nlflag);
extern void NC_EndBuffer(void);
extern int  NC_readchar(char *ch);
extern int  NC_readstr(char **str);
extern int  NC_SetDebugLevel(int level);


#if defined(UNIXSYS)
/* These are empty definitions for UNIX systems */
#  define _EXTERN_
#  define _EXPORT_
#elif defined(WINDOWS_SYS)

#  define _EXTERN_ extern

#    ifdef PASCAL_DLL 
#      define _EXPORT_ _export pascal
#    elif  C_DLL
#      define _EXPORT_ _export
#    else
#      define _EXPORT_
#    endif

#else
#error Something should be defined here for _EXTERN_ and _EXPORT_
#endif

/* INITIALIZATION commands */

_EXTERN_ char *_EXPORT_ NC_LoadLibrary(void);
_EXTERN_ char *_EXPORT_ NC_UnLoadLibrary(void);
_EXTERN_ int _EXPORT_ NC_SetNialRoot(char *inRoot);
_EXTERN_ int _EXPORT_
NC_InitNial(long inSessionSettings,
            long inDefaultWindSettings,
            long inDefaultIOContext);
_EXTERN_ int _EXPORT_ NC_StopNial(void);

 /* SESSIONSETTINGS commands */
_EXTERN_ long _EXPORT_ NC_CreateSessionSettings(void);
_EXTERN_ int _EXPORT_
NC_SetSessionSetting(long ioSettings,
                     int inFeature, int inValue);
_EXTERN_ int _EXPORT_
NC_GetSessionSetting(long ioSettings,
                     int inFeature, int *inValue);
_EXTERN_ int _EXPORT_ NC_DestroySessionSettings(long ioSettings);
_EXTERN_ int _EXPORT_
NC_SetCheckUserBreak(long ioSettings,
                     int (*inFunc) (int));



/* WINDOWSETTINGS commands */
_EXTERN_ long _EXPORT_ NC_CreateWindowSettings(void);
_EXTERN_ int _EXPORT_
NC_SetWindowSetting(long ioSettings,
                    int inFeature, int inValue);
_EXTERN_ int _EXPORT_
NC_GetWindowSetting(long ioSettings,
                    int inFeature, int *outValue);
_EXTERN_ char *_EXPORT_ NC_GetPrompt(long inSettings);
_EXTERN_ int _EXPORT_ NC_DestroyWindowSettings(long ioSettings);


/* IOCONTEXT commands */
_EXTERN_ long _EXPORT_ NC_CreateIOContext(void);
_EXTERN_ int _EXPORT_ NC_SetIOMode(long ioIOContext, int inMode);
_EXTERN_ int _EXPORT_
NC_SetWriteCommand(long ioIOContext,
                   void (*inFunc) (void *, char *), void *inTheArg);
_EXTERN_ int _EXPORT_
NC_SetReadStringCommand(long ioIOContext,
                        char *(*inFunc) (void *), void *inTheArg);
_EXTERN_ int _EXPORT_
NC_SetReadCharCommand(long ioIOContext,
                      char (*inFunc) (void *), void *inTheArg);
_EXTERN_ int _EXPORT_ NC_DestroyIOContext(long ioIOContext);


/* BUFFER commands */
_EXTERN_ int _EXPORT_ NC_SetBufferSize(int inSize, int inIncrement);
_EXTERN_ char *_EXPORT_ NC_GetBuffer(void);
_EXTERN_ int _EXPORT_ NC_CopyBuffer(char *, int);
_EXTERN_ void _EXPORT_ NC_ResetBuffer(void);


/* INTERPRETER command */
_EXTERN_ int _EXPORT_
NC_CommandInterpret(char *inCommand,
                    long ioParams,
                    long inIOContext);


#endif             /* _COREIF_P_H_ */
