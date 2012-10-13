
/*=============================================================

  MODULE   UNIXIF.C

  COPYRIGHT NIAL Systems Limited  1983-2005

  Contains all UNIX specific interface routines

================================================================*/

#ifdef UNIXSYS

/* Q'Nial file that selects features and loads the xxxspecs.h file */


/* standard library header files */

#include "switches.h"


#define SIGLIB
#define TIMELIB
#define STDLIB
#define SJLIB
#define IOLIB
#define MATHLIB
#define STLIB

#include "sysincl.h"


#include "if.h"
#include "qniallim.h"
#include "lib_main.h"
#include "absmach.h"

#include "fileio.h"          /* for writelog */
#include "coreif_p.h"        /* for NC_Write */
#include "mainlp.h"          /* for exit_cover */

#include "messages.h"


FILE       *stream;


#ifdef TIMEHOOK
static double sttime;        /* start time for session */
#endif

static void fpehandler(int signo);
static void restoresignals(void);

#ifndef LINUX
extern int  errno;
#endif


FILE       *
openfile(char *flnm, char modechar, char type)
 /* flnm is a C string containing the name of the file to be opened. The name
  * is assumed to be in a format suitable to the host system or else this
  * routine has to massage it. For Unix all file names are assumed to be in
  * the appropriate form. */

 /* modechar may be one of: 'r'     open for input.  The file must exist. 'w'
  * create new file for output. 'a'     append to existing file, or create if
  * file does not exist. Writes will cause data to be appended to the end of
  * the file. 'd'     open for direct read/write, with cursor at the end. 'i'
  * open an index file.
  * 
  */

 /* type may be one of:
  * 
  * 't'     text; file is a stream of "characters"; that is, whatever the host
  * uses to represent characters typed on the terminal.
  * 
  * 'b'     binary; file is a stream of machine addressable objects.
  * 
  * The type distinction is irrelevant on Unix but not necessarily on other
  * systems. It should be used to assist in portability.
  * 
  * Under Unix, mode a+ allows write  within a file with intervening fseek --.
  * With Microsoft C v.5. only appends can be performed with files opened
  * with a+.  No data can be over written even with intervening fseek */

{
  FILE       *fnm;
  char       *mode;

  if (modechar == 'r')
    mode = "r";
  else if (modechar == 'w')
    mode = "w";
  else if (modechar == 'a')
    mode = "a";
  else if (modechar == 'd' || modechar == 'i') {
    if (access(flnm, 00) == 0)
      mode = "r+";
    else
      mode = "w+";

    fnm = fopen(flnm, mode);
    if (fnm == NULL)
      /* NULL is the return code in stdio if an open fails */
    {
      errmsgptr = strerror(errno);
      /* errno is set by Unix if the open fails */
      return (OPENFAILED);
    }
    fseek(fnm, 0L, 2);       /* go to end of file */
    return (fnm);
  }
  else
    mode = "a+b";
  if ((fnm = fopen(flnm, mode)) == NULL)
    /* NULL is the return code in stdio if an open fails */
  {
    errmsgptr = strerror(errno);
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


long
fileptr(FILE * file)
{
  fseek(file, 0L, 2);
  return ((long) ftell(file));
}



/* Writes characters from buf onto the ioport file.  If nlflag is
   TRUE a newline indication is appended. An EOF encountered in a
   write is treated as an error.
   */

nialint
writechars(FILE * file, char *buf, nialint n, int nlflag)
{
  static int calls = 0;     /* internal counter used for breakchecking */

  if (keeplog && (file == stdout))
    writelog(buf, n, nlflag);

  /* every 25 calls check for a signal (approx one screen full) */
  if (++calls > 25) {
    calls = 0;
    checksignal(NC_CS_OUTPUT);
  }

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

nialint
readblock(FILE * file, char buf[], long n, int seekflag, long pos, int dir)
{

  /* This routine is used to read a contiguous block of n machine-addressable
   * items from a file. For all known systems n is expressed in bytes. The
   * file may be text or binary. If seekflag is true then a seek to position
   * pos from the beginning (dir=0) or the end (dir=2) is done prior to
   * reading. */
  nialint     cnt;
  int         flag;

  clearerr(file);
  if (seekflag) {
    if (fseek(file, pos, dir) != 0) {
      errmsgptr = "fseek error in readblock";
      return (IOERR);
    }
  }
  cnt = fread(buf, 1, n, file); /* cnt of 0 indicates error */

  flag = ferror(file);
  if (flag) {                /* Error in stdio */
    errmsgptr = strerror(flag);
    return (IOERR);
  }
  if (feof(file)) {          /* EOF */
    errmsgptr = "eof encountered";
    return (EOF);
  }
  return (cnt);
}


nialint
writeblock(FILE * file, char buf[], long n, int seekflag, long pos, int dir)
{
  /* This is the block file write with optional seek */
  nialint     cnt;
  int         flag;

  clearerr(file);
  if (seekflag) {
    if (fseek(file, pos, dir) != 0) {
      errmsgptr = "fseek error in writeblock";
      return (IOERR);
    }
  }
  cnt = fwrite(buf, 1, n, file);

  flag = ferror(file);
  if (flag) {                /* Error in stdio */
    errmsgptr = strerror(flag);

    return (IOERR);
  }
  if (feof(file)) {          /* EOF */
    errmsgptr = "eof encountered";
    return (EOF);
  }
  return (cnt);
}

#ifdef COMMAND               /* ( */

int
command(char *string)
{
  int         res;

  errno = 0;
  restoresignals();
  if ((res = system(string)) != NORMALRETURN) {
    if (errno == 0)
      errmsgptr = "command failed";
    else
      errmsgptr = strerror(errno);
  }
  initunixsignals();
  return (res);
}


/* editor interface */


void
calleditor(char *flnm)
{
  char       *editor;
  char        editormsg[80];

  /* under UNIX we let the user choose his editor by a setenv using the call
   * on the host. The default is established at generation time. */

  editor = getenv("EDITOR");
  if (editor == NULL)
    editor = DEFEDITOR;
  strcpy(editormsg, editor);
  strcat(editormsg, " ");
  strcat(editormsg, flnm);
  command(editormsg);
}

#endif  /* COMMAND */ /* ) */



/*  Time is an expression that returns a pair of integers.  The first item
    is user time in 1/60'th of second units.  The second item is the
    system time in 1/60'th of second units
    This function requires the unix utility 'times'.
    */
#ifdef TIMEHOOK              /* ( */

/*  Timestamp is an expression that returns a 26 character string
    which contains the time and date.
    This function requires the unix utilities 'time'  and  'ctime'.
    */

char       *ctime();

void
get_tstamp(char *timebuf)
{
  long        etime;
  char       *timestring;

  /* etime gets the elapsed time in seconds  */
  time(&etime);

  /* timestring gets the character string corresponding to etime  */
  timestring = ctime(&etime);
  strcpy(timebuf, timestring);
  timebuf[24] = '\0';        /* throw away the standard newline char */
}

#ifndef SOLARIS2
double
get_cputime()
{
  struct tms  structtime;
  double      total;

  times(&structtime);
  total = (floor((structtime.tms_stime + structtime.tms_utime) * 1000. / HZ)
           / 1000.) - sttime;
  return (total);
}

#else


double
get_cputime()
{
  struct lwpinfo structtime;
  double      total,
              floor();
  struct timespec usertime;

  _lwp_info(&structtime);
  usertime = structtime.lwp_utime;

  total = (double) usertime.tv_sec + ((double) usertime.tv_nsec / 1e9);

  /* total = (floor(total * 1000.) / 1000.) - sttime; */
  total -= sttime;
  return (total);
}

#endif             /* SOLARIS2 */

void
inittime()
{
  /* start the time clock at zero */
  sttime = 0.;
  sttime = get_cputime();
}

#endif             /* ) */


#undef link                  /* to remove the name conflict with the macro in
                              * memmacs.h */

#ifdef OMITTED               /* MAJ needed this for some old compiler */

void
rename(char *old, char *new)
{
  unlink(new);
  link(old, new);
  unlink(old);
}

#endif


void
setup_nialroot(char *nialroot)
{
  char       *nroot;

  nialroot[0] = '\0';
  nroot = getenv("NIALROOT");
  if (nroot != NULL) {
    strcpy(nialrootname, nroot);
    if ((int) strlen(nialrootname) > 0)
      if (nialrootname[strlen(nialrootname)-1] != '/')
        strcat(nialrootname, "/");
  }
}

#ifdef UNIXSYS

#ifdef JOBCONTROL
/* Name:        nial_suspend
   Function:    suspend process, if windows set then refresh screen on return
   Algorithm:   1/ clear screen
                2/ make a record of current terminal mode and put terminal
                   into sensible mode.
                3/ suspend
                4/ on return, clear screen and refresh window
*/


void
nial_suspend(int signo)
{
  fflush(stdout);
  kill(getpid(), SIGSTOP);
}

#endif             /* JOBCONTROL */


void
initunixsignals()
{
#ifdef JOBCONTROL
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
  /* removed SIGHUP ignore. causes the nial task to stay around when windows
   * are killed in SUN-OS. signal (SIGHUP,SIG_IGN); */

  signal(SIGSTOP, SIG_DFL);
#endif

  /* to catch control Z, so we can restore the screen */
#ifdef JOBCONTROL
  signal(SIGTSTP, nial_suspend);
#endif

  /* turn off the abort key signal to prevent accidental aborts */
#ifdef ABORTSIGNAL
  signal(SIGQUIT, SIG_IGN);
#endif

  /* All other signals are treated in the default way. Attempts to ignore
   * more signals leads to unusual behaviour. */
}

static void
restoresignals()
{
  int         i;

  for (i = 0; i < NSIG; i++)
    signal(i, SIG_DFL);
}

#endif


void
initfpsignal()
{                            /* to catch floating point exceptions */
  signal(SIGFPE, fpehandler);
}

#ifdef FP_EXCEPTION_FLAG
static int  fp_exception = 0;
static int  fp_type = 0;

int
fp_checksignal()
{
  if (fp_exception) {
    fp_exception = 0;
    exit_cover(NC_FLOAT_EXCEPTION_W);
  }
  return 0;
}

#endif

static void
fpehandler(int signo)
{
  signal(SIGFPE, fpehandler);
#ifdef FP_EXCEPTION_FLAG
  fp_exception = 1;
  fp_type = 3456;            /* replaces n in DOS32if version */
#else
  if (signo != SIGFPE)
    return;
  exit_cover(NC_FLOAT_EXCEPTION_W);
#endif
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

#endif             /* UNIXSYS */
