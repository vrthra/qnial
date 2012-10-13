/*=============================================================

MODULE   WIN32IF.C

  COPYRIGHT NIAL Systems Limited  1983-2005

Contains all IBMPC specific interface routines common to
Windows versions.


================================================================*/


/* Q'Nial file that selects features and loads the xxxspecs.h file */

#include "switches.h"

/* standard library header files */


#undef PASCAL
#define CDECL

#ifdef TIMEHOOK
#define TIMELIB
#define MATHLIB
#endif

#define IOLIB
#define SYSDEP
#define SIGLIB
#define SJLIB
#define STDLIB
#define STLIB
#define CLIB
#define MATHLIB
#define WINDOWSLIB
#define MALLOCLIB
#define FLOATLIB
#include "sysincl.h"



#ifdef __BORLANDC__
/*-- This allows consistent use of the sys_errlist name in the code */
#define sys_errlist _sys_errlist
#endif



#include "win32if.h"
#include "qniallim.h"
#include "absmach.h"
#include "lib_main.h"
#include "coreif_p.h"        /* for NC_Write */
#include "mainlp.h"          /* for exit_cover */
#include "fileio.h"          /* for writelog */



#ifdef FP_EXCEPTION_FLAG

/* support for catching floating point exceptions */

static int  fp_exception = 0;
static int  fp_type = 0;

int
fp_checksignal()
{
  if (fp_exception) {
    fp_exception = 0;
    _fpreset();
    _clear87();
    exit_cover(NC_FLOAT_EXCEPTION_W);
  }
  return 1;
}



#if defined(__MSC__)
static int cdecl fpehandler();

#elif defined(__BORLANDC__)
static void _USERENTRY fpehandler(int);

#else
#error No Error handlers declared for this OS/Machine
#endif

#endif

#ifdef TIMEHOOK

time_t cdecl time();


static double sttime;        /* start time for session */
static double win_clock(void);

#endif


/* routine to open a file. Converts internal coding of mode and type
  to stdio conventions.

  ARGUMENTS:

  flnm  - a C string containing the name of the file to be opened. The name
          is assumed to be in a format suitable to the host system or else
    this routine has to massage it.

 modechar - one of:
            'r'     open for input.  The file must exist.
      'w'     create new file for output.
      'a'     append to existing file, or create if file does not
              exist writes appends to the end of the file.
            'c'     open for communications
            'd'     open for direct read/write, with cursor at the end.
      'i'     open an index file.

 type - one of:
            't'     text; file is a stream of "characters"; that is,
              whatever the host uses to represent characters
        typed on the terminal.
            'b'     binary; file is a stream of machine addressable objects.
  The type distinction is irrelevant on Unix but not neccessarily on other
  systems. It is used to assist in portability. */

FILE       *
openfile(char *flnm, char modechar, char type)
{
  FILE       *fnm;
  char       *mode;

  if (modechar == 'r') {
    if (type == 'b')         /* binary */
      mode = "rb";
    else
      mode = "r";            /* text */
  }
  else if (modechar == 'w') {
    if (type == 'b')
      mode = "wb";
    else
      mode = "w";
  }
  else if (modechar == 'a') {
    if (type == 'b')
      mode = "ab";
    else
      mode = "a";
  }
  else if (modechar == 'c') {/* open for communications */
    if (access(flnm, 00) == 0) {  /* file must exist */
      if (type == 'b')
        mode = "r+b";
      else
        mode = "r+";
    }
    else {
      if (type == 'b')       /* file is created */
        mode = "w+b";
      else
        mode = "w+";
    }
  }
  /* this section of code is different from Unix. Under Unix, mode a+ allows
   * write  within a file with intervening fseek --. With Microsoft C v.5.
   * only appends can be performed with files opened with a+.  No data can be
   * over written even with intervening fseek */
  else if (access(flnm, 00) == 0) { /* file must exist */
    if (type == 'b')
      mode = "r+b";
    else
      mode = "r+";
  }
  else {
    if (type == 'b')         /* file is created */
      mode = "w+b";
    else
      mode = "w+";
  }
  if ((fnm = fopen(flnm, mode)) == NULL)
    /* NULL is the return code in stdio if an open fails */
  {
#ifndef NTVERSION
    errmsgptr = sys_errlist[errno];
#endif
    /* errno is set by Unix if the open fails */
    return (OPENFAILED);
  }
  return (fnm);
}


void
closefile(FILE * file)
{
  fclose(file);
}

/* routine to indicate where the file pointer is located */

long
fileptr(FILE * file)
{

  /* Unlike the Unix system a fseek must be used here since the file ptr will
   * always be at the beginning of the file.  This is because in opening the
   * direct access file we have not used the append mode here. */

  int         result;

  result = fseek(file, 0L, SEEK_END);
  if (result == 0)
    return ((long) ftell(file));
  else {
    exit_cover(NC_FILEPTR_W);
    return (0L);             /* fake return */
  }
}


/* routine to write n chars to file with or without newline */

int
writechars(FILE * file, char *buf, nialint n, int nlflag)
{
  if (keeplog && (file == stdout))
    writelog(buf, n, nlflag);

  checksignal(NC_CS_OUTPUT);

  if (file == stdout) {
    NC_Write(buf, n, nlflag);
  }
  else {
    int         i;

    for (i = 0; i < n; i++)
      putc(buf[i], file);
    if (nlflag)
      putc('\n', file);
    fflush(file);
  }
  return (n);
}


/* routine to read a contiguous block of n machine-addressable
   items from a file. For all known systems n is expressed in bytes.
   The file may be text or binary.
   If seekflag is true then a seek to position pos from the beginning (dir=0)
   or the end (dir=2) is done prior to reading.
   */

int
readblock(FILE * file, char *buf, long n, char seekflag, long pos, char dir)
{
  nialint     cnt;
  int         flag;

  clearerr(file);
  if (seekflag) {
    if (fseek(file, pos, dir) != 0) {
      errmsgptr = "fseek error in readblock";
      return (IOERR);
    }
  }
  cnt = fread(buf, 1, (int) n, file);

  flag = ferror(file);
  if (flag) {                /* Error in stdio */
    errmsgptr = sys_errlist[flag];
    return (IOERR);
  }
  if (feof(file)) {          /* EOF */
    errmsgptr = "eof encountered";
    return (EOF);
  }
  return (cnt);
}


/* routine to do a block file write with optional seek */

int
writeblock(FILE * file, char *buf, long n, char seekflag, long pos, char dir)
{
  nialint     cnt;
  int         flag;

  clearerr(file);
  if (seekflag) {
    if (fseek(file, pos, dir) != 0) {
      errmsgptr = "fseek error in writeblock";
      return (IOERR);
    }
  }
  cnt = fwrite(buf, 1, (int) n, file);
  flag = ferror(file);
  if (flag) {                /* Error in stdio */
    errmsgptr = sys_errlist[flag];
    return (IOERR);
  }
  if (feof(file)) {          /* EOF */
    errmsgptr = "eof encountered";
    return (EOF);
  }
  return (cnt);
}


#ifdef COMMAND

/* routine to execute a host command */

int
command(char *string)
{
  int         res;

  errno = 0;

#if defined(WINDOWS_SYS)
#ifdef WIN32GUI
/* special code to execute host command in Q'Nial for Windows */
  {
    static char keepmessage[512];
    LPSTR       lpMessageBuffer;
    BOOL        rc;
    DWORD       dwVersion;
    HANDLE      in,out;
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    char       *tmpchar;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(STARTUPINFO);
    si.lpReserved  = NULL;
    si.lpTitle     = NULL;
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;
    si.lpDesktop   = NULL;
    /*si.dwFlags     = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;*/
    si.dwFlags     = STARTF_USESHOWWINDOW;
/*    si.wShowWindow = SW_HIDE; */
/* The above line would cause a zombi process if you hosted
   a process that did not terminate */
    si.wShowWindow = SW_SHOW;
    si.hStdInput   = in;
    si.hStdOutput  = out;
    si.hStdError   = out;

    /* This silly contortion adds an extra space at the
     * end of the command line.  Often, if you issue a single
     * word command (no whitespace), then it fails, if you
     * issue the same command with a space (at the nial level)
     * then it works.  So....I'll put that same logic here */
    tmpchar = malloc(strlen(string)+1);
    if (!tmpchar) exit_cover(NC_MEM_ERR_F);
    strcpy(tmpchar,string);
    strcat(tmpchar," ");

/*  CreatePipe(&out,&in,NULL,0); */
    rc = CreateProcess(NULL,
                       tmpchar,
                       NULL,
                       NULL,
                       FALSE,
                       NORMAL_PRIORITY_CLASS,
                       NULL,
                       NULL,
                       &si,
                       &pi);
    free(tmpchar);
    if (rc) {
      WaitForSingleObject(pi.hProcess, INFINITE);
      // Close process and thread handles.
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      res = NORMALRETURN;
    }
    else {
      res = GetLastError();
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    res,
                    MAKELANGID(LANG_NEUTRAL,
                               SUBLANG_DEFAULT),  // The user default
      // language
                    (LPTSTR) & lpMessageBuffer,
                    0,
                    NULL);
      strncpy(keepmessage, lpMessageBuffer, 511);
      LocalFree(lpMessageBuffer);
    }
  }
#ifdef OMITTED
  res = WinExec(string, SW_SHOW);
  if (res < 31) {
    switch (res) {
      case 0:
          errmsgptr = "The system is out of memory or resources";
          break;
      case ERROR_BAD_FORMAT:
          errmsgptr = "The .EXE file is invalid (non-Win32 .EXE or error in .EXE image)";
          break;
      case ERROR_FILE_NOT_FOUND:
          errmsgptr = "The specified file was not found";
          break;
      case ERROR_PATH_NOT_FOUND:
          errmsgptr = "The specified path was not found";
          break;
      default:
          errmsgptr = "Unknown reason for failure of host";
    }
  }
  else
    res = NORMALRETURN;
#endif
#else
  res = system(string);
  if (res != NORMALRETURN) {
    if (errno == 0)
      errmsgptr = "command failed";
    else
      errmsgptr = sys_errlist[errno];
  }
#endif
#else
#error Please define a system command for this system/compiler
#endif
  return (res);
}

/* editor interface */


void
calleditor(char *flnm)
{
  char       *editor;
  char        editormsg[80];

  /* under UNIX and Windows console versions we let the user choose his editor by a setenv using the
   * call on the host. The default is established at generation time. */

  editor = getenv("EDITOR");
  if (editor == NULL)
    editor = DEFEDITOR;
  strcpy(editormsg, editor);
  strcat(editormsg, " ");
  strcat(editormsg, flnm);
  command(editormsg);

}

#endif             /* COMMAND */


#ifdef TIMEHOOK              /* ( */

/*  Timestamp is an expression that returns a 26 character string
    which contains the time and date.
    This function requires the unix utilities 'time'  and  'ctime'.
    */


void
get_tstamp(char *timebuf)
{
  time_t      etime;
  char       *timestring;

  /* etime gets the elapsed time in seconds  */
  time(&etime);

  /* timestring gets the character string corresponding to etime  */
  timestring = ctime(&etime);
  strcpy(timebuf, timestring);
  timebuf[24] = '\0';        /* throw away the standard newline char */
}

#ifdef WINDOWS_SYS
#include "values.h"
#include "mapiutil.h"
static
double
win_clock()
{
  OSVERSIONINFO vi;
  vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&vi);
  /* Do we have NT ? If so then we can use real process information
   * like the Unix version of this */
  if (vi.dwPlatformId == VER_PLATFORM_WIN32_NT)
  {
    double t;
    FILETIME ct,et,kt,ut;
    SYSTEMTIME st;
    GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut);

    FileTimeToSystemTime(&ut,&st);
    t = ((double)(((st.wHour*60)+st.wMinute)*60) + st.wSecond) +
         ((double)st.wMilliseconds/1000);
    return(t);
  } else {
     clock_t     ticks = clock();
     double      elapsed = ticks * 1.0 / CLK_TCK;
     return(elapsed);
  }
}
#endif

/* use the ASNI C routine clock() to get number of ticks and convert
   it to seconds, rounded to hundredths. */

double
get_cputime()
{
#ifdef WINDOWS_SYS
  double     elapsed = win_clock();
#else
  clock_t     ticks = clock();
  double      elapsed = ticks * 1.0 / CLK_TCK;
#endif

  /* nprintf(OF_DEBUG,"ticks %ld elapsed %10.2f sttime
   * %10.2f\n",ticks,elapsed,sttime); */
  return (floor(elapsed * 100.) / 100. - sttime);
}

/* routine to start the time clock at zero (this is redundant since
   the ticks counter should be initialized to zero on startup of
   a C program.) */

void
inittime()
{
  sttime = 0.;
  sttime = get_cputime();
}

#endif             /* TIMEHOOK ) */


/* routine to set up path to Nial root directory */


 /* Win32 (Windows) does not use Environtmental variables any more.  The Nial
  * root is set by the DLL (or Library) calling application. The console
  * versions get nialrootname from the environment before calling
  * NC_InitNial. */

void
setup_nialroot(char *nialroot)
{

#if defined(WINDOWS_SYS)
  if (strlen(nialroot) &&
      (nialroot[strlen(nialroot) - 1] != '\\') &&
      (strlen(nialroot) > 1))
    strcat(nialroot, "\\");
#else
#error Nialroot is not defined
#endif
}

/* routine to handle floating point exceptions */




#ifdef __BORLANDC__
int         _RTLENTRY
_matherr(struct exception * e)

#else
#error You Need a prototype for this error handler for this OS/MACHINE
#endif

{

  switch (e->type) {
 case DOMAIN:
        exit_cover(NC_MATH_LIB_DOMAIN_ERROR_W);
        break;
    case SING:
        exit_cover(NC_MATH_LIB_SING_ERROR_W);
        break;
    case OVERFLOW:
        exit_cover(NC_MATH_LIB_OVERFLOW_ERROR_W);
        break;
    case UNDERFLOW:
        exit_cover(NC_MATH_LIB_UNDERFLOW_ERROR_W);
        break;
    case TLOSS:
        exit_cover(NC_MATH_LIB_TLOSS_ERROR_W);
        break;
    case PLOSS:
        exit_cover(NC_MATH_LIB_PLOSS_ERROR_W);
        break;
  }
  return (1);
}


#ifdef __BORLANDC__
int         _RTLENTRY
_matherrl(struct _exceptionl * e)
{
  return (_matherr((struct exception *) e));
}

#endif


#ifdef __BORLANDC__
typedef void _USERENTRY(*sigfptr) (int);
#endif


#ifdef __BORLANDC__
static void _USERENTRY
fpehandler(int n)
{
  signal(SIGFPE, (sigfptr) fpehandler);
#ifdef FP_EXCEPTION_FLAG
  fp_exception = 1;
  fp_type = n;
#else
  exit_cover(NC_FLOAT_EXCEPTION_W);
#endif
}

#elif defined(__MSC__)
static int  cdecl
fpehandler()
{
  signal(SIGFPE, (sigfptr) fpehandler);
#ifdef FP_EXCEPTION_FLAG
  fp_exception = 1;
#else
  exit_cover(NC_FLOAT_EXCEPTION_W);
#endif
}

#else
#error No floating point error handler for this OS/Machine
#endif


/* routine to set up the signal handlers */

void
initfpsignal()
{                            /* to catch floating point exceptions */
/*  signal(SIGFPE, SIG_IGN);*/
  signal(SIGFPE, (sigfptr) fpehandler);
}



/* sleep function  */


void
isleep(void)
{ nialptr x;
  x = apop();
  if (kind(x)!=inttype)
  { apush(makefault("arg to sleep not an integer"));
  }
  else
  { int n = intval(x);
    sleep(n);
    apush(Nullexpr);
  }
  freeup(x);
}
