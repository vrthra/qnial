/*============================================================

  MODULE   MAIN_STU.C

  COPYRIGHT NIAL Systems Limited  1983-2005

Contains the code specific to a console version of Q'Nial.

================================================================*/

#include "switches.h"
#ifdef CONSOLE  /* ( */

#ifdef WINDOWS_SYS
#define WINDOWSLIB
#define IBMPCCHARS
#endif


#define SIGLIB
#define STLIB
#define STDLIB
#define IOLIB
#define SJLIB
#include "sysincl.h"

#include "qniallim.h"
#include "lib_main.h"
#include "coreif_p.h"        /* for core interpreter interface */
#include "if.h"
#include "mainlp.h"          /* for cleanup_ws() */
#include "absmach.h"         /* for decrrefcnt and freeup */




/* Console version uses IO_MODE.
   However we can build a version to test that the buffer method is working
   correctly. Define only one of the two following defines to test the desired IO method 
*/

/*
#define INTERNAL_BUF_MODE  
or
*/

#define IO_MODE


static int  awaitinginput;
static int  EOFsignalled;
static int  userbreakrequested;



static void CheckErr(int theErr);
static void print_syntax(void);
static int myCheckUserBreak(int);
static void myReadInput(void);

#ifdef UNIXSYS
static void controlCcatch(int signo);

#elif defined(__MSC__) 
static int cdecl controlCcatch();

#elif defined(__BORLANDC__)  
#ifdef __cplusplus
static void cdecl controlCcatch(int n);
#else
static void controlCcatch();
#endif

#else
#error You must define a controlCcatch routine for this compiler.
#endif             


static char inputline[INPUTSIZELIMIT];

/* routine myWrite writes n chars to stdout with or without newline.
   The coreif I/O routines use this for console output.
   It is used below to issue the prompt in the top level loop. */

static void
myWrite(void *theArg, char *theStr)
{
  printf("%s", theStr);
  fflush(stdout);
}


/* routine myReadString is called below in myReadInput to get the inputline.
   It is used in the coreif I/O routines to get input for ireadstring(). 
   It returns an allocated string whose pointer is kept statically so the space
   can be freed on subsequent calls. The newline is removed if encountered.
   The input string length is limited to size INPUTSIZELIMIT. This could limit
   the use of the nial executable as a filter under Unix.
   The code assumes fgets will work as expected on all operating systems.
*/


static char *
myReadString(void *infp)
{
  char        buffer[INPUTSIZELIMIT];
  static int  firsttime = true;
  static char *result;

  FILE       *fp = (FILE *) infp;

  if (!firsttime)            /* free the space allocated last time */
    free(result);
  clearerr(fp);
  fgets(buffer, INPUTSIZELIMIT, fp); /* leaves the newline in place.
                                           remove it in the copy below */
  EOFsignalled = feof(fp);
  /* EOFsignalled = false;*/
  /*printf("EOFsignalled %d userbreakrequested %d \n",EOFsignalled,userbreakrequested);*/
  result = (char *) malloc(strlen(buffer) + 1);
  if (!result) {             /* error */
    return (NULL);
  }
  if (strlen(buffer) < INPUTSIZELIMIT - 1) /* new line is present */
    buffer[strlen(buffer) - 1] = '\0'; /* replaces the newline */
  strcpy(result, buffer);
  return (result);
}

/* we may need to specialize the myReadChar routine for different 
   operating systems since getchar is not always standard 
   (e.g. on Win32 GUI versions).
*/

#pragma argused
static char
myReadChar(void *theArg)
{
#if defined(UNIXSYS) 
  return (char) getchar();
#elif (defined(WINDOWS_SYS))
  return (char) getchar();
#else
#error define myReadChar for this system
#endif
}


/* local globals used for the interface to core routines */

static long sessionSettings;
static long windSettings;
static long theContext;

/* the control as to whether this is a CGI version 
   or not is set statically by this switch.
*/

#ifdef CGINIAL
static int  iscgiversion = true;
#else
static int  iscgiversion = false;
#endif



#ifdef CMDLINE
char      **global_argv = NULL;
int         global_argc;
#endif


#ifdef WINDOWS_SYS
#  define MAX_FILENM_SIZE  _MAX_PATH
#else
#  define MAX_FILENM_SIZE 512
#endif



int
main(int argc, char *memin[])
{
  int         i,
              rc;
  int         irc;
  int         messages = true,
#ifdef IBMPCCHARS
              box_chars = true;

#else
              box_chars = false;

#endif
  char        indefsfnm[MAX_FILENM_SIZE],
              inloadfnm[MAX_FILENM_SIZE];


/* set global variables */

  defssw = false;
  defsfnm = NULL;


  initmemsize = dfmemsize;
  quiet = false;
  expansion = true;
  quiet = false;
  nouserinterrupts = false;
  loadsw = false;
  triggered = true;
  debugging_on = true;
  diag_messages_on = false;
  interpreter_running = false;

#ifdef HISTORY
  lasteval = -1;
#endif

#ifdef DEBUG
  debug = false;
#endif

#ifdef PROFILE
  profile = false;
#endif

  /* Export the command line to the rest of the code */
#ifdef CMDLINE
  global_argv = memin;
  global_argc = argc;
#endif

/* MAJ: the following code is kept in case I decide to go back
   to having the console and cgi versions the same code.
*/


  if (iscgiversion) {
    quiet = true;
    messages = false;
    box_chars = false;
    triggered = false;
    nouserinterrupts = true;
  }

#ifdef UNIXSYS
  initunixsignals();
#endif


  /* process command arguments
    
     the allowed syntax is:
     
     [(+|-)size nnnn] [-defs filenm] [-q] [-m] [-b] [-s] [-d] [workspacename] */


  if (iscgiversion) 
/*  This code will allow the cgi version to accept *.nws, *.ndf or
    *.nfm files on the command line.  No other parameters are available.
*/
  { 
    if (argc >= 2) 
    { int offset = (memin[1][0] == '\"') ? -1 : 0;
      if (STRNCASECMP((strlen(memin[1]) + 
           memin[1] - 4 + offset), ".NDF", 4) == 0) 
      {  /* arg is a Nial definition file name */
        defssw = true;
        strcpy(indefsfnm, memin[1]);
      }
      else 
      if (STRNCASECMP((strlen(memin[1]) + 
           memin[1] - 4 + offset), ".NWS", 4) == 0) 
      { /* arg is a Nial workspace file name */
        loadsw = true;
        strcpy(inloadfnm, memin[1]);
      }
    }
  }
  else 
  { i = 1;
    while (i < argc) {
      if (strcmp(memin[i], "-size") == 0) {
        /* set iniital size and allow expansion */
        initmemsize = atol(memin[i + 1]);
        if (initmemsize < minmemsize)
          initmemsize = minmemsize;
        expansion = true;
        i += 2;
      }
      else if (strcmp(memin[i], "+size") == 0) {
        /* set iniital size and disallow expansion */
        initmemsize = atol(memin[i + 1]);
        if (initmemsize < minmemsize)
          initmemsize = minmemsize;
        expansion = false;
        i += 2;
      }
      else 
      if (strcmp(memin[i], "-defs") == 0) {
        if (i < argc) {
          /* explicit defs file name given */
          defssw = true;
          strcpy(indefsfnm, memin[i + 1]);
          i += 2;
        }
        else {
          printf("missing definition file name after -defs option\n");
          exit(1);
        }
      }
      else if (strcmp(memin[i], "-q") == 0) {
        /* inhibit all output */
        quiet = true;
        i++;
      }
      else if (strcmp(memin[i], "-m") == 0) {
        /* do not display warning messages */
        messages = false;
        i++;
      }
      else if (strcmp(memin[i], "-d") == 0) {
        /* disable Nial level debugging */
        debugging_on = false;
        i++;
      }
      else
#ifdef IBMPCCHARS
      if (strcmp(memin[i], "-b") == 0) {
        /* do not use the IMB PC box characters */
        box_chars = false;
        i++;
      }
      else
#endif
      if (strcmp(memin[i], "-s") == 0) {
        /* suppress fault triggering */
        triggered = false;
        i++;
      }
      else if (strcmp(memin[i], "-h") == 0) {
        /* display the help syntax and exit */
        print_syntax();
        exit(1);
      }
      else if (memin[i][0] == '-') {
        /* invalid command line. display and exit. */
        printf("invalid command line option: %s", memin[i]);
        exit(1);
      }
      else {
        /* assume rest of line is a workspace file name */
        loadsw = true;
        strcpy(inloadfnm, memin[i]);
        i++;
      }
    }
  }

/* end of code to process arguments */

/* code to check if we are running a console version under WINDOWS. If we are 
   do not attempt to check for the keyboard, since it slows execution 
   outrageously. This has been switched out for now. Not needed since DOS version
   no longer supported.
 */

#ifdef OMITTED
  checkkeyboard = (getenv("WINDIR") == NULL ? true : false);
  /* printf("checkkeyboard %d\n", checkkeyboard); */
#endif


/* set the session variables */

  sessionSettings = NC_CreateSessionSettings();

  CheckErr(NC_SetSessionSetting(sessionSettings,
                                NC_WORKSPACE_SIZE, initmemsize));
  CheckErr(NC_SetSessionSetting(sessionSettings,
                                NC_EXPANSION, expansion));
  if (loadsw)
    CheckErr(NC_SetSessionSetting(sessionSettings,
                                  NC_INITIAL_WORKSPACE, (int) inloadfnm));
  if (defssw)
    CheckErr(NC_SetSessionSetting(sessionSettings,
                                  NC_INITIAL_DEFS, (int) indefsfnm));

  CheckErr(NC_SetSessionSetting(sessionSettings,
                                NC_QUIET, quiet));
  CheckErr(NC_SetSessionSetting(sessionSettings, NC_DEBUGGING_ON, debugging_on));
  CheckErr(NC_SetCheckUserBreak(sessionSettings, myCheckUserBreak));

  windSettings = NC_CreateWindowSettings();

  if (iscgiversion) /* avoid output by having no prompt in CGINIAL */
    CheckErr(NC_SetWindowSetting(windSettings, NC_PROMPT, (int) ""));

  /* The CGINIAL version is given a default 0 screen width setting, 
     so no lines are broken.  */

#ifdef WINDOWS_SYS
/* The following code is added, so that we can accurately set the 
   window's width.  */
  {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    CheckErr(NC_SetWindowSetting(windSettings, NC_SCREEN_WIDTH, 
       (iscgiversion ? 0 : ((csbi.srWindow.Right - csbi.srWindow.Left)))));
  }
#else
  CheckErr(NC_SetWindowSetting(windSettings, NC_SCREEN_WIDTH, (iscgiversion ? 0 : 79)));
#endif

  CheckErr(NC_SetWindowSetting(windSettings, NC_TRIGGERED, triggered));
  CheckErr(NC_SetWindowSetting(windSettings, NC_NOUSERINTERRUPTS, nouserinterrupts));

  CheckErr(NC_SetWindowSetting(windSettings, NC_MESSAGES, messages));
  CheckErr(NC_SetWindowSetting(windSettings, NC_BOX_CHARS, box_chars));


#ifdef INTERNAL_BUF_MODE
  theContext = NC_CreateIOContext();
  CheckErr(NC_SetIOMode(theContext, NC_INTERNAL_BUFFER_MODE));
  NC_SetBufferSize(5000, 1000);
#endif


#ifdef IO_MODE
  theContext = NC_CreateIOContext();
  CheckErr(NC_SetIOMode(theContext, NC_IO_MODE));
  CheckErr(NC_SetWriteCommand(theContext, myWrite, stdout));
  CheckErr(NC_SetReadStringCommand(theContext, myReadString, stdin));
  CheckErr(NC_SetReadCharCommand(theContext, myReadChar, stdin));
#endif


   /* get nialroot path from environment on console version */

  {
    char       *nroot;

    nialrootname[0] = '\0';
    nroot = getenv("NIALROOT");
    if (nroot != NULL)
      strcpy(nialrootname, nroot);
  }

  /* initialize the user break capability */ 
  signal(SIGINT, controlCcatch);


  /* initialize the core routines */
  irc = NC_InitNial(sessionSettings, windSettings, theContext);
  if (NC_IsTerminate(irc))
  {  /* initialization has signalled a termination */
     exit(0);
  }
  else
  {
    CheckErr(irc);
  }



#ifdef INTERNAL_BUF_MODE
  /* display the result in the internal buffer */
  printf("%s", NC_GetBuffer());
#endif

  /* avoid the top level loop in a RUNTIME ONLY or CGINIAL version */

#ifdef RUNTIMEONLY
  goto cleanup;
#endif

#ifdef CGINIAL
  goto cleanup;
#endif


  /* the top level loop */

  do {

retry:
    /* issue the prompt to the console */
    myWrite(stdout, NC_GetPrompt(windSettings));

    userbreakrequested = false; /* could have been set during latent call in
                                 * NC_InitNial() */
    /* get the input string */
    awaitinginput = true;
    myReadInput();
    /* check if the input included a Control C press */
    if (myCheckUserBreak(NC_CS_INPUT)) {
      /* printf("myCheckUserBreak returned true\n"); */ /* remove after testing in UNIX */
      /* make sure workspace is clean */
      cleanup_ws();
#ifdef HISTORY
      if (lasteval != -1) {
        decrrefcnt(lasteval);
        freeup(lasteval);
      }
      lasteval = (-1);
#endif
#ifdef DEBUG
      printf("userbreakrequested set during input at top level loop\n");
#endif
      userbreakrequested = false;
      goto retry;            /* to go around to the prompt again */
    }
    if (EOFsignalled)
    { /* the feof() code in Borland C returns nonzero if Ctrl-C has been
         pressed. This leads to a premature exit here. Removing the test
         causes nial.exe to loop forever if used as a filter. */
      goto cleanup;
    }

    awaitinginput = false;

    /* interpret the input */
    rc = NC_CommandInterpret(inputline, windSettings, theContext);

    /* check if there was a Control C press during CI execution */
    if (myCheckUserBreak(NC_CS_NORMAL)) {
      cleanup_ws();
#ifdef DEBUG
      printf("userbreakrequested set during input at top level loop\n");
#endif
      userbreakrequested = false;
      goto retry;            /* to go around to the prompt again */
    }

#ifdef INTERNAL_BUF_MODE
    /* display the output if buffered */
    printf("%s\n", NC_GetBuffer());
#endif

    /* check the return code from CI */

    if (!(NC_IsNoError(rc) || NC_IsWarning(rc)
          || NC_IsBreak(rc) || NC_IsTerminate(rc))) {
      /* display error information to console */
      printf("Return Code: %s ErrorTypeName : %s\n", NC_ErrorTypeName(rc),
             NC_ErrorMessage(rc));
    }

  } while (NC_IsWarning(rc) || NC_IsNoError(rc) || NC_IsBreak(rc));

  /* end of top level loop code */

cleanup:

  /* stop the interpreter and clean up its resource usage */
  NC_StopNial();

  /* clean up the core interface resources */

  CheckErr(NC_DestroyIOContext(theContext));
  CheckErr(NC_DestroyWindowSettings(windSettings));
  CheckErr(NC_DestroySessionSettings(sessionSettings));


  /* return from the console version of Q'Nial */

  /* getchar(); */ 
      /* MAJ: put in the above getchar here to delay exit so that debug output can be
         seen when running from the Borland interface */
  exit(0);
  return 1;                  /* to satisfy syntax error checking */
}

/* general error checking routine to watch for a terminal error on calls
   to the core interface. */

static void
CheckErr(int theErr)
{
  if (NC_IsTerminate(theErr) ||
      NC_IsStartupError(theErr) ||
      NC_IsInternal(theErr) ||
      NC_IsFatal(theErr)) {
    printf("Terminated with a %s: (%s)\n", NC_ErrorTypeName(theErr),
           NC_ErrorMessage(theErr));
    exit(theErr);
  }
  return;
}

/* routine to display the command line syntax when -h is used */

static void
print_syntax()
{
  fprintf(stderr, "\n"
  "   SYNTAX: nial [Wsname] [(+|-)size Wssize] [-defs Filename] [-q] [-s] \n"
          "               [-b] [-m] [-d] [-h]\n"
          "\n"
          "Wsname 	The named workspace is loaded instead of the clearws.nws file that\n"
          "	is normally entered or created on invocation. \n"
          "\n"
          "-size Wssize	Begin with a workspace size of Wssize words. The \n"
          "		workspace can expand if space is available.\n"
          "\n"
     "+size Wssize	Fix the workspace size at Wssize words. This option is\n"
          "		used when very large workspaces are needed.\n"
          "\n"
    "-defs Filename	After loading the initial workspace and executing its\n"
          "		Latent expression, load the file Filename.ndf using loaddefs\n"
          "		without displaying it.\n"
          "\n"
          "-q	Start quietly without displaying the banner and other default output.\n"
          "\n"
          "-s	Supress the triggering mechanism that interrupts computation\n"
          "	whenever a fault is created.\n"
          "\n"
          "-m      Do not display warning messages about internal buffer size changes.\n"
          "\n"
          "-d      Turn off debugging features (improves performance)\n"
          "\n"
     "-b	Use normal ASCII characters for box diagrams. (PC versions only)\n"
          "\n"
          "-h      Display command line syntax.\n"
          "\n"
          "	   Examples:\n"
          "	nial\n"
          "\n"
          "	nial -q appl\n"
          "\n"
          "	nial +size 500000 -defs newfns\n"
  );
}


/* routines to support control C catch in console version */


/* routine that checks for a Control C being set */

#pragma argused
static int
myCheckUserBreak(int code)
{                            
  if (userbreakrequested) {
    /* user break request has been encountered. Turn off flag */
    userbreakrequested = false;
    return true;             
  }
  return false;     
}

/* handler for user interrupts signalled with Control C */


#ifdef UNIXSYS
static void
controlCcatch(int signo)
{
  signal(SIGINT, controlCcatch);
  if (signo != SIGINT)
    return;
  userbreakrequested = true;
}

#elif defined(__MSC__)       /* use usual signal version */

static int  cdecl
controlCcatch()
{
  signal(SIGINT, controlCcatch);
  userbreakrequested = true;
}


#elif defined(__BORLANDC__)  /* use usual signal version */
#ifdef __cplusplus
static void cdecl
controlCcatch(int n)
#else
static void
controlCcatch()
#endif
{
  signal(SIGINT, controlCcatch);
  userbreakrequested = true;
}

#else
#error You must define a controlCcatch routine for this compiler.
#endif             /* end of compiler chain */



/* routine to accept multiple lines as input */

/* myReadInput reads one or more lines from standard input using the '\'
   symbol as an escape newline symbol.
*/

static void
myReadInput()
{
  int         lastpos,
              cont = true;

  inputline[0] = '\0';
  while (cont) {
    char       *str = myReadString(stdin);  /* keeps the new line */

    if (strlen(str) > 0) {
      lastpos = strlen(str) - 1;
      if (str[lastpos] == '\\') /* remove the symbol */
        str[lastpos] = '\0';
      else                   /* stop the loop */
        cont = false;
      strcat(inputline, str);
    }
    else                     /* stop the loop */
      cont = false;
    free(str);               /* since it is a temporary value */
  }
}



#endif /* CONSOLE  ) */
