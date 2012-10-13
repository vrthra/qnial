/*==============================================================

  PROTOS.H:  

  COPYRIGHT NIAL Systems Limited  1983-2005

  This contains Global variables,types and macros to hold the actual DLL functions

================================================================*/

typedef DWORD NC_SessionSettings,
            NC_WindowSettings,
            NC_IOContext;

/* INITIALIZATION commands */
typedef     DWORD(FAR PASCAL * SETNIALROOT) (LPSTR, int);

#define SETNIALROOT_INDEX     1
typedef
DWORD(FAR PASCAL * INITNIAL) (NC_SessionSettings,
                              NC_WindowSettings,
                              NC_IOContext, int);

#define INITNIAL_INDEX       3


/* SESSION SETTINGS commands */
typedef     NC_SessionSettings(FAR PASCAL * CREATESESSIONSETTINGS) (int);

#define CREATESESSIONSETTINGS_INDEX     4
typedef
DWORD(FAR PASCAL * SETSESSIONSETTING) (NC_SessionSettings,
                                       DWORD, DWORD, int);

#define SETSESSIONSETTING_INDEX      5
typedef     DWORD(FAR PASCAL * DESTROYSESSIONSETTINGS) (NC_SessionSettings, int);

#define DESTROYSESSIONSETTINGS_INDEX    6


/* WINDOW SETTINGS commands */
typedef     NC_WindowSettings(FAR PASCAL * CREATEWINDOWSETTINGS) (int);

#define CREATEWINDOWSETTINGS_INDEX      7
typedef
DWORD(FAR PASCAL * SETWINDOWSETTING) (NC_WindowSettings,
                                      DWORD, DWORD, int);

#define SETWINDOWSETTING_INDEX       8
typedef
DWORD(FAR PASCAL * GETWINDOWSETTING) (NC_WindowSettings,
                                      DWORD, DWORD, int);

#define GETWINDOWSETTING_INDEX         9
typedef     LPSTR(FAR PASCAL * GETPROMPT_) (NC_WindowSettings, int);

#define GETPROMPT_INDEX                10
typedef     DWORD(FAR PASCAL * DESTROYWINDOWSETTINGS) (NC_WindowSettings, int);

#define DESTROYWINDOWSETTINGS_INDEX       11

/* IOCONTEXT commands */
typedef     NC_IOContext(FAR PASCAL * CREATEIOCONTEXT) (int);

#define CREATEIOCONTEXT_INDEX              12
typedef     DWORD(FAR PASCAL * SETIOMODE) (NC_IOContext, DWORD, int);

#define SETIOMODE_INDEX                  13
typedef
DWORD(FAR PASCAL * SETWRITECOMMAND) (NC_IOContext,
                                     void (*) (void *, LPSTR),
                                     void *, int);

#define SETWRITECOMMAND_INDEX               14
typedef
DWORD(FAR PASCAL * SETREADSTRINGCOMMAND) (NC_IOContext,
                                          LPSTR(*) (void *), void *, int);

#define SETREADSTRINGCOMMAND_INDEX            15
typedef
DWORD(FAR PASCAL * SETREADCHARCOMMAND) (NC_IOContext,
                                        char (*) (void *), void *, int);

#define SETREADCHARCOMMAND_INDEX              16
typedef     DWORD(FAR PASCAL * DESTROYIOCONTEXT) (NC_IOContext, int);

#define DESTROYIOCONTEXT_INDEX                17


/* BUFFER commands */
typedef     DWORD(FAR PASCAL * SETBUFFERSIZE) (DWORD, DWORD, int);

#define SETBUFFERSIZE_INDEX              18
typedef     LPSTR(FAR PASCAL * GETBUFFER) (int);

#define GETBUFFER_INDEX                 19
typedef     DWORD(FAR PASCAL * COPYBUFFER) (LPSTR, DWORD, int);

#define COPYBUFFER_INDEX                2
typedef     DWORD(FAR PASCAL * RESETBUFFER) (int);

#define RESETBUFFER_INDEX              20


/* INTERPRETER command */
typedef
DWORD(FAR PASCAL * COMMANDINTERPRET) (LPSTR, NC_WindowSettings,
                                      NC_IOContext, int);

#define COMMANDINTERPRET_INDEX       22
