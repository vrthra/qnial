/*==============================================================

  QNIALLIM.H:  

  COPYRIGHT NIAL Systems Limited  1983-2005

  Implementation Limits common to all versions of Q'Nial

================================================================*/

#ifndef _QNIALLIM_H_
#define _QNIALLIM_H_


/* define stack full test here for each operating system / compiler */

#ifdef WINDOWS_SYS

#if defined(__BORLANDC__)

/* set stack overflow size depending on type of version being built */

/* These values have been determined experimentally.  Nial
  calls the CSTACKFULL macro every once and a while, and thus
  Nial may use up the stack between calls to CSTACKFULL.  So
  CSTACKFULL must use a C_STACK_OVERFLOW_SIZE big enough that
  NIAL wont use that amount of stack between calls to CSTACKFULL.
 
   This varies from system to system. In particular, when running under
   the Windows GUI, the Core may allow a significant amount of windows
   processing during interrupt checks, and the OWL GUI code can make
   significant use of the stack (more so that a simple console version).
   This is why the C_STACK_OVERFLOW_SIZE is so large for the WIN32GUI
   Windows version.
 */

#  if defined(CONSOLE) || defined(PROFDLL)
#    define C_STACK_OVERFLOW_SIZE 10000
#  elif defined(WIN32GUI)
#    define C_STACK_OVERFLOW_SIZE 120000
#  else
#    error set C_STACK_OVERFLOW_SIZE for this type of version
#  endif

/* define stack full test */

#  define CSTACKFULL  (stackavail() < C_STACK_OVERFLOW_SIZE)

#else
#  error set C_STACK_OVERFLOW_SIZE  and CSTACKFULL for this compiler on WINDOWS
#endif


#elif defined(UNIXSYS)

#  define CSTACKFULL false

#else
#  error Please define a CSTACKFULL macro here for the OS
#endif


#define dfstklim  10000
 /* initial nial stack size */

#define dfatomtblsize 10000
 /* atom table size */

#define dfmemsize  2000000    /* default workspace size in units */

#define minmemsize (dfatomtblsize * 4 + 20000)


#define MAXPGMLINE 70
 /* maximum length of token or of descaned output line */

#define MAXFILES 203
 /* maximum number of files managed by Q'Nial. 3 are used by the system files
  * stdin, stdout, stderr. */

#define MAXIDSIZE 80
 /* maximum number of characters in an identifier */


#define NOBNAMES 500
 /* upper limit on the number of basic names , 
    increase if applytab overflows in basics.c */

#define NOINTS 64
 /* number of small ints retained uniquely */


#define INBUFSIZE 500        /* size requested from Cstack area for input,
                                used for buffering data as it is read into
                                Q'Nial from ascii files */

#define GENBUFFERSIZE 4096
 /* used to size gcharbuf. Limits size of phrases sent in and out using
    read/write array  */


#define TOKENSTREAMSIZE 10000
 /* size of array used to hold tokens in scan, it also grows in amounts of
  * this size. */

#define STATELIMIT 5000
 /* depth of expression states in parse stack */

#define LISTSIZE 500
 /* list holder in parse grows in this amount */

#define EXPRSTACKSIZE 500
 /* initial and increment size for expression stack used in parse to gather
  * strands and compositions as well as expressions in general */

#define INITCBUFFERSIZE 1000
 /* initial size for Cbuffer area */

#define MINHEAPSPACE (TOKENSTREAMSIZE + 500)

#define INPUTSIZELIMIT 5000
 /* upper limit on length of lines joined on input */

#define MAXNORECOVERS 10
 /* upper limit on number of times recover can hit on a single pass through
  * CommandInterpret */

#define MAXPROMPTSIZE 255
 /* maximum size for prompt. Can't be set longer. */

#define MAXLOGFNMSIZE 255
 /* maximum size for log file name. Longer ones are truncated. */

#define MAXTLMSGSIZE 255
 /* maximum size for top level message. This is always an internal error
  * message, so the above size is safe. */

#endif
